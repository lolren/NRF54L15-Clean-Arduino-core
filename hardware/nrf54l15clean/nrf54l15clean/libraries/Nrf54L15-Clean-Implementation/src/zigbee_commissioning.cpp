#include "zigbee_commissioning.h"

#include <Arduino.h>
#include <string.h>

namespace xiao_nrf54l15 {

namespace {

constexpr uint8_t kPersistentFlagJoined = 0x01U;
constexpr uint8_t kPersistentFlagSecurityEnabled = 0x02U;
constexpr uint8_t kPersistentFlagActiveKeyPresent = 0x04U;
constexpr uint8_t kPersistentFlagAlternateKeyPresent = 0x08U;
constexpr uint8_t kMinZigbeeChannel = 11U;
constexpr uint8_t kMaxZigbeeChannel = 26U;

bool attemptBudgetExceeded(uint32_t attempts, uint8_t maxAttempts) {
  return maxAttempts != 0U && attempts >= maxAttempts;
}

bool keySequenceIsNewer(uint8_t candidate, uint8_t current) {
  const uint8_t delta = static_cast<uint8_t>(candidate - current);
  return delta != 0U && delta < 0x80U;
}

void appendScanMaskChannels(uint32_t mask, bool seenChannels[32],
                            uint8_t* channels, uint8_t* ioCount) {
  if (seenChannels == nullptr || channels == nullptr || ioCount == nullptr) {
    return;
  }
  for (uint8_t channel = kMinZigbeeChannel; channel <= kMaxZigbeeChannel;
       ++channel) {
    if ((mask & (1UL << channel)) == 0UL || seenChannels[channel]) {
      continue;
    }
    seenChannels[channel] = true;
    channels[(*ioCount)++] = channel;
  }
}

void buildScanChannelOrder(const ZigbeeCommissioningPolicy& policy,
                           uint8_t outChannels[16], uint8_t* outCount) {
  if (outChannels == nullptr || outCount == nullptr) {
    return;
  }

  *outCount = 0U;
  bool seenChannels[32] = {false};
  appendScanMaskChannels(policy.primaryChannelMask, seenChannels, outChannels,
                         outCount);
  appendScanMaskChannels(policy.secondaryChannelMask, seenChannels, outChannels,
                         outCount);
  if (*outCount != 0U) {
    return;
  }
  for (uint8_t channel = kMinZigbeeChannel; channel <= kMaxZigbeeChannel;
       ++channel) {
    outChannels[(*outCount)++] = channel;
  }
}

void clearNetworkKey(ZigbeeEndDeviceCommonState* state) {
  if (state == nullptr) {
    return;
  }
  memset(state->activeNetworkKey, 0, sizeof(state->activeNetworkKey));
  state->activeNetworkKeySequence = 0U;
  state->haveActiveNetworkKey = false;
}

void clearAlternateNetworkKey(ZigbeeEndDeviceCommonState* state) {
  if (state == nullptr) {
    return;
  }
  memset(state->alternateNetworkKey, 0, sizeof(state->alternateNetworkKey));
  state->alternateNetworkKeySequence = 0U;
  state->haveAlternateNetworkKey = false;
}

uint32_t timeoutIndexToMsInternal(uint8_t timeoutIndex) {
  switch (timeoutIndex) {
    case 0x00U:
      return 10000UL;
    case 0x01U:
      return 120000UL;
    case 0x02U:
      return 240000UL;
    case 0x03U:
      return 480000UL;
    case 0x04U:
      return 960000UL;
    case 0x05U:
      return 1920000UL;
    case 0x06U:
      return 3840000UL;
    case 0x07U:
      return 7680000UL;
    case 0x08U:
      return 15360000UL;
    case 0x09U:
      return 30720000UL;
    case 0x0AU:
      return 61440000UL;
    case 0x0BU:
      return 122880000UL;
    case 0x0CU:
      return 245760000UL;
    case 0x0DU:
      return 491520000UL;
    case 0x0EU:
      return 983040000UL;
    default:
      return 0UL;
  }
}

uint32_t defaultPollIntervalMs(const ZigbeeEndDeviceCommonState& state) {
  return (state.policy.initialPollIntervalMs != 0UL)
             ? state.policy.initialPollIntervalMs
             : 250UL;
}

uint32_t negotiatedPollIntervalMs(const ZigbeeEndDeviceCommonState& state,
                                  uint8_t timeoutIndex) {
  const uint32_t timeoutMs = timeoutIndexToMsInternal(timeoutIndex);
  if (timeoutMs == 0UL) {
    return defaultPollIntervalMs(state);
  }

  uint32_t pollMs = timeoutMs / 8UL;
  if (pollMs < defaultPollIntervalMs(state)) {
    pollMs = defaultPollIntervalMs(state);
  }
  if (pollMs > 30000UL) {
    pollMs = 30000UL;
  }
  return pollMs;
}

bool waitForAssociationResponse(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                                uint16_t panId, uint16_t parentShort,
                                uint64_t localIeee,
                                uint16_t* outAssignedShort) {
  if (ioMacSequence == nullptr || outAssignedShort == nullptr) {
    return false;
  }

  const uint32_t deadline = millis() + 1500U;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    uint8_t pollFrame[127] = {0U};
    uint8_t pollLength = 0U;
    if (!ZigbeeCodec::buildDataRequest((*ioMacSequence)++, panId, parentShort,
                                       localIeee, pollFrame, &pollLength)) {
      return false;
    }
    (void)radio.transmit(pollFrame, pollLength, false, 1200000UL);

    const uint32_t listenDeadline = millis() + 90U;
    while (static_cast<int32_t>(millis() - listenDeadline) < 0) {
      ZigbeeFrame frame{};
      if (!radio.receive(&frame, 5000U, 350000UL)) {
        continue;
      }

      ZigbeeMacAssociationResponseView response{};
      if (!ZigbeeCodec::parseAssociationResponse(frame.psdu, frame.length,
                                                 &response) ||
          !response.valid ||
          response.destinationExtended != localIeee ||
          response.panId != panId || response.status != 0x00U) {
        continue;
      }

      *outAssignedShort = response.assignedShort;
      return true;
    }

    delay(25);
  }

  return false;
}

bool attemptAssociationRequest(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                               uint16_t panId, uint16_t parentShort,
                               uint64_t localIeee,
                               uint8_t capabilityInformation,
                               ZigbeeEndDeviceCommonState* state,
                               uint16_t* outAssignedShort) {
  if (ioMacSequence == nullptr || state == nullptr || outAssignedShort == nullptr) {
    return false;
  }

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildAssociationRequest(
          (*ioMacSequence)++, panId, parentShort, localIeee,
          capabilityInformation, request, &requestLength) ||
      !radio.transmit(request, requestLength, false, 1200000UL)) {
    state->lastFailure = ZigbeeCommissioningFailure::kAssociationRequestFailed;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }

  if (!waitForAssociationResponse(radio, ioMacSequence, panId, parentShort,
                                  localIeee, outAssignedShort)) {
    state->lastFailure = ZigbeeCommissioningFailure::kAssociationTimeout;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }

  return true;
}

bool validTrustCenterSource(const ZigbeeEndDeviceCommonState& state,
                            uint16_t sourceShort, uint64_t securedSourceIeee,
                            bool nwkSecured) {
  if (state.coordinatorShort != 0U && sourceShort != state.coordinatorShort) {
    return false;
  }
  if (state.coordinatorShort == 0U && sourceShort != 0U) {
    return false;
  }

  const uint64_t expectedTc = ZigbeeCommissioning::expectedTrustCenterIeee(state);
  if (!nwkSecured || expectedTc == 0U) {
    return true;
  }
  return securedSourceIeee != 0U && securedSourceIeee == expectedTc;
}

bool validTransportKeyLifecycle(const ZigbeeEndDeviceCommonState& state) {
  if (!state.haveActiveNetworkKey) {
    return !state.joined && !state.rejoinPending &&
           state.state == ZigbeeCommissioningState::kWaitingTransportKey;
  }

  return state.securityEnabled && state.joined && !state.rejoinPending &&
         state.state == ZigbeeCommissioningState::kJoined;
}

bool waitForCoordinatorRealignment(ZigbeeRadio& radio, uint64_t localIeee,
                                   ZigbeeMacCoordinatorRealignmentView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (outView == nullptr) {
    return false;
  }

  const uint32_t deadline = millis() + 180U;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    ZigbeeFrame frame{};
    if (!radio.receive(&frame, 5000U, 300000UL)) {
      continue;
    }

    ZigbeeMacCoordinatorRealignmentView realignment{};
    if (!ZigbeeCodec::parseCoordinatorRealignment(frame.psdu, frame.length,
                                                  &realignment) ||
        !realignment.valid ||
        realignment.destinationExtended != localIeee) {
      continue;
    }

    *outView = realignment;
    return true;
  }

  return false;
}

bool attemptOrphanRecovery(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                           uint64_t localIeee,
                           ZigbeeEndDeviceCommonState* state,
                           uint8_t channel) {
  if (ioMacSequence == nullptr || state == nullptr || channel < kMinZigbeeChannel ||
      channel > kMaxZigbeeChannel || !radio.setChannel(channel)) {
    return false;
  }

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildOrphanNotification((*ioMacSequence)++, localIeee,
                                            request, &requestLength) ||
      !radio.transmit(request, requestLength, false, 1200000UL)) {
    return false;
  }

  ZigbeeMacCoordinatorRealignmentView realignment{};
  if (!waitForCoordinatorRealignment(radio, localIeee, &realignment) ||
      !realignment.valid) {
    return false;
  }

  state->channel = realignment.channel;
  state->panId = realignment.panId;
  state->parentShort = realignment.coordinatorShort;
  state->localShort = realignment.assignedShort;
  state->joined = false;
  state->rejoinPending = true;
  state->securityEnabled = true;
  state->state = ZigbeeCommissioningState::kWaitingUpdateDevice;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
  return true;
}

bool waitForNwkRejoinResponse(ZigbeeRadio& radio, uint16_t panId,
                              uint16_t localShort, uint16_t parentShort,
                              uint64_t expectedParentIeee,
                              const uint8_t networkKey[16],
                              uint8_t keySequence,
                              uint32_t lastInboundFrameCounter,
                              ZigbeeNwkRejoinResponse* outResponse,
                              uint32_t* outInboundFrameCounter) {
  if (outResponse != nullptr) {
    memset(outResponse, 0, sizeof(*outResponse));
  }
  if (outInboundFrameCounter != nullptr) {
    *outInboundFrameCounter = 0U;
  }
  if (networkKey == nullptr || outResponse == nullptr ||
      outInboundFrameCounter == nullptr) {
    return false;
  }

  const uint32_t deadline = millis() + 900U;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    ZigbeeFrame frame{};
    if (!radio.receive(&frame, 5000U, 300000UL)) {
      continue;
    }

    ZigbeeDataFrameView macData{};
    if (!ZigbeeRadio::parseDataFrameShort(frame.psdu, frame.length, &macData) ||
        !macData.valid || macData.panId != panId ||
        macData.destinationShort != localShort ||
        macData.sourceShort != parentShort) {
      continue;
    }

    ZigbeeNetworkFrame nwk{};
    ZigbeeNwkSecurityHeader security{};
    uint8_t decryptedPayload[127] = {0U};
    uint8_t decryptedPayloadLength = 0U;
    if (!ZigbeeSecurity::parseSecuredNwkFrame(
            macData.payload, macData.payloadLength, networkKey, &nwk, &security,
            decryptedPayload, &decryptedPayloadLength) ||
        !nwk.valid || !security.valid ||
        nwk.frameType != ZigbeeNwkFrameType::kCommand ||
        nwk.destinationShort != localShort ||
        nwk.sourceShort != parentShort || security.keySequence != keySequence ||
        security.frameCounter <= lastInboundFrameCounter ||
        (expectedParentIeee != 0U && security.sourceIeee != expectedParentIeee)) {
      continue;
    }

    ZigbeeNwkRejoinResponse response{};
    if (!ZigbeeCodec::parseNwkRejoinResponseCommand(
            nwk.payload, nwk.payloadLength, &response) ||
        !response.valid) {
      continue;
    }

    *outResponse = response;
    *outInboundFrameCounter = security.frameCounter;
    return true;
  }

  return false;
}

bool attemptNwkRejoin(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                      uint64_t localIeee, uint8_t capabilityInformation,
                      ZigbeeEndDeviceCommonState* state) {
  if (ioMacSequence == nullptr || state == nullptr || !state->haveActiveNetworkKey ||
      !radio.setChannel(state->channel)) {
    return false;
  }

  uint8_t commandPayload[8] = {0U};
  uint8_t commandLength = 0U;
  if (!ZigbeeCodec::buildNwkRejoinRequestCommand(capabilityInformation,
                                                 commandPayload,
                                                 &commandLength)) {
    return false;
  }

  ZigbeeNetworkFrame nwk{};
  nwk.frameType = ZigbeeNwkFrameType::kCommand;
  nwk.securityEnabled = true;
  nwk.destinationShort = state->parentShort;
  nwk.sourceShort = state->localShort;
  nwk.radius = 30U;
  nwk.sequence = state->nwkSequence++;

  ZigbeeNwkSecurityHeader security{};
  security.valid = true;
  security.securityControl = kZigbeeSecurityControlNwkEncMic32;
  security.frameCounter = state->nwkSecurityFrameCounter++;
  security.sourceIeee = localIeee;
  security.keySequence = state->activeNetworkKeySequence;

  uint8_t nwkFrame[127] = {0U};
  uint8_t nwkLength = 0U;
  if (!ZigbeeSecurity::buildSecuredNwkFrame(
          nwk, security, state->activeNetworkKey, commandPayload, commandLength,
          nwkFrame, &nwkLength)) {
    return false;
  }

  uint8_t psdu[127] = {0U};
  uint8_t psduLength = 0U;
  if (!ZigbeeRadio::buildDataFrameShort((*ioMacSequence)++, state->panId,
                                        state->parentShort, state->localShort,
                                        nwkFrame, nwkLength, psdu,
                                        &psduLength, false) ||
      !radio.transmit(psdu, psduLength, false, 1200000UL)) {
    return false;
  }

  ZigbeeNwkRejoinResponse response{};
  uint32_t inboundFrameCounter = 0U;
  const uint64_t expectedParentIeee =
      ZigbeeCommissioning::expectedTrustCenterIeee(*state);
  if (!waitForNwkRejoinResponse(radio, state->panId, state->localShort,
                                state->parentShort, expectedParentIeee,
                                state->activeNetworkKey,
                                state->activeNetworkKeySequence,
                                state->incomingNwkFrameCounter, &response,
                                &inboundFrameCounter) ||
      !response.valid || response.status != 0x00U) {
    return false;
  }

  state->incomingNwkFrameCounter = inboundFrameCounter;
  state->localShort = response.networkAddress;
  state->joined = false;
  state->rejoinPending = true;
  state->securityEnabled = true;
  state->state = ZigbeeCommissioningState::kWaitingUpdateDevice;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
  return true;
}

bool tryParseSecuredUpdateDeviceCommand(
    const ZigbeeEndDeviceCommonState& state, const uint8_t* frame,
    uint8_t length, const uint8_t installCodeKey[16], bool haveInstallCodeKey,
    ZigbeeApsUpdateDevice* outUpdateDevice, ZigbeeApsSecurityHeader* outSecurity,
    uint8_t* outCounter) {
  if (outUpdateDevice == nullptr || outSecurity == nullptr ||
      outCounter == nullptr) {
    return false;
  }

  if (state.policy.allowInstallCodeKey && haveInstallCodeKey &&
      installCodeKey != nullptr &&
      ZigbeeSecurity::parseSecuredApsUpdateDeviceCommand(
          frame, length, installCodeKey, outUpdateDevice, outSecurity,
          outCounter)) {
    return true;
  }

  if (state.policy.allowWellKnownKey) {
    uint8_t linkKey[16] = {0U};
    if (ZigbeeSecurity::loadZigbeeAlliance09LinkKey(linkKey) &&
        ZigbeeSecurity::parseSecuredApsUpdateDeviceCommand(
            frame, length, linkKey, outUpdateDevice, outSecurity, outCounter)) {
      return !state.policy.installCodeOnly;
    }
  }

  return false;
}

bool tryParseSecuredSwitchKeyCommand(
    const ZigbeeEndDeviceCommonState& state, const uint8_t* frame,
    uint8_t length, const uint8_t installCodeKey[16], bool haveInstallCodeKey,
    ZigbeeApsSwitchKey* outSwitchKey, ZigbeeApsSecurityHeader* outSecurity,
    uint8_t* outCounter) {
  if (outSwitchKey == nullptr || outSecurity == nullptr ||
      outCounter == nullptr) {
    return false;
  }

  if (state.policy.allowInstallCodeKey && haveInstallCodeKey &&
      installCodeKey != nullptr &&
      ZigbeeSecurity::parseSecuredApsSwitchKeyCommand(
          frame, length, installCodeKey, outSwitchKey, outSecurity,
          outCounter)) {
    return true;
  }

  if (state.policy.allowWellKnownKey) {
    uint8_t linkKey[16] = {0U};
    if (ZigbeeSecurity::loadZigbeeAlliance09LinkKey(linkKey) &&
        ZigbeeSecurity::parseSecuredApsSwitchKeyCommand(
            frame, length, linkKey, outSwitchKey, outSecurity, outCounter)) {
      return !state.policy.installCodeOnly;
    }
  }

  return false;
}

bool scanForKnownNetwork(ZigbeeRadio& radio, uint8_t* ioMacSequence,
                         ZigbeeEndDeviceCommonState* state,
                         ZigbeeBeaconCandidate* outResult) {
  if (ioMacSequence == nullptr || state == nullptr || outResult == nullptr) {
    return false;
  }

  memset(outResult, 0, sizeof(*outResult));
  state->state = ZigbeeCommissioningState::kScanning;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildBeaconRequest((*ioMacSequence)++, request,
                                       &requestLength)) {
    state->lastFailure = ZigbeeCommissioningFailure::kAssociationRequestFailed;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }

  uint8_t channels[16] = {0U};
  uint8_t channelCount = 0U;
  buildScanChannelOrder(state->policy, channels, &channelCount);

  bool found = false;
  for (uint8_t i = 0U; i < channelCount; ++i) {
    const uint8_t channel = channels[i];
    if (!radio.setChannel(channel)) {
      continue;
    }
    (void)radio.transmit(request, requestLength, false, 1200000UL);

    const uint32_t deadline = millis() + 70U;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
      ZigbeeFrame frame{};
      if (!radio.receive(&frame, 4000U, 300000UL)) {
        continue;
      }

      ZigbeeMacBeaconView beacon{};
      if (!ZigbeeCodec::parseBeaconFrame(frame.psdu, frame.length, &beacon) ||
          !beacon.valid) {
        continue;
      }

      int16_t score = 0;
      if (!ZigbeeCommissioning::scoreKnownNetworkBeacon(
              *state, channel, frame.rssiDbm, beacon, &score)) {
        continue;
      }

      ZigbeeBeaconCandidate candidate{};
      candidate.valid = true;
      candidate.channel = channel;
      candidate.rssiDbm = frame.rssiDbm;
      candidate.score = score;
      candidate.beacon = beacon;
      if (!found || ZigbeeCommissioning::shouldReplaceCandidate(*outResult,
                                                                candidate)) {
        *outResult = candidate;
        found = true;
      }
    }
  }

  if (found) {
    radio.setChannel(outResult->channel);
    return true;
  }

  radio.setChannel(state->channel);
  state->lastFailure = ZigbeeCommissioningFailure::kNoCandidate;
  state->state = ZigbeeCommissioningState::kFailed;
  return false;
}

}  // namespace

bool ZigbeeCommissioning::channelInMask(uint32_t mask, uint8_t channel) {
  if (channel > 31U) {
    return false;
  }
  return (mask & (1UL << channel)) != 0UL;
}

bool ZigbeeCommissioning::scoreBeacon(const ZigbeeCommissioningPolicy& policy,
                                      uint8_t channel, int8_t rssiDbm,
                                      const ZigbeeMacBeaconView& beacon,
                                      int16_t* outScore) {
  if (outScore != nullptr) {
    *outScore = 0;
  }
  if (!beacon.valid) {
    return false;
  }

  const bool inPrimary = channelInMask(policy.primaryChannelMask, channel);
  const bool inSecondary = channelInMask(policy.secondaryChannelMask, channel);
  if (policy.primaryChannelMask != 0UL && !inPrimary && !inSecondary) {
    return false;
  }
  if (policy.requirePermitJoin && !beacon.associationPermit) {
    return false;
  }
  if (policy.requirePanCoordinator && !beacon.panCoordinator) {
    return false;
  }
  if (policy.requireStackProfile2 && beacon.network.stackProfile != 2U) {
    return false;
  }
  if (policy.requireProtocolVersion2 && beacon.network.protocolVersion != 2U) {
    return false;
  }
  if (beacon.network.protocolId != 0U) {
    return false;
  }

  int16_t score = static_cast<int16_t>(rssiDbm);
  score += inPrimary ? 32 : 0;
  score += inSecondary ? 8 : 0;
  score += beacon.associationPermit ? 12 : 0;
  score += beacon.network.routerCapacity ? 4 : 0;
  score += beacon.network.endDeviceCapacity ? 4 : 0;
  if (policy.preferredPanId != 0U && beacon.panId == policy.preferredPanId) {
    score += 40;
  }
  if (policy.preferredExtendedPanId != 0U &&
      beacon.network.extendedPanId == policy.preferredExtendedPanId) {
    score += 80;
  }

  if (outScore != nullptr) {
    *outScore = score;
  }
  return true;
}

bool ZigbeeCommissioning::scoreKnownNetworkBeacon(
    const ZigbeeEndDeviceCommonState& state, uint8_t channel, int8_t rssiDbm,
    const ZigbeeMacBeaconView& beacon, int16_t* outScore) {
  if (outScore != nullptr) {
    *outScore = 0;
  }
  if (!beacon.valid) {
    return false;
  }

  ZigbeeCommissioningPolicy rejoinPolicy = state.policy;
  rejoinPolicy.requirePermitJoin = false;
  if (state.panId != 0U) {
    rejoinPolicy.preferredPanId = state.panId;
  }
  if (state.extendedPanId != 0U) {
    rejoinPolicy.preferredExtendedPanId = state.extendedPanId;
  }
  int16_t score = 0;
  if (!scoreBeacon(rejoinPolicy, channel, rssiDbm, beacon, &score)) {
    return false;
  }

  const bool extendedPanMatches =
      (state.extendedPanId == 0U ||
       beacon.network.extendedPanId == state.extendedPanId);
  const bool panMatches = (state.panId == 0U || beacon.panId == state.panId);
  if (!extendedPanMatches || (!panMatches && state.extendedPanId == 0U)) {
    return false;
  }

  if (beacon.panId == state.panId) {
    score += 48;
  }
  if (beacon.network.extendedPanId == state.extendedPanId &&
      state.extendedPanId != 0U) {
    score += 96;
  }
  if (channel == state.channel) {
    score += 24;
  }
  if (beacon.sourceShort == state.parentShort && state.parentShort != 0U) {
    score += 12;
  }

  if (outScore != nullptr) {
    *outScore = score;
  }
  return true;
}

bool ZigbeeCommissioning::shouldReplaceCandidate(
    const ZigbeeBeaconCandidate& current,
    const ZigbeeBeaconCandidate& candidate) {
  if (!candidate.valid) {
    return false;
  }
  if (!current.valid) {
    return true;
  }
  if (candidate.score != current.score) {
    return candidate.score > current.score;
  }
  if (candidate.rssiDbm != current.rssiDbm) {
    return candidate.rssiDbm > current.rssiDbm;
  }
  return candidate.channel < current.channel;
}

void ZigbeeCommissioning::initializeEndDeviceState(
    ZigbeeEndDeviceCommonState* state, const ZigbeeCommissioningPolicy& policy,
    uint8_t preferredChannel, uint16_t preferredPanId, uint16_t defaultShort,
    uint16_t coordinatorShort) {
  if (state == nullptr) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->policy = policy;
  state->preferredChannel = preferredChannel;
  state->preferredPanId = preferredPanId;
  state->defaultShort = defaultShort;
  state->coordinatorShort = coordinatorShort;
  state->channel = preferredChannel;
  state->panId = preferredPanId;
  state->localShort = defaultShort;
  state->parentShort = coordinatorShort;
  state->trustCenterIeee = policy.pinnedTrustCenterIeee;
  state->nwkSequence = 1U;
  state->nwkSecurityFrameCounter = 1U;
  state->apsCounter = 1U;
  state->endDeviceTimeoutIndex = policy.requestedEndDeviceTimeout;
  state->endDeviceConfiguration = policy.endDeviceConfiguration;
  state->parentPollIntervalMs = defaultPollIntervalMs(*state);
  state->state = ZigbeeCommissioningState::kIdle;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
}

void ZigbeeCommissioning::restoreEndDeviceState(
    ZigbeeEndDeviceCommonState* state, const ZigbeePersistentState& persistent,
    uint64_t localIeee) {
  if (state == nullptr) {
    return;
  }

  const ZigbeeCommissioningPolicy policy = state->policy;
  const uint8_t preferredChannel = state->preferredChannel;
  const uint16_t preferredPanId = state->preferredPanId;
  const uint16_t defaultShort = state->defaultShort;
  const uint16_t coordinatorShort = state->coordinatorShort;
  initializeEndDeviceState(state, policy, preferredChannel, preferredPanId,
                           defaultShort, coordinatorShort);

  if (persistent.ieeeAddress != localIeee) {
    return;
  }

  state->channel = (persistent.channel >= 11U && persistent.channel <= 26U)
                       ? persistent.channel
                       : preferredChannel;
  state->panId = (persistent.panId != 0U) ? persistent.panId : preferredPanId;
  state->parentShort =
      (persistent.parentShort != 0U) ? persistent.parentShort : coordinatorShort;
  state->extendedPanId = persistent.extendedPanId;
  state->nwkSequence = 1U;
  state->nwkSecurityFrameCounter =
      (persistent.nwkFrameCounter != 0U) ? persistent.nwkFrameCounter : 1U;
  state->incomingNwkFrameCounter = persistent.incomingNwkFrameCounter;
  state->incomingApsFrameCounter = persistent.incomingApsFrameCounter;
  state->apsCounter = static_cast<uint8_t>(persistent.apsFrameCounter & 0xFFU);
  state->activeNetworkKeySequence = persistent.keySequence;
  if ((persistent.flags & kPersistentFlagActiveKeyPresent) != 0U) {
    memcpy(state->activeNetworkKey, persistent.networkKey,
           sizeof(state->activeNetworkKey));
    state->haveActiveNetworkKey = true;
  }
  state->alternateNetworkKeySequence = persistent.alternateKeySequence;
  if ((persistent.flags & kPersistentFlagAlternateKeyPresent) != 0U) {
    memcpy(state->alternateNetworkKey, persistent.alternateNetworkKey,
           sizeof(state->alternateNetworkKey));
    state->haveAlternateNetworkKey = true;
  }
  if (persistent.trustCenterIeee != 0U) {
    state->trustCenterIeee = persistent.trustCenterIeee;
  }
  if (persistent.preconfiguredKeyMode <=
      static_cast<uint8_t>(ZigbeePreconfiguredKeyMode::kInstallCodeDerived)) {
    state->preconfiguredKeyMode =
        static_cast<ZigbeePreconfiguredKeyMode>(persistent.preconfiguredKeyMode);
  }
  state->securityEnabled =
      (persistent.flags & kPersistentFlagSecurityEnabled) != 0U;
  if ((persistent.flags & kPersistentFlagJoined) != 0U &&
      persistent.nwkAddress != 0U &&
      persistent.nwkAddress != 0xFFFFU) {
    state->localShort = persistent.nwkAddress;
    state->joined = true;
    state->endDeviceTimeoutPending = true;
    state->state = ZigbeeCommissioningState::kJoined;
  } else {
    state->localShort = defaultShort;
    state->state = ZigbeeCommissioningState::kRestored;
  }
}

void ZigbeeCommissioning::clearEndDeviceState(
    ZigbeeEndDeviceCommonState* state, bool clearIdentity) {
  if (state == nullptr) {
    return;
  }

  const ZigbeeCommissioningPolicy policy = state->policy;
  const uint8_t preferredChannel = state->preferredChannel;
  const uint16_t preferredPanId = state->preferredPanId;
  const uint16_t defaultShort = state->defaultShort;
  const uint16_t coordinatorShort = state->coordinatorShort;
  const uint64_t trustCenterIeee = clearIdentity ? policy.pinnedTrustCenterIeee
                                                 : state->trustCenterIeee;
  const ZigbeePreconfiguredKeyMode keyMode =
      clearIdentity ? ZigbeePreconfiguredKeyMode::kNone
                    : state->preconfiguredKeyMode;
  const uint32_t joinAttempts = state->joinAttempts;
  const uint32_t rejoinAttempts = state->rejoinAttempts;

  initializeEndDeviceState(state, policy, preferredChannel, preferredPanId,
                           defaultShort, coordinatorShort);
  state->trustCenterIeee = trustCenterIeee;
  state->preconfiguredKeyMode = keyMode;
  state->joinAttempts = joinAttempts;
  state->rejoinAttempts = rejoinAttempts;
}

void ZigbeeCommissioning::populatePersistentState(
    const ZigbeeEndDeviceCommonState& state, uint64_t localIeee,
    ZigbeeLogicalType logicalType, uint16_t manufacturerCode,
    ZigbeePersistentState* outState) {
  if (outState == nullptr) {
    return;
  }

  ZigbeePersistentStateStore::initialize(outState);
  outState->channel = state.channel;
  outState->logicalType = static_cast<uint8_t>(logicalType);
  outState->panId = state.panId;
  outState->nwkAddress = state.localShort;
  outState->parentShort = state.parentShort;
  outState->manufacturerCode = manufacturerCode;
  outState->ieeeAddress = localIeee;
  outState->extendedPanId = state.extendedPanId;
  outState->nwkFrameCounter = state.nwkSecurityFrameCounter;
  outState->apsFrameCounter = state.apsCounter;
  outState->keySequence =
      state.haveActiveNetworkKey ? state.activeNetworkKeySequence : 0U;
  if (state.haveActiveNetworkKey) {
    memcpy(outState->networkKey, state.activeNetworkKey,
           sizeof(outState->networkKey));
  }
  outState->alternateKeySequence =
      state.haveAlternateNetworkKey ? state.alternateNetworkKeySequence : 0U;
  if (state.haveAlternateNetworkKey) {
    memcpy(outState->alternateNetworkKey, state.alternateNetworkKey,
           sizeof(outState->alternateNetworkKey));
  }
  outState->incomingNwkFrameCounter = state.incomingNwkFrameCounter;
  outState->incomingApsFrameCounter = state.incomingApsFrameCounter;
  outState->trustCenterIeee = state.trustCenterIeee;
  outState->preconfiguredKeyMode =
      static_cast<uint8_t>(state.preconfiguredKeyMode);
  outState->flags = state.joined ? kPersistentFlagJoined : 0U;
  if (state.securityEnabled) {
    outState->flags |= kPersistentFlagSecurityEnabled;
  }
  if (state.haveActiveNetworkKey) {
    outState->flags |= kPersistentFlagActiveKeyPresent;
  }
  if (state.haveAlternateNetworkKey) {
    outState->flags |= kPersistentFlagAlternateKeyPresent;
  }
}

uint64_t ZigbeeCommissioning::expectedTrustCenterIeee(
    const ZigbeeEndDeviceCommonState& state) {
  return (state.trustCenterIeee != 0U) ? state.trustCenterIeee
                                       : state.policy.pinnedTrustCenterIeee;
}

uint32_t ZigbeeCommissioning::timeoutIndexToMs(uint8_t timeoutIndex) {
  return timeoutIndexToMsInternal(timeoutIndex);
}

bool ZigbeeCommissioning::shouldAttemptSecureRejoin(
    const ZigbeeEndDeviceCommonState& state) {
  return state.haveActiveNetworkKey && expectedTrustCenterIeee(state) != 0U &&
         state.panId != 0U && state.channel >= 11U && state.channel <= 26U &&
         (!state.policy.requireUniqueTrustCenterForRejoin ||
          isUniqueLinkKeyMode(state.preconfiguredKeyMode));
}

void ZigbeeCommissioning::requestSecureRejoin(
    ZigbeeEndDeviceCommonState* state) {
  if (state == nullptr) {
    return;
  }

  if (!shouldAttemptSecureRejoin(*state)) {
    clearEndDeviceState(state, false);
    state->lastFailure = ZigbeeCommissioningFailure::kSecureRejoinUnavailable;
    state->state = ZigbeeCommissioningState::kFailed;
    return;
  }

  state->joined = false;
  state->rejoinPending = true;
  state->securityEnabled = true;
  state->state = ZigbeeCommissioningState::kRejoinPending;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
}

bool ZigbeeCommissioning::shouldPollParent(
    const ZigbeeEndDeviceCommonState& state) {
  return state.joined ||
         state.state == ZigbeeCommissioningState::kWaitingTransportKey ||
         state.state == ZigbeeCommissioningState::kWaitingUpdateDevice;
}

bool ZigbeeCommissioning::shouldRequestEndDeviceTimeout(
    const ZigbeeEndDeviceCommonState& state) {
  return state.joined && state.securityEnabled && state.haveActiveNetworkKey &&
         state.endDeviceTimeoutPending;
}

void ZigbeeCommissioning::markEndDeviceTimeoutPending(
    ZigbeeEndDeviceCommonState* state) {
  if (state == nullptr) {
    return;
  }
  state->endDeviceTimeoutPending = state->joined;
  state->endDeviceTimeoutNegotiated = false;
  state->parentInformation = 0U;
  state->endDeviceTimeoutIndex = state->policy.requestedEndDeviceTimeout;
  state->endDeviceConfiguration = state->policy.endDeviceConfiguration;
  state->parentPollIntervalMs = defaultPollIntervalMs(*state);
}

bool ZigbeeCommissioning::acceptEndDeviceTimeoutResponse(
    const ZigbeeEndDeviceCommonState& state, const uint8_t* frame,
    uint8_t length, ZigbeeNwkEndDeviceTimeoutResponse* outResponse) {
  if (outResponse != nullptr) {
    memset(outResponse, 0, sizeof(*outResponse));
  }
  if (frame == nullptr || outResponse == nullptr ||
      !shouldRequestEndDeviceTimeout(state)) {
    return false;
  }
  return ZigbeeCodec::parseNwkEndDeviceTimeoutResponseCommand(frame, length,
                                                              outResponse) &&
         outResponse->valid;
}

void ZigbeeCommissioning::applyEndDeviceTimeoutResponse(
    ZigbeeEndDeviceCommonState* state,
    const ZigbeeNwkEndDeviceTimeoutResponse& response) {
  if (state == nullptr || !response.valid) {
    return;
  }

  if (response.status == kZigbeeNwkEndDeviceTimeoutSuccess) {
    state->endDeviceTimeoutPending = false;
    state->endDeviceTimeoutNegotiated = true;
    state->parentInformation = response.parentInformation;
    state->parentPollIntervalMs =
        negotiatedPollIntervalMs(*state, state->endDeviceTimeoutIndex);
  } else {
    state->endDeviceTimeoutPending = false;
    state->endDeviceTimeoutNegotiated = false;
    state->parentInformation = response.parentInformation;
    state->parentPollIntervalMs = defaultPollIntervalMs(*state);
  }
}

ZigbeeCommissioningAction ZigbeeCommissioning::nextAction(
    ZigbeeEndDeviceCommonState* state, uint32_t nowMs) {
  if (state == nullptr || state->joined) {
    return ZigbeeCommissioningAction::kNone;
  }

  const uint32_t elapsedSinceAttempt = nowMs - state->lastJoinAttemptMs;

  if (state->state == ZigbeeCommissioningState::kWaitingTransportKey) {
    if (elapsedSinceAttempt < state->policy.transportKeyTimeoutMs) {
      return ZigbeeCommissioningAction::kPollParent;
    }
    state->lastFailure = attemptBudgetExceeded(state->joinAttempts,
                                               state->policy.maxJoinAttempts)
                             ? ZigbeeCommissioningFailure::kJoinAttemptBudgetExceeded
                             : ZigbeeCommissioningFailure::kTransportKeyTimeout;
    state->state = ZigbeeCommissioningState::kFailed;
    state->lastJoinAttemptMs = nowMs;
    return ZigbeeCommissioningAction::kNone;
  }

  if (state->state == ZigbeeCommissioningState::kWaitingUpdateDevice) {
    if (elapsedSinceAttempt < state->policy.updateDeviceTimeoutMs) {
      return ZigbeeCommissioningAction::kPollParent;
    }
    state->lastFailure =
        attemptBudgetExceeded(state->rejoinAttempts, state->policy.maxRejoinAttempts)
            ? ZigbeeCommissioningFailure::kRejoinAttemptBudgetExceeded
            : ZigbeeCommissioningFailure::kUpdateDeviceTimeout;
    state->state = ZigbeeCommissioningState::kFailed;
    state->lastJoinAttemptMs = nowMs;
    if (state->policy.fallbackToJoinAfterRejoinFailure) {
      state->rejoinPending = false;
      state->state = ZigbeeCommissioningState::kRestored;
    }
    return ZigbeeCommissioningAction::kNone;
  }

  if (state->rejoinPending || state->state == ZigbeeCommissioningState::kRejoinPending) {
    if (!shouldAttemptSecureRejoin(*state)) {
      state->lastFailure = ZigbeeCommissioningFailure::kSecureRejoinUnavailable;
      state->state = ZigbeeCommissioningState::kFailed;
      state->lastJoinAttemptMs = nowMs;
      if (state->policy.fallbackToJoinAfterRejoinFailure) {
        state->rejoinPending = false;
        state->state = ZigbeeCommissioningState::kRestored;
      }
      return ZigbeeCommissioningAction::kNone;
    }
    if (attemptBudgetExceeded(state->rejoinAttempts,
                              state->policy.maxRejoinAttempts)) {
      state->lastFailure =
          ZigbeeCommissioningFailure::kRejoinAttemptBudgetExceeded;
      state->state = ZigbeeCommissioningState::kFailed;
      state->lastJoinAttemptMs = nowMs;
      if (state->policy.fallbackToJoinAfterRejoinFailure) {
        state->rejoinPending = false;
        state->state = ZigbeeCommissioningState::kRestored;
      }
      return ZigbeeCommissioningAction::kNone;
    }
    if (elapsedSinceAttempt >= state->policy.secureRejoinRetryDelayMs) {
      state->state = ZigbeeCommissioningState::kRejoinPending;
      return ZigbeeCommissioningAction::kSecureRejoin;
    }
    return ZigbeeCommissioningAction::kNone;
  }

  if (attemptBudgetExceeded(state->joinAttempts, state->policy.maxJoinAttempts)) {
    state->lastFailure = ZigbeeCommissioningFailure::kJoinAttemptBudgetExceeded;
    state->state = ZigbeeCommissioningState::kFailed;
    return ZigbeeCommissioningAction::kNone;
  }
  if (elapsedSinceAttempt >= state->policy.joinRetryDelayMs) {
    if (state->state == ZigbeeCommissioningState::kIdle ||
        state->state == ZigbeeCommissioningState::kRestored ||
        state->state == ZigbeeCommissioningState::kFailed) {
      return ZigbeeCommissioningAction::kJoin;
    }
  }
  return ZigbeeCommissioningAction::kNone;
}

bool ZigbeeCommissioning::activeScan(ZigbeeRadio& radio,
                                     uint8_t* ioMacSequence,
                                     ZigbeeEndDeviceCommonState* state,
                                     ZigbeeBeaconCandidate* outResult) {
  if (ioMacSequence == nullptr || state == nullptr || outResult == nullptr) {
    return false;
  }

  memset(outResult, 0, sizeof(*outResult));
  state->state = ZigbeeCommissioningState::kScanning;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildBeaconRequest((*ioMacSequence)++, request,
                                       &requestLength)) {
    state->lastFailure = ZigbeeCommissioningFailure::kAssociationRequestFailed;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }

  uint8_t channels[16] = {0U};
  uint8_t channelCount = 0U;
  buildScanChannelOrder(state->policy, channels, &channelCount);

  bool found = false;
  for (uint8_t i = 0U; i < channelCount; ++i) {
    const uint8_t channel = channels[i];
    if (!radio.setChannel(channel)) {
      continue;
    }
    (void)radio.transmit(request, requestLength, false, 1200000UL);

    const uint32_t deadline = millis() + 70U;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
      ZigbeeFrame frame{};
      if (!radio.receive(&frame, 4000U, 300000UL)) {
        continue;
      }

      ZigbeeMacBeaconView beacon{};
      if (!ZigbeeCodec::parseBeaconFrame(frame.psdu, frame.length, &beacon) ||
          !beacon.valid) {
        continue;
      }

      int16_t score = 0;
      if (!scoreBeacon(state->policy, channel, frame.rssiDbm, beacon, &score)) {
        continue;
      }

      ZigbeeBeaconCandidate candidate{};
      candidate.valid = true;
      candidate.channel = channel;
      candidate.rssiDbm = frame.rssiDbm;
      candidate.score = score;
      candidate.beacon = beacon;
      if (!found || shouldReplaceCandidate(*outResult, candidate)) {
        *outResult = candidate;
        found = true;
      }
    }
  }

  if (found) {
    radio.setChannel(outResult->channel);
    return true;
  }

  radio.setChannel(state->preferredChannel);
  state->lastFailure = ZigbeeCommissioningFailure::kNoCandidate;
  state->state = ZigbeeCommissioningState::kFailed;
  return false;
}

bool ZigbeeCommissioning::performJoin(ZigbeeRadio& radio,
                                      uint8_t* ioMacSequence,
                                      uint64_t localIeee,
                                      uint8_t capabilityInformation,
                                      ZigbeeEndDeviceCommonState* state) {
  if (ioMacSequence == nullptr || state == nullptr) {
    return false;
  }

  ++state->joinAttempts;
  state->lastJoinAttemptMs = millis();

  ZigbeeBeaconCandidate candidate{};
  if (!activeScan(radio, ioMacSequence, state, &candidate) || !candidate.valid) {
    return false;
  }

  state->channel = candidate.channel;
  state->panId = candidate.beacon.panId;
  state->parentShort = candidate.beacon.sourceShort;
  state->extendedPanId = candidate.beacon.network.extendedPanId;
  state->state = ZigbeeCommissioningState::kAssociating;

  uint16_t assignedShort = 0U;
  if (!attemptAssociationRequest(radio, ioMacSequence, state->panId,
                                 state->parentShort, localIeee,
                                 capabilityInformation, state,
                                 &assignedShort)) {
    return false;
  }

  state->localShort = assignedShort;
  state->joined = false;
  state->rejoinPending = false;
  state->securityEnabled = false;
  state->state = ZigbeeCommissioningState::kWaitingTransportKey;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
  return true;
}

bool ZigbeeCommissioning::performSecureRejoin(
    ZigbeeRadio& radio, uint8_t* ioMacSequence, uint64_t localIeee,
    uint8_t capabilityInformation, ZigbeeEndDeviceCommonState* state) {
  if (ioMacSequence == nullptr || state == nullptr) {
    return false;
  }
  if (!shouldAttemptSecureRejoin(*state)) {
    state->lastFailure = ZigbeeCommissioningFailure::kSecureRejoinUnavailable;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }
  if (!radio.setChannel(state->channel)) {
    state->lastFailure = ZigbeeCommissioningFailure::kAssociationRequestFailed;
    state->state = ZigbeeCommissioningState::kFailed;
    return false;
  }

  ++state->rejoinAttempts;
  state->lastJoinAttemptMs = millis();
  state->state = ZigbeeCommissioningState::kAssociating;

  if (attemptOrphanRecovery(radio, ioMacSequence, localIeee, state,
                            state->channel)) {
    return true;
  }

  if (attemptNwkRejoin(radio, ioMacSequence, localIeee, capabilityInformation,
                       state)) {
    return true;
  }

  uint16_t assignedShort = 0U;
  bool associated = attemptAssociationRequest(
      radio, ioMacSequence, state->panId, state->parentShort, localIeee,
      capabilityInformation, state, &assignedShort);
  if (!associated) {
    ZigbeeBeaconCandidate candidate{};
    if (!scanForKnownNetwork(radio, ioMacSequence, state, &candidate) ||
        !candidate.valid) {
      return false;
    }

    state->channel = candidate.channel;
    state->panId = candidate.beacon.panId;
    state->parentShort = candidate.beacon.sourceShort;
    state->extendedPanId = candidate.beacon.network.extendedPanId;
    if (attemptOrphanRecovery(radio, ioMacSequence, localIeee, state,
                              candidate.channel)) {
      return true;
    }
    if (attemptNwkRejoin(radio, ioMacSequence, localIeee,
                         capabilityInformation, state)) {
      return true;
    }
    state->state = ZigbeeCommissioningState::kAssociating;
    associated = attemptAssociationRequest(
        radio, ioMacSequence, state->panId, state->parentShort, localIeee,
        capabilityInformation, state, &assignedShort);
    if (!associated) {
      return false;
    }
  }

  state->localShort = assignedShort;
  state->joined = false;
  state->rejoinPending = true;
  state->securityEnabled = true;
  state->state = ZigbeeCommissioningState::kWaitingUpdateDevice;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
  return true;
}

bool ZigbeeCommissioning::acceptTransportKeyCommand(
    const ZigbeeEndDeviceCommonState& state, uint64_t localIeee,
    uint16_t sourceShort, uint64_t securedSourceIeee, bool nwkSecured,
    const uint8_t* frame, uint8_t length, const uint8_t installCodeKey[16],
    bool haveInstallCodeKey, ZigbeeTransportKeyInstallResult* outResult) {
  if (outResult != nullptr) {
    memset(outResult, 0, sizeof(*outResult));
  }
  if (frame == nullptr || outResult == nullptr ||
      !validTransportKeyLifecycle(state)) {
    return false;
  }

  ZigbeeApsSecurityHeader apsSecurity{};
  ZigbeeApsTransportKey transportKey{};
  ZigbeePreconfiguredKeyMode keyMode = ZigbeePreconfiguredKeyMode::kNone;
  uint8_t counter = 0U;
  bool transportKeyParsed = false;

  if (state.policy.allowInstallCodeKey && haveInstallCodeKey &&
      installCodeKey != nullptr &&
      ZigbeeSecurity::parseSecuredApsTransportKeyCommand(
          frame, length, installCodeKey, &transportKey, &apsSecurity,
          &counter)) {
    keyMode = ZigbeePreconfiguredKeyMode::kInstallCodeDerived;
    transportKeyParsed = true;
  }

  if (!transportKeyParsed && state.policy.allowWellKnownKey) {
    uint8_t linkKey[16] = {0U};
    if (ZigbeeSecurity::loadZigbeeAlliance09LinkKey(linkKey) &&
        ZigbeeSecurity::parseSecuredApsTransportKeyCommand(
            frame, length, linkKey, &transportKey, &apsSecurity, &counter)) {
      keyMode = ZigbeePreconfiguredKeyMode::kWellKnown;
      transportKeyParsed = true;
    }
  }

  if (!transportKeyParsed && !state.policy.requireEncryptedTransportKey &&
      ZigbeeCodec::parseApsTransportKeyCommand(frame, length, &transportKey,
                                               &counter)) {
    transportKeyParsed = true;
  }

  if (!transportKeyParsed) {
    return false;
  }

  const uint64_t expectedTc = expectedTrustCenterIeee(state);
  const uint64_t effectiveSourceIeee =
      apsSecurity.valid ? apsSecurity.sourceIeee : securedSourceIeee;
  const bool effectiveSecured = nwkSecured || apsSecurity.valid;
  if (!transportKey.valid ||
      transportKey.keyType != kZigbeeApsTransportKeyStandardNetworkKey ||
      transportKey.destinationIeee != localIeee ||
      (expectedTc != 0U && transportKey.sourceIeee != expectedTc) ||
      !validTrustCenterSource(state, sourceShort, effectiveSourceIeee,
                              effectiveSecured) ||
      (!apsSecurity.valid && state.policy.requireEncryptedTransportKey) ||
      (apsSecurity.valid &&
       (apsSecurity.sourceIeee != transportKey.sourceIeee ||
        apsSecurity.frameCounter <= state.incomingApsFrameCounter)) ||
      (!apsSecurity.valid && effectiveSourceIeee != 0U &&
       effectiveSourceIeee != transportKey.sourceIeee) ||
      (state.policy.installCodeOnly &&
       keyMode != ZigbeePreconfiguredKeyMode::kInstallCodeDerived)) {
    return false;
  }

  bool activatesNetworkKey = false;
  bool stagesAlternateKey = false;
  bool refreshesActiveNetworkKey = false;
  bool refreshesAlternateKey = false;
  if (!state.haveActiveNetworkKey) {
    activatesNetworkKey = true;
  } else if (transportKey.keySequence == state.activeNetworkKeySequence) {
    if (memcmp(transportKey.key, state.activeNetworkKey,
               sizeof(state.activeNetworkKey)) != 0) {
      return false;
    }
    refreshesActiveNetworkKey = true;
  } else if (state.haveAlternateNetworkKey &&
             transportKey.keySequence == state.alternateNetworkKeySequence) {
    if (memcmp(transportKey.key, state.alternateNetworkKey,
               sizeof(state.alternateNetworkKey)) != 0) {
      return false;
    }
    refreshesAlternateKey = true;
  } else if (!keySequenceIsNewer(transportKey.keySequence,
                                 state.activeNetworkKeySequence) ||
             (state.haveAlternateNetworkKey &&
              !keySequenceIsNewer(transportKey.keySequence,
                                  state.alternateNetworkKeySequence))) {
    return false;
  } else {
    stagesAlternateKey = true;
  }

  outResult->valid = true;
  outResult->transportKey = transportKey;
  outResult->apsSecurity = apsSecurity;
  outResult->keyMode = keyMode;
  outResult->counter = counter;
  outResult->activatesNetworkKey = activatesNetworkKey;
  outResult->stagesAlternateKey = stagesAlternateKey;
  outResult->refreshesActiveNetworkKey = refreshesActiveNetworkKey;
  outResult->refreshesAlternateKey = refreshesAlternateKey;
  return true;
}

void ZigbeeCommissioning::applyTransportKeyInstall(
    ZigbeeEndDeviceCommonState* state,
    const ZigbeeTransportKeyInstallResult& result) {
  if (state == nullptr || !result.valid) {
    return;
  }

  if (state->trustCenterIeee == 0U) {
    state->trustCenterIeee = result.transportKey.sourceIeee;
  }
  if (result.apsSecurity.valid) {
    state->incomingApsFrameCounter = result.apsSecurity.frameCounter;
  }
  state->preconfiguredKeyMode = result.keyMode;
  if (result.stagesAlternateKey) {
    memcpy(state->alternateNetworkKey, result.transportKey.key,
           sizeof(state->alternateNetworkKey));
    state->alternateNetworkKeySequence = result.transportKey.keySequence;
    state->haveAlternateNetworkKey = true;
  } else if (result.activatesNetworkKey) {
    memcpy(state->activeNetworkKey, result.transportKey.key,
           sizeof(state->activeNetworkKey));
    state->activeNetworkKeySequence = result.transportKey.keySequence;
    state->haveActiveNetworkKey = true;
    clearAlternateNetworkKey(state);
  }
  state->securityEnabled = true;
  if (result.activatesNetworkKey) {
    state->incomingNwkFrameCounter = 0U;
    state->joined = true;
    state->rejoinPending = false;
    markEndDeviceTimeoutPending(state);
    state->state = ZigbeeCommissioningState::kJoined;
  }
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
}

bool ZigbeeCommissioning::acceptUpdateDeviceCommand(
    const ZigbeeEndDeviceCommonState& state, uint64_t localIeee,
    uint16_t sourceShort, uint64_t securedSourceIeee, bool nwkSecured,
    bool allowPlaintext, const uint8_t* frame, uint8_t length,
    const uint8_t installCodeKey[16], bool haveInstallCodeKey,
    ZigbeeUpdateDeviceAcceptance* outResult) {
  if (outResult != nullptr) {
    memset(outResult, 0, sizeof(*outResult));
  }
  if (frame == nullptr || outResult == nullptr ||
      (!state.rejoinPending &&
       state.state != ZigbeeCommissioningState::kWaitingUpdateDevice)) {
    return false;
  }

  ZigbeeApsUpdateDevice updateDevice{};
  ZigbeeApsSecurityHeader apsSecurity{};
  uint8_t counter = 0U;
  bool parsed = tryParseSecuredUpdateDeviceCommand(
      state, frame, length, installCodeKey, haveInstallCodeKey, &updateDevice,
      &apsSecurity, &counter);
  if (!parsed && !state.policy.requireEncryptedUpdateDevice && allowPlaintext) {
    parsed = ZigbeeCodec::parseApsUpdateDeviceCommand(frame, length,
                                                      &updateDevice, &counter);
  }
  const uint64_t effectiveSourceIeee =
      apsSecurity.valid ? apsSecurity.sourceIeee : securedSourceIeee;
  const bool effectiveSecured = nwkSecured || apsSecurity.valid;
  if (!parsed || (!allowPlaintext && !effectiveSecured) ||
      !validTrustCenterSource(state, sourceShort, effectiveSourceIeee,
                              effectiveSecured) ||
      (!apsSecurity.valid && state.policy.requireEncryptedUpdateDevice) ||
      (apsSecurity.valid &&
       apsSecurity.frameCounter <= state.incomingApsFrameCounter) ||
      !updateDevice.valid || updateDevice.deviceIeee != localIeee ||
      updateDevice.status !=
          kZigbeeApsUpdateDeviceStatusStandardSecureRejoin) {
    return false;
  }

  outResult->valid = true;
  outResult->updateDevice = updateDevice;
  outResult->apsSecurity = apsSecurity;
  outResult->counter = counter;
  return true;
}

void ZigbeeCommissioning::applyUpdateDevice(
    ZigbeeEndDeviceCommonState* state,
    const ZigbeeUpdateDeviceAcceptance& result) {
  if (state == nullptr || !result.valid) {
    return;
  }

  if (result.updateDevice.deviceShort != 0U &&
      result.updateDevice.deviceShort != 0xFFFFU) {
    state->localShort = result.updateDevice.deviceShort;
  }
  if (result.apsSecurity.valid) {
    state->incomingApsFrameCounter = result.apsSecurity.frameCounter;
  }
  if (result.updateDevice.status ==
      kZigbeeApsUpdateDeviceStatusStandardSecureRejoin) {
    state->joined = true;
    state->rejoinPending = false;
    state->securityEnabled = state->haveActiveNetworkKey;
    markEndDeviceTimeoutPending(state);
    state->state = ZigbeeCommissioningState::kJoined;
    state->lastFailure = ZigbeeCommissioningFailure::kNone;
  }
}

bool ZigbeeCommissioning::acceptSwitchKeyCommand(
    const ZigbeeEndDeviceCommonState& state, uint16_t sourceShort,
    uint64_t securedSourceIeee, bool nwkSecured, bool allowPlaintext,
    const uint8_t* frame, uint8_t length, const uint8_t installCodeKey[16],
    bool haveInstallCodeKey,
    ZigbeeSwitchKeyAcceptance* outResult) {
  if (outResult != nullptr) {
    memset(outResult, 0, sizeof(*outResult));
  }
  if (frame == nullptr || outResult == nullptr ||
      !state.securityEnabled) {
    return false;
  }

  ZigbeeApsSwitchKey switchKey{};
  ZigbeeApsSecurityHeader apsSecurity{};
  uint8_t counter = 0U;
  bool parsed = tryParseSecuredSwitchKeyCommand(
      state, frame, length, installCodeKey, haveInstallCodeKey, &switchKey,
      &apsSecurity, &counter);
  if (!parsed && !state.policy.requireEncryptedSwitchKey && allowPlaintext) {
    parsed = ZigbeeCodec::parseApsSwitchKeyCommand(frame, length, &switchKey,
                                                   &counter);
  }
  const uint64_t effectiveSourceIeee =
      apsSecurity.valid ? apsSecurity.sourceIeee : securedSourceIeee;
  const bool effectiveSecured = nwkSecured || apsSecurity.valid;
  if (!parsed || (!allowPlaintext && !effectiveSecured) ||
      !validTrustCenterSource(state, sourceShort, effectiveSourceIeee,
                              effectiveSecured) ||
      (!apsSecurity.valid && state.policy.requireEncryptedSwitchKey) ||
      (apsSecurity.valid &&
       apsSecurity.frameCounter <= state.incomingApsFrameCounter) ||
      !switchKey.valid || !state.haveAlternateNetworkKey ||
      !state.haveActiveNetworkKey ||
      switchKey.keySequence != state.alternateNetworkKeySequence) {
    return false;
  }

  outResult->valid = true;
  outResult->switchKey = switchKey;
  outResult->apsSecurity = apsSecurity;
  outResult->counter = counter;
  return true;
}

void ZigbeeCommissioning::applySwitchKey(
    ZigbeeEndDeviceCommonState* state,
    const ZigbeeSwitchKeyAcceptance& result) {
  if (state == nullptr || !result.valid || !state->haveAlternateNetworkKey ||
      result.switchKey.keySequence != state->alternateNetworkKeySequence) {
    return;
  }

  memcpy(state->activeNetworkKey, state->alternateNetworkKey,
         sizeof(state->activeNetworkKey));
  state->activeNetworkKeySequence = state->alternateNetworkKeySequence;
  state->haveActiveNetworkKey = true;
  clearAlternateNetworkKey(state);
  if (result.apsSecurity.valid) {
    state->incomingApsFrameCounter = result.apsSecurity.frameCounter;
  }
  state->nwkSecurityFrameCounter = 1U;
  state->incomingNwkFrameCounter = 0U;
  state->securityEnabled = true;
  state->joined = true;
  state->rejoinPending = false;
  state->state = ZigbeeCommissioningState::kJoined;
  state->lastFailure = ZigbeeCommissioningFailure::kNone;
}

bool ZigbeeCommissioning::isUniqueLinkKeyMode(
    ZigbeePreconfiguredKeyMode mode) {
  return mode == ZigbeePreconfiguredKeyMode::kInstallCodeDerived;
}

const char* ZigbeeCommissioning::keyModeName(ZigbeePreconfiguredKeyMode mode) {
  switch (mode) {
    case ZigbeePreconfiguredKeyMode::kWellKnown:
      return "well_known";
    case ZigbeePreconfiguredKeyMode::kInstallCodeDerived:
      return "install_code";
    case ZigbeePreconfiguredKeyMode::kNone:
    default:
      return "none";
  }
}

const char* ZigbeeCommissioning::stateName(ZigbeeCommissioningState state) {
  switch (state) {
    case ZigbeeCommissioningState::kRestored:
      return "restored";
    case ZigbeeCommissioningState::kScanning:
      return "scanning";
    case ZigbeeCommissioningState::kAssociating:
      return "associating";
    case ZigbeeCommissioningState::kWaitingTransportKey:
      return "waiting_transport_key";
    case ZigbeeCommissioningState::kRejoinPending:
      return "rejoin_pending";
    case ZigbeeCommissioningState::kWaitingUpdateDevice:
      return "waiting_update_device";
    case ZigbeeCommissioningState::kJoined:
      return "joined";
    case ZigbeeCommissioningState::kFailed:
      return "failed";
    case ZigbeeCommissioningState::kIdle:
    default:
      return "idle";
  }
}

const char* ZigbeeCommissioning::failureName(
    ZigbeeCommissioningFailure failure) {
  switch (failure) {
    case ZigbeeCommissioningFailure::kNoCandidate:
      return "no_candidate";
    case ZigbeeCommissioningFailure::kAssociationRequestFailed:
      return "association_request_failed";
    case ZigbeeCommissioningFailure::kAssociationTimeout:
      return "association_timeout";
    case ZigbeeCommissioningFailure::kSecureRejoinUnavailable:
      return "secure_rejoin_unavailable";
    case ZigbeeCommissioningFailure::kTransportKeyRejected:
      return "transport_key_rejected";
    case ZigbeeCommissioningFailure::kUpdateDeviceRejected:
      return "update_device_rejected";
    case ZigbeeCommissioningFailure::kSwitchKeyRejected:
      return "switch_key_rejected";
    case ZigbeeCommissioningFailure::kTransportKeyTimeout:
      return "transport_key_timeout";
    case ZigbeeCommissioningFailure::kUpdateDeviceTimeout:
      return "update_device_timeout";
    case ZigbeeCommissioningFailure::kJoinAttemptBudgetExceeded:
      return "join_attempt_budget_exceeded";
    case ZigbeeCommissioningFailure::kRejoinAttemptBudgetExceeded:
      return "rejoin_attempt_budget_exceeded";
    case ZigbeeCommissioningFailure::kNone:
    default:
      return "none";
  }
}

}  // namespace xiao_nrf54l15
