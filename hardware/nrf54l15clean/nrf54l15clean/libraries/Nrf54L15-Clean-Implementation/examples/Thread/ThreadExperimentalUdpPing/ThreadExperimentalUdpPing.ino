// ThreadExperimentalUdpPing
// =========================
//
// Two-board staged Thread UDP soak test.
//
// Upload this same sketch to two boards with:
//   Tools > Thread Core > Experimental Stage Core (Leader/Child/Router + UDP)
//
// Expected behavior:
//   - Both boards use the built-in demo Thread dataset.
//   - The default child-first attach policy makes one board attach as child and,
//     if no existing parent is present, one board promotes to leader.
//   - The non-leader board sends checked UDP payloads of several single-frame
//     sizes to the leader RLOC.
//   - The leader echoes each payload as an ACK.
//
// This example intentionally stays at <= 63-byte payloads. Larger 6LoWPAN
// fragmented UDP payloads are still experimental in this staged core and should
// be tested separately before being claimed as supported.
//
// A successful two-board run prints "done=1 fail=0" on the sender.

#include <nrf54_all.h>

#include <string.h>

using xiao_nrf54l15::Nrf54ThreadExperimental;

extern "C" {
__attribute__((used)) volatile uint32_t g_thread_test_results[16] = {0};
}

namespace {

constexpr uint16_t kUdpPort = 61631U;
constexpr uint32_t kSendIntervalMs = 350UL;
constexpr uint32_t kAckTimeoutMs = 2500UL;
constexpr uint8_t kMaxRetriesPerPayload = 4U;
constexpr uint16_t kMaxPayloadLength = 63U;
constexpr uint8_t kMagic = 0x54U;  // 'T'
constexpr uint8_t kPingType = 0x50U;  // 'P'
constexpr uint8_t kAckType = 0x41U;  // 'A'

constexpr uint16_t kPayloadSizes[] = {
    8U, 16U, 31U, 63U,
};
constexpr size_t kPayloadSizeCount =
    sizeof(kPayloadSizes) / sizeof(kPayloadSizes[0]);

Nrf54ThreadExperimental gThread;
Nrf54ThreadExperimental::Role gLastRole =
    Nrf54ThreadExperimental::Role::kUnknown;

uint8_t gTxBuffer[kMaxPayloadLength] = {0};
uint16_t gCurrentSeq = 1U;
size_t gPayloadIndex = 0U;
uint32_t gLastSendMs = 0U;
uint32_t gLastPrintMs = 0U;
uint8_t gRetryCount = 0U;
bool gWaitingForAck = false;
bool gDone = false;

uint32_t gPingTxCount = 0U;
uint32_t gPingRxCount = 0U;
uint32_t gAckTxCount = 0U;
uint32_t gAckRxCount = 0U;
uint32_t gPassCount = 0U;
uint32_t gFailCount = 0U;
uint32_t gRetryTotal = 0U;
uint32_t gInvalidRxCount = 0U;
uint16_t gLastLength = 0U;
uint16_t gLastAckSeq = 0U;

uint8_t checksum8(const uint8_t* data, uint16_t length) {
  uint8_t checksum = 0U;
  if (data == nullptr) {
    return checksum;
  }

  for (uint16_t i = 0U; i < length; ++i) {
    checksum = static_cast<uint8_t>((checksum << 1U) | (checksum >> 7U));
    checksum ^= data[i];
    checksum = static_cast<uint8_t>(checksum + 0x3DU);
  }
  return checksum;
}

bool buildPayload(uint8_t type, uint16_t seq, uint16_t length) {
  if (length < 8U || length > sizeof(gTxBuffer)) {
    return false;
  }

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

  if (payload[0] != kMagic) {
    return false;
  }

  const uint16_t declaredLength =
      static_cast<uint16_t>(payload[4]) |
      (static_cast<uint16_t>(payload[5]) << 8U);
  if (declaredLength != length) {
    return false;
  }

  if (checksum8(payload, length - 1U) != payload[length - 1U]) {
    return false;
  }

  *outType = payload[1];
  *outSeq = static_cast<uint16_t>(payload[2]) |
            (static_cast<uint16_t>(payload[3]) << 8U);
  *outDeclaredLength = declaredLength;
  return true;
}

void updateResults() {
  g_thread_test_results[0] = gThread.started() ? 1U : 0U;
  g_thread_test_results[1] = gThread.attached() ? 1U : 0U;
  g_thread_test_results[2] = static_cast<uint32_t>(gThread.role());
  g_thread_test_results[3] = gThread.rloc16();
  g_thread_test_results[4] = gThread.partitionId();
  g_thread_test_results[5] = static_cast<uint32_t>(gThread.lastError());
  g_thread_test_results[6] = static_cast<uint32_t>(gThread.lastUdpError());
  g_thread_test_results[7] = gPingTxCount;
  g_thread_test_results[8] = gPingRxCount;
  g_thread_test_results[9] = gAckTxCount;
  g_thread_test_results[10] = gAckRxCount;
  g_thread_test_results[11] = gPassCount;
  g_thread_test_results[12] = gFailCount;
  g_thread_test_results[13] = gDone ? 1U : 0U;
  g_thread_test_results[14] = gInvalidRxCount;
  g_thread_test_results[15] = gLastLength;
}

void printStatus(const char* reason) {
  Serial.print("thread_udp_soak reason=");
  Serial.print(reason);
  Serial.print(" role=");
  Serial.print(gThread.roleName());
  Serial.print(" rloc16=0x");
  Serial.print(gThread.rloc16(), HEX);
  Serial.print(" part=0x");
  Serial.print(gThread.partitionId(), HEX);
  Serial.print(" seq=");
  Serial.print(gCurrentSeq);
  Serial.print(" idx=");
  Serial.print(static_cast<unsigned>(gPayloadIndex));
  Serial.print("/");
  Serial.print(static_cast<unsigned>(kPayloadSizeCount));
  Serial.print(" len=");
  Serial.print(gLastLength);
  Serial.print(" ping=");
  Serial.print(gPingTxCount);
  Serial.print("/");
  Serial.print(gPingRxCount);
  Serial.print(" ack=");
  Serial.print(gAckTxCount);
  Serial.print("/");
  Serial.print(gAckRxCount);
  Serial.print(" pass=");
  Serial.print(gPassCount);
  Serial.print(" fail=");
  Serial.print(gFailCount);
  Serial.print(" retry=");
  Serial.print(gRetryTotal);
  Serial.print(" invalid=");
  Serial.print(gInvalidRxCount);
  Serial.print(" wait=");
  Serial.print(gWaitingForAck ? 1 : 0);
  Serial.print(" done=");
  Serial.print(gDone ? 1 : 0);
  Serial.print(" err=");
  Serial.print(static_cast<int>(gThread.lastError()));
  Serial.print("/");
  Serial.print(static_cast<int>(gThread.lastUdpError()));
  Serial.println();
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
    ++gPingRxCount;
    gLastLength = declaredLength;
    if (buildPayload(kAckType, seq, declaredLength) &&
        gThread.sendUdp(messageInfo.mPeerAddr, messageInfo.mPeerPort, gTxBuffer,
                        declaredLength)) {
      ++gAckTxCount;
    }
    return;
  }

  if (type == kAckType) {
    ++gAckRxCount;
    gLastAckSeq = seq;
    gLastLength = declaredLength;
    if (gWaitingForAck && seq == gCurrentSeq &&
        gPayloadIndex < kPayloadSizeCount &&
        declaredLength == kPayloadSizes[gPayloadIndex]) {
      ++gPassCount;
      ++gCurrentSeq;
      ++gPayloadIndex;
      gRetryCount = 0U;
      gWaitingForAck = false;
      gDone = gPayloadIndex >= kPayloadSizeCount;
      printStatus(gDone ? "complete" : "ack");
    } else {
      ++gInvalidRxCount;
    }
  }
}

bool sendCurrentPing() {
  if (gPayloadIndex >= kPayloadSizeCount) {
    gDone = true;
    return false;
  }

  otIp6Address leaderAddr = {};
  const uint16_t payloadLength = kPayloadSizes[gPayloadIndex];
  if (!gThread.getLeaderRloc(&leaderAddr) ||
      !buildPayload(kPingType, gCurrentSeq, payloadLength)) {
    return false;
  }

  if (!gThread.sendUdp(leaderAddr, kUdpPort, gTxBuffer, payloadLength)) {
    return false;
  }

  ++gPingTxCount;
  gLastLength = payloadLength;
  gLastSendMs = millis();
  gWaitingForAck = true;
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  otOperationalDataset dataset = {};
  Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  gThread.setActiveDataset(dataset);
  gThread.begin();
  gThread.openUdp(kUdpPort, onUdp, nullptr);
  Serial.println("thread_udp_soak boot");
#else
  Serial.println(
      "Enable Tools > Thread Core > Experimental Stage Core (Leader/Child/Router + UDP).");
#endif
}

void loop() {
  gThread.process();
  updateResults();

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  const Nrf54ThreadExperimental::Role currentRole = gThread.role();
  if (currentRole != gLastRole) {
    gLastRole = currentRole;
    printStatus("role");
  }

  const bool sender =
      currentRole == Nrf54ThreadExperimental::Role::kChild ||
      currentRole == Nrf54ThreadExperimental::Role::kRouter;
  if (sender && !gDone) {
    const uint32_t now = millis();
    if (!gWaitingForAck && (now - gLastSendMs) >= kSendIntervalMs) {
      (void)sendCurrentPing();
    } else if (gWaitingForAck && (now - gLastSendMs) >= kAckTimeoutMs) {
      if (gRetryCount >= kMaxRetriesPerPayload) {
        ++gFailCount;
        ++gCurrentSeq;
        ++gPayloadIndex;
        gRetryCount = 0U;
        gWaitingForAck = false;
        gDone = gPayloadIndex >= kPayloadSizeCount;
        printStatus(gDone ? "timeout-complete" : "timeout-next");
      } else {
        ++gRetryCount;
        ++gRetryTotal;
        (void)sendCurrentPing();
      }
    }
  }

  if ((millis() - gLastPrintMs) >= 1000UL) {
    gLastPrintMs = millis();
    printStatus("tick");
  }
#endif
}
