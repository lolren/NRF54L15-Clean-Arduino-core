#pragma once

#include <stddef.h>
#include <stdint.h>

#include "openthread_platform_nrf54l15.h"

#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/udp.h>
#include <openthread/commissioner.h>
#include <openthread/joiner.h>

namespace xiao_nrf54l15 {

class Nrf54ThreadExperimental {
 public:
  static constexpr uint8_t kMaxUdpSockets = 4U;

  using UdpReceiveCallback = void (*)(void* context,
                                      const uint8_t* payload,
                                      uint16_t length,
                                      const otMessageInfo& messageInfo);

  enum class Role : uint8_t {
    kDisabled = 0U,
    kDetached = 1U,
    kChild = 2U,
    kRouter = 3U,
    kLeader = 4U,
    kUnknown = 255U,
  };
  enum class AttachPolicy : uint8_t {
    kChildFirst = 0U,
    kChildOnly = 1U,
    kRouterEligible = 2U,
  };
  using StateChangedCallback = void (*)(void* context,
                                        otChangedFlags flags,
                                        Role role);
  using CommissionerStateCallback = void (*)(void* context,
                                             otCommissionerState state);
  using CommissionerJoinerCallback = void (*)(void* context,
                                              const otExtAddress* joinerId,
                                              otError error);
  using JoinerCallback = void (*)(void* context, otError error);

  struct AttachDiagnostics {
    uint32_t currentAttachDurationMs = 0U;
    uint32_t partitionId = 0U;
    uint32_t childFirstFallbackDelayMs = 0U;
    uint32_t childFirstFallbackRemainingMs = 0U;
    uint16_t attachAttempts = 0U;
    uint16_t betterPartitionAttachAttempts = 0U;
    uint16_t betterParentAttachAttempts = 0U;
    uint16_t parentChanges = 0U;
    uint8_t attachPolicy = 0U;
    bool routerEligible = false;
    bool childFirstFallbackArmed = false;
    bool childFirstFallbackUsed = false;
  };

  struct AttachDebugState {
    bool valid = false;
    bool attachInProgress = false;
    bool attachTimerRunning = false;
    bool receivedResponseFromParent = false;
    uint8_t attachState = 0U;
    uint8_t attachMode = 0U;
    uint8_t reattachMode = 0U;
    uint8_t parentRequestCounter = 0U;
    uint8_t childIdRequestsRemaining = 0U;
    uint8_t parentCandidateState = 0U;
    uint16_t attachCounter = 0U;
    uint16_t parentCandidateRloc16 = OT_RADIO_INVALID_SHORT_ADDR;
    uint32_t attachTimerRemainingMs = 0U;
    char attachStateName[16] = {0};
    char attachModeName[20] = {0};
    char reattachModeName[24] = {0};
    char parentCandidateStateName[16] = {0};
  };

  struct AttachSummary {
    bool valid = false;
    bool attached = false;
    bool configuredForAttach = false;
    bool waitingForDataset = false;
    bool waitingForDatasetApply = false;
    bool waitingForIp6Enable = false;
    bool waitingForThreadEnable = false;
    bool waitingForParentResponse = false;
    bool waitingForChildIdResponse = false;
    bool waitingForReattachTimer = false;
    bool waitingForLeaderFallback = false;
    Role role = Role::kUnknown;
    char phaseName[32] = {0};
    char blockerName[40] = {0};
  };

  Nrf54ThreadExperimental() = default;

  bool begin(bool wipeSettings = true);
  bool beginAsChild(bool wipeSettings = true);
  bool beginAsRouter(bool wipeSettings = true);
  bool beginChildFirst(bool wipeSettings = true);
  bool stop();
  bool restart(bool wipeSettings = false);
  void process();

  bool setActiveDataset(const otOperationalDataset& dataset);
  bool setActiveDatasetTlvs(const otOperationalDatasetTlvs& datasetTlvs);
  bool setActiveDatasetHex(const char* datasetHex);
  bool getActiveDataset(otOperationalDataset* outDataset) const;
  bool getActiveDatasetTlvs(otOperationalDatasetTlvs* outDatasetTlvs) const;
  bool getConfiguredOrActiveDataset(otOperationalDataset* outDataset) const;
  bool getConfiguredOrActiveDatasetTlvs(
      otOperationalDatasetTlvs* outDatasetTlvs) const;
  bool exportConfiguredOrActiveDatasetHex(char* outBuffer, size_t outBufferSize,
                                          size_t* outHexLength = nullptr) const;
  bool wipePersistentSettings();
  bool requestRouterRole();
  bool startCommissioner();
  bool stopCommissioner();
  bool addJoinerToCommissioner(const char* pskd,
                               uint32_t timeoutSeconds = 120U);
  bool setCommissionerProvisioningUrl(const char* provisioningUrl);
  bool setCommissionerStateCallback(CommissionerStateCallback callback,
                                    void* callbackContext = nullptr);
  bool setCommissionerJoinerCallback(CommissionerJoinerCallback callback,
                                     void* callbackContext = nullptr);
  bool startJoiner(const char* pskd,
                   const char* provisioningUrl = nullptr,
                   JoinerCallback callback = nullptr,
                   void* callbackContext = nullptr);
  bool stopJoiner();

  bool openUdp(uint16_t port,
               UdpReceiveCallback callback,
               void* callbackContext = nullptr);
  bool setStateChangedCallback(StateChangedCallback callback,
                               void* callbackContext = nullptr);
  bool sendUdp(const otIp6Address& peerAddr,
               uint16_t peerPort,
               const void* payload,
               uint16_t payloadLength);
  bool sendUdpFrom(uint16_t localPort,
                   const otIp6Address& peerAddr,
                   uint16_t peerPort,
                   const void* payload,
                   uint16_t payloadLength);
  bool subscribeMulticast(const otIp6Address& multicastAddr);
  bool unsubscribeMulticast(const otIp6Address& multicastAddr);
  bool getLeaderRloc(otIp6Address* outLeaderAddr) const;
  bool getAttachDiagnostics(AttachDiagnostics* outDiagnostics) const;
  bool getAttachDebugState(AttachDebugState* outState) const;
  bool getAttachSummary(AttachSummary* outSummary) const;

  bool started() const;
  bool attached() const;
  Role role() const;
  const char* roleName() const;
  uint16_t rloc16() const;
  uint32_t partitionId() const;
  bool routerEligible() const;
  bool childFirstFallbackArmed() const;
  bool childFirstFallbackUsed() const;
  uint32_t childFirstFallbackDelayMs() const;
  bool commissionerSupported() const;
  bool commissionerActive() const;
  otCommissionerState commissionerState() const;
  const char* commissionerStateName() const;
  bool commissionerSessionId(uint16_t* outSessionId) const;
  bool joinerSupported() const;
  bool joinerActive() const;
  otJoinerState joinerState() const;
  const char* joinerStateName() const;
  bool datasetConfigured() const;
  bool restoredFromSettings() const;
  otError lastError() const;
  otError lastUdpError() const;
  otChangedFlags lastChangedFlags() const;
  otChangedFlags pendingChangedFlags() const;
  otChangedFlags consumePendingChangedFlags();
  bool udpOpened() const;
  bool udpOpened(uint16_t port) const;
  otInstance* rawInstance() const;

  static const char* roleName(Role role);
  static const char* attachPolicyName(AttachPolicy policy);
  static const char* commissionerStateName(otCommissionerState state);
  static const char* joinerStateName(otJoinerState state);
  static void buildDemoDataset(otOperationalDataset* outDataset);
  static otError generatePskc(const char* passPhrase,
                              const char* networkName,
                              const uint8_t extPanId[OT_EXT_PAN_ID_SIZE],
                              otPskc* outPskc);
  static otError buildDatasetFromPassphrase(
      const char* passPhrase,
      const char* networkName,
      const uint8_t extPanId[OT_EXT_PAN_ID_SIZE],
      otOperationalDataset* outDataset);

 private:
  bool begin(bool wipeSettings, AttachPolicy policy);
  static void handleUdpReceiveStatic(void* context,
                                     otMessage* message,
                                     const otMessageInfo* messageInfo);
  static void handleStateChangedStatic(otChangedFlags flags, void* context);
  static void handleCommissionerStateStatic(otCommissionerState state,
                                            void* context);
  static void handleCommissionerJoinerStatic(
      otCommissionerJoinerEvent event,
      const otJoinerInfo* joinerInfo,
      const otExtAddress* joinerId,
      void* context);
  static void handleJoinerStatic(otError error, void* context);
  void handleUdpReceive(otMessage* message, const otMessageInfo* messageInfo);
  void handleStateChanged(otChangedFlags flags);
  void handleCommissionerState(otCommissionerState state);
  void handleCommissionerJoiner(otCommissionerJoinerEvent event,
                                const otJoinerInfo* joinerInfo,
                                const otExtAddress* joinerId);
  void handleJoiner(otError error);
  bool restoreDatasetFromSettings();
  bool configureAttachPolicy();
  bool maybePromoteChildFirstFallback(uint32_t elapsedMs);

  struct UdpSocketSlot {
    otUdpSocket socket = {};
    UdpReceiveCallback callback = nullptr;
    void* callbackContext = nullptr;
    uint16_t port = 0U;
    bool requested = false;
    bool opened = false;
  };

  UdpSocketSlot* findUdpSlot(uint16_t port);
  const UdpSocketSlot* findUdpSlot(uint16_t port) const;
  UdpSocketSlot* firstUdpSlot(bool openedOnly);
  const UdpSocketSlot* firstUdpSlot(bool openedOnly) const;
  bool openUdpSlot(UdpSocketSlot* slot);

  static Role convertRole(otDeviceRole role);
  static uint32_t computeChildFirstFallbackDelayMs();
  static bool bytesToUpperHex(const uint8_t* data, size_t length,
                              char* outBuffer, size_t outBufferSize,
                              size_t* outHexLength = nullptr);
  static int hexNibble(char value);
  static bool hexToBytes(const char* text, uint8_t* outData,
                         size_t outCapacity, size_t* outLength = nullptr);

  static constexpr uint32_t kStageInitDelayMs = 2000UL;
  static constexpr uint32_t kStageDatasetApplyDelayMs = 4000UL;
  static constexpr uint32_t kStageIp6EnableDelayMs = 5000UL;
  static constexpr uint32_t kStageThreadEnableDelayMs = 6000UL;
  static constexpr uint32_t kChildFirstFallbackBaseMs = 12000UL;
  static constexpr uint32_t kChildFirstFallbackJitterMs = 12000UL;

  otInstance* instance_ = nullptr;
  UdpSocketSlot udpSockets_[kMaxUdpSockets] = {};
  otOperationalDataset dataset_ = {};
  StateChangedCallback stateChangedCallback_ = nullptr;
  void* stateChangedCallbackContext_ = nullptr;
  CommissionerStateCallback commissionerStateCallback_ = nullptr;
  void* commissionerStateCallbackContext_ = nullptr;
  CommissionerJoinerCallback commissionerJoinerCallback_ = nullptr;
  void* commissionerJoinerCallbackContext_ = nullptr;
  JoinerCallback joinerCallback_ = nullptr;
  void* joinerCallbackContext_ = nullptr;

  uint32_t beginMs_ = 0;
  otError lastError_ = OT_ERROR_NONE;
  otError lastUdpError_ = OT_ERROR_NONE;
  otChangedFlags lastChangedFlags_ = 0U;
  otChangedFlags pendingChangedFlags_ = 0U;

  bool beginCalled_ = false;
  AttachPolicy attachPolicy_ = AttachPolicy::kChildFirst;
  bool settingsWiped_ = false;
  bool datasetConfigured_ = false;
  bool datasetApplied_ = false;
  bool datasetRestoreAttempted_ = false;
  bool datasetRestoredFromSettings_ = false;
  bool attachPolicyConfigured_ = false;
  bool routerEligible_ = false;
  bool childFirstFallbackUsed_ = false;
  bool linkConfigured_ = false;
  bool ip6Enabled_ = false;
  bool threadEnabled_ = false;
  bool wipeSettings_ = true;
  bool stateChangedCallbackRegistered_ = false;
  bool commissionerStarted_ = false;
  bool joinerStarted_ = false;
  uint32_t childFirstFallbackDelayMs_ = kChildFirstFallbackBaseMs;
};

}  // namespace xiao_nrf54l15
