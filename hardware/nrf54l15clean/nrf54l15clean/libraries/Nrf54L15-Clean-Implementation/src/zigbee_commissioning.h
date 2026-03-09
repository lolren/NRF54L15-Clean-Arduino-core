#pragma once

#include <stdint.h>

#include "nrf54l15_hal.h"
#include "zigbee_persistence.h"
#include "zigbee_security.h"
#include "zigbee_stack.h"

namespace xiao_nrf54l15 {

enum class ZigbeePreconfiguredKeyMode : uint8_t {
  kNone = 0U,
  kWellKnown = 1U,
  kInstallCodeDerived = 2U,
};

struct ZigbeeCommissioningPolicy {
  uint32_t primaryChannelMask = 0x07FFF800UL;
  uint32_t secondaryChannelMask = 0U;
  uint32_t joinRetryDelayMs = 2000UL;
  uint32_t secureRejoinRetryDelayMs = 2000UL;
  uint32_t transportKeyTimeoutMs = 4000UL;
  uint32_t updateDeviceTimeoutMs = 4000UL;
  uint32_t deviceAnnounceRetryDelayMs = 1000UL;
  uint32_t endDeviceTimeoutRetryDelayMs = 1500UL;
  uint32_t initialPollIntervalMs = 250UL;
  uint16_t preferredPanId = 0U;
  uint64_t preferredExtendedPanId = 0U;
  uint64_t pinnedTrustCenterIeee = 0U;
  uint8_t maxJoinAttempts = 0U;
  uint8_t maxRejoinAttempts = 0U;
  uint8_t requestedEndDeviceTimeout = 0x03U;
  uint8_t endDeviceConfiguration = 0x00U;
  bool requirePermitJoin = true;
  bool requirePanCoordinator = true;
  bool requireStackProfile2 = true;
  bool requireProtocolVersion2 = true;
  bool allowWellKnownKey = true;
  bool allowInstallCodeKey = true;
  bool installCodeOnly = false;
  bool requireEncryptedTransportKey = true;
  bool requireEncryptedUpdateDevice = true;
  bool requireEncryptedSwitchKey = true;
  bool requireUniqueTrustCenterForRejoin = true;
  bool fallbackToJoinAfterRejoinFailure = true;
};

enum class ZigbeeCommissioningState : uint8_t {
  kIdle = 0U,
  kRestored = 1U,
  kScanning = 2U,
  kAssociating = 3U,
  kWaitingTransportKey = 4U,
  kRejoinPending = 5U,
  kWaitingUpdateDevice = 6U,
  kRejoinVerify = 7U,
  kJoined = 8U,
  kLeaveReset = 9U,
  kFailed = 10U,
};

enum class ZigbeeCommissioningFailure : uint8_t {
  kNone = 0U,
  kNoCandidate = 1U,
  kAssociationRequestFailed = 2U,
  kAssociationTimeout = 3U,
  kSecureRejoinUnavailable = 4U,
  kTransportKeyRejected = 5U,
  kUpdateDeviceRejected = 6U,
  kSwitchKeyRejected = 7U,
  kTransportKeyTimeout = 8U,
  kUpdateDeviceTimeout = 9U,
  kJoinAttemptBudgetExceeded = 10U,
  kRejoinAttemptBudgetExceeded = 11U,
};

enum class ZigbeeCommissioningAction : uint8_t {
  kNone = 0U,
  kPollParent = 1U,
  kJoin = 2U,
  kSecureRejoin = 3U,
  kSendDeviceAnnounce = 4U,
  kRequestEndDeviceTimeout = 5U,
};

enum class ZigbeeAcceptedLeaveDisposition : uint8_t {
  kClearState = 0U,
  kPersistRejoin = 1U,
  kClearStateAfterRejoinFailure = 2U,
};

enum class ZigbeeCommissioningStartRequest : uint8_t {
  kNone = 0U,
  kNetworkSteering = 1U,
  kSecureRejoin = 2U,
};

struct ZigbeeBeaconCandidate {
  bool valid = false;
  uint8_t channel = 0U;
  int8_t rssiDbm = -127;
  int16_t score = 0;
  ZigbeeMacBeaconView beacon{};
};

struct ZigbeeEndDeviceCommonState {
  ZigbeeCommissioningPolicy policy{};
  uint8_t preferredChannel = 11U;
  uint16_t preferredPanId = 0U;
  uint16_t defaultShort = 0U;
  uint16_t coordinatorShort = 0U;
  uint8_t channel = 11U;
  uint16_t panId = 0U;
  uint16_t localShort = 0U;
  uint16_t parentShort = 0U;
  uint64_t extendedPanId = 0U;
  uint64_t trustCenterIeee = 0U;
  uint8_t activeNetworkKey[16] = {0U};
  uint8_t activeNetworkKeySequence = 0U;
  uint8_t alternateNetworkKey[16] = {0U};
  uint8_t alternateNetworkKeySequence = 0U;
  uint8_t nwkSequence = 1U;
  uint32_t nwkSecurityFrameCounter = 1U;
  uint32_t incomingNwkFrameCounter = 0U;
  uint32_t incomingApsFrameCounter = 0U;
  uint8_t apsCounter = 1U;
  uint8_t endDeviceTimeoutIndex = 0U;
  uint8_t endDeviceConfiguration = 0U;
  uint8_t parentInformation = 0U;
  uint32_t parentPollIntervalMs = 250UL;
  uint32_t lastDeviceAnnounceMs = 0U;
  uint32_t lastEndDeviceTimeoutRequestMs = 0U;
  bool joined = false;
  bool rejoinPending = false;
  bool securityEnabled = false;
  bool haveActiveNetworkKey = false;
  bool haveAlternateNetworkKey = false;
  bool deviceAnnouncePending = false;
  bool endDeviceTimeoutPending = false;
  bool endDeviceTimeoutNegotiated = false;
  ZigbeePreconfiguredKeyMode preconfiguredKeyMode =
      ZigbeePreconfiguredKeyMode::kNone;
  ZigbeeCommissioningState state = ZigbeeCommissioningState::kIdle;
  ZigbeeCommissioningFailure lastFailure = ZigbeeCommissioningFailure::kNone;
  uint32_t joinAttempts = 0U;
  uint32_t rejoinAttempts = 0U;
  uint32_t lastJoinAttemptMs = 0U;
};

struct ZigbeeTransportKeyInstallResult {
  bool valid = false;
  ZigbeeApsTransportKey transportKey{};
  ZigbeeApsSecurityHeader apsSecurity{};
  ZigbeePreconfiguredKeyMode keyMode = ZigbeePreconfiguredKeyMode::kNone;
  uint8_t counter = 0U;
  bool activatesNetworkKey = false;
  bool stagesAlternateKey = false;
  bool refreshesActiveNetworkKey = false;
  bool refreshesAlternateKey = false;
};

struct ZigbeeUpdateDeviceAcceptance {
  bool valid = false;
  ZigbeeApsUpdateDevice updateDevice{};
  ZigbeeApsSecurityHeader apsSecurity{};
  uint8_t counter = 0U;
};

struct ZigbeeSwitchKeyAcceptance {
  bool valid = false;
  ZigbeeApsSwitchKey switchKey{};
  ZigbeeApsSecurityHeader apsSecurity{};
  uint8_t counter = 0U;
};

class ZigbeeCommissioning {
 public:
  static void buildSteeringScanMasks(const ZigbeeCommissioningPolicy& policy,
                                     uint32_t outMasks[2],
                                     uint8_t* outCount);
  static bool channelInMask(uint32_t mask, uint8_t channel);
  static bool scoreBeacon(const ZigbeeCommissioningPolicy& policy,
                          uint8_t channel, int8_t rssiDbm,
                          const ZigbeeMacBeaconView& beacon,
                          int16_t* outScore);
  static bool scoreKnownNetworkBeacon(
      const ZigbeeEndDeviceCommonState& state, uint8_t channel,
      int8_t rssiDbm, const ZigbeeMacBeaconView& beacon, int16_t* outScore);
  static bool shouldReplaceCandidate(const ZigbeeBeaconCandidate& current,
                                     const ZigbeeBeaconCandidate& candidate);
  static void initializeEndDeviceState(
      ZigbeeEndDeviceCommonState* state,
      const ZigbeeCommissioningPolicy& policy, uint8_t preferredChannel,
      uint16_t preferredPanId, uint16_t defaultShort,
      uint16_t coordinatorShort);
  static void restoreEndDeviceState(ZigbeeEndDeviceCommonState* state,
                                    const ZigbeePersistentState& persistent,
                                    uint64_t localIeee);
  static void clearEndDeviceState(ZigbeeEndDeviceCommonState* state,
                                  bool clearIdentity);
  static void populatePersistentState(const ZigbeeEndDeviceCommonState& state,
                                      uint64_t localIeee,
                                      ZigbeeLogicalType logicalType,
                                      uint16_t manufacturerCode,
                                      ZigbeePersistentState* outState);
  static uint64_t expectedTrustCenterIeee(
      const ZigbeeEndDeviceCommonState& state);
  static uint32_t timeoutIndexToMs(uint8_t timeoutIndex);
  static bool shouldAttemptSecureRejoin(
      const ZigbeeEndDeviceCommonState& state);
  static void requestNetworkSteering(ZigbeeEndDeviceCommonState* state);
  static ZigbeeCommissioningStartRequest requestRejoinOrSteering(
      ZigbeeEndDeviceCommonState* state);
  static void requestSecureRejoin(ZigbeeEndDeviceCommonState* state);
  static ZigbeeAcceptedLeaveDisposition applyAcceptedLeaveRequest(
      ZigbeeEndDeviceCommonState* state, uint8_t leaveFlags);
  static bool shouldPollParent(const ZigbeeEndDeviceCommonState& state);
  static bool shouldRequestEndDeviceTimeout(
      const ZigbeeEndDeviceCommonState& state);
  static void markDeviceAnnouncePending(
      ZigbeeEndDeviceCommonState* state);
  static void recordDeviceAnnounceAttempt(
      ZigbeeEndDeviceCommonState* state, uint32_t nowMs);
  static void completeDeviceAnnounce(
      ZigbeeEndDeviceCommonState* state);
  static void markEndDeviceTimeoutPending(ZigbeeEndDeviceCommonState* state);
  static void recordEndDeviceTimeoutRequest(
      ZigbeeEndDeviceCommonState* state, uint32_t nowMs);
  static bool acceptEndDeviceTimeoutResponse(
      const ZigbeeEndDeviceCommonState& state, const uint8_t* frame,
      uint8_t length, ZigbeeNwkEndDeviceTimeoutResponse* outResponse);
  static void applyEndDeviceTimeoutResponse(
      ZigbeeEndDeviceCommonState* state,
      const ZigbeeNwkEndDeviceTimeoutResponse& response);
  static ZigbeeCommissioningAction nextAction(
      ZigbeeEndDeviceCommonState* state, uint32_t nowMs);
  static bool activeScan(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                         ZigbeeEndDeviceCommonState* state,
                         ZigbeeBeaconCandidate* outResult);
  static bool performJoin(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                          uint64_t localIeee, uint8_t capabilityInformation,
                          ZigbeeEndDeviceCommonState* state);
  static bool performSecureRejoin(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                                  uint64_t localIeee,
                                  uint8_t capabilityInformation,
                                  ZigbeeEndDeviceCommonState* state);
  static bool acceptTransportKeyCommand(
      const ZigbeeEndDeviceCommonState& state, uint64_t localIeee,
      uint16_t sourceShort, uint64_t securedSourceIeee, bool nwkSecured,
      const uint8_t* frame, uint8_t length, const uint8_t installCodeKey[16],
      bool haveInstallCodeKey, ZigbeeTransportKeyInstallResult* outResult);
  static ZigbeeCommissioningFailure classifyRejectedTrustCenterCommand(
      const ZigbeeEndDeviceCommonState& state, const uint8_t* frame,
      uint8_t length, const uint8_t installCodeKey[16],
      bool haveInstallCodeKey);
  static void applyTransportKeyInstall(
      ZigbeeEndDeviceCommonState* state,
      const ZigbeeTransportKeyInstallResult& result);
  static bool acceptUpdateDeviceCommand(
      const ZigbeeEndDeviceCommonState& state, uint64_t localIeee,
      uint16_t sourceShort, uint64_t securedSourceIeee, bool nwkSecured,
      bool allowPlaintext, const uint8_t* frame, uint8_t length,
      const uint8_t installCodeKey[16], bool haveInstallCodeKey,
      ZigbeeUpdateDeviceAcceptance* outResult);
  static void applyUpdateDevice(ZigbeeEndDeviceCommonState* state,
                                const ZigbeeUpdateDeviceAcceptance& result);
  static void completeRejoinVerification(
      ZigbeeEndDeviceCommonState* state);
  static bool acceptSwitchKeyCommand(
      const ZigbeeEndDeviceCommonState& state, uint16_t sourceShort,
      uint64_t securedSourceIeee, bool nwkSecured, bool allowPlaintext,
      const uint8_t* frame, uint8_t length, const uint8_t installCodeKey[16],
      bool haveInstallCodeKey,
      ZigbeeSwitchKeyAcceptance* outResult);
  static void applySwitchKey(ZigbeeEndDeviceCommonState* state,
                             const ZigbeeSwitchKeyAcceptance& result);
  static bool isUniqueLinkKeyMode(ZigbeePreconfiguredKeyMode mode);
  static const char* keyModeName(ZigbeePreconfiguredKeyMode mode);
  static const char* stateName(ZigbeeCommissioningState state);
  static const char* failureName(ZigbeeCommissioningFailure failure);
};

}  // namespace xiao_nrf54l15
