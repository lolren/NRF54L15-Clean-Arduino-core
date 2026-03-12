#include <Arduino.h>

#include "nrf54l15_hal.h"
#include "zigbee_persistence.h"
#include "zigbee_stack.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeHaOnOffLightStatic."
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_CHANNEL
#define NRF54L15_CLEAN_ZIGBEE_CHANNEL 15
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PAN_ID
#define NRF54L15_CLEAN_ZIGBEE_PAN_ID 0x1234
#endif

using namespace xiao_nrf54l15;

static ZigbeeRadio g_radio;
static ZigbeeHomeAutomationDevice g_device;
static ZigbeePersistentStateStore g_store;

static uint8_t g_macSequence = 1U;
static uint8_t g_nwkSequence = 1U;
static uint8_t g_apsCounter = 1U;
static uint8_t g_zclSequence = 1U;
static uint32_t g_lastStatusMs = 0U;

static constexpr uint8_t kChannel =
    static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPanId =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kLocalShort = 0x3344U;
static constexpr uint16_t kCoordinatorShort = 0x0000U;
static constexpr uint8_t kLocalEndpoint = 1U;
static constexpr uint8_t kCoordinatorEndpoint = 1U;
static constexpr uint64_t kIeeeAddress = 0x00124B0001ABCDEFULL;

static bool persistState() {
  ZigbeePersistentState state{};
  ZigbeePersistentStateStore::initialize(&state);
  state.channel = kChannel;
  state.logicalType = static_cast<uint8_t>(ZigbeeLogicalType::kEndDevice);
  state.panId = kPanId;
  state.nwkAddress = kLocalShort;
  state.parentShort = kCoordinatorShort;
  state.manufacturerCode = g_device.config().manufacturerCode;
  state.ieeeAddress = g_device.config().ieeeAddress;
  state.onOffState = g_device.onOff();
  state.nwkFrameCounter = g_nwkSequence;
  state.apsFrameCounter = g_apsCounter;

  const ZigbeeReportingConfiguration* reporting = g_device.reportingConfigurations();
  const uint8_t reportingCount = g_device.reportingConfigurationCount();
  state.reportingCount = reportingCount;
  uint8_t copied = 0U;
  for (uint8_t i = 0U; i < 8U && copied < reportingCount; ++i) {
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

static void restoreState() {
  ZigbeePersistentState state{};
  if (!g_store.load(&state)) {
    g_device.configureReporting(kZigbeeClusterOnOff, 0x0000U,
                                ZigbeeZclDataType::kBoolean, 0U, 30U, 0U);
    persistState();
    return;
  }

  if (state.panId != kPanId || state.nwkAddress != kLocalShort ||
      state.ieeeAddress != kIeeeAddress) {
    g_device.configureReporting(kZigbeeClusterOnOff, 0x0000U,
                                ZigbeeZclDataType::kBoolean, 0U, 30U, 0U);
    persistState();
    return;
  }

  g_device.setOnOff(state.onOffState);
  g_nwkSequence = static_cast<uint8_t>(state.nwkFrameCounter & 0xFFU);
  g_apsCounter = static_cast<uint8_t>(state.apsFrameCounter & 0xFFU);
  for (uint8_t i = 0U; i < state.reportingCount && i < 8U; ++i) {
    if (!state.reporting[i].used) {
      continue;
    }
    g_device.configureReporting(state.reporting[i].clusterId,
                                state.reporting[i].attributeId,
                                state.reporting[i].dataType,
                                state.reporting[i].minimumIntervalSeconds,
                                state.reporting[i].maximumIntervalSeconds,
                                state.reporting[i].reportableChange);
  }
  for (uint8_t i = 0U; i < state.bindingCount && i < 8U; ++i) {
    if (!state.bindings[i].used) {
      continue;
    }
    g_device.addBinding(state.bindings[i].sourceEndpoint,
                        state.bindings[i].clusterId,
                        state.bindings[i].destinationAddressMode,
                        state.bindings[i].destinationGroup,
                        state.bindings[i].destinationIeee,
                        state.bindings[i].destinationEndpoint);
  }
}

static bool sendApsFrameExtended(uint16_t destinationShort, uint8_t deliveryMode,
                                 uint16_t destinationGroup,
                                 uint8_t destinationEndpoint,
                                 uint16_t clusterId, uint16_t profileId,
                                 uint8_t sourceEndpoint,
                                 const uint8_t* payload,
                                 uint8_t payloadLength) {
  ZigbeeApsDataFrame aps{};
  aps.frameType = ZigbeeApsFrameType::kData;
  aps.deliveryMode = deliveryMode;
  aps.destinationEndpoint = destinationEndpoint;
  aps.destinationGroup = destinationGroup;
  aps.clusterId = clusterId;
  aps.profileId = profileId;
  aps.sourceEndpoint = sourceEndpoint;
  aps.counter = g_apsCounter++;

  uint8_t apsFrame[127] = {0};
  uint8_t apsLength = 0U;
  if (!ZigbeeCodec::buildApsDataFrame(aps, payload, payloadLength, apsFrame,
                                      &apsLength)) {
    return false;
  }

  ZigbeeNetworkFrame nwk{};
  nwk.frameType = ZigbeeNwkFrameType::kData;
  nwk.destinationShort = destinationShort;
  nwk.sourceShort = kLocalShort;
  nwk.radius = 30U;
  nwk.sequence = g_nwkSequence++;

  uint8_t nwkFrame[127] = {0};
  uint8_t nwkLength = 0U;
  if (!ZigbeeCodec::buildNwkFrame(nwk, apsFrame, apsLength, nwkFrame,
                                  &nwkLength)) {
    return false;
  }

  uint8_t psdu[127] = {0};
  uint8_t psduLength = 0U;
  if (!ZigbeeRadio::buildDataFrameShort(g_macSequence++, kPanId,
                                        destinationShort, kLocalShort, nwkFrame,
                                        nwkLength, psdu, &psduLength, false)) {
    return false;
  }

  return g_radio.transmit(psdu, psduLength, false, 1200000UL);
}

static bool sendApsFrame(uint16_t destinationShort, uint8_t destinationEndpoint,
                         uint16_t clusterId, uint16_t profileId,
                         uint8_t sourceEndpoint, const uint8_t* payload,
                         uint8_t payloadLength) {
  return sendApsFrameExtended(destinationShort, kZigbeeApsDeliveryUnicast, 0U,
                              destinationEndpoint, clusterId, profileId,
                              sourceEndpoint, payload, payloadLength);
}

static bool sendGroupApsFrame(uint16_t destinationShort, uint16_t destinationGroup,
                              uint16_t clusterId, uint16_t profileId,
                              uint8_t sourceEndpoint, const uint8_t* payload,
                              uint8_t payloadLength) {
  return sendApsFrameExtended(destinationShort, kZigbeeApsDeliveryGroup,
                              destinationGroup, 0U, clusterId, profileId,
                              sourceEndpoint, payload, payloadLength);
}

static bool sendAttributeReport(uint16_t clusterId) {
  uint8_t zclFrame[127] = {0};
  uint8_t zclLength = 0U;
  if (!g_device.buildAttributeReport(clusterId, g_zclSequence++, zclFrame,
                                     &zclLength) ||
      zclLength == 0U) {
    return false;
  }
  ZigbeeResolvedBindingDestination destination{};
  if (g_device.resolveBindingDestination(kLocalEndpoint, clusterId,
                                         &destination)) {
    if (destination.addressMode == ZigbeeBindingAddressMode::kGroup &&
        destination.groupId != 0U) {
      return sendGroupApsFrame(
          kCoordinatorShort, destination.groupId, clusterId,
          kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame, zclLength);
    }
    if (destination.addressMode == ZigbeeBindingAddressMode::kExtended &&
        destination.endpoint != 0U) {
      return sendApsFrame(kCoordinatorShort, destination.endpoint, clusterId,
                          kZigbeeProfileHomeAutomation, kLocalEndpoint,
                          zclFrame, zclLength);
    }
  }
  return sendApsFrame(kCoordinatorShort, kCoordinatorEndpoint, clusterId,
                      kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                      zclLength);
}

static bool sendDueAttributeReport(uint32_t nowMs, uint16_t* outClusterId) {
  if (outClusterId != nullptr) {
    *outClusterId = 0U;
  }

  uint8_t zclFrame[127] = {0};
  uint8_t zclLength = 0U;
  uint16_t clusterId = 0U;
  if (!g_device.buildDueAttributeReport(nowMs, g_zclSequence++, &clusterId,
                                        zclFrame, &zclLength) ||
      zclLength == 0U) {
    return false;
  }

  ZigbeeResolvedBindingDestination destination{};
  bool ok = false;
  if (g_device.resolveBindingDestination(kLocalEndpoint, clusterId,
                                         &destination)) {
    if (destination.addressMode == ZigbeeBindingAddressMode::kGroup &&
        destination.groupId != 0U) {
      ok = sendGroupApsFrame(
          kCoordinatorShort, destination.groupId, clusterId,
          kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame, zclLength);
    } else if (destination.addressMode ==
                   ZigbeeBindingAddressMode::kExtended &&
               destination.endpoint != 0U) {
      ok = sendApsFrame(kCoordinatorShort, destination.endpoint, clusterId,
                        kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                        zclLength);
    }
  }
  if (!ok) {
    ok = sendApsFrame(kCoordinatorShort, kCoordinatorEndpoint, clusterId,
                      kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                      zclLength);
  }

  if (!ok) {
    g_device.discardDueAttributeReport();
    return false;
  }

  (void)g_device.commitDueAttributeReport(nowMs);
  if (outClusterId != nullptr) {
    *outClusterId = clusterId;
  }
  return true;
}

static bool identifyIndicatorOn(uint32_t nowMs, uint8_t effectIdentifier) {
  switch (effectIdentifier) {
    case kZigbeeIdentifyEffectBlink:
    case kZigbeeIdentifyEffectOkay:
      return ((nowMs / 150U) & 0x01UL) == 0U;
    case kZigbeeIdentifyEffectBreathe:
      return ((nowMs % 1200U) < 800U);
    case kZigbeeIdentifyEffectChannelChange:
      return ((nowMs / 75U) & 0x01UL) == 0U;
    case kZigbeeIdentifyEffectFinishEffect:
      return true;
    default:
      return ((nowMs / 250U) & 0x01UL) == 0U;
  }
}

static void applyLedState() {
  const uint32_t nowMs = millis();
  g_device.updateIdentify(nowMs);
  const bool ledOn =
      g_device.identifying()
          ? identifyIndicatorOn(nowMs, g_device.identifyEffect())
          : g_device.onOff();
  Gpio::write(kPinUserLed, !ledOn);
}

static void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch == 't') {
      g_device.setOnOff(!g_device.onOff());
      applyLedState();
      persistState();
      Serial.print("toggle onoff=");
      Serial.print(g_device.onOff() ? "ON" : "OFF");
      Serial.print("\r\n");
    } else if (ch == 'r') {
      const bool ok = sendAttributeReport(kZigbeeClusterOnOff);
      Serial.print("report ");
      Serial.print(ok ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 'c') {
      g_store.clear();
      Serial.print("state cleared\r\n");
    } else if (ch == 's') {
      Serial.print("state onoff=");
      Serial.print(g_device.onOff() ? "ON" : "OFF");
      Serial.print(" reports=");
      Serial.print(g_device.reportingConfigurationCount());
      Serial.print("\r\n");
    }
  }
}

static void maybeSendScheduledReports(uint32_t nowMs) {
  uint16_t clusterId = 0U;
  while (sendDueAttributeReport(nowMs, &clusterId)) {
    Serial.print("scheduled_report cluster=0x");
    Serial.print(clusterId, HEX);
    Serial.print("\r\n");
  }
}

static void processIncomingFrame(const ZigbeeFrame& frame) {
  ZigbeeDataFrameView macData{};
  if (!ZigbeeRadio::parseDataFrameShort(frame.psdu, frame.length, &macData) ||
      !macData.valid || macData.panId != kPanId ||
      macData.destinationShort != kLocalShort) {
    return;
  }

  ZigbeeNetworkFrame nwk{};
  if (!ZigbeeCodec::parseNwkFrame(macData.payload, macData.payloadLength, &nwk) ||
      !nwk.valid || nwk.destinationShort != kLocalShort) {
    return;
  }

  ZigbeeApsDataFrame aps{};
  if (!ZigbeeCodec::parseApsDataFrame(nwk.payload, nwk.payloadLength, &aps) ||
      !aps.valid) {
    return;
  }

  if (aps.profileId == kZigbeeProfileZdo) {
    uint16_t responseClusterId = 0U;
    uint8_t responsePayload[127] = {0};
    uint8_t responseLength = 0U;
    if (g_device.handleZdoRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                  &responseClusterId, responsePayload,
                                  &responseLength) &&
        responseLength > 0U) {
      const bool sent =
          sendApsFrame(nwk.sourceShort, 0U, responseClusterId,
                       kZigbeeProfileZdo, 0U, responsePayload, responseLength);
      Serial.print("zdo cluster=0x");
      Serial.print(aps.clusterId, HEX);
      Serial.print(" sent=");
      Serial.print(sent ? "OK" : "FAIL");
      Serial.print("\r\n");
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

  uint8_t responseFrame[127] = {0};
  uint8_t responseLength = 0U;
  if (!g_device.handleZclRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                 responseFrame, &responseLength)) {
    return;
  }

  applyLedState();
  persistState();
  Serial.print("zcl cluster=0x");
  Serial.print(aps.clusterId, HEX);
  Serial.print(" onoff=");
  Serial.print(g_device.onOff() ? "ON" : "OFF");
  Serial.print("\r\n");

  if (responseLength > 0U &&
      aps.deliveryMode == kZigbeeApsDeliveryUnicast) {
    const bool sent = sendApsFrame(nwk.sourceShort, aps.sourceEndpoint,
                                   aps.clusterId, aps.profileId,
                                   aps.destinationEndpoint, responseFrame,
                                   responseLength);
    Serial.print("zcl_rsp ");
    Serial.print(sent ? "OK" : "FAIL");
    Serial.print("\r\n");
  }
}

static void pumpRadio() {
  ZigbeeFrame frame{};
  if (!g_radio.receive(&frame, 5000U, 900000UL)) {
    return;
  }
  processIncomingFrame(frame);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);

  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-STATIC-LIGHT";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x01U;
  g_device.configureOnOffLight(kLocalEndpoint, kIeeeAddress, kLocalShort,
                               kPanId, basic, 0x0000U);

  g_store.begin("zbhalight");
  restoreState();
  applyLedState();

  const bool ok = g_radio.begin(kChannel, 8);
  Serial.print("\r\nZigbeeHaOnOffLightStatic start\r\n");
  Serial.print("radio=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" channel=");
  Serial.print(kChannel);
  Serial.print(" pan=0x");
  Serial.print(kPanId, HEX);
  Serial.print(" short=0x");
  Serial.print(kLocalShort, HEX);
  Serial.print("\r\n");
  Serial.print("serial commands: t=toggle r=report s=status c=clear\r\n");
}

void loop() {
  handleSerialCommands();
  pumpRadio();

  const uint32_t now = millis();
  maybeSendScheduledReports(now);
  applyLedState();

  if ((now - g_lastStatusMs) >= 5000U) {
    g_lastStatusMs = now;
    Serial.print("alive onoff=");
    Serial.print(g_device.onOff() ? "ON" : "OFF");
    Serial.print(" reports=");
    Serial.print(g_device.reportingConfigurationCount());
    Serial.print("\r\n");
  }
}
