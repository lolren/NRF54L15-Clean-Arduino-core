// ThreadExperimentalUdpSoak
//
// Two-board staged Thread UDP reliability probe. Flash the same sketch to two
// boards with Tools > Thread Core > Experimental Stage Core. One board should
// become leader; the child/router sends payloads to the leader and expects an
// ACK echo. The sketch also probes multicast delivery and exports the active
// dataset hex so reboot/rejoin behavior can be checked from the serial log.

#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building."
#endif

#include <string.h>

using xiao_nrf54l15::Nrf54ThreadExperimental;

extern "C" {
__attribute__((used)) volatile uint32_t g_soak_results[20] = {0};
}

namespace {

#define SOAK_PRINT(value) do { if (Serial) Serial.print(value); if (Serial1) Serial1.print(value); } while (0)
#define SOAK_PRINTLN(...) do { if (Serial) Serial.println(__VA_ARGS__); if (Serial1) Serial1.println(__VA_ARGS__); } while (0)
#define SOAK_PRINT_HEX(value) do { if (Serial) Serial.print(value, HEX); if (Serial1) Serial1.print(value, HEX); } while (0)

constexpr uint16_t kUdpPort = 61631U;
constexpr uint32_t kSendIntervalMs = 500UL;
constexpr uint32_t kAckTimeoutMs = 4000UL;
constexpr uint32_t kStatusIntervalMs = 2000UL;
constexpr uint8_t kMaxRetriesPerPayload = 3U;
constexpr uint16_t kMaxPayloadLength = 512U;
constexpr uint16_t kMaxSafeUnicastPayload = 95U;
constexpr uint16_t kMaxSafeMulticastPayload = 80U;
constexpr uint8_t kMagic = 0x54U;
constexpr uint8_t kPingType = 0x50U;
constexpr uint8_t kAckType = 0x41U;

constexpr uint16_t kPayloadSizes[] = {
    8U, 16U, 31U, 63U, 95U, 127U, 191U, 255U, 512U,
};
constexpr size_t kPayloadSizeCount =
    sizeof(kPayloadSizes) / sizeof(kPayloadSizes[0]);

constexpr uint8_t kMulticastAddrBytes[16] = {
    0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

Nrf54ThreadExperimental gThread;
Nrf54ThreadExperimental::Role gLastRole =
    Nrf54ThreadExperimental::Role::kUnknown;

uint8_t gTxBuffer[kMaxPayloadLength] = {0};
uint16_t gCurrentSeq = 1U;
size_t gUnicastIndex = 0U;
size_t gMulticastIndex = 0U;
uint32_t gLastSendMs = 0U;
uint32_t gLastPrintMs = 0U;
uint8_t gRetryCount = 0U;
bool gWaitingForAck = false;
bool gTestStarted = false;
bool gUnicastDone = false;
bool gMulticastPhase = false;
bool gMulticastDone = false;
bool gMulticastSubscribed = false;
bool gDatasetExported = false;

uint32_t gPingTxCount = 0U;
uint32_t gPingRxCount = 0U;
uint32_t gAckTxCount = 0U;
uint32_t gAckRxCount = 0U;
uint32_t gPassCount = 0U;
uint32_t gFailCount = 0U;
uint32_t gMulticastPassCount = 0U;
uint32_t gMulticastFailCount = 0U;
uint32_t gRetryTotal = 0U;
uint32_t gInvalidRxCount = 0U;
uint16_t gLastLength = 0U;
uint16_t gLastAckSeq = 0U;

char gDatasetHex[(OT_OPERATIONAL_DATASET_MAX_LENGTH * 2U) + 1U] = {0};

uint8_t checksum8(const uint8_t* data, uint16_t length) {
  uint8_t checksum = 0U;
  if (data == nullptr) return checksum;
  for (uint16_t i = 0U; i < length; ++i) {
    checksum = static_cast<uint8_t>((checksum << 1U) | (checksum >> 7U));
    checksum ^= data[i];
    checksum = static_cast<uint8_t>(checksum + 0x3DU);
  }
  return checksum;
}

bool buildPayload(uint8_t type, uint16_t seq, uint16_t length) {
  if (length < 8U || length > sizeof(gTxBuffer)) return false;
  memset(gTxBuffer, 0, sizeof(gTxBuffer));
  gTxBuffer[0] = kMagic;
  gTxBuffer[1] = type;
  gTxBuffer[2] = static_cast<uint8_t>(seq & 0xFFU);
  gTxBuffer[3] = static_cast<uint8_t>((seq >> 8U) & 0xFFU);
  gTxBuffer[4] = static_cast<uint8_t>(length & 0xFFU);
  gTxBuffer[5] = static_cast<uint8_t>((length >> 8U) & 0xFFU);
  for (uint16_t i = 6U; i < (length - 1U); ++i) {
    gTxBuffer[i] = static_cast<uint8_t>((seq * 33U) + (i * 17U) + length);
  }
  gTxBuffer[length - 1U] = checksum8(gTxBuffer, length - 1U);
  return true;
}

bool parsePayload(const uint8_t* payload, uint16_t length, uint8_t* outType,
                  uint16_t* outSeq, uint16_t* outDeclaredLength) {
  if (payload == nullptr || outType == nullptr || outSeq == nullptr ||
      outDeclaredLength == nullptr || length < 8U) {
    return false;
  }
  if (payload[0] != kMagic) return false;
  const uint16_t declaredLength =
      static_cast<uint16_t>(payload[4]) |
      (static_cast<uint16_t>(payload[5]) << 8U);
  if (declaredLength != length) return false;
  if (checksum8(payload, length - 1U) != payload[length - 1U]) return false;
  *outType = payload[1];
  *outSeq = static_cast<uint16_t>(payload[2]) |
            (static_cast<uint16_t>(payload[3]) << 8U);
  *outDeclaredLength = declaredLength;
  return true;
}

const char* failModeName(uint8_t result) {
  switch (result) {
    case 2: return "tx";
    case 4: return "timeout";
    case 5: return "checksum";
    default: return "unknown";
  }
}

void updateResults() {
  g_soak_results[0] = gThread.started() ? 1U : 0U;
  g_soak_results[1] = gThread.attached() ? 1U : 0U;
  g_soak_results[2] = static_cast<uint32_t>(gThread.role());
  g_soak_results[3] = gThread.rloc16();
  g_soak_results[4] = gPassCount;
  g_soak_results[5] = gFailCount;
  g_soak_results[6] = gMulticastPassCount;
  g_soak_results[7] = gMulticastFailCount;
  g_soak_results[8] = gUnicastDone ? 1U : 0U;
  g_soak_results[9] = gMulticastDone ? 1U : 0U;
  g_soak_results[10] = gDatasetExported ? 1U : 0U;
  g_soak_results[11] = gMulticastSubscribed ? 1U : 0U;
}

void recordResult(bool multicast, size_t idx, uint8_t result) {
  if (idx >= kPayloadSizeCount) return;
  const uint16_t len = kPayloadSizes[idx];
  if (multicast) {
    if (result == 1U) {
      ++gMulticastPassCount;
      SOAK_PRINT("soak_mcast_pass len=");
      SOAK_PRINTLN(len);
    } else {
      ++gMulticastFailCount;
      SOAK_PRINT("soak_mcast_fail len=");
      SOAK_PRINT(len);
      SOAK_PRINT(" mode=");
      SOAK_PRINTLN(failModeName(result));
    }
  } else {
    if (result == 1U) {
      ++gPassCount;
      SOAK_PRINT("soak_pass len=");
      SOAK_PRINTLN(len);
    } else {
      ++gFailCount;
      SOAK_PRINT("soak_fail len=");
      SOAK_PRINT(len);
      SOAK_PRINT(" mode=");
      SOAK_PRINTLN(failModeName(result));
    }
  }
}

void printStatus(const char* reason) {
  SOAK_PRINT("soak_stat reason=");
  SOAK_PRINT(reason);
  SOAK_PRINT(" role=");
  SOAK_PRINT(gThread.roleName());
  SOAK_PRINT(" rloc16=0x");
  SOAK_PRINT_HEX(gThread.rloc16());
  SOAK_PRINT(" part=0x");
  SOAK_PRINT_HEX(gThread.partitionId());
  SOAK_PRINT(" seq=");
  SOAK_PRINT(gCurrentSeq);
  SOAK_PRINT(" len=");
  SOAK_PRINT(gLastLength);
  SOAK_PRINT(" ping=");
  SOAK_PRINT(gPingTxCount);
  SOAK_PRINT("/");
  SOAK_PRINT(gPingRxCount);
  SOAK_PRINT(" ack=");
  SOAK_PRINT(gAckTxCount);
  SOAK_PRINT("/");
  SOAK_PRINT(gAckRxCount);
  SOAK_PRINT(" pass=");
  SOAK_PRINT(gPassCount);
  SOAK_PRINT(" fail=");
  SOAK_PRINT(gFailCount);
  SOAK_PRINT(" retry=");
  SOAK_PRINT(gRetryTotal);
  SOAK_PRINT(" invalid=");
  SOAK_PRINT(gInvalidRxCount);
  SOAK_PRINT(" wait=");
  SOAK_PRINT(gWaitingForAck ? 1 : 0);
  SOAK_PRINT(" unicast_done=");
  SOAK_PRINT(gUnicastDone ? 1 : 0);
  SOAK_PRINT(" mcast_done=");
  SOAK_PRINT(gMulticastDone ? 1 : 0);
  SOAK_PRINT(" mcast_sub=");
  SOAK_PRINT(gMulticastSubscribed ? 1 : 0);
  SOAK_PRINT(" err=");
  SOAK_PRINT(static_cast<int>(gThread.lastError()));
  SOAK_PRINT("/");
  SOAK_PRINTLN(static_cast<int>(gThread.lastUdpError()));
}

void printDone() {
  SOAK_PRINT("soak_done unicast_pass=");
  SOAK_PRINT(gPassCount);
  SOAK_PRINT(" unicast_fail=");
  SOAK_PRINT(gFailCount);
  SOAK_PRINT(" mcast_pass=");
  SOAK_PRINT(gMulticastPassCount);
  SOAK_PRINT(" mcast_fail=");
  SOAK_PRINTLN(gMulticastFailCount);
}

void onUdp(void*, const uint8_t* payload, uint16_t length,
           const otMessageInfo& messageInfo) {
  uint8_t type = 0U;
  uint16_t seq = 0U;
  uint16_t declaredLength = 0U;
  if (!parsePayload(payload, length, &type, &seq, &declaredLength)) {
    ++gInvalidRxCount;
    return;
  }

  if (type == kPingType) {
    if (messageInfo.mMulticastLoop) return;
    ++gPingRxCount;
    gLastLength = declaredLength;
    if (buildPayload(kAckType, seq, declaredLength) &&
        gThread.sendUdp(messageInfo.mPeerAddr, messageInfo.mPeerPort,
                        gTxBuffer, declaredLength)) {
      ++gAckTxCount;
    }
    return;
  }

  if (type != kAckType) return;
  ++gAckRxCount;
  gLastAckSeq = seq;
  gLastLength = declaredLength;

  if (!gWaitingForAck || seq != gCurrentSeq) return;
  const size_t expectedIdx = gMulticastPhase ? gMulticastIndex : gUnicastIndex;
  if (expectedIdx >= kPayloadSizeCount ||
      declaredLength != kPayloadSizes[expectedIdx]) {
    ++gInvalidRxCount;
    return;
  }

  recordResult(gMulticastPhase, expectedIdx, 1U);
  ++gCurrentSeq;
  gRetryCount = 0U;
  gWaitingForAck = false;
  if (gMulticastPhase) {
    ++gMulticastIndex;
    if (gMulticastIndex >= kPayloadSizeCount) {
      gMulticastDone = true;
      printDone();
    }
  } else {
    ++gUnicastIndex;
    if (gUnicastIndex >= kPayloadSizeCount) {
      gUnicastDone = true;
      printStatus("unicast-complete");
    }
  }
}

bool sendCurrentPing(bool multicast) {
  const size_t idx = multicast ? gMulticastIndex : gUnicastIndex;
  if (idx >= kPayloadSizeCount) return false;
  const uint16_t payloadLength = kPayloadSizes[idx];
  const uint16_t safeLimit =
      multicast ? kMaxSafeMulticastPayload : kMaxSafeUnicastPayload;

  if (payloadLength > safeLimit) {
    recordResult(multicast, idx, 2U);
    ++gCurrentSeq;
    gRetryCount = 0U;
    gWaitingForAck = false;
    if (multicast) {
      ++gMulticastIndex;
      if (gMulticastIndex >= kPayloadSizeCount) {
        gMulticastDone = true;
        printDone();
      }
    } else {
      ++gUnicastIndex;
      if (gUnicastIndex >= kPayloadSizeCount) {
        gUnicastDone = true;
        printStatus("unicast-complete");
      }
    }
    return false;
  }

  otIp6Address destAddr = {};
  if (multicast) {
    memcpy(destAddr.mFields.m8, kMulticastAddrBytes,
           sizeof(kMulticastAddrBytes));
  } else if (!gThread.getLeaderRloc(&destAddr)) {
    return false;
  }

  if (!buildPayload(kPingType, gCurrentSeq, payloadLength)) return false;
  const bool ok = gThread.sendUdp(destAddr, kUdpPort, gTxBuffer, payloadLength);
  ++gPingTxCount;
  gLastLength = payloadLength;
  gLastSendMs = millis();
  gWaitingForAck = ok;
  if (!ok) {
    recordResult(multicast, idx, 2U);
  }
  return ok;
}

void failCurrentTimeout(bool multicast) {
  recordResult(multicast, multicast ? gMulticastIndex : gUnicastIndex, 4U);
  ++gCurrentSeq;
  gRetryCount = 0U;
  gWaitingForAck = false;
  if (multicast) {
    ++gMulticastIndex;
    if (gMulticastIndex >= kPayloadSizeCount) {
      gMulticastDone = true;
      printDone();
    }
  } else {
    ++gUnicastIndex;
    if (gUnicastIndex >= kPayloadSizeCount) {
      gUnicastDone = true;
      printStatus("unicast-complete");
    }
  }
}

void trySubscribeMulticast() {
  if (gMulticastSubscribed || !gThread.udpOpened(kUdpPort)) {
    return;
  }
  otIp6Address mcastAddr = {};
  memcpy(mcastAddr.mFields.m8, kMulticastAddrBytes,
         sizeof(kMulticastAddrBytes));
  gMulticastSubscribed = gThread.subscribeMulticast(mcastAddr);
  if (gMulticastSubscribed) {
    SOAK_PRINTLN("soak_boot mcast_subscribed=1");
  }
}

void exportDatasetOnce() {
  if (gDatasetExported || !gThread.attached()) return;
  size_t hexLen = 0U;
  if (!gThread.exportConfiguredOrActiveDatasetHex(
          gDatasetHex, sizeof(gDatasetHex), &hexLen)) {
    SOAK_PRINTLN("soak_persist_ok=0");
    return;
  }
  gDatasetExported = true;
  SOAK_PRINT("soak_persist_ok=1 dataset_hex=");
  SOAK_PRINTLN(gDatasetHex);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && !Serial1 && (millis() - waitStart) < 1500UL) {
    delay(10);
  }

  otOperationalDataset dataset = {};
  Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  gThread.setActiveDataset(dataset);
  const bool beginOk = gThread.begin(false);
  const bool udpOk = gThread.openUdp(kUdpPort, onUdp, nullptr);

  SOAK_PRINT("soak_boot role=");
  SOAK_PRINT(gThread.roleName());
  SOAK_PRINT(" begin_ok=");
  SOAK_PRINT(beginOk ? 1 : 0);
  SOAK_PRINT(" udp_request=");
  SOAK_PRINTLN(udpOk ? 1 : 0);
}

void loop() {
  gThread.process();
  updateResults();
  trySubscribeMulticast();
  exportDatasetOnce();

  const Nrf54ThreadExperimental::Role currentRole = gThread.role();
  if (currentRole != gLastRole) {
    gLastRole = currentRole;
    printStatus("role");
  }

  const bool sender =
      currentRole == Nrf54ThreadExperimental::Role::kChild ||
      currentRole == Nrf54ThreadExperimental::Role::kRouter;

  if (sender && !gTestStarted && gThread.attached() && gThread.udpOpened(kUdpPort)) {
    gTestStarted = true;
    printStatus("test-start");
  }

  if (sender && gTestStarted && !gUnicastDone && !gMulticastPhase) {
    const uint32_t now = millis();
    if (!gWaitingForAck && (now - gLastSendMs) >= kSendIntervalMs) {
      (void)sendCurrentPing(false);
    } else if (gWaitingForAck && (now - gLastSendMs) >= kAckTimeoutMs) {
      if (gRetryCount >= kMaxRetriesPerPayload) {
        failCurrentTimeout(false);
      } else {
        ++gRetryCount;
        ++gRetryTotal;
        (void)sendCurrentPing(false);
      }
    }
  }

  if (sender && gUnicastDone && !gMulticastPhase && !gMulticastDone &&
      !gWaitingForAck) {
    gMulticastPhase = true;
    ++gCurrentSeq;
    printStatus("mcast-start");
  }

  if (sender && gMulticastPhase && !gMulticastDone) {
    const uint32_t now = millis();
    if (!gWaitingForAck && (now - gLastSendMs) >= kSendIntervalMs) {
      (void)sendCurrentPing(true);
    } else if (gWaitingForAck && (now - gLastSendMs) >= kAckTimeoutMs) {
      if (gRetryCount >= kMaxRetriesPerPayload) {
        failCurrentTimeout(true);
      } else {
        ++gRetryCount;
        ++gRetryTotal;
        (void)sendCurrentPing(true);
      }
    }
  }

  if ((millis() - gLastPrintMs) >= kStatusIntervalMs) {
    gLastPrintMs = millis();
    printStatus("tick");
  }
}
