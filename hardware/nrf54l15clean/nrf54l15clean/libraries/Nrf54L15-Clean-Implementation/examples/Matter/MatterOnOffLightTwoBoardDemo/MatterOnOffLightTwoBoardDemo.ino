// Matter On/Off Light Two-Board Demo over Thread
//
// Board A (Light Node):  ROLE = LIGHT_NODE
//   - Forms a Thread network (becomes Leader)
//   - Listens on Matter UDP port 5540 for CHIP-formatted commands
//   - Controls LED_BUILTIN based on on/off state
//   - Responds to: on, off, toggle, identify
//
// Board B (Controller):  ROLE = CONTROLLER
//   - Attaches to the same Thread network (becomes Child)
//   - Every 5 seconds sends a Matter command to the light node
//   - Cycles: on -> identify -> off -> toggle -> ...
//
// To use:
//   1. Flash Board A with ROLE = LIGHT_NODE
//   2. Flash Board B with ROLE = CONTROLLER
//   3. Power both boards
//   4. Board A LED toggles based on received commands
//   5. Serial monitor shows the full interaction


#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building."
#endif

// ═══════════════════════════════════════════════════════════════
// CHANGE THIS to switch roles: LIGHT_NODE or CONTROLLER
// ═══════════════════════════════════════════════════════════════
enum class DemoRole : uint8_t { LIGHT_NODE = 0, CONTROLLER = 1 };
constexpr DemoRole ROLE = DemoRole::LIGHT_NODE;
// ═══════════════════════════════════════════════════════════════

namespace {

using xiao_nrf54l15::Nrf54ThreadExperimental;

// Matter protocol constants
constexpr uint16_t kMatterUdpPort = 5540U;
constexpr uint16_t kProtocolSecureChannel = 0x0000U;
constexpr uint16_t kProtocolInteractionModel = 0x0001U;

// Interaction Model opcodes
constexpr uint8_t kOpReadRequest = 0x02U;
constexpr uint8_t kOpReportData = 0x05U;
constexpr uint8_t kOpInvokeCommandRequest = 0x08U;
constexpr uint8_t kOpInvokeCommandResponse = 0x09U;

// On/Off cluster IDs
constexpr uint32_t kOnOffClusterId = 0x0006U;
constexpr uint32_t kIdentifyClusterId = 0x0003U;
constexpr uint32_t kOffCommandId = 0x00U;
constexpr uint32_t kOnCommandId = 0x01U;
constexpr uint32_t kToggleCommandId = 0x02U;
constexpr uint32_t kIdentifyCommandId = 0x00U;

constexpr uint32_t kReportIntervalMs = 3000U;
constexpr uint32_t kCommandIntervalMs = 5000U;

Nrf54ThreadExperimental gThread;
bool gLightOn = false;
bool gIdentifying = false;
uint32_t gIdentifyEndMs = 0U;
uint32_t gLastReportMs = 0U;
uint32_t gLastCommandMs = 0U;
uint8_t gCommandSequence = 0U;
Nrf54ThreadExperimental::Role gLastRole =
    Nrf54ThreadExperimental::Role::kUnknown;

// ─── CHIP Message Framing ───────────────────────────────────────

void writeUint16Le(uint16_t value, uint8_t* out, size_t offset) {
  out[offset]     = static_cast<uint8_t>(value & 0xFFU);
  out[offset + 1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeUint32Le(uint32_t value, uint8_t* out, size_t offset) {
  out[offset]     = static_cast<uint8_t>(value & 0xFFU);
  out[offset + 1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  out[offset + 2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  out[offset + 3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

uint16_t readUint16Le(const uint8_t* data, size_t offset) {
  return static_cast<uint16_t>(data[offset]) |
         (static_cast<uint16_t>(data[offset + 1]) << 8U);
}

uint32_t readUint32Le(const uint8_t* data, size_t offset) {
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8U) |
         (static_cast<uint32_t>(data[offset + 2]) << 16U) |
         (static_cast<uint32_t>(data[offset + 3]) << 24U);
}

// Build a CHIP message header (unencrypted, for commissioning/test)
// Returns header length in bytes
size_t buildChipHeader(uint8_t* buffer, size_t capacity,
                       uint8_t exchangeFlags,
                       uint16_t messageId, uint16_t exchangeId,
                       uint16_t protocolId, uint8_t protocolOpcode) {
  if (capacity < 20U) return 0U;
  size_t off = 0U;
  buffer[off++] = exchangeFlags;      // Exchange Flags
  buffer[off++] = 0U;                 // Session Type (unsecured)
  buffer[off++] = 0U;                 // Security Flags
  writeUint16Le(messageId, buffer, off); off += 2U;
  writeUint32Le(0U, buffer, off); off += 4U;       // Source Node ID
  writeUint32Le(0U, buffer, off); off += 4U;       // Dest Node ID
  writeUint16Le(exchangeId, buffer, off); off += 2U;
  writeUint16Le(0U, buffer, off); off += 2U;       // Vendor ID (standard)
  writeUint16Le(protocolId, buffer, off); off += 2U;
  buffer[off++] = protocolOpcode;
  return off;
}

// Parse a CHIP message header. Returns payload offset.
bool parseChipHeader(const uint8_t* data, uint16_t length,
                     uint16_t* outProtocolId, uint8_t* outOpcode,
                     uint16_t* outExchangeId, size_t* outPayloadOffset) {
  if (length < 20U) return false;
  size_t off = 3U;                                   // skip flags+sess+sec
  uint16_t msgId = readUint16Le(data, off); off += 2U;  (void)msgId;
  off += 8U;                                         // skip node IDs
  if (outExchangeId) *outExchangeId = readUint16Le(data, off);
  off += 2U;
  off += 2U;                                         // skip vendor ID
  if (outProtocolId) *outProtocolId = readUint16Le(data, off);
  off += 2U;
  if (outOpcode) *outOpcode = data[off];
  off += 1U;
  if (outPayloadOffset) *outPayloadOffset = off;
  return true;
}

// ─── Matter Command Builder (Controller side) ───────────────────

static uint16_t sMsgId = 0U;
static uint16_t sExchangeId = 0x1234U;

uint16_t nextMsgId() {
  sMsgId++;
  if (sMsgId == 0U) sMsgId = 1U;
  return sMsgId;
}

uint16_t nextExchangeId() {
  sExchangeId++;
  if (sExchangeId == 0U) sExchangeId = 1U;
  return sExchangeId;
}

// Build and send a Matter InvokeCommandRequest for On/Off cluster
bool sendMatterCommand(const otIp6Address& peerAddr, uint16_t peerPort,
                       uint32_t clusterId, uint32_t commandId) {
  uint8_t buf[128] = {0};

  // CHIP IM InvokeCommandRequest payload:
  //   SuppressResponse (bool, 1 byte)
  //   TimedRequest (bool, 1 byte)  
  //   InvokeRequests (list)
  //     CommandPathIB: EndpointId (uint16), ClusterId (uint32), CommandId (uint32)
  //     CommandDataIB: (empty for on/off/toggle)
  size_t off = 0U;
  buf[off++] = 0U;  // SuppressResponse = false
  buf[off++] = 0U;  // TimedRequest = false

  // CommandPathIB
  writeUint16Le(1U, buf, off); off += 2U;           // EndpointId = 1
  writeUint32Le(clusterId, buf, off); off += 4U;    // ClusterId
  writeUint32Le(commandId, buf, off); off += 4U;    // CommandId

  // CommandDataIB: empty (needs a null TLV to terminate)
  buf[off++] = 0x18U;  // end-of-container TLV
  buf[off++] = 0x18U;  // end-of-container TLV (second for outer list)

  // Build CHIP header + payload
  uint8_t frame[256] = {0};
  uint16_t exchangeId = nextExchangeId();
  size_t headerLen = buildChipHeader(
      frame, sizeof(frame),
      0x05U,  // Initiator + Reliable
      nextMsgId(), exchangeId,
      kProtocolInteractionModel, kOpInvokeCommandRequest);

  if (headerLen == 0U) return false;
  memcpy(&frame[headerLen], buf, off);

  return gThread.sendUdp(peerAddr, peerPort, frame,
                         static_cast<uint16_t>(headerLen + off));
}

// ─── Matter Command Handler (Light Node side) ───────────────────

void handleInvokeCommand(const uint8_t* payload, uint16_t length) {
  if (payload == nullptr || length < 14U) return;

  // Parse CommandPathIB
  size_t off = 2U;  // skip SuppressResponse + TimedRequest
  uint16_t endpointId = readUint16Le(payload, off); off += 2U;
  uint32_t clusterId  = readUint32Le(payload, off); off += 4U;
  uint32_t commandId  = readUint32Le(payload, off); off += 4U;

  Serial.print("matter_light cmd endpoint=");
  Serial.print(endpointId);
  Serial.print(" cluster=0x");
  Serial.print(clusterId, HEX);
  Serial.print(" command=0x");
  Serial.print(commandId, HEX);

  if (clusterId == kOnOffClusterId) {
    if (commandId == kOnCommandId) {
      gLightOn = true;
      Serial.println(" -> ON");
    } else if (commandId == kOffCommandId) {
      gLightOn = false;
      Serial.println(" -> OFF");
    } else if (commandId == kToggleCommandId) {
      gLightOn = !gLightOn;
      Serial.print(" -> TOGGLE (now ");
      Serial.print(gLightOn ? "ON" : "OFF");
      Serial.println(")");
    } else {
      Serial.println(" -> UNKNOWN");
    }
  } else if (clusterId == kIdentifyClusterId) {
    if (commandId == kIdentifyCommandId) {
      gIdentifying = true;
      gIdentifyEndMs = millis() + 5000UL;
      Serial.println(" -> IDENTIFY (5s)");
    } else {
      Serial.println(" -> UNKNOWN");
    }
  } else {
    Serial.println(" -> UNSUPPORTED_CLUSTER");
  }
}

void handleReadRequest(const uint8_t* payload, uint16_t length) {
  if (payload == nullptr || length < 6U) return;

  // Parse attribute path
  size_t off = 0U;
  uint16_t endpointId = readUint16Le(payload, off); off += 2U;
  uint32_t clusterId  = readUint32Le(payload, off); off += 4U;
  // uint32_t attributeId = readUint32Le(payload, off); off += 4U;

  Serial.print("matter_light read endpoint=");
  Serial.print(endpointId);
  Serial.print(" cluster=0x");
  Serial.println(clusterId, HEX);
}

// ─── UDP Receive Handler ────────────────────────────────────────

void onUdpReceive(void*, const uint8_t* payload, uint16_t length,
                  const otMessageInfo& info) {
  if (payload == nullptr || length < 20U) return;

  uint16_t protocolId = 0U;
  uint8_t opcode = 0U;
  uint16_t exchangeId = 0U;
  size_t payloadOffset = 0U;

  if (!parseChipHeader(payload, length, &protocolId, &opcode,
                       &exchangeId, &payloadOffset)) {
    return;
  }

  const uint16_t appLen = static_cast<uint16_t>(
      length > payloadOffset ? length - payloadOffset : 0U);
  const uint8_t* appPayload = appLen > 0U ? &payload[payloadOffset] : nullptr;

  Serial.print("matter_light rx protocol=0x");
  Serial.print(protocolId, HEX);
  Serial.print(" op=0x");
  Serial.print(opcode, HEX);
  Serial.print(" len=");
  Serial.print(appLen);

  if (protocolId == kProtocolInteractionModel) {
    switch (opcode) {
      case kOpInvokeCommandRequest:
        Serial.print(" [InvokeCommandRequest] ");
        handleInvokeCommand(appPayload, appLen);
        break;
      case kOpReadRequest:
        Serial.print(" [ReadRequest] ");
        handleReadRequest(appPayload, appLen);
        break;
      default:
        Serial.println(" [unknown IM op]");
        break;
    }
  } else {
    Serial.println(" [non-IM protocol]");
  }
}

// ─── LED Control ────────────────────────────────────────────────

void applyLed() {
#if defined(LED_BUILTIN)
  if (gIdentifying) {
    // Blink during identify
    digitalWrite(LED_BUILTIN, ((millis() / 150UL) & 0x1UL) ? HIGH : LOW);
    return;
  }
  digitalWrite(LED_BUILTIN, gLightOn ? HIGH : LOW);
#endif
}

// ─── Status Reporting ────────────────────────────────────────────

void printStatus() {
  Serial.print("matter_light role=");
  Serial.print(gThread.roleName());
  Serial.print(" rloc16=0x");
  Serial.print(gThread.rloc16(), HEX);
  Serial.print(" on=");
  Serial.print(gLightOn ? 1 : 0);
  Serial.print(" identifying=");
  Serial.print(gIdentifying ? 1 : 0);
  Serial.print(" my_role=");
  Serial.println(ROLE == DemoRole::LIGHT_NODE ? "light_node" : "controller");
}

}  // namespace

// ─── Setup ──────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  Serial.println();
  Serial.print("matter_light === Matter On/Off Light 2-Board Demo ===");
  Serial.println();
  Serial.print("matter_light role=");
  Serial.println(ROLE == DemoRole::LIGHT_NODE ? "LIGHT_NODE" : "CONTROLLER");

  // Use demo dataset so both boards share the same Thread network
  otOperationalDataset dataset = {};
  Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  gThread.setActiveDataset(dataset);
  gThread.begin();

  Serial.print("matter_light thread_begin=");
  Serial.println(gThread.started() ? 1 : 0);

  if (ROLE == DemoRole::LIGHT_NODE) {
    // Light node: open UDP listener for Matter commands
    const bool udpOk = gThread.openUdp(kMatterUdpPort, onUdpReceive, nullptr);
    Serial.print("matter_light udp_open=");
    Serial.println(udpOk ? 1 : 0);
    if (!udpOk) {
      Serial.print("matter_light udp_error=");
      Serial.println(static_cast<int>(gThread.lastUdpError()));
    }
    Serial.println("matter_light Listening for Matter commands on UDP port 5540...");
  } else {
    // Controller: open a UDP socket for sending (any port)
    const bool udpOk = gThread.openUdp(kMatterUdpPort + 1U, nullptr, nullptr);
    Serial.print("matter_light controller_udp_open=");
    Serial.println(udpOk ? 1 : 0);
  }

  printStatus();
}

// ─── Loop ───────────────────────────────────────────────────────

void loop() {
  gThread.process();
  applyLed();

  // Check identify timeout
  if (gIdentifying && millis() >= gIdentifyEndMs) {
    gIdentifying = false;
  }

  const Nrf54ThreadExperimental::Role currentRole = gThread.role();
  if (currentRole != gLastRole) {
    gLastRole = currentRole;
    printStatus();
  }

  // ─── Controller: send commands to light node ──────────────
  if (ROLE == DemoRole::LIGHT_NODE && gThread.attached()) {
    if ((millis() - gLastCommandMs) >= kCommandIntervalMs) {
      gLastCommandMs = millis();

      otIp6Address leaderAddr = {};
      if (gThread.getLeaderRloc(&leaderAddr)) {
        // Cycle through commands: ON -> OFF -> TOGGLE -> TOGGLE -> loop
        const uint32_t clusterId = (gCommandSequence % 4U < 3U)
                                       ? kOnOffClusterId
                                       : kIdentifyClusterId;
        const uint32_t commandId = [](uint8_t seq) -> uint32_t {
          switch (seq % 4U) {
            case 0:  return kOnCommandId;
            case 1:  return kOffCommandId;
            case 2:  return kToggleCommandId;
            default: return kIdentifyCommandId;
          }
        }(gCommandSequence);

        Serial.print("matter_light sending cmd=0x");
        Serial.print(commandId, HEX);
        Serial.print(" to leader... ");

        const bool sent = sendMatterCommand(leaderAddr, kMatterUdpPort,
                                            clusterId, commandId);
        Serial.println(sent ? "OK" : "FAIL");
        gCommandSequence++;
      }
    }
  }

  // Periodic status
  if ((millis() - gLastReportMs) >= kReportIntervalMs) {
    gLastReportMs = millis();
    printStatus();
  }
}
