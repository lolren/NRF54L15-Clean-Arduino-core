#include <Arduino.h>

#include "nrf54l15_hal.h"
#include "zigbee_persistence.h"
#include "zigbee_stack.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeHaTemperatureSensorStatic."
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
static TempSensor g_temp;

static uint8_t g_macSequence = 1U;
static uint8_t g_nwkSequence = 1U;
static uint8_t g_apsCounter = 1U;
static uint8_t g_zclSequence = 1U;
static uint32_t g_lastStatusMs = 0U;
static uint32_t g_lastSampleMs = 0U;

static constexpr uint8_t kChannel =
    static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPanId =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kLocalShort = 0x4455U;
static constexpr uint16_t kCoordinatorShort = 0x0000U;
static constexpr uint8_t kLocalEndpoint = 1U;
static constexpr uint8_t kCoordinatorEndpoint = 1U;
static constexpr uint64_t kIeeeAddress = 0x00124B0001ABCD01ULL;

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
  state.nwkFrameCounter = g_nwkSequence;
  state.apsFrameCounter = g_apsCounter;

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

static void restoreState() {
  ZigbeePersistentState state{};
  if (!g_store.load(&state) || state.panId != kPanId ||
      state.nwkAddress != kLocalShort || state.ieeeAddress != kIeeeAddress) {
    g_device.configureReporting(kZigbeeClusterTemperatureMeasurement, 0x0000U,
                                ZigbeeZclDataType::kInt16, 5U, 60U, 25U);
    g_device.configureReporting(kZigbeeClusterPowerConfiguration, 0x0020U,
                                ZigbeeZclDataType::kUint8, 30U, 300U, 1U);
    g_device.configureReporting(kZigbeeClusterPowerConfiguration, 0x0021U,
                                ZigbeeZclDataType::kUint8, 30U, 300U, 2U);
    persistState();
    return;
  }

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

static void sampleSensors() {
  int32_t tempMilliC = 0;
  if (g_temp.sampleMilliDegreesC(&tempMilliC, 400000UL)) {
    g_device.setTemperatureState(static_cast<int16_t>(tempMilliC / 10L), -4000,
                                 12500, 50U);
  }

  int32_t vbatMv = 0;
  uint8_t vbatPercent = 0;
  if (BoardControl::sampleBatteryMilliVolts(&vbatMv) &&
      BoardControl::sampleBatteryPercent(&vbatPercent)) {
    const uint8_t decivolts =
        static_cast<uint8_t>(vbatMv <= 0 ? 0 : (vbatMv / 100));
    const uint8_t halfPercent =
        static_cast<uint8_t>(vbatPercent >= 100U ? 200U : vbatPercent * 2U);
    g_device.setBatteryStatus(decivolts, halfPercent);
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

static void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch == 'r') {
      const bool tempOk = sendAttributeReport(kZigbeeClusterTemperatureMeasurement);
      const bool battOk = sendAttributeReport(kZigbeeClusterPowerConfiguration);
      Serial.print("report temp=");
      Serial.print(tempOk ? "OK" : "FAIL");
      Serial.print(" batt=");
      Serial.print(battOk ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 's') {
      Serial.print("state reports=");
      Serial.print(g_device.reportingConfigurationCount());
      Serial.print("\r\n");
    } else if (ch == 'c') {
      g_store.clear();
      Serial.print("state cleared\r\n");
    }
  }
}

static void maybeSendScheduledReports(uint32_t nowMs) {
  while (sendDueAttributeReport(nowMs, nullptr)) {
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
      sendApsFrame(nwk.sourceShort, 0U, responseClusterId, kZigbeeProfileZdo,
                   0U, responsePayload, responseLength);
    }
    return;
  }

  if (aps.profileId != kZigbeeProfileHomeAutomation ||
      aps.destinationEndpoint != kLocalEndpoint) {
    return;
  }

  uint8_t responseFrame[127] = {0};
  uint8_t responseLength = 0U;
  if (!g_device.handleZclRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                 responseFrame, &responseLength)) {
    return;
  }

  persistState();
  if (responseLength > 0U) {
    sendApsFrame(nwk.sourceShort, aps.sourceEndpoint, aps.clusterId,
                 aps.profileId, aps.destinationEndpoint, responseFrame,
                 responseLength);
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

  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-STATIC-TEMP";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x03U;
  g_device.configureTemperatureSensor(kLocalEndpoint, kIeeeAddress,
                                      kLocalShort, kPanId, basic, 0x0000U);

  g_store.begin("zbhatemp");
  sampleSensors();
  restoreState();

  const bool ok = g_radio.begin(kChannel, 8);
  Serial.print("\r\nZigbeeHaTemperatureSensorStatic start\r\n");
  Serial.print("radio=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" channel=");
  Serial.print(kChannel);
  Serial.print(" pan=0x");
  Serial.print(kPanId, HEX);
  Serial.print(" short=0x");
  Serial.print(kLocalShort, HEX);
  Serial.print("\r\n");
  Serial.print("serial commands: r=report s=status c=clear\r\n");
}

void loop() {
  handleSerialCommands();
  pumpRadio();

  const uint32_t now = millis();
  if ((now - g_lastSampleMs) >= 5000U) {
    g_lastSampleMs = now;
    sampleSensors();
  }
  maybeSendScheduledReports(now);

  if ((now - g_lastStatusMs) >= 5000U) {
    g_lastStatusMs = now;
    Serial.print("alive reports=");
    Serial.print(g_device.reportingConfigurationCount());
    Serial.print("\r\n");
  }
}
