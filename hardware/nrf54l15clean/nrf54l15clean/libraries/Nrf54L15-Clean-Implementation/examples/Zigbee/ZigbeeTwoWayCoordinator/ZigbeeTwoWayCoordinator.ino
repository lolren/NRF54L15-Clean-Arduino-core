#include <Arduino.h>
#include <string.h>

#include "nrf54l15_hal.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeTwoWayCoordinator."
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_CHANNEL
#define NRF54L15_CLEAN_ZIGBEE_CHANNEL 15
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PAN_ID
#define NRF54L15_CLEAN_ZIGBEE_PAN_ID 0x1234
#endif

using namespace xiao_nrf54l15;

// Two-way Zigbee coordinator - accepts join requests, receives telemetry,
// and can send commands to end devices.
//
// Commands:
// - 'L' - toggle LED on end device
// - 'T' - trigger telemetry report
// - 'P' - query power state

static ZigbeeRadio g_zb;
static uint8_t g_sequence = 1U;
static uint32_t g_lastStatusMs = 0U;
static uint32_t g_rxFrames = 0U;
static uint32_t g_joinAccepted = 0U;
static uint32_t g_appRx = 0U;
static uint16_t g_nextShortAddress = 0x0100U;

static constexpr uint8_t kChannel = static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPanId = static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kCoordinatorShort = 0x0000U;
static constexpr uint8_t kJoinReqCmdId = 0xA1U;
static constexpr uint8_t kJoinRspCmdId = 0xA2U;
static constexpr uint8_t kRoleEndDevice = 2U;

struct NodeEntry {
  bool used;
  uint16_t shortAddr;
  uint16_t tempShort;
  uint8_t role;
  uint32_t lastSeenMs;
  bool ledOn;
};

static NodeEntry g_nodes[8] = {};

static NodeEntry* findNode(uint16_t shortAddr) {
  for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
    if (g_nodes[i].used && g_nodes[i].shortAddr == shortAddr) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

static NodeEntry* findNodeByTemp(uint16_t tempShort) {
  for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
    if (g_nodes[i].used && g_nodes[i].tempShort == tempShort) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

static NodeEntry* allocateNode() {
  for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
    if (!g_nodes[i].used) {
      g_nodes[i].used = true;
      return &g_nodes[i];
    }
  }
  return nullptr;
}

static bool assignNode(uint16_t requesterShort, uint8_t role,
                       uint16_t* outAssignedShort) {
  if (outAssignedShort == nullptr) {
    return false;
  }

  NodeEntry* existing = findNode(requesterShort);
  if (existing == nullptr) {
    existing = findNodeByTemp(requesterShort);
  }
  if (existing != nullptr) {
    existing->lastSeenMs = millis();
    existing->role = role;
    *outAssignedShort = existing->shortAddr;
    return true;
  }

  NodeEntry* entry = allocateNode();
  if (entry == nullptr) {
    return false;
  }

  entry->role = role;
  entry->lastSeenMs = millis();
  entry->shortAddr = g_nextShortAddress++;
  entry->tempShort = requesterShort;
  entry->ledOn = false;
  *outAssignedShort = entry->shortAddr;
  ++g_joinAccepted;
  return true;
}

static void sendJoinResponse(uint16_t destinationShort, uint8_t nonce,
                             uint8_t status, uint16_t assignedShort,
                             uint8_t acceptedRole) {
  uint8_t payload[5] = {
      status,
      static_cast<uint8_t>(assignedShort & 0xFFU),
      static_cast<uint8_t>((assignedShort >> 8U) & 0xFFU),
      acceptedRole,
      nonce,
  };

  uint8_t psdu[127] = {0};
  uint8_t psduLen = 0U;
  const bool built = ZigbeeRadio::buildMacCommandFrameShort(
      g_sequence++, kPanId, destinationShort, kCoordinatorShort, kJoinRspCmdId,
      payload, sizeof(payload), psdu, &psduLen, false);
  if (built) {
    (void)g_zb.transmit(psdu, psduLen, false, 1200000UL);
  }
}

static void handleJoinRequest(const ZigbeeMacCommandView& view, int8_t rssiDbm) {
  if (view.payloadLength < 2U) {
    return;
  }

  const uint8_t requestedRole = view.payload[0];
  const uint8_t nonce = view.payload[1];
  uint16_t assignedShort = 0U;
  const bool assigned = assignNode(view.sourceShort, requestedRole, &assignedShort);
  const uint8_t status = assigned ? 0U : 1U;
  sendJoinResponse(view.sourceShort, nonce, status, assignedShort, requestedRole);

  Serial.print("join src=0x");
  Serial.print(view.sourceShort, HEX);
  Serial.print(" role=");
  Serial.print(requestedRole == kRoleEndDevice ? "end" : "other");
  Serial.print(" status=");
  Serial.print(status == 0U ? "OK" : "NO_SLOT");
  if (assigned) {
    Serial.print(" assigned=0x");
    Serial.print(assignedShort, HEX);
  }
  Serial.print(" rssi=");
  Serial.print(rssiDbm);
  Serial.println("dBm");
}

static void sendCommand(uint16_t destinationShort, uint8_t command) {
  uint8_t payload[1] = {command};
  uint8_t psdu[127] = {0};
  uint8_t psduLen = 0U;
  const bool built = ZigbeeRadio::buildDataFrameShort(
      g_sequence++, kPanId, destinationShort, kCoordinatorShort, payload,
      sizeof(payload), psdu, &psduLen, false);
  if (built) {
    (void)g_zb.transmit(psdu, psduLen, false, 1200000UL);
    Serial.print("cmd->0x");
    Serial.print(destinationShort, HEX);
    Serial.print(" cmd=");
    Serial.write(command);
    Serial.println();
  }
}

static void handleAppData(const ZigbeeDataFrameView& view, int8_t rssiDbm) {
  if (view.payloadLength < 5U) {
    return;
  }

  // Check for telemetry (TEL) or response (RSP)
  if (!((view.payload[0] == 'T' && view.payload[1] == 'E' && view.payload[2] == 'L') ||
        (view.payload[0] == 'R' && view.payload[1] == 'S' && view.payload[2] == 'P'))) {
    return;
  }

  NodeEntry* node = findNode(view.sourceShort);
  if (node == nullptr) {
    Serial.print("app_drop src=0x");
    Serial.print(view.sourceShort, HEX);
    Serial.println(" reason=not_joined");
    return;
  }

  ++g_appRx;
  node->lastSeenMs = millis();

  // Send ACK
  uint8_t ackPayload[4] = {'A', 'C', 'K', view.payload[3]};
  uint8_t ackPsdu[127] = {0};
  uint8_t ackLen = 0U;
  if (ZigbeeRadio::buildDataFrameShort(
          g_sequence++, kPanId, view.sourceShort, kCoordinatorShort, ackPayload,
          sizeof(ackPayload), ackPsdu, &ackLen, false)) {
    (void)g_zb.transmit(ackPsdu, ackLen, false, 1200000UL);
  }

  Serial.print("app src=0x");
  Serial.print(view.sourceShort, HEX);
  Serial.print(" seq=");
  Serial.print(view.payload[3]);
  Serial.print(" sample=");
  Serial.print(view.payload[4]);
  Serial.print(" rssi=");
  Serial.print(rssiDbm);
  Serial.println("dBm");
}

static void handleSerialCommand() {
  if (!Serial.available()) {
    return;
  }

  const char cmd = Serial.read();
  Serial.print("serial_cmd="); Serial.write(cmd); Serial.println();
  if (cmd == 'L') {
    // Toggle LED on all nodes
    for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
      if (g_nodes[i].used) {
        g_nodes[i].ledOn = !g_nodes[i].ledOn;
        sendCommand(g_nodes[i].shortAddr, g_nodes[i].ledOn ? 'O' : 'F');
      }
    }
  } else if (cmd == 'T') {
    // Trigger telemetry from all nodes
    for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
      if (g_nodes[i].used) {
        sendCommand(g_nodes[i].shortAddr, 'T');
      }
    }
  } else if (cmd == 'P') {
    // Query power state from all nodes
    for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
      if (g_nodes[i].used) {
        sendCommand(g_nodes[i].shortAddr, 'P');
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(350);

  Serial.println("\r\nZigbeeTwoWayCoordinator start");
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  const bool ok = g_zb.begin(kChannel, 8);
  Serial.print("zigbee_phy_init=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" channel=");
  Serial.print(kChannel);
  Serial.print(" pan=0x");
  Serial.println(kPanId, HEX);
}

void loop() {
  ZigbeeFrame frame{};
  if (g_zb.receive(&frame, 9000U, 1000000UL)) {
    ++g_rxFrames;

    ZigbeeMacCommandView cmdView{};
    if (ZigbeeRadio::parseMacCommandFrameShort(frame.psdu, frame.length, &cmdView) &&
        cmdView.valid && cmdView.panId == kPanId &&
        cmdView.destinationShort == kCoordinatorShort &&
        cmdView.commandId == kJoinReqCmdId) {
      handleJoinRequest(cmdView, frame.rssiDbm);
      Gpio::write(kPinUserLed, false);
    }

    ZigbeeDataFrameView dataView{};
    if (ZigbeeRadio::parseDataFrameShort(frame.psdu, frame.length, &dataView) &&
        dataView.valid && dataView.panId == kPanId &&
        dataView.destinationShort == kCoordinatorShort) {
      handleAppData(dataView, frame.rssiDbm);
      Gpio::write(kPinUserLed, false);
    }
  }

  handleSerialCommand();

  const uint32_t now = millis();
  if ((now - g_lastStatusMs) >= 3000U) {
    g_lastStatusMs = now;
    uint8_t active = 0U;
    for (size_t i = 0; i < (sizeof(g_nodes) / sizeof(g_nodes[0])); ++i) {
      if (g_nodes[i].used) {
        ++active;
      }
    }

    Serial.print("t=");
    Serial.print(now);
    Serial.print(" rx=");
    Serial.print(g_rxFrames);
    Serial.print(" joins=");
    Serial.print(g_joinAccepted);
    Serial.print(" app=");
    Serial.print(g_appRx);
    Serial.print(" nodes=");
    Serial.println(active);
    Gpio::write(kPinUserLed, true);
  }

  delay(1);
}
