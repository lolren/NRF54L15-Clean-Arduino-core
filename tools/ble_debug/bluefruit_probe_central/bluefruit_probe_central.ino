#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;

static const char kTargetName[] = "X54 NUS Loopback";
static const char kTargetNameAlt[] = "X54-LB";
static const uint8_t kTargetAddress[6] = {0x37, 0x00, 0x15, 0x54, 0xDE, 0xC0};
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
#ifndef NRF54L15_CENTRAL_PROBE_BG_SERVICE
#define NRF54L15_CENTRAL_PROBE_BG_SERVICE 1
#endif
static constexpr bool kEnableBleBgService =
    (NRF54L15_CENTRAL_PROBE_BG_SERVICE != 0);

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
volatile uint16_t g_characteristicCursorHandle = 0U;
volatile uint16_t g_rxHandle = 0U;
volatile uint16_t g_txHandle = 0U;
volatile uint8_t g_lastErrorCode = 0U;
volatile uint8_t g_lastRequestOpcode = 0U;
volatile uint32_t g_disconnectCount = 0U;
volatile uint32_t g_scanMatchCount = 0U;
volatile uint8_t g_seenAddress[6] = {0U};
volatile uint8_t g_seenRandom = 0U;
volatile uint32_t g_connectAttemptCount = 0U;
volatile uint32_t g_traceCount = 0U;
volatile char g_lastTrace[32] = {0};
volatile uint32_t g_pollEventCount = 0U;
volatile uint32_t g_pollNoEventCount = 0U;
volatile uint32_t g_serviceQueueAttemptCount = 0U;
volatile uint32_t g_serviceQueueSuccessCount = 0U;
volatile uint32_t g_characteristicQueueAttemptCount = 0U;
volatile uint32_t g_characteristicQueueSuccessCount = 0U;
volatile uint8_t g_lastAttOpcode = 0U;
volatile uint8_t g_lastEvtPayloadLength = 0U;
volatile uint8_t g_requestInFlightDebug = 0U;
volatile uint32_t g_txPayloadEventCount = 0U;
volatile uint32_t g_txEmptyAckCount = 0U;
volatile uint8_t g_lastTxAttOpcode = 0U;
volatile uint8_t g_lastTxPayloadLength = 0U;
volatile uint16_t g_lastEventCounter = 0U;
volatile uint8_t g_lastDataChannel = 0U;
volatile uint32_t g_eventStartedCount = 0U;
volatile uint32_t g_pollFailedAfterEventStartCount = 0U;
volatile uint32_t g_dbgConnRxTimeoutCount = 0U;
volatile uint32_t g_dbgConnTxTimeoutCount = 0U;
volatile uint32_t g_dbgConnMissedEventCountLast = 0U;
volatile uint32_t g_dbgConnMissedEventCountMax = 0U;
volatile uint32_t g_dbgLatePollCount = 0U;
volatile uint32_t g_dbgConnImplicitAttProgressAckCount = 0U;
volatile uint8_t g_dbgConnLastDisconnectReason = 0U;
volatile uint8_t g_dbgConnLastDisconnectRemote = 0U;
volatile uint8_t g_dbgConnLastDisconnectValid = 0U;
volatile uint8_t g_lastDisconnectValid = 0U;
volatile uint8_t g_lastDisconnectReason = 0U;
volatile uint8_t g_lastDisconnectRole = 0U;
volatile uint8_t g_lastDisconnectErrorCode = 0U;
volatile uint8_t g_lastDisconnectExpectedRxSn = 0U;
volatile uint8_t g_lastDisconnectTxSn = 0U;
volatile uint8_t g_lastDisconnectFreshTxAllowed = 0U;
volatile uint8_t g_lastDisconnectTxHistoryValid = 0U;
volatile uint8_t g_lastDisconnectPendingTxValid = 0U;
volatile uint8_t g_lastDisconnectPendingTxLlid = 0U;
volatile uint8_t g_lastDisconnectPendingTxLength = 0U;
volatile uint8_t g_lastDisconnectLastTxLlid = 0U;
volatile uint8_t g_lastDisconnectLastTxLength = 0U;
volatile uint8_t g_lastDisconnectLastTxOpcode = 0U;
volatile uint8_t g_lastDisconnectLastRxLlid = 0U;
volatile uint8_t g_lastDisconnectLastRxLength = 0U;
volatile uint8_t g_lastDisconnectLastRxOpcode = 0U;
volatile uint8_t g_lastDisconnectLastRxNesn = 0U;
volatile uint8_t g_lastDisconnectLastRxSn = 0U;
volatile uint8_t g_lastDisconnectLastPacketIsNew = 0U;
volatile uint8_t g_lastDisconnectLastPeerAckedLastTx = 0U;
volatile uint16_t g_lastDisconnectEventCounter = 0U;
volatile uint16_t g_lastDisconnectMissedEventCount = 0U;
volatile uint32_t g_lastDisconnectSequence = 0U;
volatile uint32_t g_lastDisconnectNextEventUs = 0U;

static uint32_t g_lastConnectAttemptMs = 0U;
static bool g_requestInFlight = false;
static bool g_wasConnected = false;

static void snapshotDisconnectDebug() {
  BleDisconnectDebug dbg{};
  if (!g_ble.getDisconnectDebug(&dbg) || dbg.valid == 0U) {
    return;
  }
  g_lastDisconnectValid = dbg.valid;
  g_lastDisconnectReason = dbg.reason;
  g_lastDisconnectRole = dbg.role;
  g_lastDisconnectErrorCode = dbg.errorCode;
  g_lastDisconnectExpectedRxSn = dbg.expectedRxSn;
  g_lastDisconnectTxSn = dbg.txSn;
  g_lastDisconnectFreshTxAllowed = dbg.freshTxAllowed;
  g_lastDisconnectTxHistoryValid = dbg.txHistoryValid;
  g_lastDisconnectPendingTxValid = dbg.pendingTxValid;
  g_lastDisconnectPendingTxLlid = dbg.pendingTxLlid;
  g_lastDisconnectPendingTxLength = dbg.pendingTxLength;
  g_lastDisconnectLastTxLlid = dbg.lastTxLlid;
  g_lastDisconnectLastTxLength = dbg.lastTxLength;
  g_lastDisconnectLastTxOpcode = dbg.lastTxOpcode;
  g_lastDisconnectLastRxLlid = dbg.lastRxLlid;
  g_lastDisconnectLastRxLength = dbg.lastRxLength;
  g_lastDisconnectLastRxOpcode = dbg.lastRxOpcode;
  g_lastDisconnectLastRxNesn = dbg.lastRxNesn;
  g_lastDisconnectLastRxSn = dbg.lastRxSn;
  g_lastDisconnectLastPacketIsNew = dbg.lastPacketIsNew;
  g_lastDisconnectLastPeerAckedLastTx = dbg.lastPeerAckedLastTx;
  g_lastDisconnectEventCounter = dbg.eventCounter;
  g_lastDisconnectMissedEventCount = dbg.missedEventCount;
  g_lastDisconnectSequence = dbg.sequence;
  g_lastDisconnectNextEventUs = dbg.nextEventUs;
}

static void onBleTrace(const char* message, void* context) {
  (void)context;
  ++g_traceCount;
  memset(const_cast<char*>(g_lastTrace), 0, sizeof(g_lastTrace));
  if (message == nullptr) {
    return;
  }
  strncpy(const_cast<char*>(g_lastTrace), message, sizeof(g_lastTrace) - 1U);
}

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

static bool activeScanMatchesTarget(const BleActiveScanResult& result) {
  if ((result.advertiserAddressRandom == kTargetAddressRandom) &&
      (memcmp(result.advertiserAddress, kTargetAddress, sizeof(kTargetAddress)) == 0)) {
    return true;
  }

  if (payloadHasUuid128(result.advData(), result.advDataLength(), kUartServiceUuid) ||
      payloadHasName(result.advData(), result.advDataLength(), kTargetName) ||
      payloadHasName(result.advData(), result.advDataLength(), kTargetNameAlt)) {
    return true;
  }

  return result.scanResponseReceived &&
         (payloadHasUuid128(result.scanRspData(), result.scanRspDataLength(),
                            kUartServiceUuid) ||
          payloadHasName(result.scanRspData(), result.scanRspDataLength(), kTargetName) ||
          payloadHasName(result.scanRspData(), result.scanRspDataLength(),
                         kTargetNameAlt));
}

static void queueServiceDiscovery() {
  ++g_serviceQueueAttemptCount;
  uint8_t request[23] = {0};
  request[0] = kAttOpFindByTypeValueReq;
  writeLe16Local(&request[1], 0x0001U);
  writeLe16Local(&request[3], 0xFFFFU);
  writeLe16Local(&request[5], kUuidPrimaryService);
  memcpy(&request[7], kUartServiceUuid, sizeof(kUartServiceUuid));
  if (g_ble.queueAttRequest(request, sizeof(request))) {
    g_requestInFlight = true;
    g_requestInFlightDebug = 1U;
    ++g_serviceQueueSuccessCount;
  }
}

static void queueCharacteristicDiscovery() {
  if (g_serviceStartHandle == 0U || g_serviceEndHandle == 0U) {
    return;
  }
  const uint16_t startHandle =
      (g_characteristicCursorHandle != 0U) ? g_characteristicCursorHandle : g_serviceStartHandle;
  if (startHandle == 0U || startHandle > g_serviceEndHandle) {
    return;
  }
  ++g_characteristicQueueAttemptCount;
  uint8_t request[7] = {0};
  request[0] = kAttOpReadByTypeReq;
  writeLe16Local(&request[1], startHandle);
  writeLe16Local(&request[3], g_serviceEndHandle);
  writeLe16Local(&request[5], kUuidCharacteristic);
  if (g_ble.queueAttRequest(request, sizeof(request))) {
    g_requestInFlight = true;
    g_requestInFlightDebug = 1U;
    ++g_characteristicQueueSuccessCount;
  }
}

static void handleErrorResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 9U) {
    return;
  }
  g_requestInFlight = false;
  g_requestInFlightDebug = 0U;
  g_lastRequestOpcode = payload[5];
  g_lastErrorCode = payload[8];
  g_state = ProbeState::kFailed;
}

static void handleServiceResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 9U) {
    return;
  }
  g_requestInFlight = false;
  g_requestInFlightDebug = 0U;
  g_serviceStartHandle = readLe16Local(&payload[5]);
  g_serviceEndHandle = readLe16Local(&payload[7]);
  g_characteristicCursorHandle = g_serviceStartHandle;
  g_state = ProbeState::kServiceFound;
}

static void handleCharacteristicResponse(const uint8_t* payload, uint8_t payloadLength) {
  if (payload == nullptr || payloadLength < 27U) {
    return;
  }
  g_requestInFlight = false;
  g_requestInFlightDebug = 0U;
  const uint8_t entryLength = payload[5];
  if (entryLength < 21U) {
    g_state = ProbeState::kFailed;
    return;
  }
  uint16_t lastDeclarationHandle = 0U;
  for (uint8_t offset = 6U;
       (offset + static_cast<uint8_t>(entryLength - 1U)) < payloadLength;
       offset = static_cast<uint8_t>(offset + entryLength)) {
    lastDeclarationHandle = readLe16Local(&payload[offset]);
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
    g_characteristicCursorHandle = 0U;
  } else {
    const uint16_t nextStartHandle = static_cast<uint16_t>(lastDeclarationHandle + 1U);
    if (lastDeclarationHandle != 0U && nextStartHandle <= g_serviceEndHandle) {
      g_characteristicCursorHandle = nextStartHandle;
      g_state = ProbeState::kServiceFound;
    } else {
      g_state = ProbeState::kFailed;
    }
  }
}

void setup() {
  if (!BoardControl::setAntennaPath(BoardAntennaPath::kCeramic)) {
    g_lastErrorCode = 0xEE;
    return;
  }
  if (!g_ble.begin(-4)) {
    g_lastErrorCode = 0xEF;
    return;
  }
  g_ble.setTraceCallback(onBleTrace, nullptr);
  static const uint8_t kAddress[6] = {0x43, 0x00, 0x15, 0x54, 0xDE, 0xC0};
  g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic);
}

void loop() {
  bool connected = g_ble.isConnected();
  if (!connected && g_wasConnected) {
    ++g_disconnectCount;
    snapshotDisconnectDebug();
    if (kEnableBleBgService) {
      g_ble.setBackgroundConnectionServiceEnabled(false);
    }
    g_state = ProbeState::kFailed;
    g_requestInFlight = false;
    g_requestInFlightDebug = 0U;
  }
  g_wasConnected = connected;

  if (!connected) {
    g_requestInFlight = false;
    if (g_state == ProbeState::kIdle || g_state == ProbeState::kConnecting ||
        g_state == ProbeState::kFailed) {
      bool connectAttempted = false;
      BleActiveScanResult result{};
      if (g_ble.scanActiveCycle(&result, 300000UL, 200000UL) &&
          activeScanMatchesTarget(result)) {
        ++g_scanMatchCount;
        memcpy(const_cast<uint8_t*>(g_seenAddress), result.advertiserAddress,
               sizeof(g_seenAddress));
        g_seenRandom = result.advertiserAddressRandom ? 1U : 0U;

        const uint32_t now = millis();
        if ((now - g_lastConnectAttemptMs) >= 250U) {
          g_lastConnectAttemptMs = now;
          g_serviceStartHandle = 0U;
          g_serviceEndHandle = 0U;
          g_characteristicCursorHandle = 0U;
          g_rxHandle = 0U;
          g_txHandle = 0U;
          ++g_connectAttemptCount;
          if (g_ble.initiateConnection(result.advertiserAddress,
                                       result.advertiserAddressRandom, 24U, 200U,
                                       9U, 1200000UL)) {
            g_lastErrorCode = 0U;
            g_state = ProbeState::kConnecting;
          } else {
            g_lastErrorCode = 0xFD;
          }
          connectAttempted = true;
        }
      }

      const uint32_t now = millis();
      if (!connectAttempted && (now - g_lastConnectAttemptMs) >= 250U) {
        g_lastConnectAttemptMs = now;
        memcpy(const_cast<uint8_t*>(g_seenAddress), kTargetAddress, sizeof(g_seenAddress));
        g_seenRandom = kTargetAddressRandom ? 1U : 0U;
        g_serviceStartHandle = 0U;
        g_serviceEndHandle = 0U;
        g_characteristicCursorHandle = 0U;
        g_rxHandle = 0U;
        g_txHandle = 0U;
        ++g_connectAttemptCount;
        if (g_ble.initiateConnection(kTargetAddress, kTargetAddressRandom, 24U, 200U,
                                     9U, 1200000UL)) {
          g_lastErrorCode = 0U;
          g_state = ProbeState::kConnecting;
        } else {
          g_lastErrorCode = 0xFD;
        }
      }
    }
    connected = g_ble.isConnected();
    if (!connected) {
      delay(1);
      return;
    }
  }

  if (g_state == ProbeState::kConnecting) {
    if (kEnableBleBgService) {
      g_ble.setBackgroundConnectionServiceEnabled(true);
    }
    g_state = ProbeState::kConnected;
  }

  BleConnectionEvent evt{};
  const bool pollOk = g_ble.pollConnectionEvent(&evt, 450000UL);
  if (evt.eventStarted) {
    ++g_eventStartedCount;
    g_lastEventCounter = evt.eventCounter;
    g_lastDataChannel = evt.dataChannel;
  }
  BleEncryptionDebugCounters dbg{};
  g_ble.getEncryptionDebugCounters(&dbg);
  g_dbgConnRxTimeoutCount = dbg.connRxTimeoutCount;
  g_dbgConnTxTimeoutCount = dbg.connTxTimeoutCount;
  g_dbgConnMissedEventCountLast = dbg.connMissedEventCountLast;
  g_dbgConnMissedEventCountMax = dbg.connMissedEventCountMax;
  g_dbgLatePollCount = dbg.connLatePollCount;
  g_dbgConnImplicitAttProgressAckCount = dbg.connImplicitAttProgressAckCount;
  g_dbgConnLastDisconnectReason = dbg.connLastDisconnectReason;
  g_dbgConnLastDisconnectRemote = dbg.connLastDisconnectRemote;
  g_dbgConnLastDisconnectValid = dbg.connLastDisconnectValid;
  if (!pollOk) {
    if (evt.eventStarted) {
      ++g_pollFailedAfterEventStartCount;
    }
    ++g_pollNoEventCount;
  } else {
    ++g_pollEventCount;

    if (evt.terminateInd) {
      snapshotDisconnectDebug();
      g_state = ProbeState::kFailed;
      return;
    }

    if (evt.txPacketSent) {
      if (evt.txPayloadLength > 0U && evt.txPayload != nullptr) {
        ++g_txPayloadEventCount;
        g_lastTxPayloadLength = evt.txPayloadLength;
        if (evt.txPayloadLength >= 5U) {
          g_lastTxAttOpcode = evt.txPayload[4];
        }
      } else if (evt.emptyAckTransmitted) {
        ++g_txEmptyAckCount;
      }
    }

    if (evt.packetReceived && evt.crcOk && evt.attPacket && evt.payload != nullptr) {
      const uint8_t* payload = evt.payload;
      g_lastEvtPayloadLength = evt.payloadLength;
      g_lastAttOpcode = payload[4];
      const uint8_t attOpcode = payload[4];
      if (attOpcode == kAttOpErrorRsp) {
        handleErrorResponse(payload, evt.payloadLength);
      } else if (attOpcode == kAttOpFindByTypeValueRsp) {
        handleServiceResponse(payload, evt.payloadLength);
      } else if (attOpcode == kAttOpReadByTypeRsp) {
        handleCharacteristicResponse(payload, evt.payloadLength);
      }
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
