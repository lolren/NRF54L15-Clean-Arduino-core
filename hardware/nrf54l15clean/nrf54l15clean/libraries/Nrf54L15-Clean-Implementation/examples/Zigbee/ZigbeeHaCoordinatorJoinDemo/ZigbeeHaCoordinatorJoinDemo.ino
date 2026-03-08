#include <Arduino.h>
#include <string.h>

#include "nrf54l15_hal.h"
#include "zigbee_security.h"
#include "zigbee_stack.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeHaCoordinatorJoinDemo."
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
static uint8_t g_macSequence = 1U;
static uint8_t g_nwkSequence = 1U;
static uint32_t g_nwkSecurityFrameCounter = 1U;
static uint8_t g_apsCounter = 1U;
static uint8_t g_zclSequence = 1U;
static uint8_t g_zdoSequence = 1U;
static uint32_t g_lastBeaconMs = 0U;
static uint32_t g_lastStatusMs = 0U;

static constexpr uint8_t kChannel =
    static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPanId =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kCoordinatorShort = 0x0000U;
static constexpr uint8_t kCoordinatorEndpoint = 1U;
static constexpr uint64_t kCoordinatorIeee = 0x00124B000054A11FULL;
static constexpr uint64_t kExtendedPanId = 0x00124B000054C0DEULL;
static constexpr uint8_t kDemoNetworkKeySequence = 0x01U;
static const uint8_t kDemoNetworkKey[16] = {0xA1U, 0xB2U, 0xC3U, 0xD4U,
                                            0xE5U, 0xF6U, 0x07U, 0x18U,
                                            0x29U, 0x3AU, 0x4BU, 0x5CU,
                                            0x6DU, 0x7EU, 0x8FU, 0x90U};
static constexpr uint8_t kMaxNodes = 8U;
static constexpr uint8_t kPendingPayloadMax = 96U;
static constexpr uint8_t kZclCommandConfigureReportingResponse = 0x07U;
static constexpr uint8_t kZclCommandReportAttributes = 0x0AU;
static constexpr uint8_t kZclCommandDefaultResponse = 0x0BU;
static constexpr uint8_t kOnOffCommandOff = 0x00U;
static constexpr uint8_t kOnOffCommandOn = 0x01U;
static constexpr uint8_t kOnOffCommandToggle = 0x02U;
static constexpr uint8_t kGroupsCommandAddGroup = 0x00U;
static constexpr uint8_t kLevelControlCommandMoveToLevelWithOnOff = 0x04U;
static constexpr uint8_t kLevelControlCommandStepWithOnOff = 0x06U;
static constexpr uint16_t kDemoGroupId = 0x1001U;

enum class NodeStage : uint8_t {
  kIdle = 0U,
  kAwaitingActiveEndpoints = 1U,
  kAwaitingSimpleDescriptor = 2U,
  kAwaitingBasicRead = 3U,
  kAwaitingReporting = 4U,
  kReady = 5U,
};

struct PendingApsFrame {
  bool used = false;
  uint8_t deliveryMode = kZigbeeApsDeliveryUnicast;
  uint16_t destinationGroup = 0U;
  uint16_t clusterId = 0U;
  uint16_t profileId = 0U;
  uint8_t destinationEndpoint = 0U;
  uint8_t sourceEndpoint = 0U;
  uint8_t payloadLength = 0U;
  uint8_t payload[kPendingPayloadMax] = {0U};
};

struct NodeEntry {
  bool used = false;
  uint64_t ieeeAddress = 0U;
  uint16_t shortAddress = 0U;
  uint32_t lastSeenMs = 0U;
  bool secureNwkSeen = false;
  uint32_t lastInboundSecurityFrameCounter = 0U;
  bool pendingTransportKey = false;
  bool pendingAssociationResponse = false;
  uint16_t pendingAssignedShort = 0U;
  uint8_t pendingAssociationStatus = 0U;
  PendingApsFrame pending{};
  bool announced = false;
  uint8_t endpoint = 0U;
  uint16_t profileId = 0U;
  uint16_t deviceId = 0U;
  bool supportsOnOff = false;
  bool supportsLevelControl = false;
  bool supportsIdentify = false;
  bool supportsTemperature = false;
  bool supportsPowerConfiguration = false;
  bool basicRead = false;
  bool onOffBindingConfigured = false;
  bool levelBindingConfigured = false;
  bool temperatureBindingConfigured = false;
  bool powerBindingConfigured = false;
  bool onOffReportingConfigured = false;
  bool levelReportingConfigured = false;
  bool temperatureReportingConfigured = false;
  bool powerReportingConfigured = false;
  bool awaitingBindResponse = false;
  uint16_t awaitingClusterId = 0U;
  bool haveOnOffState = false;
  bool onOffState = false;
  bool haveLevelState = false;
  uint8_t levelState = 0U;
  bool demoGroupConfigured = false;
  NodeStage stage = NodeStage::kIdle;
};

static NodeEntry g_nodes[kMaxNodes] = {};
static uint16_t g_nextShortAddress = 0x1000U;

NodeEntry* findNodeByIeee(uint64_t ieeeAddress) {
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (g_nodes[i].used && g_nodes[i].ieeeAddress == ieeeAddress) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

NodeEntry* findNodeByShort(uint16_t shortAddress) {
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (g_nodes[i].used && g_nodes[i].shortAddress == shortAddress) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

NodeEntry* allocateNode(uint64_t ieeeAddress) {
  NodeEntry* existing = findNodeByIeee(ieeeAddress);
  if (existing != nullptr) {
    return existing;
  }

  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (!g_nodes[i].used) {
      memset(&g_nodes[i], 0, sizeof(g_nodes[i]));
      g_nodes[i].used = true;
      g_nodes[i].ieeeAddress = ieeeAddress;
      return &g_nodes[i];
    }
  }
  return nullptr;
}

uint16_t allocateShortAddress() {
  while (findNodeByShort(g_nextShortAddress) != nullptr ||
         g_nextShortAddress == 0x0000U || g_nextShortAddress == 0xFFFFU) {
    ++g_nextShortAddress;
  }
  return g_nextShortAddress++;
}

bool sendPsdu(const uint8_t* psdu, uint8_t length) {
  return g_radio.transmit(psdu, length, false, 1200000UL);
}

bool sendBeacon() {
  ZigbeeMacBeaconPayload payload{};
  payload.valid = true;
  payload.protocolId = 0U;
  payload.stackProfile = 2U;
  payload.protocolVersion = 2U;
  payload.routerCapacity = true;
  payload.endDeviceCapacity = true;
  payload.extendedPanId = kExtendedPanId;
  payload.txOffset = 0x00FFFFFFUL;
  payload.updateId = 0U;

  uint8_t frame[127] = {0U};
  uint8_t length = 0U;
  if (!ZigbeeCodec::buildBeaconFrame(g_macSequence++, kPanId,
                                     kCoordinatorShort, payload, frame,
                                     &length)) {
    return false;
  }
  return sendPsdu(frame, length);
}

bool queuePendingApsFrameExtended(NodeEntry* node, uint8_t deliveryMode,
                                  uint16_t destinationGroup,
                                  uint16_t clusterId, uint16_t profileId,
                                  uint8_t destinationEndpoint,
                                  uint8_t sourceEndpoint,
                                  const uint8_t* payload,
                                  uint8_t payloadLength) {
  if (node == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (node->pendingAssociationResponse || node->pendingTransportKey ||
      node->pending.used ||
      payloadLength > kPendingPayloadMax) {
    return false;
  }

  node->pending.used = true;
  node->pending.deliveryMode = deliveryMode;
  node->pending.destinationGroup = destinationGroup;
  node->pending.clusterId = clusterId;
  node->pending.profileId = profileId;
  node->pending.destinationEndpoint = destinationEndpoint;
  node->pending.sourceEndpoint = sourceEndpoint;
  node->pending.payloadLength = payloadLength;
  if (payloadLength > 0U) {
    memcpy(node->pending.payload, payload, payloadLength);
  }
  return true;
}

bool queuePendingApsFrame(NodeEntry* node, uint16_t clusterId,
                          uint16_t profileId, uint8_t destinationEndpoint,
                          uint8_t sourceEndpoint, const uint8_t* payload,
                          uint8_t payloadLength) {
  return queuePendingApsFrameExtended(
      node, kZigbeeApsDeliveryUnicast, 0U, clusterId, profileId,
      destinationEndpoint, sourceEndpoint, payload, payloadLength);
}

bool sendApsFrameExtended(uint16_t destinationShort, uint8_t deliveryMode,
                          uint16_t destinationGroup,
                          uint8_t destinationEndpoint, uint16_t clusterId,
                          uint16_t profileId, uint8_t sourceEndpoint,
                          const uint8_t* payload, uint8_t payloadLength) {
  NodeEntry* node = findNodeByShort(destinationShort);
  const bool useSecurity = (node != nullptr && node->secureNwkSeen);

  ZigbeeApsDataFrame aps{};
  aps.frameType = ZigbeeApsFrameType::kData;
  aps.deliveryMode = deliveryMode;
  aps.destinationEndpoint = destinationEndpoint;
  aps.destinationGroup = destinationGroup;
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
  nwk.sourceShort = kCoordinatorShort;
  nwk.radius = 30U;
  nwk.sequence = g_nwkSequence++;

  uint8_t nwkFrame[127] = {0U};
  uint8_t nwkLength = 0U;
  if (useSecurity) {
    ZigbeeNwkSecurityHeader security{};
    security.valid = true;
    security.securityControl = kZigbeeSecurityControlNwkEncMic32;
    security.frameCounter = g_nwkSecurityFrameCounter++;
    security.sourceIeee = kCoordinatorIeee;
    security.keySequence = kDemoNetworkKeySequence;
    if (!ZigbeeSecurity::buildSecuredNwkFrame(nwk, security, kDemoNetworkKey,
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
  if (!ZigbeeRadio::buildDataFrameShort(g_macSequence++, kPanId,
                                        destinationShort, kCoordinatorShort,
                                        nwkFrame, nwkLength, psdu,
                                        &psduLength, false)) {
    return false;
  }
  return sendPsdu(psdu, psduLength);
}

bool sendTransportKey(NodeEntry* node) {
  if (node == nullptr || node->shortAddress == 0U || node->ieeeAddress == 0U) {
    return false;
  }

  ZigbeeApsTransportKey transportKey{};
  transportKey.valid = true;
  transportKey.keyType = kZigbeeApsTransportKeyStandardNetworkKey;
  memcpy(transportKey.key, kDemoNetworkKey, sizeof(transportKey.key));
  transportKey.keySequence = kDemoNetworkKeySequence;
  transportKey.destinationIeee = node->ieeeAddress;
  transportKey.sourceIeee = kCoordinatorIeee;

  uint8_t apsFrame[127] = {0U};
  uint8_t apsLength = 0U;
  if (!ZigbeeCodec::buildApsTransportKeyCommand(transportKey, g_apsCounter++,
                                                apsFrame, &apsLength)) {
    return false;
  }

  ZigbeeNetworkFrame nwk{};
  nwk.frameType = ZigbeeNwkFrameType::kData;
  nwk.securityEnabled = false;
  nwk.destinationShort = node->shortAddress;
  nwk.sourceShort = kCoordinatorShort;
  nwk.radius = 30U;
  nwk.sequence = g_nwkSequence++;

  uint8_t nwkFrame[127] = {0U};
  uint8_t nwkLength = 0U;
  if (!ZigbeeCodec::buildNwkFrame(nwk, apsFrame, apsLength, nwkFrame,
                                  &nwkLength)) {
    return false;
  }

  uint8_t psdu[127] = {0U};
  uint8_t psduLength = 0U;
  if (!ZigbeeRadio::buildDataFrameShort(g_macSequence++, kPanId,
                                        node->shortAddress, kCoordinatorShort,
                                        nwkFrame, nwkLength, psdu,
                                        &psduLength, false)) {
    return false;
  }
  return sendPsdu(psdu, psduLength);
}

bool sendApsFrame(uint16_t destinationShort, uint8_t destinationEndpoint,
                  uint16_t clusterId, uint16_t profileId,
                  uint8_t sourceEndpoint, const uint8_t* payload,
                  uint8_t payloadLength) {
  return sendApsFrameExtended(destinationShort, kZigbeeApsDeliveryUnicast, 0U,
                              destinationEndpoint, clusterId, profileId,
                              sourceEndpoint, payload, payloadLength);
}

bool queuePendingTransportKey(NodeEntry* node) {
  if (node == nullptr || node->pendingAssociationResponse ||
      node->pendingTransportKey || node->pending.used) {
    return false;
  }
  node->pendingTransportKey = true;
  return true;
}

bool sendPendingAssociationResponse(NodeEntry* node) {
  if (node == nullptr || !node->pendingAssociationResponse) {
    return false;
  }

  uint8_t frame[127] = {0U};
  uint8_t length = 0U;
  const bool built = ZigbeeCodec::buildAssociationResponse(
      g_macSequence++, kPanId, node->ieeeAddress, kCoordinatorShort,
      node->pendingAssignedShort, node->pendingAssociationStatus, frame, &length);
  if (!built) {
    return false;
  }
  const bool sent = sendPsdu(frame, length);
  if (sent) {
    node->pendingAssociationResponse = false;
    node->shortAddress = node->pendingAssignedShort;
    node->lastSeenMs = millis();
    if (node->pendingAssociationStatus == 0x00U) {
      (void)queuePendingTransportKey(node);
    }
  }
  return sent;
}

bool sendPendingApsFrame(NodeEntry* node) {
  if (node == nullptr) {
    return false;
  }
  if (node->pendingTransportKey) {
    const bool sent = sendTransportKey(node);
    if (sent) {
      node->pendingTransportKey = false;
    }
    return sent;
  }
  if (!node->pending.used) {
    return false;
  }

  const bool sent =
      sendApsFrameExtended(node->shortAddress, node->pending.deliveryMode,
                           node->pending.destinationGroup,
                           node->pending.destinationEndpoint,
                           node->pending.clusterId, node->pending.profileId,
                           node->pending.sourceEndpoint, node->pending.payload,
                           node->pending.payloadLength);
  if (sent) {
    node->pending.used = false;
    node->pending.payloadLength = 0U;
  }
  return sent;
}

bool queueActiveEndpointsRequest(NodeEntry* node) {
  if (node == nullptr) {
    return false;
  }
  uint8_t payload[8] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildZdoActiveEndpointsRequest(g_zdoSequence++,
                                                   node->shortAddress,
                                                   payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeZdoActiveEndpointsRequest,
                           kZigbeeProfileZdo, 0U, 0U, payload, payloadLength);
  if (queued) {
    node->stage = NodeStage::kAwaitingActiveEndpoints;
  }
  return queued;
}

bool queueSimpleDescriptorRequest(NodeEntry* node, uint8_t endpoint) {
  if (node == nullptr || endpoint == 0U) {
    return false;
  }
  uint8_t payload[8] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildZdoSimpleDescriptorRequest(g_zdoSequence++,
                                                    node->shortAddress, endpoint,
                                                    payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeZdoSimpleDescriptorRequest,
                           kZigbeeProfileZdo, 0U, 0U, payload, payloadLength);
  if (queued) {
    node->stage = NodeStage::kAwaitingSimpleDescriptor;
  }
  return queued;
}

bool queueBasicReadRequest(NodeEntry* node) {
  if (node == nullptr || node->endpoint == 0U) {
    return false;
  }
  const uint16_t attributes[] = {0x0004U, 0x0005U, 0x0007U};
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildReadAttributesRequest(
          attributes,
          static_cast<uint8_t>(sizeof(attributes) / sizeof(attributes[0])),
          g_zclSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeClusterBasic,
                           kZigbeeProfileHomeAutomation, node->endpoint,
                           kCoordinatorEndpoint, payload, payloadLength);
  if (queued) {
    node->stage = NodeStage::kAwaitingBasicRead;
  }
  return queued;
}

bool queueOnOffConfigureReporting(NodeEntry* node) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsOnOff) {
    return false;
  }
  ZigbeeReportingConfiguration configuration{};
  configuration.used = true;
  configuration.clusterId = kZigbeeClusterOnOff;
  configuration.attributeId = 0x0000U;
  configuration.dataType = ZigbeeZclDataType::kBoolean;
  configuration.minimumIntervalSeconds = 0U;
  configuration.maximumIntervalSeconds = 60U;
  configuration.reportableChange = 0U;

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildConfigureReportingRequest(
          &configuration, 1U, g_zclSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeClusterOnOff,
                           kZigbeeProfileHomeAutomation, node->endpoint,
                           kCoordinatorEndpoint, payload, payloadLength);
  if (queued) {
    node->awaitingBindResponse = false;
    node->awaitingClusterId = kZigbeeClusterOnOff;
    node->stage = NodeStage::kAwaitingReporting;
  }
  return queued;
}

bool queueBindRequest(NodeEntry* node, uint16_t clusterId) {
  if (node == nullptr || node->endpoint == 0U || node->ieeeAddress == 0U) {
    return false;
  }

  uint8_t payload[32] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildZdoBindRequest(
          g_zdoSequence++, node->ieeeAddress, node->endpoint, clusterId,
          ZigbeeBindingAddressMode::kExtended, 0U, kCoordinatorIeee,
          kCoordinatorEndpoint, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeZdoBindRequest, kZigbeeProfileZdo, 0U,
                           0U, payload, payloadLength);
  if (queued) {
    node->awaitingBindResponse = true;
    node->awaitingClusterId = clusterId;
    node->stage = NodeStage::kAwaitingReporting;
  }
  return queued;
}

bool queueLevelConfigureReporting(NodeEntry* node) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsLevelControl) {
    return false;
  }
  ZigbeeReportingConfiguration configuration{};
  configuration.used = true;
  configuration.clusterId = kZigbeeClusterLevelControl;
  configuration.attributeId = 0x0000U;
  configuration.dataType = ZigbeeZclDataType::kUint8;
  configuration.minimumIntervalSeconds = 0U;
  configuration.maximumIntervalSeconds = 60U;
  configuration.reportableChange = 16U;

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildConfigureReportingRequest(
          &configuration, 1U, g_zclSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeClusterLevelControl,
                           kZigbeeProfileHomeAutomation, node->endpoint,
                           kCoordinatorEndpoint, payload, payloadLength);
  if (queued) {
    node->awaitingBindResponse = false;
    node->awaitingClusterId = kZigbeeClusterLevelControl;
    node->stage = NodeStage::kAwaitingReporting;
  }
  return queued;
}

bool queueTemperatureConfigureReporting(NodeEntry* node) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsTemperature) {
    return false;
  }
  ZigbeeReportingConfiguration configuration{};
  configuration.used = true;
  configuration.clusterId = kZigbeeClusterTemperatureMeasurement;
  configuration.attributeId = 0x0000U;
  configuration.dataType = ZigbeeZclDataType::kInt16;
  configuration.minimumIntervalSeconds = 5U;
  configuration.maximumIntervalSeconds = 60U;
  configuration.reportableChange = 25U;

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildConfigureReportingRequest(
          &configuration, 1U, g_zclSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeClusterTemperatureMeasurement,
                           kZigbeeProfileHomeAutomation, node->endpoint,
                           kCoordinatorEndpoint, payload, payloadLength);
  if (queued) {
    node->awaitingBindResponse = false;
    node->awaitingClusterId = kZigbeeClusterTemperatureMeasurement;
    node->stage = NodeStage::kAwaitingReporting;
  }
  return queued;
}

bool queuePowerConfigureReporting(NodeEntry* node) {
  if (node == nullptr || node->endpoint == 0U ||
      !node->supportsPowerConfiguration) {
    return false;
  }

  ZigbeeReportingConfiguration configurations[2];
  memset(configurations, 0, sizeof(configurations));
  configurations[0].used = true;
  configurations[0].clusterId = kZigbeeClusterPowerConfiguration;
  configurations[0].attributeId = 0x0020U;
  configurations[0].dataType = ZigbeeZclDataType::kUint8;
  configurations[0].minimumIntervalSeconds = 30U;
  configurations[0].maximumIntervalSeconds = 300U;
  configurations[0].reportableChange = 1U;
  configurations[1].used = true;
  configurations[1].clusterId = kZigbeeClusterPowerConfiguration;
  configurations[1].attributeId = 0x0021U;
  configurations[1].dataType = ZigbeeZclDataType::kUint8;
  configurations[1].minimumIntervalSeconds = 30U;
  configurations[1].maximumIntervalSeconds = 300U;
  configurations[1].reportableChange = 2U;

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!ZigbeeCodec::buildConfigureReportingRequest(
          configurations, 2U, g_zclSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool queued =
      queuePendingApsFrame(node, kZigbeeClusterPowerConfiguration,
                           kZigbeeProfileHomeAutomation, node->endpoint,
                           kCoordinatorEndpoint, payload, payloadLength);
  if (queued) {
    node->awaitingBindResponse = false;
    node->awaitingClusterId = kZigbeeClusterPowerConfiguration;
    node->stage = NodeStage::kAwaitingReporting;
  }
  return queued;
}

bool queueOnOffCommand(NodeEntry* node, uint8_t commandId) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsOnOff) {
    return false;
  }
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = false;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = commandId;
  if (!ZigbeeCodec::buildZclFrame(frame, nullptr, 0U, payload, &payloadLength)) {
    return false;
  }
  return queuePendingApsFrame(node, kZigbeeClusterOnOff,
                              kZigbeeProfileHomeAutomation, node->endpoint,
                              kCoordinatorEndpoint, payload, payloadLength);
}

bool queueLevelMoveToLevel(NodeEntry* node, uint8_t level) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsLevelControl) {
    return false;
  }

  const uint8_t commandPayload[] = {level, 0x00U, 0x00U};
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = false;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = kLevelControlCommandMoveToLevelWithOnOff;
  if (!ZigbeeCodec::buildZclFrame(frame, commandPayload,
                                  static_cast<uint8_t>(sizeof(commandPayload)),
                                  payload, &payloadLength)) {
    return false;
  }

  return queuePendingApsFrame(node, kZigbeeClusterLevelControl,
                              kZigbeeProfileHomeAutomation, node->endpoint,
                              kCoordinatorEndpoint, payload, payloadLength);
}

bool queueLevelStep(NodeEntry* node, bool increase, uint8_t stepSize) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsLevelControl) {
    return false;
  }

  const uint8_t commandPayload[] = {
      static_cast<uint8_t>(increase ? 0x00U : 0x01U), stepSize, 0x00U, 0x00U};
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = false;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = kLevelControlCommandStepWithOnOff;
  if (!ZigbeeCodec::buildZclFrame(frame, commandPayload,
                                  static_cast<uint8_t>(sizeof(commandPayload)),
                                  payload, &payloadLength)) {
    return false;
  }

  return queuePendingApsFrame(node, kZigbeeClusterLevelControl,
                              kZigbeeProfileHomeAutomation, node->endpoint,
                              kCoordinatorEndpoint, payload, payloadLength);
}

bool queueAddGroup(NodeEntry* node, uint16_t groupId, const char* name) {
  if (node == nullptr || node->endpoint == 0U ||
      (!node->supportsOnOff && !node->supportsLevelControl)) {
    return false;
  }

  uint8_t commandPayload[32] = {0U};
  const uint8_t nameLength =
      (name == nullptr)
          ? 0U
          : static_cast<uint8_t>(strnlen(name, sizeof(commandPayload) - 3U));
  commandPayload[0] = static_cast<uint8_t>(groupId & 0xFFU);
  commandPayload[1] = static_cast<uint8_t>((groupId >> 8U) & 0xFFU);
  commandPayload[2] = nameLength;
  if (nameLength > 0U) {
    memcpy(&commandPayload[3], name, nameLength);
  }

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = false;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = kGroupsCommandAddGroup;
  if (!ZigbeeCodec::buildZclFrame(
          frame, commandPayload, static_cast<uint8_t>(3U + nameLength), payload,
          &payloadLength)) {
    return false;
  }

  return queuePendingApsFrame(node, kZigbeeClusterGroups,
                              kZigbeeProfileHomeAutomation, node->endpoint,
                              kCoordinatorEndpoint, payload, payloadLength);
}

bool queueGroupOnOffCommand(NodeEntry* node, uint16_t groupId,
                            uint8_t commandId) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsOnOff ||
      !node->demoGroupConfigured) {
    return false;
  }

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = true;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = commandId;
  if (!ZigbeeCodec::buildZclFrame(frame, nullptr, 0U, payload, &payloadLength)) {
    return false;
  }

  return queuePendingApsFrameExtended(
      node, kZigbeeApsDeliveryGroup, groupId, kZigbeeClusterOnOff,
      kZigbeeProfileHomeAutomation, 0U, kCoordinatorEndpoint, payload,
      payloadLength);
}

bool queueGroupLevelMoveToLevel(NodeEntry* node, uint16_t groupId,
                                uint8_t level) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsLevelControl ||
      !node->demoGroupConfigured) {
    return false;
  }

  const uint8_t commandPayload[] = {level, 0x00U, 0x00U};
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = true;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = kLevelControlCommandMoveToLevelWithOnOff;
  if (!ZigbeeCodec::buildZclFrame(frame, commandPayload,
                                  static_cast<uint8_t>(sizeof(commandPayload)),
                                  payload, &payloadLength)) {
    return false;
  }

  return queuePendingApsFrameExtended(
      node, kZigbeeApsDeliveryGroup, groupId, kZigbeeClusterLevelControl,
      kZigbeeProfileHomeAutomation, 0U, kCoordinatorEndpoint, payload,
      payloadLength);
}

bool queueGroupLevelStep(NodeEntry* node, uint16_t groupId, bool increase,
                         uint8_t stepSize) {
  if (node == nullptr || node->endpoint == 0U || !node->supportsLevelControl ||
      !node->demoGroupConfigured) {
    return false;
  }

  const uint8_t commandPayload[] = {
      static_cast<uint8_t>(increase ? 0x00U : 0x01U), stepSize, 0x00U, 0x00U};
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = true;
  frame.transactionSequence = g_zclSequence++;
  frame.commandId = kLevelControlCommandStepWithOnOff;
  if (!ZigbeeCodec::buildZclFrame(frame, commandPayload,
                                  static_cast<uint8_t>(sizeof(commandPayload)),
                                  payload, &payloadLength)) {
    return false;
  }

  return queuePendingApsFrameExtended(
      node, kZigbeeApsDeliveryGroup, groupId, kZigbeeClusterLevelControl,
      kZigbeeProfileHomeAutomation, 0U, kCoordinatorEndpoint, payload,
      payloadLength);
}

bool queueDemoGroupEnrollment() {
  bool queuedAny = false;
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    NodeEntry& node = g_nodes[i];
    if (!node.used || node.shortAddress == 0U || node.endpoint == 0U ||
        node.demoGroupConfigured ||
        (!node.supportsOnOff && !node.supportsLevelControl)) {
      continue;
    }
    queuedAny = queueAddGroup(&node, kDemoGroupId, "DemoGrp") || queuedAny;
  }
  return queuedAny;
}

bool queueDemoGroupOnOff(uint8_t commandId) {
  bool queuedAny = false;
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    queuedAny = queueGroupOnOffCommand(&g_nodes[i], kDemoGroupId, commandId) ||
                queuedAny;
  }
  return queuedAny;
}

bool queueDemoGroupLevelMoveToLevel(uint8_t level) {
  bool queuedAny = false;
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    queuedAny =
        queueGroupLevelMoveToLevel(&g_nodes[i], kDemoGroupId, level) ||
        queuedAny;
  }
  return queuedAny;
}

bool queueDemoGroupLevelStep(bool increase, uint8_t stepSize) {
  bool queuedAny = false;
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    queuedAny =
        queueGroupLevelStep(&g_nodes[i], kDemoGroupId, increase, stepSize) ||
        queuedAny;
  }
  return queuedAny;
}

NodeEntry* firstOnOffNode() {
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (g_nodes[i].used && g_nodes[i].shortAddress != 0U &&
        g_nodes[i].supportsOnOff && g_nodes[i].endpoint != 0U) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

NodeEntry* firstLevelNode() {
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (g_nodes[i].used && g_nodes[i].shortAddress != 0U &&
        g_nodes[i].supportsLevelControl && g_nodes[i].endpoint != 0U) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

NodeEntry* firstJoinedNode() {
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (g_nodes[i].used && g_nodes[i].shortAddress != 0U) {
      return &g_nodes[i];
    }
  }
  return nullptr;
}

void updateNodeAttributeState(NodeEntry* node, uint16_t clusterId,
                              const ZigbeeAttributeValue& value,
                              uint16_t attributeId) {
  if (node == nullptr) {
    return;
  }

  if (clusterId == kZigbeeClusterOnOff && attributeId == 0x0000U &&
      value.type == ZigbeeZclDataType::kBoolean) {
    node->haveOnOffState = true;
    node->onOffState = value.data.boolValue;
    return;
  }

  if (clusterId == kZigbeeClusterLevelControl && attributeId == 0x0000U &&
      value.type == ZigbeeZclDataType::kUint8) {
    node->haveLevelState = true;
    node->levelState = value.data.u8;
  }
}

void applyReadAttributeState(NodeEntry* node, uint16_t clusterId,
                             const ZigbeeReadAttributeRecord* records,
                             uint8_t recordCount) {
  if (node == nullptr || records == nullptr) {
    return;
  }
  for (uint8_t i = 0U; i < recordCount; ++i) {
    if (records[i].status != 0x00U) {
      continue;
    }
    updateNodeAttributeState(node, clusterId, records[i].value,
                             records[i].attributeId);
  }
}

void applyReportedAttributeState(NodeEntry* node, uint16_t clusterId,
                                 const ZigbeeAttributeReportRecord* records,
                                 uint8_t recordCount) {
  if (node == nullptr || records == nullptr) {
    return;
  }
  for (uint8_t i = 0U; i < recordCount; ++i) {
    updateNodeAttributeState(node, clusterId, records[i].value,
                             records[i].attributeId);
  }
}

bool queueNextReportingStep(NodeEntry* node) {
  if (node == nullptr) {
    return false;
  }
  if (node->supportsOnOff && !node->onOffBindingConfigured) {
    return queueBindRequest(node, kZigbeeClusterOnOff);
  }
  if (node->supportsOnOff && !node->onOffReportingConfigured) {
    return queueOnOffConfigureReporting(node);
  }
  if (node->supportsLevelControl && !node->levelBindingConfigured) {
    return queueBindRequest(node, kZigbeeClusterLevelControl);
  }
  if (node->supportsLevelControl && !node->levelReportingConfigured) {
    return queueLevelConfigureReporting(node);
  }
  if (node->supportsTemperature && !node->temperatureBindingConfigured) {
    return queueBindRequest(node, kZigbeeClusterTemperatureMeasurement);
  }
  if (node->supportsTemperature && !node->temperatureReportingConfigured) {
    return queueTemperatureConfigureReporting(node);
  }
  if (node->supportsPowerConfiguration && !node->powerBindingConfigured) {
    return queueBindRequest(node, kZigbeeClusterPowerConfiguration);
  }
  if (node->supportsPowerConfiguration && !node->powerReportingConfigured) {
    return queuePowerConfigureReporting(node);
  }
  return false;
}

void printAttributeValue(const ZigbeeAttributeValue& value) {
  switch (value.type) {
    case ZigbeeZclDataType::kBoolean:
      Serial.print(value.data.boolValue ? "true" : "false");
      break;
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      Serial.print(value.data.u8);
      break;
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
      Serial.print(value.data.u16);
      break;
    case ZigbeeZclDataType::kInt16:
      Serial.print(value.data.i16);
      break;
    case ZigbeeZclDataType::kUint32:
      Serial.print(value.data.u32);
      break;
    case ZigbeeZclDataType::kCharString:
      for (uint8_t i = 0U; i < value.stringLength; ++i) {
        Serial.print(value.stringValue[i]);
      }
      break;
    default:
      Serial.print("unsupported");
      break;
  }
}

void listNodes() {
  Serial.print("nodes\r\n");
  for (uint8_t i = 0U; i < kMaxNodes; ++i) {
    if (!g_nodes[i].used) {
      continue;
    }
    Serial.print(" slot=");
    Serial.print(i);
    Serial.print(" ieee=0x");
    Serial.print(static_cast<uint32_t>(g_nodes[i].ieeeAddress >> 32U), HEX);
    Serial.print(static_cast<uint32_t>(g_nodes[i].ieeeAddress & 0xFFFFFFFFUL), HEX);
    Serial.print(" short=0x");
    Serial.print(g_nodes[i].shortAddress, HEX);
    Serial.print(" ep=");
    Serial.print(g_nodes[i].endpoint);
    Serial.print(" dev=0x");
    Serial.print(g_nodes[i].deviceId, HEX);
    Serial.print(" onoff=");
    Serial.print(g_nodes[i].supportsOnOff ? "yes" : "no");
    Serial.print(" level=");
    Serial.print(g_nodes[i].supportsLevelControl ? "yes" : "no");
    Serial.print(" temp=");
    Serial.print(g_nodes[i].supportsTemperature ? "yes" : "no");
    Serial.print(" group=");
    Serial.print(g_nodes[i].demoGroupConfigured ? "0x1001" : "no");
    Serial.print(" pending=");
    Serial.print((g_nodes[i].pending.used || g_nodes[i].pendingAssociationResponse) ? "yes"
                                                                                     : "no");
    Serial.print(" state=");
    if (g_nodes[i].haveOnOffState) {
      Serial.print(g_nodes[i].onOffState ? "ON" : "OFF");
    } else {
      Serial.print("?");
    }
    if (g_nodes[i].supportsLevelControl) {
      Serial.print(" lvl=");
      if (g_nodes[i].haveLevelState) {
        Serial.print(g_nodes[i].levelState);
      } else {
        Serial.print("?");
      }
    }
    Serial.print("\r\n");
  }
}

void handleAssociationRequest(const ZigbeeMacAssociationRequestView& request,
                              int8_t rssiDbm) {
  if (request.coordinatorPanId != kPanId ||
      request.coordinatorShort != kCoordinatorShort) {
    return;
  }

  NodeEntry* node = allocateNode(request.deviceExtended);
  if (node == nullptr) {
    Serial.print("assoc_drop reason=no_slot\r\n");
    return;
  }

  if (node->shortAddress == 0U) {
    node->pendingAssignedShort = allocateShortAddress();
  } else {
    node->pendingAssignedShort = node->shortAddress;
  }
  node->pendingAssociationStatus = 0U;
  node->pendingAssociationResponse = true;
  node->lastSeenMs = millis();

  Serial.print("assoc ieee=0x");
  Serial.print(static_cast<uint32_t>(request.deviceExtended >> 32U), HEX);
  Serial.print(static_cast<uint32_t>(request.deviceExtended & 0xFFFFFFFFUL), HEX);
  Serial.print(" assigned=0x");
  Serial.print(node->pendingAssignedShort, HEX);
  Serial.print(" cap=0x");
  Serial.print(request.capabilityInformation, HEX);
  Serial.print(" rssi=");
  Serial.print(rssiDbm);
  Serial.print("dBm\r\n");
}

void handleDataRequest(const ZigbeeMacFrame& frame) {
  if (frame.source.mode != ZigbeeMacAddressMode::kExtended) {
    return;
  }

  NodeEntry* node = findNodeByIeee(frame.source.extendedAddress);
  if (node == nullptr) {
    return;
  }

  node->lastSeenMs = millis();
  if (node->pendingAssociationResponse) {
    const bool sent = sendPendingAssociationResponse(node);
    Serial.print("assoc_rsp ");
    Serial.print(sent ? "OK" : "FAIL");
    Serial.print(" short=0x");
    Serial.print(node->pendingAssignedShort, HEX);
    Serial.print("\r\n");
    return;
  }

  if (node->pending.used) {
    const bool sent = sendPendingApsFrame(node);
    Serial.print("poll_deliver ");
    Serial.print(sent ? "OK" : "FAIL");
    Serial.print(" dst=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print("\r\n");
  }
}

void handleZdoFrame(NodeEntry* node, const ZigbeeApsDataFrame& aps) {
  if (node == nullptr) {
    return;
  }

  if (aps.clusterId == kZigbeeZdoDeviceAnnounce && aps.payloadLength >= 12U) {
    const uint16_t announcedShort =
        static_cast<uint16_t>(aps.payload[1]) |
        (static_cast<uint16_t>(aps.payload[2]) << 8U);
    node->shortAddress = announcedShort;
    node->announced = true;
    Serial.print("device_announce short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" ieee=0x");
    Serial.print(static_cast<uint32_t>(node->ieeeAddress >> 32U), HEX);
    Serial.print(static_cast<uint32_t>(node->ieeeAddress & 0xFFFFFFFFUL), HEX);
    Serial.print("\r\n");
    if (node->endpoint == 0U) {
      (void)queueActiveEndpointsRequest(node);
    }
    return;
  }

  if (aps.clusterId == kZigbeeZdoActiveEndpointsResponse) {
    ZigbeeZdoActiveEndpointsResponseView view{};
    if (!ZigbeeCodec::parseZdoActiveEndpointsResponse(
            aps.payload, aps.payloadLength, &view) ||
        !view.valid || view.status != 0x00U || view.endpointCount == 0U) {
      return;
    }

    node->endpoint = view.endpoints[0];
    Serial.print("active_ep short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" ep=");
    Serial.print(node->endpoint);
    Serial.print("\r\n");
    (void)queueSimpleDescriptorRequest(node, node->endpoint);
    return;
  }

  if (aps.clusterId == kZigbeeZdoBindResponse) {
    uint8_t transactionSequence = 0U;
    uint8_t status = 0xFFU;
    if (!ZigbeeCodec::parseZdoStatusResponse(aps.payload, aps.payloadLength,
                                             &transactionSequence, &status)) {
      return;
    }

    Serial.print("bind_rsp short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" cluster=0x");
    Serial.print(node->awaitingClusterId, HEX);
    Serial.print(" status=0x");
    Serial.print(status, HEX);
    Serial.print("\r\n");

    if (node->awaitingClusterId == kZigbeeClusterOnOff) {
      node->onOffBindingConfigured = true;
    } else if (node->awaitingClusterId == kZigbeeClusterLevelControl) {
      node->levelBindingConfigured = true;
    } else if (node->awaitingClusterId ==
               kZigbeeClusterTemperatureMeasurement) {
      node->temperatureBindingConfigured = true;
    } else if (node->awaitingClusterId == kZigbeeClusterPowerConfiguration) {
      node->powerBindingConfigured = true;
    }
    node->awaitingBindResponse = false;
    if (queueNextReportingStep(node)) {
      return;
    }
    node->stage = NodeStage::kReady;
    return;
  }

  if (aps.clusterId == kZigbeeZdoSimpleDescriptorResponse) {
    ZigbeeZdoSimpleDescriptorResponseView view{};
    if (!ZigbeeCodec::parseZdoSimpleDescriptorResponse(
            aps.payload, aps.payloadLength, &view) ||
        !view.valid || view.status != 0x00U) {
      return;
    }

    node->endpoint = view.endpoint;
    node->profileId = view.profileId;
    node->deviceId = view.deviceId;
    node->supportsOnOff = false;
    node->supportsLevelControl = false;
    node->supportsIdentify = false;
    node->supportsTemperature = false;
    node->supportsPowerConfiguration = false;
    node->onOffBindingConfigured = false;
    node->levelBindingConfigured = false;
    node->temperatureBindingConfigured = false;
    node->powerBindingConfigured = false;
    node->onOffReportingConfigured = false;
    node->levelReportingConfigured = false;
    node->temperatureReportingConfigured = false;
    node->powerReportingConfigured = false;
    node->awaitingBindResponse = false;
    node->awaitingClusterId = 0U;
    node->demoGroupConfigured = false;
    for (uint8_t i = 0U; i < view.inputClusterCount; ++i) {
      if (view.inputClusters[i] == kZigbeeClusterOnOff) {
        node->supportsOnOff = true;
      } else if (view.inputClusters[i] == kZigbeeClusterLevelControl) {
        node->supportsLevelControl = true;
      } else if (view.inputClusters[i] == kZigbeeClusterIdentify) {
        node->supportsIdentify = true;
      } else if (view.inputClusters[i] == kZigbeeClusterTemperatureMeasurement) {
        node->supportsTemperature = true;
      } else if (view.inputClusters[i] == kZigbeeClusterPowerConfiguration) {
        node->supportsPowerConfiguration = true;
      }
    }

    Serial.print("simple_desc short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" ep=");
    Serial.print(node->endpoint);
    Serial.print(" profile=0x");
    Serial.print(node->profileId, HEX);
    Serial.print(" device=0x");
    Serial.print(node->deviceId, HEX);
    Serial.print(" onoff=");
    Serial.print(node->supportsOnOff ? "yes" : "no");
    Serial.print(" level=");
    Serial.print(node->supportsLevelControl ? "yes" : "no");
    Serial.print(" temp=");
    Serial.print(node->supportsTemperature ? "yes" : "no");
    Serial.print("\r\n");

    if (!queueBasicReadRequest(node)) {
      node->stage = NodeStage::kReady;
    }
  }
}

void handleHaFrame(NodeEntry* node, const ZigbeeApsDataFrame& aps) {
  if (node == nullptr) {
    return;
  }

  ZigbeeZclFrame zcl{};
  if (!ZigbeeCodec::parseZclFrame(aps.payload, aps.payloadLength, &zcl) ||
      !zcl.valid) {
    return;
  }

  if (zcl.frameType == ZigbeeZclFrameType::kGlobal &&
      zcl.commandId == 0x01U) {
    ZigbeeReadAttributeRecord records[8];
    uint8_t recordCount = 0U;
    if (!ZigbeeCodec::parseReadAttributesResponse(
            zcl.payload, zcl.payloadLength, records,
            static_cast<uint8_t>(sizeof(records) / sizeof(records[0])),
            &recordCount)) {
      return;
    }
    applyReadAttributeState(node, aps.clusterId, records, recordCount);

    Serial.print("read_rsp short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" cluster=0x");
    Serial.print(aps.clusterId, HEX);
    Serial.print(" ");
    for (uint8_t i = 0U; i < recordCount; ++i) {
      Serial.print("attr=0x");
      Serial.print(records[i].attributeId, HEX);
      Serial.print(":");
      if (records[i].status == 0x00U) {
        printAttributeValue(records[i].value);
      } else {
        Serial.print("status=0x");
        Serial.print(records[i].status, HEX);
      }
      Serial.print(" ");
    }
    Serial.print("\r\n");

    if (aps.clusterId == kZigbeeClusterBasic) {
      node->basicRead = true;
      if (!queueNextReportingStep(node)) {
        node->stage = NodeStage::kReady;
      }
    }
    return;
  }

  if (zcl.frameType == ZigbeeZclFrameType::kGlobal &&
      zcl.commandId == kZclCommandConfigureReportingResponse) {
    Serial.print("cfg_reporting_rsp short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" cluster=0x");
    Serial.print(aps.clusterId, HEX);
    Serial.print("\r\n");
    if (aps.clusterId == kZigbeeClusterOnOff) {
      node->onOffReportingConfigured = true;
    } else if (aps.clusterId == kZigbeeClusterLevelControl) {
      node->levelReportingConfigured = true;
    } else if (aps.clusterId == kZigbeeClusterTemperatureMeasurement) {
      node->temperatureReportingConfigured = true;
    } else if (aps.clusterId == kZigbeeClusterPowerConfiguration) {
      node->powerReportingConfigured = true;
    }
    if (queueNextReportingStep(node)) {
      return;
    }
    node->stage = NodeStage::kReady;
    return;
  }

  if (aps.clusterId == kZigbeeClusterGroups &&
      zcl.frameType == ZigbeeZclFrameType::kClusterSpecific &&
      zcl.commandId == kGroupsCommandAddGroup && zcl.payloadLength >= 3U) {
    const uint8_t status = zcl.payload[0];
    const uint16_t groupId = static_cast<uint16_t>(zcl.payload[1]) |
                             (static_cast<uint16_t>(zcl.payload[2]) << 8U);
    if (status == 0x00U && groupId == kDemoGroupId) {
      node->demoGroupConfigured = true;
    }
    Serial.print("group_add_rsp short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" group=0x");
    Serial.print(groupId, HEX);
    Serial.print(" status=0x");
    Serial.print(status, HEX);
    Serial.print("\r\n");
    return;
  }

  if (zcl.frameType == ZigbeeZclFrameType::kGlobal &&
      zcl.commandId == kZclCommandReportAttributes) {
    ZigbeeAttributeReportRecord records[8];
    uint8_t recordCount = 0U;
    if (!ZigbeeCodec::parseAttributeReport(
            zcl.payload, zcl.payloadLength, records,
            static_cast<uint8_t>(sizeof(records) / sizeof(records[0])),
            &recordCount)) {
      return;
    }
    applyReportedAttributeState(node, aps.clusterId, records, recordCount);

    Serial.print("report short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" cluster=0x");
    Serial.print(aps.clusterId, HEX);
    Serial.print(" ");
    for (uint8_t i = 0U; i < recordCount; ++i) {
      Serial.print("attr=0x");
      Serial.print(records[i].attributeId, HEX);
      Serial.print(":");
      printAttributeValue(records[i].value);
      Serial.print(" ");
    }
    Serial.print("\r\n");
    return;
  }

  if (zcl.frameType == ZigbeeZclFrameType::kGlobal &&
      zcl.commandId == kZclCommandDefaultResponse && zcl.payloadLength >= 2U) {
    Serial.print("default_rsp short=0x");
    Serial.print(node->shortAddress, HEX);
    Serial.print(" cmd=0x");
    Serial.print(zcl.payload[0], HEX);
    Serial.print(" status=0x");
    Serial.print(zcl.payload[1], HEX);
    Serial.print("\r\n");
  }
}

void processIncomingFrame(const ZigbeeFrame& frame) {
  ZigbeeMacFrame mac{};
  if (!ZigbeeCodec::parseMacFrame(frame.psdu, frame.length, &mac) || !mac.valid) {
    return;
  }

  if (mac.frameType == ZigbeeMacFrameType::kCommand) {
    if (mac.commandId == kZigbeeMacCommandBeaconRequest) {
      (void)sendBeacon();
      return;
    }

    ZigbeeMacAssociationRequestView association{};
    if (ZigbeeCodec::parseAssociationRequest(frame.psdu, frame.length,
                                             &association)) {
      handleAssociationRequest(association, frame.rssiDbm);
      return;
    }

    if (mac.commandId == kZigbeeMacCommandDataRequest &&
        mac.destination.mode == ZigbeeMacAddressMode::kShort &&
        mac.destination.panId == kPanId &&
        mac.destination.shortAddress == kCoordinatorShort) {
      handleDataRequest(mac);
      return;
    }
    return;
  }

  ZigbeeDataFrameView macData{};
  if (!ZigbeeRadio::parseDataFrameShort(frame.psdu, frame.length, &macData) ||
      !macData.valid || macData.panId != kPanId ||
      macData.destinationShort != kCoordinatorShort) {
    return;
  }

  NodeEntry* node = findNodeByShort(macData.sourceShort);
  if (node == nullptr) {
    return;
  }
  node->lastSeenMs = millis();

  ZigbeeNetworkFrame nwk{};
  ZigbeeNwkSecurityHeader security{};
  uint8_t decryptedPayload[127] = {0U};
  uint8_t decryptedPayloadLength = 0U;
  bool nwkValid = ZigbeeSecurity::parseSecuredNwkFrame(
      macData.payload, macData.payloadLength, kDemoNetworkKey, &nwk, &security,
      decryptedPayload, &decryptedPayloadLength);
  if (nwkValid &&
      (!security.valid || security.sourceIeee != node->ieeeAddress ||
       security.keySequence != kDemoNetworkKeySequence ||
       security.frameCounter <= node->lastInboundSecurityFrameCounter)) {
    return;
  }
  if (!nwkValid) {
    nwkValid =
        ZigbeeCodec::parseNwkFrame(macData.payload, macData.payloadLength, &nwk);
  }
  if (!nwkValid || !nwk.valid || nwk.destinationShort != kCoordinatorShort) {
    return;
  }
  if (security.valid) {
    node->secureNwkSeen = true;
    node->lastInboundSecurityFrameCounter = security.frameCounter;
  }

  ZigbeeApsDataFrame aps{};
  if (!ZigbeeCodec::parseApsDataFrame(nwk.payload, nwk.payloadLength, &aps) ||
      !aps.valid) {
    return;
  }

  if (aps.profileId == kZigbeeProfileZdo) {
    handleZdoFrame(node, aps);
    return;
  }
  if (aps.profileId == kZigbeeProfileHomeAutomation) {
    handleHaFrame(node, aps);
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch == 'b') {
      const bool sent = sendBeacon();
      Serial.print("beacon ");
      Serial.print(sent ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 'l') {
      listNodes();
    } else if (ch == 'd') {
      NodeEntry* node = firstJoinedNode();
      const bool queued = (node != nullptr) && queueActiveEndpointsRequest(node);
      Serial.print("discover ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 't' || ch == 'o' || ch == 'f') {
      NodeEntry* node = firstOnOffNode();
      uint8_t commandId = kOnOffCommandToggle;
      if (ch == 'o') {
        commandId = kOnOffCommandOn;
      } else if (ch == 'f') {
        commandId = kOnOffCommandOff;
      }
      const bool queued = (node != nullptr) && queueOnOffCommand(node, commandId);
      Serial.print("queue_cmd ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 'U' || ch == 'D' || ch == 'M') {
      NodeEntry* node = firstLevelNode();
      bool queued = false;
      if (ch == 'U') {
        queued = (node != nullptr) && queueLevelStep(node, true, 32U);
      } else if (ch == 'D') {
        queued = (node != nullptr) && queueLevelStep(node, false, 32U);
      } else {
        queued = (node != nullptr) && queueLevelMoveToLevel(node, 128U);
      }
      Serial.print("queue_level ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 'g') {
      const bool queued = queueDemoGroupEnrollment();
      Serial.print("queue_group_enroll ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print(" group=0x");
      Serial.print(kDemoGroupId, HEX);
      Serial.print("\r\n");
    } else if (ch == 'O' || ch == 'F' || ch == 'T') {
      uint8_t commandId = kOnOffCommandToggle;
      if (ch == 'O') {
        commandId = kOnOffCommandOn;
      } else if (ch == 'F') {
        commandId = kOnOffCommandOff;
      }
      const bool queued = queueDemoGroupOnOff(commandId);
      Serial.print("queue_group_cmd ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print(" group=0x");
      Serial.print(kDemoGroupId, HEX);
      Serial.print("\r\n");
    } else if (ch == '+' || ch == '-' || ch == 'm') {
      bool queued = false;
      if (ch == '+') {
        queued = queueDemoGroupLevelStep(true, 32U);
      } else if (ch == '-') {
        queued = queueDemoGroupLevelStep(false, 32U);
      } else {
        queued = queueDemoGroupLevelMoveToLevel(128U);
      }
      Serial.print("queue_group_level ");
      Serial.print(queued ? "OK" : "FAIL");
      Serial.print(" group=0x");
      Serial.print(kDemoGroupId, HEX);
      Serial.print("\r\n");
    }
  }
}

void pumpRadio() {
  ZigbeeFrame frame{};
  if (!g_radio.receive(&frame, 5000U, 900000UL)) {
    return;
  }
  processIncomingFrame(frame);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  const bool ok = g_radio.begin(kChannel, 8);
  Serial.print("\r\nZigbeeHaCoordinatorJoinDemo start\r\n");
  Serial.print("radio=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" channel=");
  Serial.print(kChannel);
  Serial.print(" pan=0x");
  Serial.print(kPanId, HEX);
  Serial.print(" extpan=0x");
  Serial.print(static_cast<uint32_t>(kExtendedPanId >> 32U), HEX);
  Serial.print(static_cast<uint32_t>(kExtendedPanId & 0xFFFFFFFFUL), HEX);
  Serial.print("\r\n");
  Serial.print("serial commands: b=beacon l=list d=discover t=toggle o=on f=off U=brighter D=dimmer M=mid g=enroll_group O/F/T=group on/off/toggle +/-/m=group level\r\n");
}

void loop() {
  handleSerialCommands();
  pumpRadio();

  const uint32_t now = millis();
  if ((now - g_lastBeaconMs) >= 1500U) {
    g_lastBeaconMs = now;
    (void)sendBeacon();
  }

  if ((now - g_lastStatusMs) >= 5000U) {
    g_lastStatusMs = now;
    uint8_t joined = 0U;
    uint8_t pending = 0U;
    for (uint8_t i = 0U; i < kMaxNodes; ++i) {
      if (!g_nodes[i].used || g_nodes[i].shortAddress == 0U) {
        continue;
      }
      ++joined;
      if (g_nodes[i].pending.used || g_nodes[i].pendingAssociationResponse) {
        ++pending;
      }
    }

    Serial.print("alive joined=");
    Serial.print(joined);
    Serial.print(" pending=");
    Serial.print(pending);
    Serial.print("\r\n");
    Gpio::toggle(kPinUserLed);
  }
}
