// MatterCommissionAndControl
//
// Staged two-board demo for the current in-tree Thread/Matter work:
//   Board A ROLE=COMMISSIONER: forms the demo Thread network, accepts a compact
//   PSK-authenticated dataset request, then sends Matter On/Off commands.
//   Board B ROLE=JOINER: joins the same staged Thread network, requests the
//   compact dataset proof, then acts as a Matter On/Off light.
//
// This is not full Matter ecosystem commissioning yet. It is a practical
// Thread + staged Matter control-path exercise while the full CASE/PASE path
// is still being brought into the core.

#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building."
#endif

using xiao_nrf54l15::MatterPbkdf2;
using xiao_nrf54l15::Nrf54ThreadExperimental;

enum class Role : uint8_t { COMMISSIONER = 0, JOINER = 1 };
constexpr Role ROLE = Role::COMMISSIONER;

namespace {

constexpr uint16_t kCommissionPort = 5542U;
constexpr uint16_t kMatterPort = 5540U;
constexpr char kPSKd[] = "THREADJOIN";
constexpr uint32_t kStatusIntervalMs = 3000U;
constexpr uint32_t kAnnounceIntervalMs = 3000U;
constexpr uint32_t kJoinRequestIntervalMs = 5000U;
constexpr uint32_t kCommandIntervalMs = 5000U;

constexpr uint16_t kProtocolInteractionModel = 0x0001U;
constexpr uint8_t kOpInvokeCommandRequest = 0x08U;
constexpr uint32_t kOnOffClusterId = 0x0006U;
constexpr uint32_t kIdentifyClusterId = 0x0003U;
constexpr uint32_t kOffCommandId = 0x00U;
constexpr uint32_t kOnCommandId = 0x01U;
constexpr uint32_t kToggleCommandId = 0x02U;
constexpr uint32_t kIdentifyCommandId = 0x00U;

constexpr uint8_t kMsgAnnounce = 0x00U;
constexpr uint8_t kMsgJoinReq = 0x01U;
constexpr uint8_t kMsgJoinResp = 0x02U;
constexpr size_t kCompactDatasetLen = 68U;

static const otIp6Address kMeshLocalAllNodes = {
  .mFields = {
    .m8 = {0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}
  }
};

Nrf54ThreadExperimental gThread;
uint8_t gPsk[16] = {0};
otIp6Address gPeerAddr = {};
bool gPeerKnown = false;
bool gCommissioned = false;
bool gJoined = false;
bool gMatterPortOpened = false;
bool gLightOn = false;
uint32_t gLastStatusMs = 0U;
uint32_t gLastAnnounceMs = 0U;
uint32_t gLastJoinRequestMs = 0U;
uint32_t gLastCommandMs = 0U;
uint8_t gCommandSeq = 0U;

void writeU16(uint16_t value, uint8_t* out, size_t off) {
  out[off] = static_cast<uint8_t>(value & 0xFFU);
  out[off + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(uint32_t value, uint8_t* out, size_t off) {
  out[off] = static_cast<uint8_t>(value & 0xFFU);
  out[off + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  out[off + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  out[off + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

uint16_t readU16(const uint8_t* data, size_t off) {
  return static_cast<uint16_t>(data[off]) |
         (static_cast<uint16_t>(data[off + 1U]) << 8U);
}

uint32_t readU32(const uint8_t* data, size_t off) {
  return static_cast<uint32_t>(data[off]) |
         (static_cast<uint32_t>(data[off + 1U]) << 8U) |
         (static_cast<uint32_t>(data[off + 2U]) << 16U) |
         (static_cast<uint32_t>(data[off + 3U]) << 24U);
}

void computeMac(const uint8_t key[16], const uint8_t* data, size_t len,
                uint8_t mac[16]) {
  uint8_t hmac[32] = {0};
  MatterPbkdf2::hmacSha256(key, 16, data, len, hmac);
  memcpy(mac, hmac, 16);
}

void aesCtr(const uint8_t key[16], const uint8_t* iv, const uint8_t* in,
            uint8_t* out, size_t len) {
  uint8_t ctr[16] = {0};
  memcpy(ctr, iv, 16);
  for (size_t i = 0; i < len; i += 16U) {
    uint8_t stream[32] = {0};
    uint8_t digest[32] = {0};
    memcpy(stream, key, 16);
    memcpy(stream + 16, ctr, 16);
    MatterPbkdf2::sha256(stream, sizeof(stream), digest);
    for (size_t j = 0; j < 16U && (i + j) < len; ++j) {
      out[i + j] = in[i + j] ^ digest[j];
    }
    for (int j = 15; j >= 0; --j) {
      ctr[j]++;
      if (ctr[j] != 0U) break;
    }
  }
}

void derivePsk() {
  constexpr char kSalt[] = "ThreadJoinSalt";
  MatterPbkdf2::deriveKey(reinterpret_cast<const uint8_t*>(kPSKd),
                          strlen(kPSKd),
                          reinterpret_cast<const uint8_t*>(kSalt),
                          strlen(kSalt), 1000, sizeof(gPsk), gPsk);
}

void markCompactDatasetComponents(otOperationalDataset* dataset) {
  if (dataset == nullptr) return;
  dataset->mComponents.mIsActiveTimestampPresent = true;
  dataset->mComponents.mIsNetworkKeyPresent = true;
  dataset->mComponents.mIsNetworkNamePresent = true;
  dataset->mComponents.mIsExtendedPanIdPresent = true;
  dataset->mComponents.mIsPanIdPresent = true;
  dataset->mComponents.mIsChannelPresent = true;
  dataset->mComponents.mIsPskcPresent = true;
}

bool buildCompactDataset(uint8_t out[kCompactDatasetLen],
                         const uint8_t nonce[8]) {
  if (out == nullptr || nonce == nullptr) return false;
  otOperationalDataset dataset = {};
  if (!gThread.getConfiguredOrActiveDataset(&dataset)) return false;
  memset(out, 0, kCompactDatasetLen);
  memcpy(out, &dataset.mChannel, 2);
  memcpy(out + 2, &dataset.mPanId, 2);
  memcpy(out + 4, dataset.mExtendedPanId.m8, 8);
  memcpy(out + 12, dataset.mNetworkName.m8, 16);
  memcpy(out + 28, dataset.mNetworkKey.m8, 16);
  memcpy(out + 44, dataset.mPskc.m8, 16);
  memcpy(out + 60, nonce, 8);
  return true;
}

void applyCompactDataset(const uint8_t data[kCompactDatasetLen]) {
  otOperationalDataset dataset = {};
  memcpy(&dataset.mChannel, data, 2);
  memcpy(&dataset.mPanId, data + 2, 2);
  memcpy(dataset.mExtendedPanId.m8, data + 4, 8);
  memcpy(dataset.mNetworkName.m8, data + 12, 16);
  memcpy(dataset.mNetworkKey.m8, data + 28, 16);
  memcpy(dataset.mPskc.m8, data + 44, 16);
  dataset.mActiveTimestamp.mSeconds = 1ULL;
  dataset.mActiveTimestamp.mAuthoritative = true;
  markCompactDatasetComponents(&dataset);
  gThread.setActiveDataset(dataset);
}

static uint16_t gMsgId = 0U;
static uint16_t gExchangeId = 0x1234U;

bool sendMatterCommand(const otIp6Address& peer, uint32_t clusterId,
                       uint32_t commandId) {
  uint8_t frame[64] = {0};
  frame[0] = 0x05U;
  frame[1] = 0U;
  frame[2] = 0U;
  writeU16(++gMsgId, frame, 3);
  writeU32(0U, frame, 5);
  writeU32(0U, frame, 9);
  writeU16(++gExchangeId, frame, 13);
  writeU16(0U, frame, 15);
  writeU16(kProtocolInteractionModel, frame, 17);
  frame[19] = kOpInvokeCommandRequest;

  size_t off = 20U;
  frame[off++] = 0U;
  frame[off++] = 0U;
  writeU16(1U, frame, off);
  off += 2U;
  writeU32(clusterId, frame, off);
  off += 4U;
  writeU32(commandId, frame, off);
  off += 4U;
  frame[off++] = 0x18U;
  frame[off++] = 0x18U;
  return gThread.sendUdpFrom(kMatterPort, peer, kMatterPort, frame,
                             static_cast<uint16_t>(off));
}

void handleJoinRequest(const uint8_t* data, uint16_t len,
                       const otMessageInfo& info) {
  if (len != 25U) return;
  memcpy(&gPeerAddr, &info.mPeerAddr, sizeof(gPeerAddr));
  gPeerKnown = true;

  const uint8_t* nonce = data + 1;
  const uint8_t* mac = data + 9;
  uint8_t expectedMac[16] = {0};
  computeMac(gPsk, nonce, 8, expectedMac);
  if (memcmp(mac, expectedMac, sizeof(expectedMac)) != 0) {
    Serial.println("mcc join mac fail");
    return;
  }

  uint8_t compact[kCompactDatasetLen] = {0};
  if (!buildCompactDataset(compact, nonce)) return;

  uint8_t packet[1 + kCompactDatasetLen + 16] = {0};
  packet[0] = kMsgJoinResp;
  uint8_t iv[16] = {0};
  aesCtr(gPsk, iv, compact, packet + 1, kCompactDatasetLen);
  computeMac(gPsk, packet + 1, kCompactDatasetLen,
             packet + 1 + kCompactDatasetLen);
  for (uint8_t i = 0; i < 3U; ++i) {
    gThread.sendUdpFrom(kCommissionPort, info.mPeerAddr, info.mPeerPort,
                        packet, sizeof(packet));
    delay(80);
  }
  gCommissioned = true;
  Serial.println("mcc commissioned");
}

void handleJoinResponse(const uint8_t* data, uint16_t len) {
  if (len != (1U + kCompactDatasetLen + 16U)) return;
  const uint8_t* encrypted = data + 1;
  const uint8_t* mac = data + 1 + kCompactDatasetLen;
  uint8_t expectedMac[16] = {0};
  computeMac(gPsk, encrypted, kCompactDatasetLen, expectedMac);
  if (memcmp(mac, expectedMac, sizeof(expectedMac)) != 0) {
    Serial.println("mcc response mac fail");
    return;
  }

  uint8_t compact[kCompactDatasetLen] = {0};
  uint8_t iv[16] = {0};
  aesCtr(gPsk, iv, encrypted, compact, sizeof(compact));
  applyCompactDataset(compact);
  gJoined = true;
  Serial.println("mcc joined");
}

void handleInvoke(const uint8_t* payload, uint16_t len) {
  if (payload == nullptr || len < 12U) return;
  const uint32_t clusterId = readU32(payload, 4);
  const uint32_t commandId = readU32(payload, 8);
  Serial.print("mcc matter cluster=0x");
  Serial.print(clusterId, HEX);
  Serial.print(" cmd=0x");
  Serial.print(commandId, HEX);
  if (clusterId == kOnOffClusterId) {
    if (commandId == kOnCommandId) {
      gLightOn = true;
      Serial.println(" on");
    } else if (commandId == kOffCommandId) {
      gLightOn = false;
      Serial.println(" off");
    } else if (commandId == kToggleCommandId) {
      gLightOn = !gLightOn;
      Serial.println(" toggle");
    } else {
      Serial.println(" unknown");
    }
  } else if (clusterId == kIdentifyClusterId &&
             commandId == kIdentifyCommandId) {
    Serial.println(" identify");
  } else {
    Serial.println(" unsupported");
  }
}

void onCommissionUdp(void*, const uint8_t* data, uint16_t len,
                     const otMessageInfo& info) {
  if (data == nullptr || len < 1U) return;
  if (data[0] == kMsgAnnounce) {
    memcpy(&gPeerAddr, &info.mPeerAddr, sizeof(gPeerAddr));
    gPeerKnown = true;
    return;
  }
  if (ROLE == Role::COMMISSIONER && data[0] == kMsgJoinReq) {
    handleJoinRequest(data, len, info);
  } else if (ROLE == Role::JOINER && data[0] == kMsgJoinResp) {
    handleJoinResponse(data, len);
  }
}

void onMatterUdp(void*, const uint8_t* data, uint16_t len,
                 const otMessageInfo&) {
  if (data == nullptr || len < 20U) return;
  const uint16_t protocolId = readU16(data, 17);
  const uint8_t opcode = data[19];
  if (protocolId == kProtocolInteractionModel &&
      opcode == kOpInvokeCommandRequest) {
    handleInvoke(data + 20, static_cast<uint16_t>(len - 20U));
  }
}

void printStatus() {
  Serial.print("mcc role=");
  Serial.print(ROLE == Role::COMMISSIONER ? "commissioner" : "joiner");
  Serial.print(" thread=");
  Serial.print(gThread.roleName());
  Serial.print(" udp_comm=");
  Serial.print(gThread.udpOpened(kCommissionPort) ? 1 : 0);
  Serial.print(" udp_matter=");
  Serial.print(gThread.udpOpened(kMatterPort) ? 1 : 0);
  Serial.print(" peer=");
  Serial.print(gPeerKnown ? 1 : 0);
  Serial.print(" commissioned=");
  Serial.print(gCommissioned ? 1 : 0);
  Serial.print(" joined=");
  Serial.print(gJoined ? 1 : 0);
  Serial.print(" light=");
  Serial.println(gLightOn ? 1 : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && !Serial1 && (millis() - waitStart) < 1500UL) {
    delay(10);
  }

#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  Serial.println();
  Serial.print("mcc MatterCommissionAndControl role=");
  Serial.println(ROLE == Role::COMMISSIONER ? "COMMISSIONER" : "JOINER");

  derivePsk();
  otOperationalDataset dataset = {};
  Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  gThread.setActiveDataset(dataset);
  if (ROLE == Role::COMMISSIONER) {
    gThread.beginAsRouter();
  } else {
    gThread.begin(false);
  }
  gThread.openUdp(kCommissionPort, onCommissionUdp, nullptr);
  if (ROLE == Role::COMMISSIONER) {
    gThread.openUdp(kMatterPort, onMatterUdp, nullptr);
    gMatterPortOpened = true;
  }
  printStatus();
}

void loop() {
  gThread.process();

#if defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, gLightOn ? HIGH : LOW);
#endif

  if (ROLE == Role::COMMISSIONER && gThread.udpOpened(kCommissionPort) &&
      (millis() - gLastAnnounceMs) >= kAnnounceIntervalMs) {
    gLastAnnounceMs = millis();
    const uint8_t announce[1] = {kMsgAnnounce};
    gThread.sendUdpFrom(kCommissionPort, kMeshLocalAllNodes,
                        kCommissionPort, announce, sizeof(announce));
  }

  if (ROLE == Role::JOINER && gPeerKnown && !gJoined &&
      (millis() - gLastJoinRequestMs) >= kJoinRequestIntervalMs) {
    gLastJoinRequestMs = millis();
    uint8_t request[25] = {0};
    request[0] = kMsgJoinReq;
    const uint32_t now = millis();
    memcpy(request + 1, &now, sizeof(now));
    request[5] = 'J';
    request[6] = 'O';
    request[7] = 'I';
    request[8] = 'N';
    computeMac(gPsk, request + 1, 8, request + 9);
    gThread.sendUdpFrom(kCommissionPort, gPeerAddr, kCommissionPort,
                        request, sizeof(request));
    Serial.println("mcc join request sent");
  }

  if (ROLE == Role::JOINER && gJoined && !gMatterPortOpened &&
      gThread.attached()) {
    gMatterPortOpened = gThread.openUdp(kMatterPort, onMatterUdp, nullptr);
    if (gMatterPortOpened) {
      Serial.println("mcc light ready");
    }
  }

  if (ROLE == Role::COMMISSIONER && gCommissioned && gPeerKnown &&
      gThread.attached() &&
      (millis() - gLastCommandMs) >= kCommandIntervalMs) {
    gLastCommandMs = millis();
    const uint32_t clusterId =
        (gCommandSeq % 4U < 3U) ? kOnOffClusterId : kIdentifyClusterId;
    const uint32_t commandId = [](uint8_t seq) -> uint32_t {
      switch (seq % 4U) {
        case 0: return kOnCommandId;
        case 1: return kOffCommandId;
        case 2: return kToggleCommandId;
        default: return kIdentifyCommandId;
      }
    }(gCommandSeq);
    Serial.print("mcc send cmd=0x");
    Serial.print(commandId, HEX);
    Serial.print(" ");
    Serial.println(sendMatterCommand(gPeerAddr, clusterId, commandId) ? "ok" : "fail");
    ++gCommandSeq;
  }

  if ((millis() - gLastStatusMs) >= kStatusIntervalMs) {
    gLastStatusMs = millis();
    printStatus();
  }
}
