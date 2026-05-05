#include "nrf54_thread_experimental.h"

#include <Arduino.h>
#include "openthread-core-user-config.h"
#include <openthread/dataset_ftd.h>
#include <openthread/message.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/settings.h>

#include <string.h>

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0) &&     \
    defined(OPENTHREAD_FTD) && (OPENTHREAD_FTD != 0) && \
    defined(OPENTHREAD_CONFIG_COMMISSIONER_ENABLE) &&   \
    (OPENTHREAD_CONFIG_COMMISSIONER_ENABLE != 0)
#define NRF54_THREAD_COMMISSIONER_COMPILED 1
#else
#define NRF54_THREAD_COMMISSIONER_COMPILED 0
#endif

#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0) &&   \
    defined(OPENTHREAD_CONFIG_JOINER_ENABLE) &&       \
    (OPENTHREAD_CONFIG_JOINER_ENABLE != 0)
#define NRF54_THREAD_JOINER_COMPILED 1
#else
#define NRF54_THREAD_JOINER_COMPILED 0
#endif

namespace xiao_nrf54l15 {
namespace {

constexpr otPanId kDemoPanId = 0x5D6AU;
constexpr uint32_t kDemoChannelMask = OT_CHANNEL_15_MASK;
constexpr uint8_t kDemoChannel = 15U;
constexpr char kDemoNetworkName[] = "Nrf54Stage";
constexpr uint8_t kDemoNetworkKey[OT_NETWORK_KEY_SIZE] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
};
constexpr uint8_t kDemoExtPanId[OT_EXT_PAN_ID_SIZE] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
};
constexpr uint8_t kDemoMeshLocalPrefix[OT_MESH_LOCAL_PREFIX_SIZE] = {
    0xFD, 0x54, 0x15, 0xC0, 0xDE, 0x00, 0x00, 0x00,
};
constexpr uint8_t kDemoPskc[OT_PSKC_MAX_SIZE] = {
    0xA5, 0x4C, 0x8D, 0x11, 0x72, 0x3F, 0x90, 0xBE,
    0x4A, 0x62, 0x18, 0xD4, 0xCE, 0x07, 0x39, 0x5B,
};

}  // namespace

bool Nrf54ThreadExperimental::begin(bool wipeSettings) {
  return begin(wipeSettings, AttachPolicy::kChildFirst);
}

bool Nrf54ThreadExperimental::beginAsChild(bool wipeSettings) {
  return begin(wipeSettings, AttachPolicy::kChildOnly);
}

bool Nrf54ThreadExperimental::beginAsRouter(bool wipeSettings) {
  return begin(wipeSettings, AttachPolicy::kRouterEligible);
}

bool Nrf54ThreadExperimental::beginChildFirst(bool wipeSettings) {
  return begin(wipeSettings, AttachPolicy::kChildFirst);
}

bool Nrf54ThreadExperimental::begin(bool wipeSettings, AttachPolicy policy) {
  if (beginCalled_) {
    return false;
  }

  OpenThreadPlatformSkeleton::begin();
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  otPlatSettingsInit(nullptr, nullptr, 0);
  if (wipeSettings) {
    otPlatSettingsWipe(nullptr);
    settingsWiped_ = true;
  }
#endif
  wipeSettings_ = wipeSettings;
  beginMs_ = millis();
  beginCalled_ = true;
  attachPolicy_ = policy;
  lastError_ = OT_ERROR_NONE;
  lastUdpError_ = OT_ERROR_NONE;
  lastChangedFlags_ = 0U;
  pendingChangedFlags_ = 0U;
  datasetRestoreAttempted_ = false;
  datasetRestoredFromSettings_ = false;
  attachPolicyConfigured_ = false;
  routerEligible_ = false;
  childFirstFallbackUsed_ = false;
  childFirstFallbackDelayMs_ = computeChildFirstFallbackDelayMs();
  stateChangedCallbackRegistered_ = false;
  commissionerStarted_ = false;
  joinerStarted_ = false;
  return true;
}

bool Nrf54ThreadExperimental::stop() {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (!beginCalled_) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  if (instance_ != nullptr && joinerStarted_) {
#if NRF54_THREAD_JOINER_COMPILED
    otJoinerStop(instance_);
#endif
    joinerStarted_ = false;
  }

  if (instance_ != nullptr && commissionerStarted_) {
#if NRF54_THREAD_COMMISSIONER_COMPILED
    lastError_ = otCommissionerStop(instance_);
    if (lastError_ != OT_ERROR_NONE && lastError_ != OT_ERROR_ALREADY) {
      return false;
    }
#endif
    commissionerStarted_ = false;
  }

  if (instance_ != nullptr) {
    for (UdpSocketSlot& slot : udpSockets_) {
      if (!slot.opened) {
        continue;
      }
      lastUdpError_ = otUdpClose(instance_, &slot.socket);
      if (lastUdpError_ != OT_ERROR_NONE) {
        return false;
      }
      memset(&slot.socket, 0, sizeof(slot.socket));
      slot.opened = false;
    }
  }

  if (instance_ != nullptr && threadEnabled_) {
    lastError_ = otThreadSetEnabled(instance_, false);
    if (lastError_ != OT_ERROR_NONE) {
      return false;
    }
  }

  if (instance_ != nullptr && ip6Enabled_) {
    lastError_ = otIp6SetEnabled(instance_, false);
    if (lastError_ != OT_ERROR_NONE) {
      return false;
    }
  }

  linkConfigured_ = false;
  ip6Enabled_ = false;
  threadEnabled_ = false;
  datasetApplied_ = false;
  attachPolicyConfigured_ = false;
  routerEligible_ = false;
  childFirstFallbackUsed_ = false;
  pendingChangedFlags_ = 0U;
  commissionerStarted_ = false;
  joinerStarted_ = false;
  lastError_ = OT_ERROR_NONE;
  return true;
#endif
}

bool Nrf54ThreadExperimental::restart(bool wipeSettings) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (!beginCalled_) {
    return begin(wipeSettings);
  }

  if (!stop()) {
    return false;
  }

  if (wipeSettings && !wipePersistentSettings()) {
    return false;
  }

  wipeSettings_ = wipeSettings;
  settingsWiped_ = wipeSettings;
  beginMs_ = millis() - kStageInitDelayMs;
  lastError_ = OT_ERROR_NONE;
  lastUdpError_ = OT_ERROR_NONE;
  lastChangedFlags_ = 0U;
  pendingChangedFlags_ = 0U;
  datasetRestoreAttempted_ = false;
  attachPolicyConfigured_ = false;
  routerEligible_ = false;
  childFirstFallbackUsed_ = false;
  childFirstFallbackDelayMs_ = computeChildFirstFallbackDelayMs();
  commissionerStarted_ = false;
  joinerStarted_ = false;
  if (wipeSettings_) {
    datasetRestoredFromSettings_ = false;
  }
  return true;
#endif
}

void Nrf54ThreadExperimental::process() {
  OpenThreadPlatformSkeleton::process(instance_);

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  return;
#else
  if (!beginCalled_) {
    return;
  }

  const uint32_t elapsedMs = millis() - beginMs_;

  if (instance_ == nullptr && elapsedMs >= kStageInitDelayMs) {
    instance_ = otInstanceInitSingle();
    if (instance_ == nullptr || !otInstanceIsInitialized(instance_)) {
      lastError_ = OT_ERROR_FAILED;
      return;
    }
  }

  if (instance_ != nullptr && !stateChangedCallbackRegistered_) {
    lastError_ =
        otSetStateChangedCallback(instance_, handleStateChangedStatic, this);
    if (lastError_ != OT_ERROR_NONE) {
      return;
    }
    stateChangedCallbackRegistered_ = true;
  }

  if (instance_ != nullptr && !attachPolicyConfigured_) {
    if (!configureAttachPolicy()) {
      return;
    }
  }

  if (instance_ != nullptr && !wipeSettings_ && !datasetConfigured_ &&
      !datasetApplied_ && !datasetRestoreAttempted_) {
    datasetRestoreAttempted_ = true;
    (void)restoreDatasetFromSettings();
  }

  if (instance_ != nullptr && datasetConfigured_ && !datasetApplied_ &&
      elapsedMs >= kStageDatasetApplyDelayMs) {
    if (attachPolicy_ != AttachPolicy::kRouterEligible) {
      dataset_.mActiveTimestamp.mAuthoritative = false;
      dataset_.mActiveTimestamp.mSeconds = 0;
    }
    lastError_ = otDatasetSetActive(instance_, &dataset_);
    if (lastError_ == OT_ERROR_NONE) {
      datasetApplied_ = true;
    }
  }

  if (instance_ != nullptr && datasetApplied_ && !linkConfigured_ &&
      elapsedMs >= kStageIp6EnableDelayMs) {
    // Child-first uses non-sleepy MTD mode first. If no parent appears, the
    // deterministic fallback below flips back to FTD/router-eligible mode.
    const bool fullThreadDevice =
        (attachPolicy_ == AttachPolicy::kRouterEligible);
    const otLinkModeConfig mode = {true, fullThreadDevice, true};
    lastError_ = otThreadSetLinkMode(instance_, mode);
    if (lastError_ == OT_ERROR_NONE) {
      linkConfigured_ = true;
      lastError_ = otIp6SetEnabled(instance_, true);
      ip6Enabled_ = (lastError_ == OT_ERROR_NONE);
    }
  }

  if (instance_ != nullptr && ip6Enabled_ && !threadEnabled_ &&
      elapsedMs >= kStageThreadEnableDelayMs) {
    lastError_ = otThreadSetEnabled(instance_, true);
    threadEnabled_ = (lastError_ == OT_ERROR_NONE);
  }

  if (instance_ != nullptr && threadEnabled_) {
    (void)maybePromoteChildFirstFallback(elapsedMs);
  }

  if (instance_ != nullptr && ip6Enabled_) {
    for (UdpSocketSlot& slot : udpSockets_) {
      if (slot.requested && !slot.opened) {
        (void)openUdpSlot(&slot);
      }
    }
  }
#endif
}

bool Nrf54ThreadExperimental::setActiveDataset(
    const otOperationalDataset& dataset) {
  dataset_ = dataset;
  datasetConfigured_ = true;
  datasetApplied_ = false;
  datasetRestoredFromSettings_ = false;
  lastError_ = OT_ERROR_NONE;
  return true;
}

bool Nrf54ThreadExperimental::setActiveDatasetTlvs(
    const otOperationalDatasetTlvs& datasetTlvs) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)datasetTlvs;
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (!otDatasetIsValid(&datasetTlvs, true)) {
    lastError_ = OT_ERROR_PARSE;
    return false;
  }

  otOperationalDataset dataset = {};
  lastError_ = otDatasetParseTlvs(&datasetTlvs, &dataset);
  if (lastError_ != OT_ERROR_NONE) {
    memset(&dataset, 0, sizeof(dataset));
    return false;
  }
  return setActiveDataset(dataset);
#endif
}

bool Nrf54ThreadExperimental::setActiveDatasetHex(const char* datasetHex) {
  otOperationalDatasetTlvs datasetTlvs = {};
  size_t tlvLength = 0U;
  if (!hexToBytes(datasetHex, datasetTlvs.mTlvs, sizeof(datasetTlvs.mTlvs),
                  &tlvLength)) {
    lastError_ = OT_ERROR_PARSE;
    return false;
  }

  datasetTlvs.mLength = static_cast<uint8_t>(tlvLength);
  return setActiveDatasetTlvs(datasetTlvs);
}

bool Nrf54ThreadExperimental::getActiveDataset(
    otOperationalDataset* outDataset) const {
  if (outDataset == nullptr || instance_ == nullptr) {
    return false;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return otDatasetGetActive(instance_, outDataset) == OT_ERROR_NONE;
#else
  memset(outDataset, 0, sizeof(*outDataset));
  return false;
#endif
}

bool Nrf54ThreadExperimental::getActiveDatasetTlvs(
    otOperationalDatasetTlvs* outDatasetTlvs) const {
  if (outDatasetTlvs == nullptr || instance_ == nullptr) {
    return false;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return otDatasetGetActiveTlvs(instance_, outDatasetTlvs) == OT_ERROR_NONE;
#else
  memset(outDatasetTlvs, 0, sizeof(*outDatasetTlvs));
  return false;
#endif
}

bool Nrf54ThreadExperimental::getConfiguredOrActiveDataset(
    otOperationalDataset* outDataset) const {
  if (outDataset == nullptr) {
    return false;
  }

  if (datasetConfigured_) {
    *outDataset = dataset_;
    return true;
  }

  if (getActiveDataset(outDataset)) {
    return true;
  }

  memset(outDataset, 0, sizeof(*outDataset));
  return false;
}

bool Nrf54ThreadExperimental::getConfiguredOrActiveDatasetTlvs(
    otOperationalDatasetTlvs* outDatasetTlvs) const {
  if (outDatasetTlvs == nullptr) {
    return false;
  }

  if (datasetConfigured_) {
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
    memset(outDatasetTlvs, 0, sizeof(*outDatasetTlvs));
    otDatasetConvertToTlvs(&dataset_, outDatasetTlvs);
    return otDatasetIsValid(outDatasetTlvs, true);
#else
    memset(outDatasetTlvs, 0, sizeof(*outDatasetTlvs));
    return false;
#endif
  }

  if (getActiveDatasetTlvs(outDatasetTlvs)) {
    return true;
  }

  memset(outDatasetTlvs, 0, sizeof(*outDatasetTlvs));
  return false;
}

bool Nrf54ThreadExperimental::exportConfiguredOrActiveDatasetHex(
    char* outBuffer, size_t outBufferSize, size_t* outHexLength) const {
  otOperationalDatasetTlvs datasetTlvs = {};
  if (!getConfiguredOrActiveDatasetTlvs(&datasetTlvs)) {
    if (outHexLength != nullptr) {
      *outHexLength = 0U;
    }
    return false;
  }

  return bytesToUpperHex(datasetTlvs.mTlvs, datasetTlvs.mLength, outBuffer,
                         outBufferSize, outHexLength);
}

bool Nrf54ThreadExperimental::wipePersistentSettings() {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  OpenThreadPlatformSkeleton::wipeSettings();
  settingsWiped_ = true;
  dataset_ = {};
  datasetConfigured_ = false;
  datasetApplied_ = false;
  datasetRestoreAttempted_ = false;
  datasetRestoredFromSettings_ = false;
  lastError_ = OT_ERROR_NONE;
  return true;
#endif
}

bool Nrf54ThreadExperimental::requestRouterRole() {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr || !threadEnabled_) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  const Role currentRole = role();
  if (currentRole == Role::kRouter || currentRole == Role::kLeader) {
    lastError_ = OT_ERROR_NONE;
    return true;
  }

  const otLinkModeConfig mode = {true, true, true};
  lastError_ = otThreadSetLinkMode(instance_, mode);
  if (lastError_ != OT_ERROR_NONE) {
    return false;
  }

  lastError_ = otThreadSetRouterEligible(instance_, true);
  if (lastError_ != OT_ERROR_NONE) {
    return false;
  }
  routerEligible_ = true;

  lastError_ = otThreadBecomeRouter(instance_);
  return lastError_ == OT_ERROR_NONE;
#endif
}

bool Nrf54ThreadExperimental::startCommissioner() {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ == nullptr || !attached()) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  lastError_ = otCommissionerStart(instance_, handleCommissionerStateStatic,
                                   handleCommissionerJoinerStatic, this);
  if (lastError_ == OT_ERROR_NONE || lastError_ == OT_ERROR_ALREADY) {
    commissionerStarted_ = true;
    lastError_ = OT_ERROR_NONE;
    return true;
  }
  return false;
#else
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::stopCommissioner() {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ == nullptr) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  lastError_ = otCommissionerStop(instance_);
  if (lastError_ == OT_ERROR_NONE || lastError_ == OT_ERROR_ALREADY) {
    commissionerStarted_ = false;
    lastError_ = OT_ERROR_NONE;
    return true;
  }
  return false;
#else
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::addJoinerToCommissioner(
    const char* pskd, uint32_t timeoutSeconds) {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ == nullptr || pskd == nullptr) {
    lastError_ = OT_ERROR_INVALID_ARGS;
    return false;
  }

  lastError_ =
      otCommissionerAddJoiner(instance_, nullptr, pskd, timeoutSeconds);
  return lastError_ == OT_ERROR_NONE;
#else
  (void)pskd;
  (void)timeoutSeconds;
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::setCommissionerProvisioningUrl(
    const char* provisioningUrl) {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ == nullptr || provisioningUrl == nullptr) {
    lastError_ = OT_ERROR_INVALID_ARGS;
    return false;
  }

  lastError_ = otCommissionerSetProvisioningUrl(instance_, provisioningUrl);
  return lastError_ == OT_ERROR_NONE;
#else
  (void)provisioningUrl;
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::setCommissionerStateCallback(
    CommissionerStateCallback callback, void* callbackContext) {
  commissionerStateCallback_ = callback;
  commissionerStateCallbackContext_ = callbackContext;
  return true;
}

bool Nrf54ThreadExperimental::setCommissionerJoinerCallback(
    CommissionerJoinerCallback callback, void* callbackContext) {
  commissionerJoinerCallback_ = callback;
  commissionerJoinerCallbackContext_ = callbackContext;
  return true;
}

bool Nrf54ThreadExperimental::startJoiner(const char* pskd,
                                          const char* provisioningUrl,
                                          JoinerCallback callback,
                                          void* callbackContext) {
  joinerCallback_ = callback;
  joinerCallbackContext_ = callbackContext;

#if NRF54_THREAD_JOINER_COMPILED
  if (instance_ == nullptr || pskd == nullptr) {
    lastError_ = OT_ERROR_INVALID_ARGS;
    return false;
  }

  lastError_ = otJoinerStart(instance_, pskd, provisioningUrl, nullptr, nullptr,
                             nullptr, nullptr, handleJoinerStatic, this);
  if (lastError_ == OT_ERROR_NONE) {
    joinerStarted_ = true;
    return true;
  }
  return false;
#else
  (void)pskd;
  (void)provisioningUrl;
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::stopJoiner() {
#if NRF54_THREAD_JOINER_COMPILED
  if (instance_ == nullptr) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  otJoinerStop(instance_);
  joinerStarted_ = false;
  lastError_ = OT_ERROR_NONE;
  return true;
#else
  lastError_ = OT_ERROR_NOT_IMPLEMENTED;
  return false;
#endif
}

bool Nrf54ThreadExperimental::openUdp(uint16_t port,
                                      UdpReceiveCallback callback,
                                      void* callbackContext) {
  if (port == 0U) {
    lastUdpError_ = OT_ERROR_INVALID_ARGS;
    return false;
  }

  UdpSocketSlot* slot = findUdpSlot(port);
  if (slot == nullptr) {
    slot = firstUdpSlot(false);
  }
  if (slot == nullptr) {
    lastUdpError_ = OT_ERROR_NO_BUFS;
    return false;
  }

  if (slot->opened && slot->port != port) {
    lastUdpError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  slot->port = port;
  slot->callback = callback;
  slot->callbackContext = callbackContext;
  slot->requested = true;

  if (slot->opened) {
    lastUdpError_ = OT_ERROR_NONE;
    return true;
  }

  if (instance_ != nullptr && ip6Enabled_) {
    return openUdpSlot(slot);
  }

  lastUdpError_ = OT_ERROR_NONE;
  return true;
}

bool Nrf54ThreadExperimental::setStateChangedCallback(
    StateChangedCallback callback, void* callbackContext) {
  stateChangedCallback_ = callback;
  stateChangedCallbackContext_ = callbackContext;
  return true;
}

bool Nrf54ThreadExperimental::sendUdp(const otIp6Address& peerAddr,
                                      uint16_t peerPort,
                                      const void* payload,
                                      uint16_t payloadLength) {
  return sendUdpFrom(0U, peerAddr, peerPort, payload, payloadLength);
}

bool Nrf54ThreadExperimental::sendUdpFrom(uint16_t localPort,
                                          const otIp6Address& peerAddr,
                                          uint16_t peerPort,
                                          const void* payload,
                                          uint16_t payloadLength) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)localPort;
  (void)peerAddr;
  (void)peerPort;
  (void)payload;
  (void)payloadLength;
  lastUdpError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr || payload == nullptr ||
      payloadLength == 0U) {
    lastUdpError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  UdpSocketSlot* slot = nullptr;
  if (localPort != 0U) {
    slot = findUdpSlot(localPort);
  } else {
    slot = findUdpSlot(peerPort);
  }
  if (slot == nullptr || !slot->opened) {
    slot = firstUdpSlot(true);
  }
  if (slot == nullptr || !slot->opened) {
    lastUdpError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  otMessage* message = otUdpNewMessage(instance_, nullptr);
  if (message == nullptr) {
    lastUdpError_ = OT_ERROR_NO_BUFS;
    return false;
  }

  lastUdpError_ =
      otMessageAppend(message, static_cast<const uint8_t*>(payload), payloadLength);
  if (lastUdpError_ != OT_ERROR_NONE) {
    otMessageFree(message);
    return false;
  }

  otMessageInfo messageInfo = {};
  messageInfo.mPeerAddr = peerAddr;
  messageInfo.mPeerPort = peerPort;
  messageInfo.mSockPort = slot->port;
  messageInfo.mHopLimit = 64U;

  lastUdpError_ = otUdpSend(instance_, &slot->socket, message, &messageInfo);
  if (lastUdpError_ != OT_ERROR_NONE) {
    otMessageFree(message);
    return false;
  }
  return true;
#endif
}

bool Nrf54ThreadExperimental::subscribeMulticast(
    const otIp6Address& multicastAddr) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)multicastAddr;
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr || !ip6Enabled_) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }
  lastError_ = otIp6SubscribeMulticastAddress(instance_, &multicastAddr);
  return lastError_ == OT_ERROR_NONE || lastError_ == OT_ERROR_ALREADY;
#endif
}

bool Nrf54ThreadExperimental::unsubscribeMulticast(
    const otIp6Address& multicastAddr) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)multicastAddr;
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr || !ip6Enabled_) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }
  lastError_ = otIp6UnsubscribeMulticastAddress(instance_, &multicastAddr);
  return lastError_ == OT_ERROR_NONE || lastError_ == OT_ERROR_NOT_FOUND;
#endif
}

bool Nrf54ThreadExperimental::getLeaderRloc(otIp6Address* outLeaderAddr) const {
  if (outLeaderAddr == nullptr || instance_ == nullptr) {
    return false;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return otThreadGetLeaderRloc(instance_, outLeaderAddr) == OT_ERROR_NONE;
#else
  memset(outLeaderAddr, 0, sizeof(*outLeaderAddr));
  return false;
#endif
}

bool Nrf54ThreadExperimental::getAttachDiagnostics(
    AttachDiagnostics* outDiagnostics) const {
  if (outDiagnostics == nullptr) {
    return false;
  }

  *outDiagnostics = {};
  outDiagnostics->attachPolicy = static_cast<uint8_t>(attachPolicy_);
  outDiagnostics->routerEligible = routerEligible_;
  outDiagnostics->childFirstFallbackDelayMs = childFirstFallbackDelayMs_;
  outDiagnostics->childFirstFallbackArmed = childFirstFallbackArmed();
  outDiagnostics->childFirstFallbackUsed = childFirstFallbackUsed_;
  if (outDiagnostics->childFirstFallbackArmed) {
    const uint32_t elapsedMs = millis() - beginMs_;
    outDiagnostics->childFirstFallbackRemainingMs =
        (elapsedMs >= childFirstFallbackDelayMs_)
            ? 0U
            : (childFirstFallbackDelayMs_ - elapsedMs);
  }

  if (instance_ == nullptr) {
    return false;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  outDiagnostics->currentAttachDurationMs =
      otThreadGetCurrentAttachDuration(instance_);
  outDiagnostics->partitionId = otThreadGetPartitionId(instance_);
  const otMleCounters* counters = otThreadGetMleCounters(instance_);
  if (counters != nullptr) {
    outDiagnostics->attachAttempts = counters->mAttachAttempts;
    outDiagnostics->betterPartitionAttachAttempts =
        counters->mBetterPartitionAttachAttempts;
    outDiagnostics->betterParentAttachAttempts =
        counters->mBetterParentAttachAttempts;
    outDiagnostics->parentChanges = counters->mParentChanges;
  }
  return true;
#else
  return false;
#endif
}

bool Nrf54ThreadExperimental::getAttachDebugState(
    AttachDebugState* outState) const {
  if (outState == nullptr) {
    return false;
  }

  *outState = {};
  OpenThreadPlatformSkeletonSnapshot snapshot = {};
  if (!OpenThreadPlatformSkeleton::snapshot(&snapshot)) {
    return false;
  }

  outState->valid = snapshot.threadCoreDebugValid;
  outState->attachInProgress = snapshot.threadAttachInProgress;
  outState->attachTimerRunning = snapshot.threadAttachTimerRunning;
  outState->receivedResponseFromParent =
      snapshot.threadReceivedResponseFromParent;
  outState->attachState = snapshot.threadAttachState;
  outState->attachMode = snapshot.threadAttachMode;
  outState->reattachMode = snapshot.threadReattachMode;
  outState->parentRequestCounter = snapshot.threadParentRequestCounter;
  outState->childIdRequestsRemaining = snapshot.threadChildIdRequestsRemaining;
  outState->parentCandidateState = snapshot.threadParentCandidateState;
  outState->attachCounter = snapshot.threadAttachCounter;
  outState->parentCandidateRloc16 = snapshot.threadParentCandidateRloc16;
  outState->attachTimerRemainingMs = snapshot.threadAttachTimerRemainingMs;
  strncpy(outState->attachStateName, snapshot.threadAttachStateName,
          sizeof(outState->attachStateName) - 1U);
  strncpy(outState->attachModeName, snapshot.threadAttachModeName,
          sizeof(outState->attachModeName) - 1U);
  strncpy(outState->reattachModeName, snapshot.threadReattachModeName,
          sizeof(outState->reattachModeName) - 1U);
  strncpy(outState->parentCandidateStateName,
          snapshot.threadParentCandidateStateName,
          sizeof(outState->parentCandidateStateName) - 1U);
  return true;
}

bool Nrf54ThreadExperimental::getAttachSummary(AttachSummary* outSummary) const {
  if (outSummary == nullptr) {
    return false;
  }

  *outSummary = {};
  outSummary->valid = beginCalled_;
  outSummary->attached = attached();
  outSummary->configuredForAttach = datasetConfigured_ || datasetApplied_;
  outSummary->role = role();

  auto setText = [](char* destination, size_t length, const char* source) {
    if (destination == nullptr || length == 0U) {
      return;
    }
    destination[0] = '\0';
    if (source == nullptr) {
      return;
    }
    strncpy(destination, source, length - 1U);
    destination[length - 1U] = '\0';
  };

  if (!beginCalled_) {
    setText(outSummary->phaseName, sizeof(outSummary->phaseName), "not_started");
    setText(outSummary->blockerName, sizeof(outSummary->blockerName),
            "begin_not_called");
    return true;
  }

  const uint32_t elapsedMs = millis() - beginMs_;
  AttachDebugState debugState = {};
  const bool haveDebugState = getAttachDebugState(&debugState);

  if (instance_ == nullptr) {
    setText(outSummary->phaseName, sizeof(outSummary->phaseName), "bootstrap");
    setText(outSummary->blockerName, sizeof(outSummary->blockerName),
            "waiting_instance_init");
    return true;
  }

  if (!datasetConfigured_ && !datasetApplied_) {
    outSummary->waitingForDataset = true;
    setText(outSummary->phaseName, sizeof(outSummary->phaseName),
            "dataset_pending");
    if (!wipeSettings_ && !datasetRestoreAttempted_) {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_restore_probe");
    } else {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_dataset");
    }
    return true;
  }

  if (!datasetApplied_) {
    outSummary->waitingForDatasetApply = true;
    setText(outSummary->phaseName, sizeof(outSummary->phaseName),
            "dataset_apply");
    if (elapsedMs < kStageDatasetApplyDelayMs) {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_dataset_apply_delay");
    } else {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_dataset_apply");
    }
    return true;
  }

  if (!linkConfigured_ || !ip6Enabled_) {
    outSummary->waitingForIp6Enable = true;
    setText(outSummary->phaseName, sizeof(outSummary->phaseName), "ip6_enable");
    if (elapsedMs < kStageIp6EnableDelayMs) {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_ip6_enable_delay");
    } else {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_ip6_enable");
    }
    return true;
  }

  if (!threadEnabled_) {
    outSummary->waitingForThreadEnable = true;
    setText(outSummary->phaseName, sizeof(outSummary->phaseName),
            "thread_enable");
    if (elapsedMs < kStageThreadEnableDelayMs) {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_thread_enable_delay");
    } else {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_thread_enable");
    }
    return true;
  }

  if (childFirstFallbackArmed()) {
    outSummary->waitingForLeaderFallback = true;
    setText(outSummary->phaseName, sizeof(outSummary->phaseName),
            "child_first_attach");
    setText(outSummary->blockerName, sizeof(outSummary->blockerName),
            "waiting_parent_or_leader_fallback");
    return true;
  }

  if (outSummary->attached) {
    setText(outSummary->phaseName, sizeof(outSummary->phaseName), "attached");
    setText(outSummary->blockerName, sizeof(outSummary->blockerName), "none");
    return true;
  }

  setText(outSummary->phaseName, sizeof(outSummary->phaseName), "attach");
  if (haveDebugState && debugState.valid) {
    if (debugState.attachTimerRunning) {
      outSummary->waitingForReattachTimer = true;
    }
    if (strcmp(debugState.attachStateName, "ParentReq") == 0 &&
        !debugState.receivedResponseFromParent) {
      outSummary->waitingForParentResponse = true;
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_parent_response");
      return true;
    }
    if (strcmp(debugState.attachStateName, "ChildIdReq") == 0) {
      outSummary->waitingForChildIdResponse = true;
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_child_id_response");
      return true;
    }
    if (debugState.attachTimerRunning) {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              "waiting_reattach_timer");
      return true;
    }
    if (debugState.attachStateName[0] != '\0') {
      setText(outSummary->blockerName, sizeof(outSummary->blockerName),
              debugState.attachStateName);
      return true;
    }
  }

  setText(outSummary->blockerName, sizeof(outSummary->blockerName),
          "detached_idle");
  return true;
}

bool Nrf54ThreadExperimental::started() const { return beginCalled_; }

bool Nrf54ThreadExperimental::attached() const {
  const Role currentRole = role();
  return currentRole == Role::kChild || currentRole == Role::kRouter ||
         currentRole == Role::kLeader;
}

bool Nrf54ThreadExperimental::udpOpened() const {
  return firstUdpSlot(true) != nullptr;
}

bool Nrf54ThreadExperimental::udpOpened(uint16_t port) const {
  const UdpSocketSlot* slot = findUdpSlot(port);
  return slot != nullptr && slot->opened;
}

Nrf54ThreadExperimental::UdpSocketSlot*
Nrf54ThreadExperimental::findUdpSlot(uint16_t port) {
  if (port == 0U) {
    return nullptr;
  }
  for (UdpSocketSlot& slot : udpSockets_) {
    if (slot.requested && slot.port == port) {
      return &slot;
    }
  }
  return nullptr;
}

const Nrf54ThreadExperimental::UdpSocketSlot*
Nrf54ThreadExperimental::findUdpSlot(uint16_t port) const {
  if (port == 0U) {
    return nullptr;
  }
  for (const UdpSocketSlot& slot : udpSockets_) {
    if (slot.requested && slot.port == port) {
      return &slot;
    }
  }
  return nullptr;
}

Nrf54ThreadExperimental::UdpSocketSlot*
Nrf54ThreadExperimental::firstUdpSlot(bool openedOnly) {
  for (UdpSocketSlot& slot : udpSockets_) {
    if (openedOnly) {
      if (slot.opened) {
        return &slot;
      }
    } else if (!slot.requested) {
      return &slot;
    }
  }
  return nullptr;
}

const Nrf54ThreadExperimental::UdpSocketSlot*
Nrf54ThreadExperimental::firstUdpSlot(bool openedOnly) const {
  for (const UdpSocketSlot& slot : udpSockets_) {
    if (openedOnly) {
      if (slot.opened) {
        return &slot;
      }
    } else if (!slot.requested) {
      return &slot;
    }
  }
  return nullptr;
}

bool Nrf54ThreadExperimental::openUdpSlot(UdpSocketSlot* slot) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)slot;
  lastUdpError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (slot == nullptr || !slot->requested || slot->port == 0U) {
    lastUdpError_ = OT_ERROR_INVALID_ARGS;
    return false;
  }
  if (slot->opened) {
    lastUdpError_ = OT_ERROR_NONE;
    return true;
  }
  if (instance_ == nullptr || !ip6Enabled_) {
    lastUdpError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  memset(&slot->socket, 0, sizeof(slot->socket));
  lastUdpError_ = otUdpOpen(instance_, &slot->socket,
                            handleUdpReceiveStatic, this);
  if (lastUdpError_ != OT_ERROR_NONE) {
    return false;
  }

  otSockAddr sockAddr = {};
  sockAddr.mPort = slot->port;
  lastUdpError_ =
      otUdpBind(instance_, &slot->socket, &sockAddr, OT_NETIF_THREAD_INTERNAL);
  if (lastUdpError_ != OT_ERROR_NONE) {
    (void)otUdpClose(instance_, &slot->socket);
    memset(&slot->socket, 0, sizeof(slot->socket));
    slot->opened = false;
    return false;
  }

  slot->opened = true;
  return true;
#endif
}

Nrf54ThreadExperimental::Role Nrf54ThreadExperimental::role() const {
  if (instance_ == nullptr) {
    return Role::kDisabled;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return convertRole(otThreadGetDeviceRole(instance_));
#else
  return Role::kDisabled;
#endif
}

const char* Nrf54ThreadExperimental::roleName() const {
  return roleName(role());
}

uint16_t Nrf54ThreadExperimental::rloc16() const {
  if (instance_ == nullptr) {
    return OT_RADIO_INVALID_SHORT_ADDR;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return otThreadGetRloc16(instance_);
#else
  return OT_RADIO_INVALID_SHORT_ADDR;
#endif
}

uint32_t Nrf54ThreadExperimental::partitionId() const {
  if (instance_ == nullptr) {
    return 0U;
  }
#if defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) && \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE != 0)
  return otThreadGetPartitionId(instance_);
#else
  return 0U;
#endif
}

bool Nrf54ThreadExperimental::routerEligible() const {
  return routerEligible_;
}

bool Nrf54ThreadExperimental::childFirstFallbackArmed() const {
  if (attachPolicy_ != AttachPolicy::kChildFirst || childFirstFallbackUsed_ ||
      !threadEnabled_ || attached()) {
    return false;
  }
  return (millis() - beginMs_) < childFirstFallbackDelayMs_;
}

bool Nrf54ThreadExperimental::childFirstFallbackUsed() const {
  return childFirstFallbackUsed_;
}

uint32_t Nrf54ThreadExperimental::childFirstFallbackDelayMs() const {
  return childFirstFallbackDelayMs_;
}

bool Nrf54ThreadExperimental::commissionerSupported() const {
  return NRF54_THREAD_COMMISSIONER_COMPILED != 0;
}

bool Nrf54ThreadExperimental::commissionerActive() const {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  return instance_ != nullptr &&
         otCommissionerGetState(instance_) == OT_COMMISSIONER_STATE_ACTIVE;
#else
  return false;
#endif
}

otCommissionerState Nrf54ThreadExperimental::commissionerState() const {
#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ != nullptr) {
    return otCommissionerGetState(instance_);
  }
#endif
  return OT_COMMISSIONER_STATE_DISABLED;
}

const char* Nrf54ThreadExperimental::commissionerStateName() const {
  return commissionerStateName(commissionerState());
}

bool Nrf54ThreadExperimental::commissionerSessionId(
    uint16_t* outSessionId) const {
  if (outSessionId == nullptr) {
    return false;
  }

#if NRF54_THREAD_COMMISSIONER_COMPILED
  if (instance_ != nullptr && commissionerActive()) {
    *outSessionId = otCommissionerGetSessionId(instance_);
    return true;
  }
#endif

  *outSessionId = 0U;
  return false;
}

bool Nrf54ThreadExperimental::joinerSupported() const {
  return NRF54_THREAD_JOINER_COMPILED != 0;
}

bool Nrf54ThreadExperimental::joinerActive() const {
#if NRF54_THREAD_JOINER_COMPILED
  if (instance_ == nullptr) {
    return false;
  }

  const otJoinerState state = otJoinerGetState(instance_);
  return state != OT_JOINER_STATE_IDLE && state != OT_JOINER_STATE_JOINED;
#else
  return false;
#endif
}

otJoinerState Nrf54ThreadExperimental::joinerState() const {
#if NRF54_THREAD_JOINER_COMPILED
  if (instance_ != nullptr) {
    return otJoinerGetState(instance_);
  }
#endif
  return OT_JOINER_STATE_IDLE;
}

const char* Nrf54ThreadExperimental::joinerStateName() const {
  return joinerStateName(joinerState());
}

bool Nrf54ThreadExperimental::datasetConfigured() const {
  return datasetConfigured_;
}

bool Nrf54ThreadExperimental::restoredFromSettings() const {
  return datasetRestoredFromSettings_;
}

otError Nrf54ThreadExperimental::lastError() const { return lastError_; }

otError Nrf54ThreadExperimental::lastUdpError() const { return lastUdpError_; }

otChangedFlags Nrf54ThreadExperimental::lastChangedFlags() const {
  return lastChangedFlags_;
}

otChangedFlags Nrf54ThreadExperimental::pendingChangedFlags() const {
  return pendingChangedFlags_;
}

otChangedFlags Nrf54ThreadExperimental::consumePendingChangedFlags() {
  const otChangedFlags flags = pendingChangedFlags_;
  pendingChangedFlags_ = 0U;
  return flags;
}

otInstance* Nrf54ThreadExperimental::rawInstance() const { return instance_; }

bool Nrf54ThreadExperimental::restoreDatasetFromSettings() {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  otOperationalDataset restored = {};
  const otError error = otDatasetGetActive(instance_, &restored);
  if (error != OT_ERROR_NONE) {
    if (error != OT_ERROR_NOT_FOUND) {
      lastError_ = error;
    }
    return false;
  }

  dataset_ = restored;
  datasetConfigured_ = true;
  datasetApplied_ = true;
  datasetRestoredFromSettings_ = true;
  lastError_ = OT_ERROR_NONE;
  return true;
#endif
}

bool Nrf54ThreadExperimental::configureAttachPolicy() {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (instance_ == nullptr) {
    lastError_ = OT_ERROR_INVALID_STATE;
    return false;
  }

  const bool allowRouter = (attachPolicy_ == AttachPolicy::kRouterEligible);
  lastError_ = otThreadSetRouterEligible(instance_, allowRouter);
  if (lastError_ != OT_ERROR_NONE) {
    return false;
  }

  if (allowRouter) {
    otThreadSetRouterSelectionJitter(instance_, 1U);
  }

  routerEligible_ = allowRouter;
  attachPolicyConfigured_ = true;
  lastError_ = OT_ERROR_NONE;
  return true;
#endif
}

bool Nrf54ThreadExperimental::maybePromoteChildFirstFallback(uint32_t elapsedMs) {
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  (void)elapsedMs;
  lastError_ = OT_ERROR_INVALID_STATE;
  return false;
#else
  if (attachPolicy_ != AttachPolicy::kChildFirst || instance_ == nullptr ||
      childFirstFallbackUsed_ || routerEligible_ || attached() ||
      elapsedMs < childFirstFallbackDelayMs_) {
    return false;
  }

  const otLinkModeConfig mode = {true, true, true};
  lastError_ = otThreadSetLinkMode(instance_, mode);
  if (lastError_ != OT_ERROR_NONE) {
    return false;
  }

  lastError_ = otThreadSetRouterEligible(instance_, true);
  if (lastError_ != OT_ERROR_NONE) {
    return false;
  }

  routerEligible_ = true;
  childFirstFallbackUsed_ = true;
  otThreadSetRouterSelectionJitter(instance_, 1U);

  otError leaderError = otThreadBecomeLeader(instance_);
  if (leaderError == OT_ERROR_NONE || leaderError == OT_ERROR_ALREADY ||
      leaderError == OT_ERROR_INVALID_STATE || leaderError == OT_ERROR_BUSY) {
    lastError_ = OT_ERROR_NONE;
    return true;
  }

  lastError_ = leaderError;
  return false;
#endif
}

const char* Nrf54ThreadExperimental::roleName(Role role) {
  switch (role) {
    case Role::kDisabled:
      return "disabled";
    case Role::kDetached:
      return "detached";
    case Role::kChild:
      return "child";
    case Role::kRouter:
      return "router";
    case Role::kLeader:
      return "leader";
    default:
      return "unknown";
  }
}

const char* Nrf54ThreadExperimental::attachPolicyName(AttachPolicy policy) {
  switch (policy) {
    case AttachPolicy::kChildFirst:
      return "child-first";
    case AttachPolicy::kChildOnly:
      return "child-only";
    case AttachPolicy::kRouterEligible:
      return "router-eligible";
    default:
      return "unknown";
  }
}

const char* Nrf54ThreadExperimental::commissionerStateName(
    otCommissionerState state) {
  switch (state) {
    case OT_COMMISSIONER_STATE_DISABLED:
      return "disabled";
    case OT_COMMISSIONER_STATE_PETITION:
      return "petition";
    case OT_COMMISSIONER_STATE_ACTIVE:
      return "active";
    default:
      return "unknown";
  }
}

const char* Nrf54ThreadExperimental::joinerStateName(otJoinerState state) {
  switch (state) {
    case OT_JOINER_STATE_IDLE:
      return "idle";
    case OT_JOINER_STATE_DISCOVER:
      return "discover";
    case OT_JOINER_STATE_CONNECT:
      return "connect";
    case OT_JOINER_STATE_CONNECTED:
      return "connected";
    case OT_JOINER_STATE_ENTRUST:
      return "entrust";
    case OT_JOINER_STATE_JOINED:
      return "joined";
    default:
      return "unknown";
  }
}

void Nrf54ThreadExperimental::buildDemoDataset(
    otOperationalDataset* outDataset) {
  if (outDataset == nullptr) {
    return;
  }

  memset(outDataset, 0, sizeof(*outDataset));
  outDataset->mActiveTimestamp.mSeconds = 1ULL;
  outDataset->mActiveTimestamp.mAuthoritative = true;
  memcpy(outDataset->mNetworkKey.m8, kDemoNetworkKey, sizeof(kDemoNetworkKey));
  strncpy(outDataset->mNetworkName.m8,
          kDemoNetworkName,
          sizeof(outDataset->mNetworkName.m8) - 1U);
  memcpy(outDataset->mExtendedPanId.m8, kDemoExtPanId, sizeof(kDemoExtPanId));
  memcpy(outDataset->mMeshLocalPrefix.m8,
         kDemoMeshLocalPrefix,
         sizeof(kDemoMeshLocalPrefix));
  memcpy(outDataset->mPskc.m8, kDemoPskc, sizeof(kDemoPskc));
  outDataset->mPanId = kDemoPanId;
  outDataset->mChannel = kDemoChannel;
  outDataset->mWakeupChannel = kDemoChannel;
  outDataset->mChannelMask = kDemoChannelMask;
  outDataset->mSecurityPolicy.mRotationTime = 672U;
  outDataset->mSecurityPolicy.mObtainNetworkKeyEnabled = true;
  outDataset->mSecurityPolicy.mNativeCommissioningEnabled = true;
  outDataset->mSecurityPolicy.mRoutersEnabled = true;
  outDataset->mSecurityPolicy.mExternalCommissioningEnabled = true;
  outDataset->mSecurityPolicy.mNetworkKeyProvisioningEnabled = true;
  outDataset->mSecurityPolicy.mVersionThresholdForRouting = 3U;

  outDataset->mComponents.mIsActiveTimestampPresent = true;
  outDataset->mComponents.mIsNetworkKeyPresent = true;
  outDataset->mComponents.mIsNetworkNamePresent = true;
  outDataset->mComponents.mIsExtendedPanIdPresent = true;
  outDataset->mComponents.mIsMeshLocalPrefixPresent = true;
  outDataset->mComponents.mIsPanIdPresent = true;
  outDataset->mComponents.mIsChannelPresent = true;
  outDataset->mComponents.mIsPskcPresent = true;
  outDataset->mComponents.mIsSecurityPolicyPresent = true;
  outDataset->mComponents.mIsChannelMaskPresent = true;
  outDataset->mComponents.mIsWakeupChannelPresent = true;
}

otError Nrf54ThreadExperimental::generatePskc(
    const char* passPhrase, const char* networkName,
    const uint8_t extPanId[OT_EXT_PAN_ID_SIZE], otPskc* outPskc) {
  if (passPhrase == nullptr || networkName == nullptr || extPanId == nullptr ||
      outPskc == nullptr) {
    return OT_ERROR_INVALID_ARGS;
  }

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  memset(outPskc, 0, sizeof(*outPskc));
  return OT_ERROR_INVALID_STATE;
#else
  otNetworkName parsedNetworkName = {};
  otExtendedPanId parsedExtPanId = {};
  otError error = otNetworkNameFromString(&parsedNetworkName, networkName);
  if (error != OT_ERROR_NONE) {
    memset(outPskc, 0, sizeof(*outPskc));
    return error;
  }

  memcpy(parsedExtPanId.m8, extPanId, sizeof(parsedExtPanId.m8));
  error = otDatasetGeneratePskc(passPhrase, &parsedNetworkName, &parsedExtPanId,
                                outPskc);
  if (error != OT_ERROR_NONE) {
    memset(outPskc, 0, sizeof(*outPskc));
  }
  return error;
#endif
}

otError Nrf54ThreadExperimental::buildDatasetFromPassphrase(
    const char* passPhrase, const char* networkName,
    const uint8_t extPanId[OT_EXT_PAN_ID_SIZE], otOperationalDataset* outDataset) {
  if (passPhrase == nullptr || networkName == nullptr || extPanId == nullptr ||
      outDataset == nullptr) {
    return OT_ERROR_INVALID_ARGS;
  }

  buildDemoDataset(outDataset);

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
  return OT_ERROR_INVALID_STATE;
#else
  otError error = otNetworkNameFromString(&outDataset->mNetworkName, networkName);
  if (error != OT_ERROR_NONE) {
    return error;
  }

  memcpy(outDataset->mExtendedPanId.m8, extPanId, OT_EXT_PAN_ID_SIZE);
  error = generatePskc(passPhrase, networkName, extPanId, &outDataset->mPskc);
  if (error != OT_ERROR_NONE) {
    return error;
  }

  outDataset->mComponents.mIsNetworkNamePresent = true;
  outDataset->mComponents.mIsExtendedPanIdPresent = true;
  outDataset->mComponents.mIsPskcPresent = true;
  return OT_ERROR_NONE;
#endif
}

bool Nrf54ThreadExperimental::bytesToUpperHex(
    const uint8_t* data, size_t length, char* outBuffer, size_t outBufferSize,
    size_t* outHexLength) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";

  if (outHexLength != nullptr) {
    *outHexLength = 0U;
  }
  if (data == nullptr || outBuffer == nullptr ||
      outBufferSize < ((length * 2U) + 1U)) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    outBuffer[i * 2U] = kHexDigits[(data[i] >> 4U) & 0x0FU];
    outBuffer[(i * 2U) + 1U] = kHexDigits[data[i] & 0x0FU];
  }
  outBuffer[length * 2U] = '\0';
  if (outHexLength != nullptr) {
    *outHexLength = length * 2U;
  }
  return true;
}

int Nrf54ThreadExperimental::hexNibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return 10 + (value - 'A');
  }
  if (value >= 'a' && value <= 'f') {
    return 10 + (value - 'a');
  }
  return -1;
}

bool Nrf54ThreadExperimental::hexToBytes(
    const char* text, uint8_t* outData, size_t outCapacity, size_t* outLength) {
  if (outLength != nullptr) {
    *outLength = 0U;
  }
  if (text == nullptr || outData == nullptr) {
    return false;
  }

  int highNibble = -1;
  size_t length = 0U;
  for (const char* current = text; *current != '\0'; ++current) {
    const char c = *current;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' ||
        c == '-') {
      continue;
    }

    const int nibble = hexNibble(c);
    if (nibble < 0) {
      return false;
    }

    if (highNibble < 0) {
      highNibble = nibble;
      continue;
    }

    if (length >= outCapacity) {
      return false;
    }

    outData[length++] =
        static_cast<uint8_t>((highNibble << 4U) | static_cast<uint8_t>(nibble));
    highNibble = -1;
  }

  if (highNibble >= 0 || length == 0U) {
    return false;
  }

  if (outLength != nullptr) {
    *outLength = length;
  }
  return true;
}

void Nrf54ThreadExperimental::handleUdpReceiveStatic(
    void* context, otMessage* message, const otMessageInfo* messageInfo) {
  if (context == nullptr) {
    return;
  }
  static_cast<Nrf54ThreadExperimental*>(context)->handleUdpReceive(message,
                                                                   messageInfo);
}

void Nrf54ThreadExperimental::handleStateChangedStatic(otChangedFlags flags,
                                                       void* context) {
  if (context == nullptr) {
    return;
  }
  static_cast<Nrf54ThreadExperimental*>(context)->handleStateChanged(flags);
}

void Nrf54ThreadExperimental::handleCommissionerStateStatic(
    otCommissionerState state, void* context) {
  if (context == nullptr) {
    return;
  }
  static_cast<Nrf54ThreadExperimental*>(context)->handleCommissionerState(state);
}

void Nrf54ThreadExperimental::handleCommissionerJoinerStatic(
    otCommissionerJoinerEvent event,
    const otJoinerInfo* joinerInfo,
    const otExtAddress* joinerId,
    void* context) {
  if (context == nullptr) {
    return;
  }
  static_cast<Nrf54ThreadExperimental*>(context)->handleCommissionerJoiner(
      event, joinerInfo, joinerId);
}

void Nrf54ThreadExperimental::handleJoinerStatic(otError error,
                                                 void* context) {
  if (context == nullptr) {
    return;
  }
  static_cast<Nrf54ThreadExperimental*>(context)->handleJoiner(error);
}

void Nrf54ThreadExperimental::handleUdpReceive(
    otMessage* message, const otMessageInfo* messageInfo) {
  if (message == nullptr || messageInfo == nullptr) {
    return;
  }

  const UdpSocketSlot* slot = findUdpSlot(messageInfo->mSockPort);
  if (slot == nullptr || slot->callback == nullptr) {
    slot = nullptr;
    for (const UdpSocketSlot& candidate : udpSockets_) {
      if (candidate.opened && candidate.callback != nullptr) {
        slot = &candidate;
        break;
      }
    }
  }
  if (slot == nullptr || slot->callback == nullptr) {
    return;
  }

  const uint16_t length = otMessageGetLength(message);
  uint8_t buffer[256] = {0};
  uint16_t copyLength = length;
  if (copyLength > sizeof(buffer)) {
    copyLength = sizeof(buffer);
  }
  if (copyLength != 0U) {
    otMessageRead(message, 0, buffer, copyLength);
  }

  slot->callback(slot->callbackContext, buffer, copyLength, *messageInfo);
}

void Nrf54ThreadExperimental::handleStateChanged(otChangedFlags flags) {
  lastChangedFlags_ = flags;
  pendingChangedFlags_ |= flags;

  if (stateChangedCallback_ != nullptr) {
    stateChangedCallback_(stateChangedCallbackContext_, flags, role());
  }
}

void Nrf54ThreadExperimental::handleCommissionerState(
    otCommissionerState state) {
  if (state == OT_COMMISSIONER_STATE_ACTIVE) {
    commissionerStarted_ = true;
  } else if (state == OT_COMMISSIONER_STATE_DISABLED) {
    commissionerStarted_ = false;
  }

  if (commissionerStateCallback_ != nullptr) {
    commissionerStateCallback_(commissionerStateCallbackContext_, state);
  }
}

void Nrf54ThreadExperimental::handleCommissionerJoiner(
    otCommissionerJoinerEvent event,
    const otJoinerInfo* joinerInfo,
    const otExtAddress* joinerId) {
  (void)joinerInfo;

  if (commissionerJoinerCallback_ == nullptr) {
    return;
  }

  otError error = OT_ERROR_NONE;
  if (event == OT_COMMISSIONER_JOINER_REMOVED) {
    error = OT_ERROR_ABORT;
  }
  commissionerJoinerCallback_(commissionerJoinerCallbackContext_, joinerId,
                              error);
}

void Nrf54ThreadExperimental::handleJoiner(otError error) {
  joinerStarted_ = false;
  if (joinerCallback_ != nullptr) {
    joinerCallback_(joinerCallbackContext_, error);
  }
}

Nrf54ThreadExperimental::Role Nrf54ThreadExperimental::convertRole(
    otDeviceRole role) {
  switch (role) {
    case OT_DEVICE_ROLE_DISABLED:
      return Role::kDisabled;
    case OT_DEVICE_ROLE_DETACHED:
      return Role::kDetached;
    case OT_DEVICE_ROLE_CHILD:
      return Role::kChild;
    case OT_DEVICE_ROLE_ROUTER:
      return Role::kRouter;
    case OT_DEVICE_ROLE_LEADER:
      return Role::kLeader;
    default:
      return Role::kUnknown;
  }
}

uint32_t Nrf54ThreadExperimental::computeChildFirstFallbackDelayMs() {
  uint8_t eui64[OT_EXT_ADDRESS_SIZE] = {0};
  otPlatRadioGetIeeeEui64(nullptr, eui64);

  uint32_t hash = 2166136261UL;
  for (uint8_t value : eui64) {
    hash ^= value;
    hash *= 16777619UL;
  }

  const uint32_t now = micros();
  hash ^= now;
  hash *= 16777619UL;

  return kChildFirstFallbackBaseMs +
         (hash % (kChildFirstFallbackJitterMs + 1UL));
}

}  // namespace xiao_nrf54l15
