#include "openthread_platform_nrf54l15.h"

#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/message.h>
#include <openthread/platform/settings.h>
#include <openthread/thread.h>
#include <openthread/udp.h>

using xiao_nrf54l15::OpenThreadPlatformSkeleton;

namespace {

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
constexpr uint32_t kStageInitDelayMs = 2000UL;
constexpr uint32_t kStageDatasetDelayMs = 3000UL;
constexpr uint32_t kStageDatasetApplyDelayMs = 4000UL;
constexpr uint32_t kStageIp6EnableDelayMs = 5000UL;
constexpr uint32_t kStageThreadEnableDelayMs = 6000UL;
constexpr uint32_t kUdpRetryMs = 4000UL;
constexpr uint16_t kUdpPort = 61631U;
constexpr otPanId kFixedPanId = 0x5D6A;
constexpr uint32_t kFixedChannelMask = OT_CHANNEL_15_MASK;
constexpr uint8_t kFixedChannel = 15U;
constexpr char kFixedNetworkName[] = "Nrf54Stage";
constexpr char kPingPayload[] = "stage-ping";
constexpr char kPongPayload[] = "stage-pong";
constexpr uint8_t kFixedNetworkKey[OT_NETWORK_KEY_SIZE] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
};
constexpr uint8_t kFixedExtPanId[OT_EXT_PAN_ID_SIZE] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
};
constexpr uint8_t kFixedMeshLocalPrefix[OT_MESH_LOCAL_PREFIX_SIZE] = {
    0xFD, 0x54, 0x15, 0xC0, 0xDE, 0x00, 0x00, 0x00,
};
constexpr uint8_t kFixedPskc[OT_PSKC_MAX_SIZE] = {
    0xA5, 0x4C, 0x8D, 0x11, 0x72, 0x3F, 0x90, 0xBE,
    0x4A, 0x62, 0x18, 0xD4, 0xCE, 0x07, 0x39, 0x5B,
};

otInstance* gInstance = nullptr;
otUdpSocket gSocket = {};
otOperationalDataset gDataset = {};
otError gDatasetSetError = OT_ERROR_NONE;
otError gDatasetReadError = OT_ERROR_NONE;
otError gLinkModeSetError = OT_ERROR_NONE;
otError gIp6SetError = OT_ERROR_NONE;
otError gThreadSetError = OT_ERROR_NONE;
otError gSocketOpenError = OT_ERROR_NONE;
otError gSocketBindError = OT_ERROR_NONE;
otError gLastPingError = OT_ERROR_NONE;
otError gLastPongError = OT_ERROR_NONE;
otDeviceRole gLastRole = OT_DEVICE_ROLE_DISABLED;
otIp6Address gPendingReplyAddr = {};
uint32_t gLastReportMs = 0;
uint32_t gLastPingAttemptMs = 0;
uint32_t gPingTxCount = 0;
uint32_t gPingRxCount = 0;
uint32_t gPongTxCount = 0;
uint32_t gPongRxCount = 0;
uint16_t gRloc16 = OT_RADIO_INVALID_SHORT_ADDR;
uint16_t gPendingReplyPort = 0U;
bool gInstanceCreated = false;
bool gInstanceInitialized = false;
bool gDatasetGenerated = false;
bool gDatasetApplied = false;
bool gIp6EnableAttempted = false;
bool gThreadEnableAttempted = false;
bool gSocketOpened = false;
bool gReplyPending = false;
bool gChildPayloadSuccess = false;
bool gLeaderPayloadSuccess = false;
char gLastRxText[24] = {0};
char gLastTxText[24] = {0};
#endif

bool serialLogReady() {
  return static_cast<bool>(Serial);
}

void logLine(const char* line) {
  if (!serialLogReady()) {
    return;
  }
  Serial.println(line);
}

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
const char* roleToString(otDeviceRole role) {
  switch (role) {
    case OT_DEVICE_ROLE_DISABLED:
      return "disabled";
    case OT_DEVICE_ROLE_DETACHED:
      return "detached";
    case OT_DEVICE_ROLE_CHILD:
      return "child";
    case OT_DEVICE_ROLE_ROUTER:
      return "router";
    case OT_DEVICE_ROLE_LEADER:
      return "leader";
    default:
      return "unknown";
  }
}

void buildFixedDataset(otOperationalDataset* dataset) {
  if (dataset == nullptr) {
    return;
  }

  memset(dataset, 0, sizeof(*dataset));
  dataset->mActiveTimestamp.mSeconds = 1ULL;
  dataset->mActiveTimestamp.mAuthoritative = true;
  memcpy(dataset->mNetworkKey.m8, kFixedNetworkKey, sizeof(kFixedNetworkKey));
  strncpy(dataset->mNetworkName.m8,
          kFixedNetworkName,
          sizeof(dataset->mNetworkName.m8) - 1U);
  memcpy(dataset->mExtendedPanId.m8, kFixedExtPanId, sizeof(kFixedExtPanId));
  memcpy(dataset->mMeshLocalPrefix.m8,
         kFixedMeshLocalPrefix,
         sizeof(kFixedMeshLocalPrefix));
  memcpy(dataset->mPskc.m8, kFixedPskc, sizeof(kFixedPskc));
  dataset->mPanId = kFixedPanId;
  dataset->mChannel = kFixedChannel;
  dataset->mWakeupChannel = kFixedChannel;
  dataset->mChannelMask = kFixedChannelMask;
  dataset->mSecurityPolicy.mRotationTime = 672U;
  dataset->mSecurityPolicy.mObtainNetworkKeyEnabled = true;
  dataset->mSecurityPolicy.mNativeCommissioningEnabled = true;
  dataset->mSecurityPolicy.mRoutersEnabled = true;
  dataset->mSecurityPolicy.mExternalCommissioningEnabled = true;
  dataset->mSecurityPolicy.mNetworkKeyProvisioningEnabled = true;
  dataset->mSecurityPolicy.mVersionThresholdForRouting = 3U;

  dataset->mComponents.mIsActiveTimestampPresent = true;
  dataset->mComponents.mIsNetworkKeyPresent = true;
  dataset->mComponents.mIsNetworkNamePresent = true;
  dataset->mComponents.mIsExtendedPanIdPresent = true;
  dataset->mComponents.mIsMeshLocalPrefixPresent = true;
  dataset->mComponents.mIsPanIdPresent = true;
  dataset->mComponents.mIsChannelPresent = true;
  dataset->mComponents.mIsPskcPresent = true;
  dataset->mComponents.mIsSecurityPolicyPresent = true;
  dataset->mComponents.mIsChannelMaskPresent = true;
  dataset->mComponents.mIsWakeupChannelPresent = true;
}

void storeText(char* dst, size_t dstLen, const char* src) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }
  memset(dst, 0, dstLen);
  if (src != nullptr) {
    strncpy(dst, src, dstLen - 1U);
  }
}

void handleUdpReceive(void*, otMessage* message, const otMessageInfo* messageInfo) {
  if (message == nullptr || messageInfo == nullptr) {
    return;
  }

  char payload[24] = {0};
  uint16_t length = otMessageGetLength(message);
  if (length >= sizeof(payload)) {
    length = sizeof(payload) - 1U;
  }
  if (length != 0U) {
    const uint16_t copied = otMessageRead(message, 0, payload, length);
    payload[copied] = '\0';
  }

  storeText(gLastRxText, sizeof(gLastRxText), payload);

  if (strcmp(payload, kPingPayload) == 0) {
    ++gPingRxCount;
    gPendingReplyAddr = messageInfo->mPeerAddr;
    gPendingReplyPort = messageInfo->mPeerPort;
    gReplyPending = true;
  } else if (strcmp(payload, kPongPayload) == 0) {
    ++gPongRxCount;
    gChildPayloadSuccess = true;
  }
}

bool sendUdpPayload(const char* payload,
                    const otIp6Address& peerAddr,
                    uint16_t peerPort,
                    otError* outError) {
  if (outError != nullptr) {
    *outError = OT_ERROR_NONE;
  }
  if (gInstance == nullptr || !gSocketOpened || payload == nullptr) {
    if (outError != nullptr) {
      *outError = OT_ERROR_INVALID_STATE;
    }
    return false;
  }

  otMessage* message = otUdpNewMessage(gInstance, nullptr);
  if (message == nullptr) {
    if (outError != nullptr) {
      *outError = OT_ERROR_NO_BUFS;
    }
    return false;
  }

  otError error = otMessageAppend(message, payload, strlen(payload));
  if (error != OT_ERROR_NONE) {
    otMessageFree(message);
    if (outError != nullptr) {
      *outError = error;
    }
    return false;
  }

  otMessageInfo messageInfo = {};
  const otIp6Address* meshLocalEid = otThreadGetMeshLocalEid(gInstance);
  if (meshLocalEid != nullptr) {
    messageInfo.mSockAddr = *meshLocalEid;
  }
  messageInfo.mSockPort = kUdpPort;
  messageInfo.mPeerAddr = peerAddr;
  messageInfo.mPeerPort = peerPort;

  error = otUdpSend(gInstance, &gSocket, message, &messageInfo);
  if (error != OT_ERROR_NONE) {
    otMessageFree(message);
    if (outError != nullptr) {
      *outError = error;
    }
    return false;
  }

  storeText(gLastTxText, sizeof(gLastTxText), payload);
  return true;
}

void printState() {
  if (!serialLogReady()) {
    return;
  }

  Serial.print("ot_udp role=");
  Serial.print(roleToString(gLastRole));
  Serial.print(" rloc16=0x");
  Serial.print(gRloc16, HEX);
  Serial.print(" dataset=");
  Serial.print(gDatasetApplied ? 1 : 0);
  Serial.print("/");
  Serial.print(static_cast<int>(gDatasetSetError));
  Serial.print("/");
  Serial.print(static_cast<int>(gDatasetReadError));
  Serial.print(" ip6=");
  Serial.print(static_cast<int>(gIp6SetError));
  Serial.print(" thread=");
  Serial.print(static_cast<int>(gThreadSetError));
  Serial.print(" socket=");
  Serial.print(gSocketOpened ? 1 : 0);
  Serial.print("/");
  Serial.print(static_cast<int>(gSocketOpenError));
  Serial.print("/");
  Serial.print(static_cast<int>(gSocketBindError));
  Serial.print(" ping=");
  Serial.print(static_cast<unsigned long>(gPingTxCount));
  Serial.print("/");
  Serial.print(static_cast<unsigned long>(gPingRxCount));
  Serial.print("/");
  Serial.print(static_cast<int>(gLastPingError));
  Serial.print(" pong=");
  Serial.print(static_cast<unsigned long>(gPongTxCount));
  Serial.print("/");
  Serial.print(static_cast<unsigned long>(gPongRxCount));
  Serial.print("/");
  Serial.print(static_cast<int>(gLastPongError));
  Serial.print(" ok=");
  Serial.print(gLeaderPayloadSuccess ? 1 : 0);
  Serial.print("/");
  Serial.print(gChildPayloadSuccess ? 1 : 0);
  Serial.print(" lasttx=");
  Serial.print(gLastTxText);
  Serial.print(" lastrx=");
  Serial.print(gLastRxText);
  Serial.println();
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  logLine("ot_udp boot");
  OpenThreadPlatformSkeleton::begin();
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  otPlatSettingsInit(nullptr, nullptr, 0);
  otPlatSettingsWipe(nullptr);
#endif
  logLine("ot_udp platform-ready");
}

void loop() {
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  OpenThreadPlatformSkeleton::process(gInstance);

  if (!gInstanceCreated && millis() >= kStageInitDelayMs) {
    gInstance = otInstanceInitSingle();
    gInstanceCreated = gInstance != nullptr;
    gInstanceInitialized =
        gInstanceCreated && otInstanceIsInitialized(gInstance);
    logLine("ot_udp init");
  }

  if (gInstanceInitialized && !gDatasetGenerated &&
      millis() >= kStageDatasetDelayMs) {
    buildFixedDataset(&gDataset);
    gDatasetGenerated = true;
    logLine("ot_udp dataset-generated");
  }

  if (gDatasetGenerated && !gDatasetApplied &&
      millis() >= kStageDatasetApplyDelayMs) {
    gDatasetSetError = otDatasetSetActive(gInstance, &gDataset);
    if (gDatasetSetError == OT_ERROR_NONE) {
      otOperationalDataset activeDataset = {};
      gDatasetReadError = otDatasetGetActive(gInstance, &activeDataset);
      gDatasetApplied = (gDatasetReadError == OT_ERROR_NONE);
    }
    logLine("ot_udp dataset-applied");
  }

  if (gDatasetApplied && !gIp6EnableAttempted &&
      millis() >= kStageIp6EnableDelayMs) {
    const otLinkModeConfig mode = {true, true, true};
    gIp6EnableAttempted = true;
    gLinkModeSetError = otThreadSetLinkMode(gInstance, mode);
    gIp6SetError = otIp6SetEnabled(gInstance, true);
    logLine("ot_udp ip6-enabled");
  }

  if (gIp6EnableAttempted && !gThreadEnableAttempted &&
      millis() >= kStageThreadEnableDelayMs) {
    gThreadEnableAttempted = true;
    gThreadSetError = otThreadSetEnabled(gInstance, true);
    logLine("ot_udp thread-enabled");
  }

  if (gInstanceInitialized && gIp6EnableAttempted && !gSocketOpened) {
    memset(&gSocket, 0, sizeof(gSocket));
    gSocketOpenError = otUdpOpen(gInstance, &gSocket, handleUdpReceive, nullptr);
    if (gSocketOpenError == OT_ERROR_NONE) {
      otSockAddr sockAddr = {};
      sockAddr.mPort = kUdpPort;
      gSocketBindError =
          otUdpBind(gInstance, &gSocket, &sockAddr, OT_NETIF_THREAD_INTERNAL);
      gSocketOpened = (gSocketBindError == OT_ERROR_NONE);
    }
    logLine("ot_udp socket");
  }

  if (gInstance != nullptr) {
    const otDeviceRole role = otThreadGetDeviceRole(gInstance);
    gRloc16 = otThreadGetRloc16(gInstance);
    if (role != gLastRole) {
      gLastRole = role;
      logLine("ot_udp role-change");
    }
  }

  if (gReplyPending && gLastRole == OT_DEVICE_ROLE_LEADER) {
    if (sendUdpPayload(kPongPayload,
                       gPendingReplyAddr,
                       gPendingReplyPort != 0U ? gPendingReplyPort : kUdpPort,
                       &gLastPongError)) {
      ++gPongTxCount;
      gLeaderPayloadSuccess = true;
      gReplyPending = false;
    }
  }

  if (gSocketOpened && gLastRole == OT_DEVICE_ROLE_CHILD && !gChildPayloadSuccess &&
      (millis() - gLastPingAttemptMs) >= kUdpRetryMs) {
    otIp6Address leaderRloc = {};
    gLastPingAttemptMs = millis();
    gLastPingError = otThreadGetLeaderRloc(gInstance, &leaderRloc);
    if (gLastPingError == OT_ERROR_NONE &&
        sendUdpPayload(kPingPayload, leaderRloc, kUdpPort, &gLastPingError)) {
      ++gPingTxCount;
    }
  }

  if ((millis() - gLastReportMs) >= 1000UL) {
    gLastReportMs = millis();
    printState();
  }
#else
  OpenThreadPlatformSkeleton::process(nullptr);
#endif
}
