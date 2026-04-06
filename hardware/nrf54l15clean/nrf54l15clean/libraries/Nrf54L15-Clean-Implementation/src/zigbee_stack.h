#pragma once

#include <stddef.h>
#include <stdint.h>

namespace xiao_nrf54l15 {

constexpr uint16_t kZigbeeProfileZdo = 0x0000U;
constexpr uint16_t kZigbeeProfileHomeAutomation = 0x0104U;

constexpr uint16_t kZigbeeClusterBasic = 0x0000U;
constexpr uint16_t kZigbeeClusterPowerConfiguration = 0x0001U;
constexpr uint16_t kZigbeeClusterIdentify = 0x0003U;
constexpr uint16_t kZigbeeClusterGroups = 0x0004U;
constexpr uint16_t kZigbeeClusterScenes = 0x0005U;
constexpr uint16_t kZigbeeClusterOnOff = 0x0006U;
constexpr uint16_t kZigbeeClusterLevelControl = 0x0008U;
constexpr uint16_t kZigbeeClusterOtaUpgrade = 0x0019U;
constexpr uint16_t kZigbeeClusterTemperatureMeasurement = 0x0402U;

constexpr uint8_t kZigbeeIdentifyEffectBlink = 0x00U;
constexpr uint8_t kZigbeeIdentifyEffectBreathe = 0x01U;
constexpr uint8_t kZigbeeIdentifyEffectOkay = 0x02U;
constexpr uint8_t kZigbeeIdentifyEffectChannelChange = 0x0BU;
constexpr uint8_t kZigbeeIdentifyEffectFinishEffect = 0xFEU;
constexpr uint8_t kZigbeeIdentifyEffectStopEffect = 0xFFU;
constexpr uint8_t kZigbeeIdentifyEffectNone = 0xFFU;

constexpr uint16_t kZigbeeZdoNetworkAddressRequest = 0x0000U;
constexpr uint16_t kZigbeeZdoIeeeAddressRequest = 0x0001U;
constexpr uint16_t kZigbeeZdoNodeDescriptorRequest = 0x0002U;
constexpr uint16_t kZigbeeZdoPowerDescriptorRequest = 0x0003U;
constexpr uint16_t kZigbeeZdoSimpleDescriptorRequest = 0x0004U;
constexpr uint16_t kZigbeeZdoActiveEndpointsRequest = 0x0005U;
constexpr uint16_t kZigbeeZdoMatchDescriptorRequest = 0x0006U;
constexpr uint16_t kZigbeeZdoDeviceAnnounce = 0x0013U;
constexpr uint16_t kZigbeeZdoExtendedSimpleDescriptorRequest = 0x001DU;
constexpr uint16_t kZigbeeZdoExtendedActiveEndpointsRequest = 0x001EU;
constexpr uint16_t kZigbeeZdoEndDeviceBindRequest = 0x0020U;
constexpr uint16_t kZigbeeZdoMgmtLqiRequest = 0x0031U;
constexpr uint16_t kZigbeeZdoMgmtRtgRequest = 0x0032U;
constexpr uint16_t kZigbeeZdoMgmtBindRequest = 0x0033U;
constexpr uint16_t kZigbeeZdoBindRequest = 0x0021U;
constexpr uint16_t kZigbeeZdoUnbindRequest = 0x0022U;
constexpr uint16_t kZigbeeZdoMgmtLeaveRequest = 0x0034U;
constexpr uint16_t kZigbeeZdoMgmtPermitJoinRequest = 0x0036U;

constexpr uint16_t kZigbeeZdoNetworkAddressResponse = 0x8000U;
constexpr uint16_t kZigbeeZdoIeeeAddressResponse = 0x8001U;
constexpr uint16_t kZigbeeZdoNodeDescriptorResponse = 0x8002U;
constexpr uint16_t kZigbeeZdoPowerDescriptorResponse = 0x8003U;
constexpr uint16_t kZigbeeZdoSimpleDescriptorResponse = 0x8004U;
constexpr uint16_t kZigbeeZdoActiveEndpointsResponse = 0x8005U;
constexpr uint16_t kZigbeeZdoMatchDescriptorResponse = 0x8006U;
constexpr uint16_t kZigbeeZdoExtendedSimpleDescriptorResponse = 0x801DU;
constexpr uint16_t kZigbeeZdoExtendedActiveEndpointsResponse = 0x801EU;
constexpr uint16_t kZigbeeZdoEndDeviceBindResponse = 0x8020U;
constexpr uint16_t kZigbeeZdoMgmtLqiResponse = 0x8031U;
constexpr uint16_t kZigbeeZdoMgmtRtgResponse = 0x8032U;
constexpr uint16_t kZigbeeZdoMgmtBindResponse = 0x8033U;
constexpr uint16_t kZigbeeZdoBindResponse = 0x8021U;
constexpr uint16_t kZigbeeZdoUnbindResponse = 0x8022U;
constexpr uint16_t kZigbeeZdoMgmtLeaveResponse = 0x8034U;
constexpr uint16_t kZigbeeZdoMgmtPermitJoinResponse = 0x8036U;

constexpr uint8_t kZigbeeMgmtLeaveFlagRemoveChildren = 0x40U;
constexpr uint8_t kZigbeeMgmtLeaveFlagRejoin = 0x80U;

constexpr uint16_t kZigbeeDeviceIdOnOffLight = 0x0100U;
constexpr uint16_t kZigbeeDeviceIdDimmableLight = 0x0101U;
constexpr uint16_t kZigbeeDeviceIdTemperatureSensor = 0x0302U;

constexpr uint8_t kZigbeeApsDeliveryUnicast = 0x00U;
constexpr uint8_t kZigbeeApsDeliveryIndirect = 0x01U;
constexpr uint8_t kZigbeeApsDeliveryBroadcast = 0x02U;
constexpr uint8_t kZigbeeApsDeliveryGroup = 0x03U;
constexpr uint8_t kZigbeeApsCommandTransportKey = 0x05U;
constexpr uint8_t kZigbeeApsCommandUpdateDevice = 0x06U;
constexpr uint8_t kZigbeeApsCommandSwitchKey = 0x09U;
constexpr uint8_t kZigbeeApsTransportKeyStandardNetworkKey = 0x01U;
constexpr uint8_t kZigbeeApsUpdateDeviceStatusStandardSecureRejoin = 0x00U;

constexpr uint8_t kZigbeeNwkCommandRejoinRequest = 0x06U;
constexpr uint8_t kZigbeeNwkCommandRejoinResponse = 0x07U;
constexpr uint8_t kZigbeeNwkCommandEndDeviceTimeoutRequest = 0x0BU;
constexpr uint8_t kZigbeeNwkCommandEndDeviceTimeoutResponse = 0x0CU;
constexpr uint8_t kZigbeeNwkEndDeviceTimeoutSuccess = 0x00U;
constexpr uint8_t kZigbeeNwkParentInfoMacDataPollKeepalive = 0x01U;
constexpr uint8_t kZigbeeNwkParentInfoEndDeviceTimeoutSupported = 0x02U;

constexpr uint8_t kZigbeeMacCommandAssociationRequest = 0x01U;
constexpr uint8_t kZigbeeMacCommandAssociationResponse = 0x02U;
constexpr uint8_t kZigbeeMacCommandDataRequest = 0x04U;
constexpr uint8_t kZigbeeMacCommandOrphanNotification = 0x06U;
constexpr uint8_t kZigbeeMacCommandBeaconRequest = 0x07U;
constexpr uint8_t kZigbeeMacCommandCoordinatorRealignment = 0x08U;

enum class ZigbeeMacFrameType : uint8_t {
  kBeacon = 0U,
  kData = 1U,
  kAcknowledgement = 2U,
  kCommand = 3U,
};

enum class ZigbeeMacAddressMode : uint8_t {
  kNone = 0U,
  kReserved = 1U,
  kShort = 2U,
  kExtended = 3U,
};

enum class ZigbeeLogicalType : uint8_t {
  kCoordinator = 0U,
  kRouter = 1U,
  kEndDevice = 2U,
};

enum class ZigbeeNwkFrameType : uint8_t {
  kData = 0U,
  kCommand = 1U,
  kReserved = 2U,
  kInterPan = 3U,
};

enum class ZigbeeApsFrameType : uint8_t {
  kData = 0U,
  kCommand = 1U,
  kAcknowledgement = 2U,
};

enum class ZigbeeZclFrameType : uint8_t {
  kGlobal = 0U,
  kClusterSpecific = 1U,
};

enum class ZigbeeZclDataType : uint8_t {
  kBoolean = 0x10U,
  kBitmap8 = 0x18U,
  kBitmap16 = 0x19U,
  kBitmap32 = 0x1BU,
  kUint8 = 0x20U,
  kUint16 = 0x21U,
  kUint32 = 0x23U,
  kInt16 = 0x29U,
  kCharString = 0x42U,
};

enum class ZigbeeBindingAddressMode : uint8_t {
  kGroup = 0x01U,
  kExtended = 0x03U,
};

struct ZigbeeMacAddress {
  ZigbeeMacAddressMode mode = ZigbeeMacAddressMode::kNone;
  uint16_t panId = 0U;
  uint16_t shortAddress = 0U;
  uint64_t extendedAddress = 0U;
};

struct ZigbeeMacFrame {
  bool valid = false;
  ZigbeeMacFrameType frameType = ZigbeeMacFrameType::kData;
  bool securityEnabled = false;
  bool framePending = false;
  bool ackRequested = false;
  bool panCompression = false;
  // Zigbee uses legacy IEEE 802.15.4 MAC framing on the air.
  uint8_t frameVersion = 0U;
  uint8_t sequence = 0U;
  ZigbeeMacAddress destination{};
  ZigbeeMacAddress source{};
  uint8_t commandId = 0U;
  const uint8_t* payload = nullptr;
  uint8_t payloadLength = 0U;
};

struct ZigbeeMacAssociationRequestView {
  bool valid = false;
  uint8_t sequence = 0U;
  uint16_t coordinatorPanId = 0U;
  uint16_t coordinatorShort = 0U;
  uint64_t deviceExtended = 0U;
  uint8_t capabilityInformation = 0U;
};

struct ZigbeeMacAssociationResponseView {
  bool valid = false;
  uint8_t sequence = 0U;
  uint16_t panId = 0U;
  uint16_t coordinatorShort = 0U;
  uint64_t destinationExtended = 0U;
  uint16_t assignedShort = 0U;
  uint8_t status = 0U;
};

struct ZigbeeMacOrphanNotificationView {
  bool valid = false;
  uint8_t sequence = 0U;
  uint16_t panId = 0xFFFFU;
  uint64_t deviceExtended = 0U;
};

struct ZigbeeMacCoordinatorRealignmentView {
  bool valid = false;
  uint8_t sequence = 0U;
  uint16_t panId = 0U;
  uint16_t coordinatorShort = 0U;
  uint8_t channel = 0U;
  uint16_t assignedShort = 0U;
  uint64_t destinationExtended = 0U;
};

struct ZigbeeMacBeaconPayload {
  bool valid = false;
  uint8_t protocolId = 0U;
  uint8_t stackProfile = 0U;
  uint8_t protocolVersion = 0U;
  bool panCoordinator = true;
  bool associationPermit = true;
  bool routerCapacity = false;
  bool endDeviceCapacity = false;
  uint64_t extendedPanId = 0U;
  uint32_t txOffset = 0U;
  uint8_t updateId = 0U;
};

struct ZigbeeMacBeaconView {
  bool valid = false;
  uint8_t sequence = 0U;
  uint16_t panId = 0U;
  uint16_t sourceShort = 0U;
  uint16_t superframeSpecification = 0U;
  bool panCoordinator = false;
  bool associationPermit = false;
  ZigbeeMacBeaconPayload network{};
};

struct ZigbeeNetworkFrame {
  bool valid = false;
  ZigbeeNwkFrameType frameType = ZigbeeNwkFrameType::kData;
  uint8_t discoverRoute = 0U;
  bool multicast = false;
  bool securityEnabled = false;
  bool sourceRoute = false;
  bool extendedDestination = false;
  bool extendedSource = false;
  uint16_t destinationShort = 0U;
  uint16_t sourceShort = 0U;
  uint8_t radius = 0U;
  uint8_t sequence = 0U;
  uint64_t destinationExtended = 0U;
  uint64_t sourceExtended = 0U;
  const uint8_t* payload = nullptr;
  uint8_t payloadLength = 0U;
};

struct ZigbeeNwkRejoinRequest {
  bool valid = false;
  uint8_t capabilityInformation = 0U;
};

struct ZigbeeNwkRejoinResponse {
  bool valid = false;
  uint16_t networkAddress = 0U;
  uint8_t status = 0U;
};

struct ZigbeeNwkEndDeviceTimeoutRequest {
  bool valid = false;
  uint8_t requestedTimeout = 0U;
  uint8_t endDeviceConfiguration = 0U;
};

struct ZigbeeNwkEndDeviceTimeoutResponse {
  bool valid = false;
  uint8_t status = 0U;
  uint8_t parentInformation = 0U;
};

struct ZigbeeApsDataFrame {
  bool valid = false;
  ZigbeeApsFrameType frameType = ZigbeeApsFrameType::kData;
  uint8_t deliveryMode = kZigbeeApsDeliveryUnicast;
  bool securityEnabled = false;
  bool ackRequested = false;
  uint8_t destinationEndpoint = 0U;
  uint16_t destinationGroup = 0U;
  uint16_t clusterId = 0U;
  uint16_t profileId = 0U;
  uint8_t sourceEndpoint = 0U;
  uint8_t counter = 0U;
  const uint8_t* payload = nullptr;
  uint8_t payloadLength = 0U;
};

struct ZigbeeApsCommandFrame {
  bool valid = false;
  ZigbeeApsFrameType frameType = ZigbeeApsFrameType::kCommand;
  uint8_t deliveryMode = kZigbeeApsDeliveryUnicast;
  bool securityEnabled = false;
  bool ackRequested = false;
  uint8_t counter = 0U;
  uint8_t commandId = 0U;
  const uint8_t* payload = nullptr;
  uint8_t payloadLength = 0U;
};

struct ZigbeeApsAcknowledgementFrame {
  bool valid = false;
  ZigbeeApsFrameType frameType = ZigbeeApsFrameType::kAcknowledgement;
  uint8_t deliveryMode = kZigbeeApsDeliveryUnicast;
  bool securityEnabled = false;
  bool ackFormatCommand = false;
  uint8_t destinationEndpoint = 0U;
  uint16_t clusterId = 0U;
  uint16_t profileId = 0U;
  uint8_t sourceEndpoint = 0U;
  uint8_t counter = 0U;
};

struct ZigbeeApsTransportKey {
  bool valid = false;
  uint8_t keyType = kZigbeeApsTransportKeyStandardNetworkKey;
  uint8_t key[16] = {0U};
  uint8_t keySequence = 0U;
  uint64_t destinationIeee = 0U;
  uint64_t sourceIeee = 0U;
};

struct ZigbeeApsUpdateDevice {
  bool valid = false;
  uint64_t deviceIeee = 0U;
  uint16_t deviceShort = 0U;
  uint8_t status = 0U;
};

struct ZigbeeApsSwitchKey {
  bool valid = false;
  uint8_t keySequence = 0U;
};

struct ZigbeeZclFrame {
  bool valid = false;
  ZigbeeZclFrameType frameType = ZigbeeZclFrameType::kGlobal;
  bool manufacturerSpecific = false;
  bool directionToClient = false;
  bool disableDefaultResponse = false;
  uint16_t manufacturerCode = 0U;
  uint8_t transactionSequence = 0U;
  uint8_t commandId = 0U;
  const uint8_t* payload = nullptr;
  uint8_t payloadLength = 0U;
};

struct ZigbeeAttributeValue {
  ZigbeeZclDataType type = ZigbeeZclDataType::kUint8;
  union {
    bool boolValue;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    int16_t i16;
  } data = {};
  const char* stringValue = nullptr;
  uint8_t stringLength = 0U;
};

struct ZigbeeReadAttributeRecord {
  uint16_t attributeId = 0U;
  uint8_t status = 0x86U;
  ZigbeeAttributeValue value{};
};

struct ZigbeeDiscoveredAttributeRecord {
  uint16_t attributeId = 0U;
  ZigbeeZclDataType dataType = ZigbeeZclDataType::kUint8;
};

struct ZigbeeDiscoveredExtendedAttributeRecord {
  uint16_t attributeId = 0U;
  ZigbeeZclDataType dataType = ZigbeeZclDataType::kUint8;
  uint8_t accessControl = 0U;
};

struct ZigbeeAttributeReportRecord {
  uint16_t attributeId = 0U;
  ZigbeeAttributeValue value{};
};

struct ZigbeeWriteAttributeRecord {
  uint16_t attributeId = 0U;
  ZigbeeAttributeValue value{};
};

struct ZigbeeWriteAttributeStatusRecord {
  uint8_t status = 0x86U;
  uint16_t attributeId = 0U;
};

struct ZigbeeReportingConfiguration {
  bool used = false;
  uint16_t clusterId = 0U;
  uint16_t attributeId = 0U;
  ZigbeeZclDataType dataType = ZigbeeZclDataType::kUint8;
  uint16_t minimumIntervalSeconds = 0U;
  uint16_t maximumIntervalSeconds = 0U;
  uint32_t reportableChange = 0U;
};

struct ZigbeeReportingRuntimeState {
  bool baselineValid = false;
  bool pending = false;
  uint32_t lastReportMs = 0U;
  ZigbeeAttributeValue lastReportedValue{};
  ZigbeeAttributeValue pendingValue{};
};

struct ZigbeeConfigureReportingStatusRecord {
  uint8_t status = 0U;
  uint8_t direction = 0U;
  uint16_t attributeId = 0U;
};

struct ZigbeeReadReportingConfigurationRecord {
  uint8_t direction = 0U;
  uint16_t attributeId = 0U;
};

struct ZigbeeReadReportingConfigurationResponseRecord {
  uint8_t status = 0U;
  uint8_t direction = 0U;
  uint16_t attributeId = 0U;
  ZigbeeZclDataType dataType = ZigbeeZclDataType::kUint8;
  uint16_t minimumIntervalSeconds = 0U;
  uint16_t maximumIntervalSeconds = 0U;
  uint16_t timeoutPeriodSeconds = 0U;
  uint32_t reportableChange = 0U;
};

struct ZigbeeZdoAddressResponseView {
  bool valid = false;
  uint8_t transactionSequence = 0U;
  uint8_t status = 0U;
  uint64_t ieeeAddress = 0U;
  uint16_t nwkAddress = 0U;
  uint8_t associatedDeviceCount = 0U;
  uint8_t startIndex = 0U;
  uint8_t associatedDeviceListCount = 0U;
  uint16_t associatedDevices[8] = {0U};
};

struct ZigbeeZdoNodeDescriptorResponseView {
  bool valid = false;
  uint8_t transactionSequence = 0U;
  uint8_t status = 0U;
  uint16_t nwkAddressOfInterest = 0U;
  uint8_t logicalType = 0U;
  uint8_t frequencyBand = 0U;
  uint8_t macCapabilityFlags = 0U;
  uint16_t manufacturerCode = 0U;
  uint8_t maxBufferSize = 0U;
  uint16_t maxIncomingTransferSize = 0U;
  uint16_t serverMask = 0U;
  uint16_t maxOutgoingTransferSize = 0U;
  uint8_t descriptorCapability = 0U;
};

struct ZigbeeZdoPowerDescriptorResponseView {
  bool valid = false;
  uint8_t transactionSequence = 0U;
  uint8_t status = 0U;
  uint16_t nwkAddressOfInterest = 0U;
  uint8_t availablePowerSources = 0U;
  uint8_t currentPowerSource = 0U;
  uint8_t currentPowerSourceLevel = 0U;
};

struct ZigbeeZdoActiveEndpointsResponseView {
  bool valid = false;
  uint8_t transactionSequence = 0U;
  uint8_t status = 0U;
  uint16_t nwkAddressOfInterest = 0U;
  uint8_t endpointCount = 0U;
  uint8_t endpoints[8] = {0U};
};

struct ZigbeeZdoSimpleDescriptorResponseView {
  bool valid = false;
  uint8_t transactionSequence = 0U;
  uint8_t status = 0U;
  uint16_t nwkAddressOfInterest = 0U;
  uint8_t endpoint = 0U;
  uint16_t profileId = 0U;
  uint16_t deviceId = 0U;
  uint8_t deviceVersion = 0U;
  uint8_t inputClusterCount = 0U;
  uint16_t inputClusters[8] = {0U};
  uint8_t outputClusterCount = 0U;
  uint16_t outputClusters[8] = {0U};
};

struct ZigbeeSimpleDescriptor {
  uint8_t endpoint = 0U;
  uint16_t profileId = 0U;
  uint16_t deviceId = 0U;
  uint8_t deviceVersion = 0U;
  const uint16_t* inputClusters = nullptr;
  uint8_t inputClusterCount = 0U;
  const uint16_t* outputClusters = nullptr;
  uint8_t outputClusterCount = 0U;
};

struct ZigbeeBasicClusterConfig {
  const char* manufacturerName = nullptr;
  const char* modelIdentifier = nullptr;
  const char* swBuildId = nullptr;
  uint8_t zclVersion = 3U;
  uint8_t applicationVersion = 1U;
  uint8_t stackVersion = 1U;
  uint8_t hwVersion = 1U;
  uint8_t powerSource = 0U;
};

struct ZigbeePowerConfigurationState {
  bool batteryBacked = false;
  uint8_t batteryVoltageDecivolts = 0U;
  uint8_t batteryPercentageRemainingHalf = 0U;
};

struct ZigbeeTemperatureMeasurementState {
  bool enabled = false;
  int16_t measuredValueCentiDegrees = 0;
  int16_t minMeasuredValueCentiDegrees = 0;
  int16_t maxMeasuredValueCentiDegrees = 0;
  uint16_t toleranceCentiDegrees = 0U;
};

struct ZigbeeOnOffState {
  bool enabled = false;
  bool on = false;
};

struct ZigbeeIdentifyState {
  bool enabled = false;
  uint16_t identifyTimeSeconds = 0U;
  uint8_t effectIdentifier = kZigbeeIdentifyEffectNone;
};

struct ZigbeeLevelControlState {
  bool enabled = false;
  uint8_t currentLevel = 0U;
  uint8_t minLevel = 1U;
  uint8_t maxLevel = 0xFEU;
};

struct ZigbeeGroupEntry {
  bool used = false;
  uint16_t groupId = 0U;
  uint8_t nameLength = 0U;
  char name[16] = {0};
};

struct ZigbeeGroupsState {
  bool enabled = false;
  ZigbeeGroupEntry entries[8] = {};
};

struct ZigbeeSceneEntry {
  bool used = false;
  uint16_t groupId = 0U;
  uint8_t sceneId = 0U;
  uint16_t transitionTimeDeciseconds = 0U;
  uint8_t nameLength = 0U;
  char name[16] = {0};
  bool hasOnOff = false;
  bool onOff = false;
  bool hasLevel = false;
  uint8_t level = 0U;
};

struct ZigbeeScenesState {
  bool enabled = false;
  uint16_t currentGroupId = 0U;
  uint8_t currentSceneId = 0U;
  bool sceneValid = false;
  ZigbeeSceneEntry entries[8] = {};
};

struct ZigbeeBindingEntry {
  bool used = false;
  uint8_t sourceEndpoint = 0U;
  uint16_t clusterId = 0U;
  ZigbeeBindingAddressMode destinationAddressMode =
      ZigbeeBindingAddressMode::kExtended;
  uint16_t destinationGroup = 0U;
  uint64_t destinationIeee = 0U;
  uint8_t destinationEndpoint = 0U;
};

struct ZigbeeResolvedBindingDestination {
  ZigbeeBindingAddressMode addressMode = ZigbeeBindingAddressMode::kExtended;
  uint16_t groupId = 0U;
  uint64_t ieeeAddress = 0U;
  uint8_t endpoint = 0U;
};

struct ZigbeeHomeAutomationConfig {
  ZigbeeLogicalType logicalType = ZigbeeLogicalType::kEndDevice;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t panId = 0U;
  uint8_t endpointCount = 0U;
  ZigbeeSimpleDescriptor endpointDescriptors[4] = {};
  ZigbeeBasicClusterConfig basic{};
  ZigbeePowerConfigurationState power{};
  ZigbeeTemperatureMeasurementState temperature{};
  ZigbeeIdentifyState identify{};
  ZigbeeGroupsState groups{};
  ZigbeeScenesState scenes{};
  ZigbeeOnOffState onOff{};
  ZigbeeLevelControlState level{};
  ZigbeeBindingEntry bindings[8] = {};
};

class ZigbeeCodec {
 public:
  static bool buildMacFrame(const ZigbeeMacFrame& frame, const uint8_t* payload,
                            uint8_t payloadLength, uint8_t* outFrame,
                            uint8_t* outLength);
  static bool parseMacFrame(const uint8_t* frame, uint8_t length,
                            ZigbeeMacFrame* outFrame);
  static bool buildAssociationRequest(uint8_t sequence, uint16_t panId,
                                      uint16_t coordinatorShort,
                                      uint64_t deviceExtended,
                                      uint8_t capabilityInformation,
                                      uint8_t* outFrame, uint8_t* outLength);
  static bool parseAssociationRequest(
      const uint8_t* frame, uint8_t length,
      ZigbeeMacAssociationRequestView* outView);
  static bool buildAssociationResponse(uint8_t sequence, uint16_t panId,
                                       uint64_t destinationExtended,
                                       uint16_t coordinatorShort,
                                       uint16_t assignedShort, uint8_t status,
                                       uint8_t* outFrame, uint8_t* outLength);
  static bool parseAssociationResponse(
      const uint8_t* frame, uint8_t length,
      ZigbeeMacAssociationResponseView* outView);
  static bool buildOrphanNotification(uint8_t sequence,
                                      uint64_t deviceExtended,
                                      uint8_t* outFrame,
                                      uint8_t* outLength);
  static bool parseOrphanNotification(
      const uint8_t* frame, uint8_t length,
      ZigbeeMacOrphanNotificationView* outView);
  static bool buildCoordinatorRealignment(uint8_t sequence, uint16_t panId,
                                          uint16_t coordinatorShort,
                                          uint8_t channel,
                                          uint16_t assignedShort,
                                          uint64_t destinationExtended,
                                          uint8_t* outFrame,
                                          uint8_t* outLength);
  static bool parseCoordinatorRealignment(
      const uint8_t* frame, uint8_t length,
      ZigbeeMacCoordinatorRealignmentView* outView);
  static bool buildBeaconFrame(uint8_t sequence, uint16_t panId,
                               uint16_t coordinatorShort,
                               const ZigbeeMacBeaconPayload& payload,
                               uint8_t* outFrame, uint8_t* outLength);
  static bool parseBeaconFrame(const uint8_t* frame, uint8_t length,
                               ZigbeeMacBeaconView* outView);
  static bool buildBeaconRequest(uint8_t sequence, uint8_t* outFrame,
                                 uint8_t* outLength);
  static bool buildDataRequest(uint8_t sequence, uint16_t panId,
                               uint16_t coordinatorShort,
                               uint64_t deviceExtended, uint8_t* outFrame,
                               uint8_t* outLength);
  static bool buildDataRequestShort(uint8_t sequence, uint16_t panId,
                                    uint16_t coordinatorShort,
                                    uint16_t deviceShort, uint8_t* outFrame,
                                    uint8_t* outLength);

  static bool buildNwkFrame(const ZigbeeNetworkFrame& frame,
                            const uint8_t* payload, uint8_t payloadLength,
                            uint8_t* outFrame, uint8_t* outLength);
  static bool parseNwkFrame(const uint8_t* frame, uint8_t length,
                            ZigbeeNetworkFrame* outFrame);
  static bool buildNwkRejoinRequestCommand(uint8_t capabilityInformation,
                                           uint8_t* outFrame,
                                           uint8_t* outLength);
  static bool parseNwkRejoinRequestCommand(
      const uint8_t* frame, uint8_t length,
      ZigbeeNwkRejoinRequest* outRequest);
  static bool buildNwkRejoinResponseCommand(uint16_t networkAddress,
                                            uint8_t status,
                                            uint8_t* outFrame,
                                            uint8_t* outLength);
  static bool parseNwkRejoinResponseCommand(
      const uint8_t* frame, uint8_t length,
      ZigbeeNwkRejoinResponse* outResponse);
  static bool buildNwkEndDeviceTimeoutRequestCommand(
      uint8_t requestedTimeout, uint8_t endDeviceConfiguration,
      uint8_t* outFrame, uint8_t* outLength);
  static bool parseNwkEndDeviceTimeoutRequestCommand(
      const uint8_t* frame, uint8_t length,
      ZigbeeNwkEndDeviceTimeoutRequest* outRequest);
  static bool buildNwkEndDeviceTimeoutResponseCommand(
      uint8_t status, uint8_t parentInformation, uint8_t* outFrame,
      uint8_t* outLength);
  static bool parseNwkEndDeviceTimeoutResponseCommand(
      const uint8_t* frame, uint8_t length,
      ZigbeeNwkEndDeviceTimeoutResponse* outResponse);

  static bool buildApsDataFrame(const ZigbeeApsDataFrame& frame,
                                const uint8_t* payload, uint8_t payloadLength,
                                uint8_t* outFrame, uint8_t* outLength);
  static bool parseApsDataFrame(const uint8_t* frame, uint8_t length,
                                ZigbeeApsDataFrame* outFrame);
  static bool buildApsCommandFrame(const ZigbeeApsCommandFrame& frame,
                                   const uint8_t* payload,
                                   uint8_t payloadLength, uint8_t* outFrame,
                                   uint8_t* outLength);
  static bool parseApsCommandFrame(const uint8_t* frame, uint8_t length,
                                   ZigbeeApsCommandFrame* outFrame);
  static bool buildApsAcknowledgementFrame(
      const ZigbeeApsAcknowledgementFrame& frame, uint8_t* outFrame,
      uint8_t* outLength);
  static bool parseApsAcknowledgementFrame(
      const uint8_t* frame, uint8_t length,
      ZigbeeApsAcknowledgementFrame* outFrame);
  static bool buildApsDataAcknowledgement(const ZigbeeApsDataFrame& request,
                                          uint8_t* outFrame,
                                          uint8_t* outLength);
  static bool buildApsTransportKeyCommand(const ZigbeeApsTransportKey& key,
                                          uint8_t counter, uint8_t* outFrame,
                                          uint8_t* outLength);
  static bool parseApsTransportKeyCommand(const uint8_t* frame, uint8_t length,
                                          ZigbeeApsTransportKey* outKey,
                                          uint8_t* outCounter);
  static bool buildApsUpdateDeviceCommand(const ZigbeeApsUpdateDevice& device,
                                          uint8_t counter, uint8_t* outFrame,
                                          uint8_t* outLength);
  static bool parseApsUpdateDeviceCommand(const uint8_t* frame, uint8_t length,
                                          ZigbeeApsUpdateDevice* outDevice,
                                          uint8_t* outCounter);
  static bool buildApsSwitchKeyCommand(const ZigbeeApsSwitchKey& key,
                                       uint8_t counter, uint8_t* outFrame,
                                       uint8_t* outLength);
  static bool parseApsSwitchKeyCommand(const uint8_t* frame, uint8_t length,
                                       ZigbeeApsSwitchKey* outKey,
                                       uint8_t* outCounter);

  static bool buildZclFrame(const ZigbeeZclFrame& frame,
                            const uint8_t* payload, uint8_t payloadLength,
                            uint8_t* outFrame, uint8_t* outLength);
  static bool parseZclFrame(const uint8_t* frame, uint8_t length,
                            ZigbeeZclFrame* outFrame);

  static bool parseReadAttributesRequest(const uint8_t* payload, uint8_t length,
                                         uint16_t* outAttributeIds,
                                         uint8_t maxAttributeIds,
                                         uint8_t* outCount);
  static bool buildReadAttributesRequest(const uint16_t* attributeIds,
                                         uint8_t attributeCount,
                                         uint8_t transactionSequence,
                                         uint8_t* outFrame,
                                         uint8_t* outLength);
  static bool parseReadAttributesResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeReadAttributeRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool buildWriteAttributesRequest(
      const ZigbeeWriteAttributeRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength,
      bool noResponse = false);
  static bool buildWriteAttributesUndividedRequest(
      const ZigbeeWriteAttributeRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool parseWriteAttributesResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeWriteAttributeStatusRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool parseDiscoverAttributesRequest(const uint8_t* payload,
                                             uint8_t length,
                                             uint16_t* outStartAttributeId,
                                             uint8_t* outMaxAttributeIds);
  static bool buildDiscoverAttributesRequest(uint16_t startAttributeId,
                                             uint8_t maxAttributeIds,
                                             uint8_t transactionSequence,
                                             uint8_t* outFrame,
                                             uint8_t* outLength);
  static bool buildDiscoverAttributesExtendedRequest(uint16_t startAttributeId,
                                                     uint8_t maxAttributeIds,
                                                     uint8_t transactionSequence,
                                                     uint8_t* outFrame,
                                                     uint8_t* outLength);
  static bool parseDiscoverAttributesResponse(
      const uint8_t* payload, uint8_t length, bool* outDiscoveryComplete,
      ZigbeeDiscoveredAttributeRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool parseDiscoverAttributesExtendedResponse(
      const uint8_t* payload, uint8_t length, bool* outDiscoveryComplete,
      ZigbeeDiscoveredExtendedAttributeRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool parseDiscoverCommandsRequest(const uint8_t* payload,
                                           uint8_t length,
                                           uint8_t* outStartCommandId,
                                           uint8_t* outMaxCommandIds);
  static bool buildDiscoverCommandsReceivedRequest(
      uint8_t startCommandId, uint8_t maxCommandIds,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildDiscoverCommandsGeneratedRequest(
      uint8_t startCommandId, uint8_t maxCommandIds,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool parseDiscoverCommandsResponse(const uint8_t* payload,
                                            uint8_t length,
                                            bool* outDiscoveryComplete,
                                            uint8_t* outCommandIds,
                                            uint8_t maxCommandIds,
                                            uint8_t* outCount);
  static bool parseConfigureReportingRequest(
      const uint8_t* payload, uint8_t length,
      ZigbeeReportingConfiguration* outConfigurations,
      uint8_t maxConfigurations, uint8_t* outCount);
  static bool buildConfigureReportingRequest(
      const ZigbeeReportingConfiguration* configurations,
      uint8_t configurationCount, uint8_t transactionSequence,
      uint8_t* outFrame, uint8_t* outLength);
  static bool parseConfigureReportingResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeConfigureReportingStatusRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool parseReadReportingConfigurationRequest(
      const uint8_t* payload, uint8_t length,
      ZigbeeReadReportingConfigurationRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount);
  static bool buildReadReportingConfigurationRequest(
      const ZigbeeReadReportingConfigurationRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool parseReadReportingConfigurationResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeReadReportingConfigurationResponseRecord* outRecords,
      uint8_t maxRecords, uint8_t* outCount);
  static bool parseAttributeReport(const uint8_t* payload, uint8_t length,
                                   ZigbeeAttributeReportRecord* outRecords,
                                   uint8_t maxRecords, uint8_t* outCount);
  static bool buildReadAttributesResponse(
      const ZigbeeReadAttributeRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildWriteAttributesResponse(
      const ZigbeeWriteAttributeStatusRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildDiscoverAttributesResponse(
      const ZigbeeDiscoveredAttributeRecord* records, uint8_t recordCount,
      bool discoveryComplete, uint8_t transactionSequence, uint8_t* outFrame,
      uint8_t* outLength);
  static bool buildDiscoverAttributesExtendedResponse(
      const ZigbeeDiscoveredExtendedAttributeRecord* records,
      uint8_t recordCount, bool discoveryComplete, uint8_t transactionSequence,
      uint8_t* outFrame, uint8_t* outLength);
  static bool buildDiscoverCommandsReceivedResponse(
      const uint8_t* commandIds, uint8_t commandCount, bool discoveryComplete,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildDiscoverCommandsGeneratedResponse(
      const uint8_t* commandIds, uint8_t commandCount, bool discoveryComplete,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildConfigureReportingResponse(
      const ZigbeeConfigureReportingStatusRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildReadReportingConfigurationResponse(
      const ZigbeeReadReportingConfigurationResponseRecord* records,
      uint8_t recordCount, uint8_t transactionSequence, uint8_t* outFrame,
      uint8_t* outLength);
  static bool buildAttributeReport(const ZigbeeAttributeReportRecord* records,
                                   uint8_t recordCount,
                                   uint8_t transactionSequence,
                                   uint8_t* outFrame, uint8_t* outLength);
  static bool buildDefaultResponse(uint8_t transactionSequence,
                                   bool directionToClient, uint8_t commandId,
                                   uint8_t status, uint8_t* outFrame,
                                   uint8_t* outLength);
  static bool buildZdoNetworkAddressRequest(uint8_t transactionSequence,
                                            uint64_t ieeeAddressOfInterest,
                                            bool requestExtendedResponse,
                                            uint8_t startIndex,
                                            uint8_t* outPayload,
                                            uint8_t* outLength);
  static bool buildZdoIeeeAddressRequest(uint8_t transactionSequence,
                                         uint16_t nwkAddressOfInterest,
                                         bool requestExtendedResponse,
                                         uint8_t startIndex,
                                         uint8_t* outPayload,
                                         uint8_t* outLength);
  static bool buildZdoNodeDescriptorRequest(uint8_t transactionSequence,
                                            uint16_t nwkAddressOfInterest,
                                            uint8_t* outPayload,
                                            uint8_t* outLength);
  static bool buildZdoPowerDescriptorRequest(uint8_t transactionSequence,
                                             uint16_t nwkAddressOfInterest,
                                             uint8_t* outPayload,
                                             uint8_t* outLength);
  static bool buildZdoActiveEndpointsRequest(uint8_t transactionSequence,
                                             uint16_t nwkAddressOfInterest,
                                             uint8_t* outPayload,
                                             uint8_t* outLength);
  static bool buildZdoSimpleDescriptorRequest(uint8_t transactionSequence,
                                              uint16_t nwkAddressOfInterest,
                                              uint8_t endpoint,
                                              uint8_t* outPayload,
                                              uint8_t* outLength);
  static bool buildZdoMatchDescriptorRequest(
      uint8_t transactionSequence, uint16_t nwkAddressOfInterest,
      uint16_t profileId, const uint16_t* inputClusters,
      uint8_t inputClusterCount, const uint16_t* outputClusters,
      uint8_t outputClusterCount, uint8_t* outPayload, uint8_t* outLength);
  static bool buildZdoBindRequest(uint8_t transactionSequence,
                                  uint64_t sourceIeeeAddress,
                                  uint8_t sourceEndpoint, uint16_t clusterId,
                                  ZigbeeBindingAddressMode destinationMode,
                                  uint16_t destinationGroup,
                                  uint64_t destinationIeeeAddress,
                                  uint8_t destinationEndpoint,
                                  uint8_t* outPayload, uint8_t* outLength);
  static bool buildZdoUnbindRequest(uint8_t transactionSequence,
                                    uint64_t sourceIeeeAddress,
                                    uint8_t sourceEndpoint,
                                    uint16_t clusterId,
                                    ZigbeeBindingAddressMode destinationMode,
                                    uint16_t destinationGroup,
                                    uint64_t destinationIeeeAddress,
                                    uint8_t destinationEndpoint,
                                    uint8_t* outPayload, uint8_t* outLength);
  static bool buildZdoMgmtLeaveRequest(uint8_t transactionSequence,
                                       uint64_t deviceIeeeAddress,
                                       uint8_t flags, uint8_t* outPayload,
                                       uint8_t* outLength);
  static bool parseZdoMgmtLeaveRequest(const uint8_t* payload, uint8_t length,
                                       uint8_t* outTransactionSequence,
                                       uint64_t* outDeviceIeeeAddress,
                                       uint8_t* outFlags);
  static bool buildZdoMgmtPermitJoinRequest(uint8_t transactionSequence,
                                            uint8_t permitDurationSeconds,
                                            bool trustCenterSignificance,
                                            uint8_t* outPayload,
                                            uint8_t* outLength);
  static bool parseZdoMgmtPermitJoinRequest(
      const uint8_t* payload, uint8_t length, uint8_t* outTransactionSequence,
      uint8_t* outPermitDurationSeconds,
      bool* outTrustCenterSignificance);
  static bool parseZdoStatusResponse(const uint8_t* payload, uint8_t length,
                                     uint8_t* outTransactionSequence,
                                     uint8_t* outStatus);
  static bool parseZdoAddressResponse(const uint8_t* payload, uint8_t length,
                                      ZigbeeZdoAddressResponseView* outView);
  static bool parseZdoNodeDescriptorResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeZdoNodeDescriptorResponseView* outView);
  static bool parseZdoPowerDescriptorResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeZdoPowerDescriptorResponseView* outView);
  static bool parseZdoActiveEndpointsResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeZdoActiveEndpointsResponseView* outView);
  static bool parseZdoSimpleDescriptorResponse(
      const uint8_t* payload, uint8_t length,
      ZigbeeZdoSimpleDescriptorResponseView* outView);
};

class ZigbeeHomeAutomationDevice {
 public:
  ZigbeeHomeAutomationDevice();

  bool configureOnOffLight(uint8_t endpoint, uint64_t ieeeAddress,
                           uint16_t nwkAddress, uint16_t panId,
                           const ZigbeeBasicClusterConfig& basic,
                           uint16_t manufacturerCode = 0U);
  bool configureDimmableLight(uint8_t endpoint, uint64_t ieeeAddress,
                              uint16_t nwkAddress, uint16_t panId,
                              const ZigbeeBasicClusterConfig& basic,
                              uint16_t manufacturerCode = 0U);
  bool configureTemperatureSensor(uint8_t endpoint, uint64_t ieeeAddress,
                                  uint16_t nwkAddress, uint16_t panId,
                                  const ZigbeeBasicClusterConfig& basic,
                                  uint16_t manufacturerCode = 0U);

  bool setBatteryStatus(uint8_t batteryVoltageDecivolts,
                        uint8_t batteryPercentageRemainingHalf);
  bool setTemperatureState(int16_t measuredValueCentiDegrees,
                           int16_t minMeasuredValueCentiDegrees,
                           int16_t maxMeasuredValueCentiDegrees,
                           uint16_t toleranceCentiDegrees);
  bool setOnOff(bool on);
  bool setLevel(uint8_t level);
  void updateIdentify(uint32_t nowMs);
  bool identifying() const;
  uint16_t identifyTimeSeconds() const;
  uint8_t identifyEffect() const;
  bool onOff() const;
  uint8_t level() const;
  const ZigbeeHomeAutomationConfig& config() const;

  bool handleZdoRequest(uint16_t clusterId, const uint8_t* request,
                        uint8_t requestLength, uint16_t* outResponseClusterId,
                        uint8_t* outPayload, uint8_t* outLength);
  bool handleZclRequest(uint16_t clusterId, const uint8_t* request,
                        uint8_t requestLength, uint8_t* outFrame,
                        uint8_t* outLength);
  bool configureReporting(uint16_t clusterId, uint16_t attributeId,
                          ZigbeeZclDataType dataType,
                          uint16_t minimumIntervalSeconds,
                          uint16_t maximumIntervalSeconds,
                          uint32_t reportableChange = 0U);
  bool buildAttributeReport(uint16_t clusterId, uint8_t transactionSequence,
                            uint8_t* outFrame, uint8_t* outLength) const;
  bool buildDueAttributeReport(uint32_t nowMs, uint8_t transactionSequence,
                               uint16_t* outClusterId, uint8_t* outFrame,
                               uint8_t* outLength);
  bool commitDueAttributeReport(uint32_t nowMs);
  void discardDueAttributeReport();
  uint8_t reportingConfigurationCount() const;
  const ZigbeeReportingConfiguration* reportingConfigurations() const;
  bool addBinding(uint8_t sourceEndpoint, uint16_t clusterId,
                  ZigbeeBindingAddressMode destinationMode,
                  uint16_t destinationGroup, uint64_t destinationIeee,
                  uint8_t destinationEndpoint);
  bool removeBinding(uint8_t sourceEndpoint, uint16_t clusterId,
                     ZigbeeBindingAddressMode destinationMode,
                     uint16_t destinationGroup, uint64_t destinationIeee,
                     uint8_t destinationEndpoint);
  uint8_t bindingCount() const;
  const ZigbeeBindingEntry* bindings() const;
  bool resolveBindingDestination(
      uint8_t sourceEndpoint, uint16_t clusterId,
      ZigbeeResolvedBindingDestination* outDestination) const;
  bool resolveBindingDestination(uint8_t sourceEndpoint, uint16_t clusterId,
                                 uint64_t* outDestinationIeee,
                                 uint8_t* outDestinationEndpoint) const;
  bool isInGroup(uint16_t groupId) const;
  bool leaveRequested() const;
  uint8_t leaveRequestFlags() const;
  bool leaveRequestWantsRejoin() const;
  bool consumeLeaveRequest();
  bool consumeLeaveRequest(uint8_t* outFlags);

  bool buildDeviceAnnounce(uint8_t transactionSequence, uint8_t* outPayload,
                           uint8_t* outLength) const;

 private:
  bool buildNodeDescriptorResponse(uint8_t transactionSequence,
                                   uint16_t requestNwkAddress,
                                   uint8_t* outPayload,
                                   uint8_t* outLength) const;
  bool buildPowerDescriptorResponse(uint8_t transactionSequence,
                                    uint16_t requestNwkAddress,
                                    uint8_t* outPayload,
                                    uint8_t* outLength) const;
  bool buildActiveEndpointsResponse(uint8_t transactionSequence,
                                    uint16_t requestNwkAddress,
                                    uint8_t* outPayload,
                                    uint8_t* outLength) const;
  bool buildSimpleDescriptorResponse(uint8_t transactionSequence,
                                     uint16_t requestNwkAddress,
                                     uint8_t endpoint, uint8_t* outPayload,
                                     uint8_t* outLength) const;
  bool buildExtendedSimpleDescriptorResponse(uint8_t transactionSequence,
                                             uint16_t requestNwkAddress,
                                             uint8_t endpoint,
                                             uint8_t startIndex,
                                             uint8_t* outPayload,
                                             uint8_t* outLength) const;
  bool buildMatchDescriptorResponse(uint8_t transactionSequence,
                                    uint16_t requestNwkAddress,
                                    uint16_t profileId,
                                    const uint16_t* inputClusters,
                                    uint8_t inputClusterCount,
                                    const uint16_t* outputClusters,
                                    uint8_t outputClusterCount,
                                    uint8_t* outPayload,
                                    uint8_t* outLength) const;
  bool buildExtendedActiveEndpointsResponse(uint8_t transactionSequence,
                                            uint16_t requestNwkAddress,
                                            uint8_t startIndex,
                                            uint8_t* outPayload,
                                            uint8_t* outLength) const;
  bool buildMgmtBindResponse(uint8_t transactionSequence, uint8_t startIndex,
                             uint8_t* outPayload, uint8_t* outLength) const;
  bool collectDiscoverAttributesForCluster(
      uint16_t clusterId, uint16_t startAttributeId, uint8_t maxAttributeIds,
      ZigbeeDiscoveredAttributeRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount, bool* outDiscoveryComplete) const;
  bool collectDiscoverAttributesExtendedForCluster(
      uint16_t clusterId, uint16_t startAttributeId, uint8_t maxAttributeIds,
      ZigbeeDiscoveredExtendedAttributeRecord* outRecords, uint8_t maxRecords,
      uint8_t* outCount, bool* outDiscoveryComplete) const;
  bool collectDiscoverCommandsForCluster(uint16_t clusterId, bool generated,
                                         uint8_t startCommandId,
                                         uint8_t maxCommandIds,
                                         uint8_t* outCommandIds,
                                         uint8_t maxRecords,
                                         uint8_t* outCount,
                                         bool* outDiscoveryComplete) const;
  uint8_t discoverAttributeAccessControl(
      uint16_t clusterId, uint16_t attributeId,
      const ZigbeeAttributeValue& value) const;
  bool appendReadRecordForCluster(uint16_t clusterId, uint16_t attributeId,
                                  ZigbeeReadAttributeRecord* outRecord) const;
  bool appendReadReportingRecordForCluster(
      uint16_t clusterId, uint8_t direction, uint16_t attributeId,
      ZigbeeReadReportingConfigurationResponseRecord* outRecord) const;
  uint8_t validateWriteAttributeForCluster(
      uint16_t clusterId, uint16_t attributeId,
      const ZigbeeAttributeValue& value) const;
  uint8_t writeAttributeForCluster(uint16_t clusterId, uint16_t attributeId,
                                   const ZigbeeAttributeValue& value);
  bool makeAttributeValueForCluster(uint16_t clusterId, uint16_t attributeId,
                                    ZigbeeAttributeValue* outValue) const;
  void resetReportingState(uint8_t index);
  void seedReportingState(uint8_t index);
  bool setBinding(uint8_t sourceEndpoint, uint16_t clusterId,
                  ZigbeeBindingAddressMode destinationMode,
                  uint16_t destinationGroup, uint64_t destinationIeee,
                  uint8_t destinationEndpoint);
  bool clearBinding(uint8_t sourceEndpoint, uint16_t clusterId,
                    ZigbeeBindingAddressMode destinationMode,
                    uint16_t destinationGroup, uint64_t destinationIeee,
                    uint8_t destinationEndpoint);
  const ZigbeeSimpleDescriptor* findEndpoint(uint8_t endpoint) const;
  bool endpointMatches(const ZigbeeSimpleDescriptor& descriptor,
                       uint16_t profileId, const uint16_t* inputClusters,
                       uint8_t inputClusterCount,
                       const uint16_t* outputClusters,
                       uint8_t outputClusterCount) const;

  ZigbeeHomeAutomationConfig config_;
  uint16_t onOffLightInputClusters_[5];
  uint16_t onOffLightOutputClusters_[1];
  uint16_t dimmableLightInputClusters_[6];
  uint16_t dimmableLightOutputClusters_[1];
  uint16_t temperatureSensorInputClusters_[4];
  uint16_t temperatureSensorOutputClusters_[1];
  ZigbeeReportingConfiguration reporting_[8];
  ZigbeeReportingRuntimeState reportingState_[8];
  uint32_t identifyLastTickMs_ = 0U;
  bool leaveRequested_ = false;
  uint8_t leaveRequestFlags_ = 0U;
};

}  // namespace xiao_nrf54l15
