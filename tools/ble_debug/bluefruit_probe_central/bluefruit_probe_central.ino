#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;

static const char kTargetName[] = "XIAO nRF54L15";
static const char kTargetNameAlt[] = "XIAO54 BLEUART";
static const uint8_t kTargetAddress[6] = {0x6E, 0x22, 0x59, 0xF9, 0xAC, 0xD0};
static constexpr bool kTargetAddressRandom = true;
static const uint8_t kUartServiceUuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,
                                             0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5,
                                             0x01, 0x00, 0x40, 0x6E};
static const uint8_t kUartRxUuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,
                                        0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5,
                                        0x02, 0x00, 0x40, 0x6E};
static const uint8_t kUartTxUuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,
                                        0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5,
                                        0x03, 0x00, 0x40, 0x6E};

static constexpr uint8_t kAttOpErrorRsp = 0x01U;
static constexpr uint8_t kAttOpFindByTypeValueReq = 0x06U;
static constexpr uint8_t kAttOpFindByTypeValueRsp = 0x07U;
static constexpr uint8_t kAttOpReadByTypeReq = 0x08U;
static constexpr uint8_t kAttOpReadByTypeRsp = 0x09U;
static constexpr uint16_t kUuidPrimaryService = 0x2800U;
static constexpr uint16_t kUuidCharacteristic = 0x2803U;

enum class ProbeState : uint32_t {
  kIdle = 0U,
  kConnecting = 1U,
  kConnected = 2U,
  kServiceFound = 3U,
  kCharacteristicsFound = 4U,
  kFailed = 0xE0U,
};

volatile ProbeState g_state = ProbeState::kIdle;
volatile uint16_t g_serviceStartHandle = 0U;
volatile uint16_t g_serviceEndHandle = 0U;
volatile uint16_t g_rxHandle = 0U;
volatile uint16_t g_txHandle = 0U;
volatile uint8_t g_lastErrorCode = 0U;
volatile uint8_t g_lastRequestOpcode = 0U;
volatile uint32_t g_disconnectCount = 0U;
volatile uint32_t g_scanMatchCount = 0U;
volatile uint8_t g_seenAddress[6] = {0U};
volatile uint8_t g_seenRandom = 0U;
volatile uint32_t g_connectAttemptCount = 0U;

static uint32_t g_lastConnectAttemptMs = 0U;
static bool g_requestInFlight = false;

static uint16_t readLe16Local(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
}

static void writeLe16Local(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

static bool uuidEquals128(const uint8_t* lhs, const uint8_t* rhs) {
  return memcmp(lhs, rhs, 16U) == 0;
}

static bool payloadHasName(const uint8_t* payload, uint8_t length, const char* name) {
  if (payload == nullptr || name == nullptr) {
    return false;
  }
  const size_t nameLength = strlen(name);
  uint8_t offset = 0U;
  while (offset < length) {
    const uint8_t fieldLength = payload[offset];
    if (fieldLength == 0U || static_cast<uint16_t>(offset + fieldLength) >=
                                 static_cast<uint16_t>(length + 1U)) {
      break;
    }
    const uint8_t type = payload[offset + 1U];
    if (type == 0x08U || type == 0x09U) {
      const uint8_t valueLength = static_cast<uint8_t>(fieldLength - 1U);
      if (valueLength == nameLength &&
          memcmp(&payload[offset + 2U], name, valueLength) == 0) {
        return true;
      }
    }
    offset = static_cast<uint8_t>(offset + fieldLength + 1U);
  }
  return false;
}

static bool payloadHasUuid128(const uint8_t* payload, uint8_t length,
                              const uint8_t uuid128[16]) {
  if (payload == nullptr || uuid128 == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  while (offset < length) {
    const uint8_t fieldLength = payload[offset];
    if (fieldLength == 0U || static_cast<uint16_t>(offset + fieldLength) >=
                                 static_cast<uint16_t>(length + 1U)) {
      break;
    }

    const uint8_t type = payload[offset + 1U];
    if ((type == 0x06U || type == 0x07U) && fieldLength >= 17U) {
      const uint8_t valueLength = static_cast<uint8_t>(fieldLength - 1U);
      for (uint8_t pos = 0U; (pos + 16U) <= valueLength;
           pos = static_cast<uint8_t>(pos + 16U)) {
        if (memcmp(&payload[offset + 2U + pos], uuid128, 16U) == 0) {
          return true;
        }
      }
    }

    const uint8_t step = static_cast<uint8_t>(fieldLength + 1U);
    offset = static_cast<uint8_t>(offset + step);
  }

  return false;
}

static void queueServiceDiscovery() {
  uint8_t request[23] = {0};
  request[0] = kAttOpFindByTypeValueReq;
  writeLe16Local(&request[1], 0x0001U);
  writeLe16Local(&request[3], 0xFFFFU);
  writeLe16Local(&request[5], kUuidPrimaryService);
  memcpy(&request[7], kUartServiceUuid, sizeof(kUartServiceUuid));
  if (g_ble.queueAttRequest(request, sizeof(request))) {
    g_requestInFlight = true;
  }
}

static void queueCharacteristicDiscovery() {
  if (g_serviceStartHandle == 0U || g_serviceEndHandle == 0U) {
    return;
  }
  uint8_t request[7] = {0};
  request[0] = kAttOpReadByTypeReq;
  writeLe16Local(&request[1], g_serviceStartHandle);
  writeLe16Local(&request[3], g_serviceEndHandle);
  writeLe16Local(&request[5], kUuidCharacteristic);
  if (g_ble.queueAttRequest(request, sizeof(request))) {
    g_requestInFlight = true;
  }
}

static void handleErrorResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 9U) {
    return;
  }
  g_requestInFlight = false;
  g_lastRequestOpcode = payload[5];
  g_lastErrorCode = payload[8];
  g_state = ProbeState::kFailed;
}

static void handleServiceResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 9U) {
    return;
  }
  g_requestInFlight = false;
  g_serviceStartHandle = readLe16Local(&payload[5]);
  g_serviceEndHandle = readLe16Local(&payload[7]);
  g_state = ProbeState::kServiceFound;
}

static void handleCharacteristicResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 27U) {
    return;
  }
  g_requestInFlight = false;
  const uint8_t entryLength = payload[5];
  if (entryLength < 21U) {
    g_state = ProbeState::kFailed;
    return;
  }
  for (uint8_t offset = 6U;
       (offset + static_cast<uint8_t>(entryLength - 1U)) < payloadLength;
       offset = static_cast<uint8_t>(offset + entryLength)) {
    const uint16_t valueHandle = readLe16Local(&payload[offset + 3U]);
    const uint8_t* uuid = &payload[offset + 5U];
    if (uuidEquals128(uuid, kUartRxUuid)) {
      g_rxHandle = valueHandle;
    } else if (uuidEquals128(uuid, kUartTxUuid)) {
      g_txHandle = valueHandle;
    }
  }
  if (g_rxHandle != 0U && g_txHandle != 0U) {
    g_state = ProbeState::kCharacteristicsFound;
  } else {
    g_state = ProbeState::kFailed;
  }
}

void setup() {
  g_ble.begin(-4);
  static const uint8_t kAddress[6] = {0x43, 0x00, 0x15, 0x54, 0xDE, 0xC0};
  g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic);
}

void loop() {
  if (!g_ble.isConnected()) {
    ++g_disconnectCount;
    g_requestInFlight = false;
    if (g_state == ProbeState::kIdle || g_state == ProbeState::kConnecting ||
        g_state == ProbeState::kFailed) {
      const uint32_t now = millis();
      if ((now - g_lastConnectAttemptMs) >= 250U) {
        g_lastConnectAttemptMs = now;
        memcpy(const_cast<uint8_t*>(g_seenAddress), kTargetAddress,
               sizeof(kTargetAddress));
        g_seenRandom = kTargetAddressRandom ? 1U : 0U;
        ++g_connectAttemptCount;
        if (g_ble.initiateConnection(kTargetAddress, kTargetAddressRandom, 24U,
                                     200U, 9U, 1200000UL)) {
          g_state = ProbeState::kConnecting;
        } else {
          g_lastErrorCode = 0xFD;
        }
      }
    }
    delay(1);
    return;
  }

  if (g_state == ProbeState::kConnecting) {
    g_state = ProbeState::kConnected;
  }

  BleConnectionEvent evt{};
  if (!g_ble.pollConnectionEvent(&evt, 450000UL)) {
    return;
  }

  if (evt.terminateInd) {
    g_state = ProbeState::kFailed;
    return;
  }

  if (evt.packetReceived && evt.crcOk && evt.attPacket && evt.payload != nullptr) {
    const uint8_t* payload = evt.payload;
    const uint8_t attOpcode = payload[4];
    if (attOpcode == kAttOpErrorRsp) {
      handleErrorResponse(payload, evt.payloadLength);
    } else if (attOpcode == kAttOpFindByTypeValueRsp) {
      handleServiceResponse(payload, evt.payloadLength);
    } else if (attOpcode == kAttOpReadByTypeRsp) {
      handleCharacteristicResponse(payload, evt.payloadLength);
    }
  }

  if (!g_requestInFlight) {
    if (g_state == ProbeState::kConnected) {
      queueServiceDiscovery();
    } else if (g_state == ProbeState::kServiceFound) {
      queueCharacteristicDiscovery();
    }
  }
}
