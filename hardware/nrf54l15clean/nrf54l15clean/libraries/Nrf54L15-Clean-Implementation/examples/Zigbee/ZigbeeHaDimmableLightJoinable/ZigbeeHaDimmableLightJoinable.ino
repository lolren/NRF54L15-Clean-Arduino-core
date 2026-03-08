#include <Arduino.h>
#include <string.h>

#include "nrf54l15_hal.h"
#include "zigbee_persistence.h"
#include "zigbee_security.h"
#include "zigbee_stack.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeHaDimmableLightJoinable."
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_CHANNEL
#define NRF54L15_CLEAN_ZIGBEE_CHANNEL 15
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PAN_ID
#define NRF54L15_CLEAN_ZIGBEE_PAN_ID 0x1234
#endif

using namespace xiao_nrf54l15;

namespace {

static ZigbeeRadio g_radio;
static ZigbeeHomeAutomationDevice g_device;
static ZigbeePersistentStateStore g_store;
static Pwm g_pwm20(nrf54l15::PWM20_BASE);

static uint8_t g_macSequence = 1U;
static uint8_t g_nwkSequence = 1U;
static uint32_t g_nwkSecurityFrameCounter = 1U;
static uint32_t g_lastInboundSecurityFrameCounter = 0U;
static uint8_t g_apsCounter = 1U;
static uint8_t g_zclSequence = 1U;
static uint8_t g_zdoSequence = 1U;
static uint32_t g_lastJoinAttemptMs = 0U;
static uint32_t g_lastStatusMs = 0U;
static uint32_t g_lastPollMs = 0U;
static uint32_t g_lastReportMs[8] = {0U};
static uint8_t g_activeNetworkKey[16] = {0U};
static uint8_t g_activeNetworkKeySequence = 0U;
static bool g_joined = false;
static bool g_securityEnabled = false;
static bool g_haveActiveNetworkKey = false;
static bool g_savedOnOffState = false;
static uint8_t g_savedLevelState = 0x80U;
static bool g_pwmReady = false;

static ZigbeePersistentState g_restoredState{};
static bool g_haveRestoredState = false;

static constexpr uint8_t kPreferredChannel =
    static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPreferredPanId =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kCoordinatorShort = 0x0000U;
static constexpr uint16_t kLightShort = 0x7E02U;
static constexpr uint8_t kLocalEndpoint = 1U;
static constexpr uint8_t kCoordinatorEndpoint = 1U;
static constexpr uint64_t kCoordinatorIeee = 0x00124B000054A11FULL;
static constexpr uint64_t kIeeeAddress = 0x00124B0001AC1002ULL;
static constexpr uint8_t kStateFlagJoined = 0x01U;
static constexpr uint8_t kStateFlagSecurityEnabled = 0x02U;

static uint8_t g_channel = kPreferredChannel;
static uint16_t g_panId = kPreferredPanId;
static uint16_t g_localShort = kLightShort;
static uint16_t g_parentShort = kCoordinatorShort;
static uint64_t g_extendedPanId = 0U;

struct ScanResult {
  bool valid = false;
  uint8_t channel = 0U;
  int8_t rssiDbm = -127;
  ZigbeeMacBeaconView beacon{};
};

void applyDefaultReporting() {
  g_device.configureReporting(kZigbeeClusterOnOff, 0x0000U,
                              ZigbeeZclDataType::kBoolean, 0U, 30U, 0U);
  g_device.configureReporting(kZigbeeClusterLevelControl, 0x0000U,
                              ZigbeeZclDataType::kUint8, 0U, 30U, 16U);
}

void applyReportingState() {
  if (!g_haveRestoredState || g_restoredState.reportingCount == 0U) {
    applyDefaultReporting();
    return;
  }

  for (uint8_t i = 0U; i < g_restoredState.reportingCount && i < 8U; ++i) {
    if (!g_restoredState.reporting[i].used) {
      continue;
    }
    g_device.configureReporting(g_restoredState.reporting[i].clusterId,
                                g_restoredState.reporting[i].attributeId,
                                g_restoredState.reporting[i].dataType,
                                g_restoredState.reporting[i].minimumIntervalSeconds,
                                g_restoredState.reporting[i].maximumIntervalSeconds,
                                g_restoredState.reporting[i].reportableChange);
  }
}

void applyBindingState() {
  if (!g_haveRestoredState || g_restoredState.bindingCount == 0U) {
    return;
  }

  for (uint8_t i = 0U; i < g_restoredState.bindingCount && i < 8U; ++i) {
    if (!g_restoredState.bindings[i].used) {
      continue;
    }
    (void)g_device.addBinding(g_restoredState.bindings[i].sourceEndpoint,
                              g_restoredState.bindings[i].clusterId,
                              g_restoredState.bindings[i].destinationAddressMode,
                              g_restoredState.bindings[i].destinationGroup,
                              g_restoredState.bindings[i].destinationIeee,
                              g_restoredState.bindings[i].destinationEndpoint);
  }
}

void applyLedState() {
  uint16_t dutyPermille = 0U;
  if (g_device.onOff()) {
    const uint16_t level = g_device.level();
    dutyPermille =
        static_cast<uint16_t>((static_cast<uint32_t>(level) * 1000UL) / 0xFEUL);
    if (dutyPermille == 0U && level > 0U) {
      dutyPermille = 1U;
    }
  }

  if (g_pwmReady) {
    (void)g_pwm20.setDutyPermille(dutyPermille);
    return;
  }

  (void)Gpio::write(kPinUserLed, dutyPermille == 0U);
}

void clearActiveNetworkKey() {
  memset(g_activeNetworkKey, 0, sizeof(g_activeNetworkKey));
  g_activeNetworkKeySequence = 0U;
  g_haveActiveNetworkKey = false;
}

void configureDeviceForCurrentNetwork() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-JOIN-DIM";
  basic.swBuildId = "0.3.0";
  basic.powerSource = 0x01U;
  g_device.configureDimmableLight(kLocalEndpoint, kIeeeAddress, g_localShort,
                                  g_panId, basic, 0x0000U);
  applyReportingState();
  applyBindingState();
  (void)g_device.setLevel(g_savedLevelState);
  (void)g_device.setOnOff(g_savedOnOffState);
}

bool persistState() {
  ZigbeePersistentState state{};
  ZigbeePersistentStateStore::initialize(&state);
  state.channel = g_channel;
  state.logicalType = static_cast<uint8_t>(ZigbeeLogicalType::kEndDevice);
  state.panId = g_panId;
  state.nwkAddress = g_localShort;
  state.parentShort = g_parentShort;
  state.manufacturerCode = g_device.config().manufacturerCode;
  state.ieeeAddress = kIeeeAddress;
  state.extendedPanId = g_extendedPanId;
  state.nwkFrameCounter = g_nwkSecurityFrameCounter;
  state.apsFrameCounter = g_apsCounter;
  state.keySequence = g_haveActiveNetworkKey ? g_activeNetworkKeySequence : 0U;
  if (g_haveActiveNetworkKey) {
    memcpy(state.networkKey, g_activeNetworkKey, sizeof(state.networkKey));
  }
  state.incomingNwkFrameCounter = g_lastInboundSecurityFrameCounter;
  state.flags = g_joined ? kStateFlagJoined : 0U;
  if (g_securityEnabled) {
    state.flags |= kStateFlagSecurityEnabled;
  }
  state.onOffState = g_device.onOff();
  state.levelState = g_device.level();

  const ZigbeeReportingConfiguration* reporting = g_device.reportingConfigurations();
  uint8_t copied = 0U;
  for (uint8_t i = 0U; i < 8U && copied < 8U; ++i) {
    if (!reporting[i].used) {
      continue;
    }
    state.reporting[copied++] = reporting[i];
  }
  state.reportingCount = copied;
  const ZigbeeBindingEntry* bindings = g_device.bindings();
  copied = 0U;
  for (uint8_t i = 0U; i < 8U && copied < 8U; ++i) {
    if (!bindings[i].used) {
      continue;
    }
    state.bindings[copied++] = bindings[i];
  }
  state.bindingCount = copied;
  return g_store.save(state);
}

void restoreState() {
  memset(&g_restoredState, 0, sizeof(g_restoredState));
  g_haveRestoredState = false;
  g_savedOnOffState = false;
  g_savedLevelState = 0x80U;
  g_joined = false;
  g_securityEnabled = false;
  g_lastInboundSecurityFrameCounter = 0U;
  clearActiveNetworkKey();
  g_channel = kPreferredChannel;
  g_panId = kPreferredPanId;
  g_localShort = kLightShort;
  g_parentShort = kCoordinatorShort;
  g_extendedPanId = 0U;
  g_nwkSecurityFrameCounter = 1U;

  ZigbeePersistentState state{};
  if (g_store.load(&state) && state.ieeeAddress == kIeeeAddress) {
    g_restoredState = state;
    g_haveRestoredState = true;
    g_savedOnOffState = state.onOffState;
    g_savedLevelState = state.levelState;
    if (g_savedLevelState == 0U && state.onOffState) {
      g_savedLevelState = 0x80U;
    }
    g_channel = (state.channel >= 11U && state.channel <= 26U) ? state.channel
                                                                : kPreferredChannel;
    g_panId = (state.panId != 0U) ? state.panId : kPreferredPanId;
    g_parentShort = (state.parentShort != 0U) ? state.parentShort
                                               : kCoordinatorShort;
    g_extendedPanId = state.extendedPanId;
    g_nwkSequence = 1U;
    g_nwkSecurityFrameCounter =
        (state.nwkFrameCounter != 0U) ? state.nwkFrameCounter : 1U;
    g_lastInboundSecurityFrameCounter = state.incomingNwkFrameCounter;
    g_apsCounter = static_cast<uint8_t>(state.apsFrameCounter & 0xFFU);
    g_activeNetworkKeySequence = state.keySequence;
    if (state.keySequence != 0U) {
      memcpy(g_activeNetworkKey, state.networkKey, sizeof(g_activeNetworkKey));
      g_haveActiveNetworkKey = true;
    }
    g_securityEnabled = (state.flags & kStateFlagSecurityEnabled) != 0U;
    if ((state.flags & kStateFlagJoined) != 0U && state.nwkAddress != 0U &&
        state.nwkAddress != 0xFFFFU) {
      g_localShort = state.nwkAddress;
      g_joined = true;
    }
  }

  configureDeviceForCurrentNetwork();
  applyLedState();
}

void clearJoinState(bool clearStore) {
  g_joined = false;
  g_securityEnabled = false;
  g_lastInboundSecurityFrameCounter = 0U;
  clearActiveNetworkKey();
  g_channel = kPreferredChannel;
  g_panId = kPreferredPanId;
  g_localShort = kLightShort;
  g_parentShort = kCoordinatorShort;
  g_extendedPanId = 0U;
  if (clearStore) {
    memset(&g_restoredState, 0, sizeof(g_restoredState));
    g_haveRestoredState = false;
    g_savedOnOffState = false;
    g_savedLevelState = 0x80U;
  }
  configureDeviceForCurrentNetwork();
  applyLedState();
  if (clearStore) {
    g_store.clear();
  } else {
    persistState();
  }
}

bool sendApsFrame(uint16_t destinationShort, uint8_t destinationEndpoint,
                  uint16_t clusterId, uint16_t profileId, uint8_t sourceEndpoint,
                  const uint8_t* payload, uint8_t payloadLength) {
  if (!g_joined) {
    return false;
  }
  const bool useSecurity = g_securityEnabled && g_haveActiveNetworkKey;

  ZigbeeApsDataFrame aps{};
  aps.frameType = ZigbeeApsFrameType::kData;
  aps.destinationEndpoint = destinationEndpoint;
  aps.clusterId = clusterId;
  aps.profileId = profileId;
  aps.sourceEndpoint = sourceEndpoint;
  aps.counter = g_apsCounter++;

  uint8_t apsFrame[127] = {0U};
  uint8_t apsLength = 0U;
  if (!ZigbeeCodec::buildApsDataFrame(aps, payload, payloadLength, apsFrame,
                                      &apsLength)) {
    return false;
  }

  ZigbeeNetworkFrame nwk{};
  nwk.frameType = ZigbeeNwkFrameType::kData;
  nwk.securityEnabled = useSecurity;
  nwk.destinationShort = destinationShort;
  nwk.sourceShort = g_localShort;
  nwk.radius = 30U;
  nwk.sequence = g_nwkSequence++;

  uint8_t nwkFrame[127] = {0U};
  uint8_t nwkLength = 0U;
  if (useSecurity) {
    ZigbeeNwkSecurityHeader security{};
    security.valid = true;
    security.securityControl = kZigbeeSecurityControlNwkEncMic32;
    security.frameCounter = g_nwkSecurityFrameCounter++;
    security.sourceIeee = kIeeeAddress;
    security.keySequence = g_activeNetworkKeySequence;
    if (!ZigbeeSecurity::buildSecuredNwkFrame(nwk, security, g_activeNetworkKey,
                                              apsFrame, apsLength, nwkFrame,
                                              &nwkLength)) {
      return false;
    }
  } else if (!ZigbeeCodec::buildNwkFrame(nwk, apsFrame, apsLength, nwkFrame,
                                         &nwkLength)) {
    return false;
  }

  uint8_t psdu[127] = {0U};
  uint8_t psduLength = 0U;
  if (!ZigbeeRadio::buildDataFrameShort(g_macSequence++, g_panId,
                                        destinationShort, g_localShort, nwkFrame,
                                        nwkLength, psdu, &psduLength, false)) {
    return false;
  }

  return g_radio.transmit(psdu, psduLength, false, 1200000UL);
}

bool sendDeviceAnnounce() {
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!g_device.buildDeviceAnnounce(g_zdoSequence++, payload, &payloadLength)) {
    return false;
  }
  return sendApsFrame(g_parentShort, 0U, kZigbeeZdoDeviceAnnounce,
                      kZigbeeProfileZdo, 0U, payload, payloadLength);
}

bool sendAttributeReport(uint16_t clusterId) {
  uint8_t zclFrame[127] = {0U};
  uint8_t zclLength = 0U;
  if (!g_device.buildAttributeReport(clusterId, g_zclSequence++, zclFrame,
                                     &zclLength) ||
      zclLength == 0U) {
    return false;
  }

  uint64_t destinationIeee = 0U;
  uint8_t destinationEndpoint = kCoordinatorEndpoint;
  if (g_device.resolveBindingDestination(kLocalEndpoint, clusterId,
                                         &destinationIeee,
                                         &destinationEndpoint) &&
      destinationEndpoint != 0U) {
    return sendApsFrame(g_parentShort, destinationEndpoint, clusterId,
                        kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                        zclLength);
  }
  return sendApsFrame(g_parentShort, kCoordinatorEndpoint, clusterId,
                      kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                      zclLength);
}

bool handleApsCommand(const uint8_t* frame, uint8_t length) {
  uint8_t counter = 0U;
  uint8_t linkKey[16] = {0U};
  (void)ZigbeeSecurity::loadZigbeeAlliance09LinkKey(linkKey);
  ZigbeeApsSecurityHeader apsSecurity{};
  ZigbeeApsTransportKey transportKey{};
  if (ZigbeeSecurity::parseSecuredApsTransportKeyCommand(
          frame, length, linkKey, &transportKey, &apsSecurity, &counter) ||
      ZigbeeCodec::parseApsTransportKeyCommand(frame, length, &transportKey,
                                               &counter)) {
    if (!transportKey.valid ||
        transportKey.keyType != kZigbeeApsTransportKeyStandardNetworkKey ||
        transportKey.destinationIeee != kIeeeAddress ||
        transportKey.sourceIeee != kCoordinatorIeee) {
      return false;
    }
    memcpy(g_activeNetworkKey, transportKey.key, sizeof(g_activeNetworkKey));
    g_activeNetworkKeySequence = transportKey.keySequence;
    g_haveActiveNetworkKey = true;
    g_securityEnabled = true;
    g_lastInboundSecurityFrameCounter = 0U;
    persistState();
    Serial.print("transport_key seq=");
    Serial.print(g_activeNetworkKeySequence);
    Serial.print(" ctr=");
    Serial.print(counter);
    if (apsSecurity.valid) {
      Serial.print(" aps_sec_fc=");
      Serial.print(apsSecurity.frameCounter);
    }
    Serial.print("\r\n");
    (void)sendDeviceAnnounce();
    return true;
  }

  ZigbeeApsUpdateDevice updateDevice{};
  if (ZigbeeCodec::parseApsUpdateDeviceCommand(frame, length, &updateDevice,
                                               &counter)) {
    if (updateDevice.valid && updateDevice.deviceIeee == kIeeeAddress) {
      Serial.print("update_device short=0x");
      Serial.print(updateDevice.deviceShort, HEX);
      Serial.print(" status=0x");
      Serial.print(updateDevice.status, HEX);
      Serial.print("\r\n");
    }
    return true;
  }

  return false;
}

bool activeScan(ScanResult* outResult) {
  if (outResult == nullptr) {
    return false;
  }
  memset(outResult, 0, sizeof(*outResult));

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildBeaconRequest(g_macSequence++, request, &requestLength)) {
    return false;
  }

  bool found = false;
  for (uint8_t channel = 11U; channel <= 26U; ++channel) {
    if (!g_radio.setChannel(channel)) {
      continue;
    }
    (void)g_radio.transmit(request, requestLength, false, 1200000UL);

    const uint32_t deadline = millis() + 70U;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
      ZigbeeFrame frame{};
      if (!g_radio.receive(&frame, 4000U, 300000UL)) {
        continue;
      }

      ZigbeeMacBeaconView beacon{};
      if (!ZigbeeCodec::parseBeaconFrame(frame.psdu, frame.length, &beacon) ||
          !beacon.valid || !beacon.associationPermit) {
        continue;
      }

      if (!found || frame.rssiDbm > outResult->rssiDbm) {
        outResult->valid = true;
        outResult->channel = channel;
        outResult->rssiDbm = frame.rssiDbm;
        outResult->beacon = beacon;
        found = true;
      }
    }
  }

  if (found) {
    g_radio.setChannel(outResult->channel);
  } else {
    g_radio.setChannel(kPreferredChannel);
  }
  return found;
}

bool waitForAssociationResponse(uint16_t* outAssignedShort) {
  if (outAssignedShort == nullptr) {
    return false;
  }

  const uint32_t deadline = millis() + 1500U;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    uint8_t pollFrame[127] = {0U};
    uint8_t pollLength = 0U;
    if (!ZigbeeCodec::buildDataRequest(g_macSequence++, g_panId, g_parentShort,
                                       kIeeeAddress, pollFrame, &pollLength)) {
      return false;
    }
    (void)g_radio.transmit(pollFrame, pollLength, false, 1200000UL);

    const uint32_t listenDeadline = millis() + 90U;
    while (static_cast<int32_t>(millis() - listenDeadline) < 0) {
      ZigbeeFrame frame{};
      if (!g_radio.receive(&frame, 5000U, 350000UL)) {
        continue;
      }

      ZigbeeMacAssociationResponseView response{};
      if (!ZigbeeCodec::parseAssociationResponse(frame.psdu, frame.length,
                                                 &response) ||
          !response.valid ||
          response.destinationExtended != kIeeeAddress ||
          response.panId != g_panId || response.status != 0x00U) {
        continue;
      }

      *outAssignedShort = response.assignedShort;
      return true;
    }

    delay(25);
  }

  return false;
}

bool performJoin() {
  ScanResult result{};
  if (!activeScan(&result) || !result.valid) {
    return false;
  }

  g_channel = result.channel;
  g_panId = result.beacon.panId;
  g_parentShort = result.beacon.sourceShort;
  g_extendedPanId = result.beacon.network.extendedPanId;

  const uint8_t capabilityInformation = 0xCCU;
  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildAssociationRequest(g_macSequence++, g_panId,
                                            g_parentShort, kIeeeAddress,
                                            capabilityInformation, request,
                                            &requestLength) ||
      !g_radio.transmit(request, requestLength, false, 1200000UL)) {
    return false;
  }

  uint16_t assignedShort = 0U;
  if (!waitForAssociationResponse(&assignedShort)) {
    return false;
  }

  g_localShort = assignedShort;
  g_joined = true;
  g_securityEnabled = false;
  configureDeviceForCurrentNetwork();
  applyLedState();
  persistState();
  return true;
}

void processIncomingFrame(const ZigbeeFrame& frame) {
  ZigbeeDataFrameView macData{};
  if (!ZigbeeRadio::parseDataFrameShort(frame.psdu, frame.length, &macData) ||
      !macData.valid || macData.panId != g_panId ||
      macData.destinationShort != g_localShort) {
    return;
  }

  ZigbeeNetworkFrame nwk{};
  ZigbeeNwkSecurityHeader security{};
  uint8_t decryptedPayload[127] = {0U};
  uint8_t decryptedPayloadLength = 0U;
  bool nwkValid = false;
  if (g_securityEnabled && g_haveActiveNetworkKey) {
    nwkValid = ZigbeeSecurity::parseSecuredNwkFrame(
        macData.payload, macData.payloadLength, g_activeNetworkKey, &nwk,
        &security, decryptedPayload, &decryptedPayloadLength);
    if (nwkValid &&
        (!security.valid || security.sourceIeee != kCoordinatorIeee ||
         security.keySequence != g_activeNetworkKeySequence ||
         security.frameCounter <= g_lastInboundSecurityFrameCounter)) {
      return;
    }
  }
  if (!nwkValid) {
    nwkValid =
        ZigbeeCodec::parseNwkFrame(macData.payload, macData.payloadLength, &nwk);
  }
  if (!nwkValid || !nwk.valid || nwk.destinationShort != g_localShort) {
    return;
  }
  if (security.valid) {
    g_lastInboundSecurityFrameCounter = security.frameCounter;
  }

  ZigbeeApsDataFrame aps{};
  if (!ZigbeeCodec::parseApsDataFrame(nwk.payload, nwk.payloadLength, &aps) ||
      !aps.valid) {
    (void)handleApsCommand(nwk.payload, nwk.payloadLength);
    return;
  }

  if (aps.profileId == kZigbeeProfileZdo) {
    uint16_t responseClusterId = 0U;
    uint8_t responsePayload[127] = {0U};
    uint8_t responseLength = 0U;
    if (g_device.handleZdoRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                  &responseClusterId, responsePayload,
                                  &responseLength) &&
        responseLength > 0U) {
      (void)sendApsFrame(nwk.sourceShort, 0U, responseClusterId,
                         kZigbeeProfileZdo, 0U, responsePayload, responseLength);
    }
    return;
  }

  const bool targetsLocalEndpoint =
      (aps.deliveryMode == kZigbeeApsDeliveryUnicast &&
       aps.destinationEndpoint == kLocalEndpoint) ||
      (aps.deliveryMode == kZigbeeApsDeliveryGroup &&
       g_device.isInGroup(aps.destinationGroup));
  if (aps.profileId != kZigbeeProfileHomeAutomation || !targetsLocalEndpoint) {
    return;
  }

  uint8_t responseFrame[127] = {0U};
  uint8_t responseLength = 0U;
  if (!g_device.handleZclRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                 responseFrame, &responseLength)) {
    return;
  }

  g_savedOnOffState = g_device.onOff();
  g_savedLevelState = g_device.level();
  applyLedState();
  persistState();
  Serial.print("zcl cluster=0x");
  Serial.print(aps.clusterId, HEX);
  Serial.print(" onoff=");
  Serial.print(g_device.onOff() ? "ON" : "OFF");
  Serial.print(" level=");
  Serial.print(g_device.level());
  Serial.print("\r\n");

  if (responseLength > 0U &&
      aps.deliveryMode == kZigbeeApsDeliveryUnicast) {
    (void)sendApsFrame(nwk.sourceShort, aps.sourceEndpoint, aps.clusterId,
                       aps.profileId, aps.destinationEndpoint, responseFrame,
                       responseLength);
  }
}

void pollCoordinator() {
  if (!g_joined) {
    return;
  }

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  if (!ZigbeeCodec::buildDataRequest(g_macSequence++, g_panId, g_parentShort,
                                     kIeeeAddress, request, &requestLength)) {
    return;
  }
  if (!g_radio.transmit(request, requestLength, false, 1200000UL)) {
    return;
  }

  const uint32_t deadline = millis() + 80U;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    ZigbeeFrame frame{};
    if (!g_radio.receive(&frame, 4000U, 250000UL)) {
      continue;
    }
    processIncomingFrame(frame);
    return;
  }
}

void maybeSendScheduledReports(uint32_t nowMs) {
  if (!g_joined) {
    return;
  }

  const ZigbeeReportingConfiguration* reporting = g_device.reportingConfigurations();
  for (uint8_t i = 0U; i < 8U; ++i) {
    if (!reporting[i].used || reporting[i].maximumIntervalSeconds == 0U ||
        reporting[i].maximumIntervalSeconds == 0xFFFFU) {
      continue;
    }

    const uint32_t periodMs =
        static_cast<uint32_t>(reporting[i].maximumIntervalSeconds) * 1000UL;
    if ((nowMs - g_lastReportMs[i]) < periodMs) {
      continue;
    }

    if (sendAttributeReport(reporting[i].clusterId)) {
      g_lastReportMs[i] = nowMs;
    }
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch == 't') {
      g_savedOnOffState = !g_device.onOff();
      (void)g_device.setOnOff(g_savedOnOffState);
      g_savedLevelState = g_device.level();
      applyLedState();
      persistState();
      Serial.print("toggle onoff=");
      Serial.print(g_device.onOff() ? "ON" : "OFF");
      Serial.print(" level=");
      Serial.print(g_device.level());
      Serial.print("\r\n");
    } else if (ch == 'U' || ch == 'D' || ch == 'M') {
      uint16_t nextLevel = g_device.level();
      if (ch == 'U') {
        nextLevel = static_cast<uint16_t>(nextLevel) + 32U;
      } else if (ch == 'D') {
        nextLevel = (nextLevel > 32U) ? static_cast<uint16_t>(nextLevel - 32U) : 1U;
      } else {
        nextLevel = 128U;
      }
      if (nextLevel > 0xFEU) {
        nextLevel = 0xFEU;
      }
      (void)g_device.setLevel(static_cast<uint8_t>(nextLevel));
      g_savedOnOffState = g_device.onOff();
      g_savedLevelState = g_device.level();
      applyLedState();
      persistState();
      Serial.print("level=");
      Serial.print(g_device.level());
      Serial.print(" onoff=");
      Serial.print(g_device.onOff() ? "ON" : "OFF");
      Serial.print("\r\n");
    } else if (ch == 'r') {
      const bool onOffOk = sendAttributeReport(kZigbeeClusterOnOff);
      const bool levelOk = sendAttributeReport(kZigbeeClusterLevelControl);
      Serial.print("report onoff=");
      Serial.print(onOffOk ? "OK" : "FAIL");
      Serial.print(" level=");
      Serial.print(levelOk ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 'j') {
      clearJoinState(false);
      Serial.print("rejoin requested\r\n");
    } else if (ch == 'c') {
      clearJoinState(true);
      Serial.print("state cleared\r\n");
    } else if (ch == 's') {
      Serial.print("state joined=");
      Serial.print(g_joined ? "yes" : "no");
      Serial.print(" ch=");
      Serial.print(g_channel);
      Serial.print(" pan=0x");
      Serial.print(g_panId, HEX);
      Serial.print(" short=0x");
      Serial.print(g_localShort, HEX);
      Serial.print(" onoff=");
      Serial.print(g_device.onOff() ? "ON" : "OFF");
      Serial.print(" level=");
      Serial.print(g_device.level());
      Serial.print("\r\n");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  g_pwmReady = g_pwm20.beginSingle(kPinUserLed, 1000UL, 0U, false) &&
               g_pwm20.start(0, 200000UL);
  if (!g_pwmReady) {
    (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput,
                          GpioPull::kDisabled);
  }

  g_store.begin("zbjoindm");
  restoreState();

  const bool ok = g_radio.begin(g_channel, 8);
  Serial.print("\r\nZigbeeHaDimmableLightJoinable start\r\n");
  Serial.print("radio=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" pwm=");
  Serial.print(g_pwmReady ? "OK" : "FAIL");
  Serial.print(" preferred_channel=");
  Serial.print(kPreferredChannel);
  Serial.print(" joined=");
  Serial.print(g_joined ? "yes" : "no");
  Serial.print("\r\n");
  Serial.print("serial commands: t=toggle U=brighter D=dimmer M=mid r=report s=status j=rejoin c=clear\r\n");

  if (g_joined) {
    g_radio.setChannel(g_channel);
    if (g_securityEnabled && g_haveActiveNetworkKey) {
      (void)sendDeviceAnnounce();
    }
  }
}

void loop() {
  handleSerialCommands();

  const uint32_t now = millis();
  if (!g_joined) {
    if ((now - g_lastJoinAttemptMs) >= 2000U) {
      g_lastJoinAttemptMs = now;
      Serial.print("scan_join start\r\n");
      if (performJoin()) {
        Serial.print("join OK ch=");
        Serial.print(g_channel);
        Serial.print(" pan=0x");
        Serial.print(g_panId, HEX);
        Serial.print(" short=0x");
        Serial.print(g_localShort, HEX);
        Serial.print("\r\n");
      } else {
        Serial.print("join MISS\r\n");
      }
    }
    delay(1);
    return;
  }

  if ((now - g_lastPollMs) >= 250U) {
    g_lastPollMs = now;
    pollCoordinator();
  }
  maybeSendScheduledReports(now);

  if ((now - g_lastStatusMs) >= 5000U) {
    g_lastStatusMs = now;
    Serial.print("alive ch=");
    Serial.print(g_channel);
    Serial.print(" pan=0x");
    Serial.print(g_panId, HEX);
    Serial.print(" short=0x");
    Serial.print(g_localShort, HEX);
    Serial.print(" onoff=");
    Serial.print(g_device.onOff() ? "ON" : "OFF");
    Serial.print(" level=");
    Serial.print(g_device.level());
    Serial.print("\r\n");
  }

  delay(1);
}
