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

constexpr uint16_t kZigbeeZdoNodeDescriptorRequest = 0x0002U;
constexpr uint16_t kZigbeeZdoPowerDescriptorRequest = 0x0003U;
constexpr uint16_t kZigbeeZdoSimpleDescriptorRequest = 0x0004U;
constexpr uint16_t kZigbeeZdoActiveEndpointsRequest = 0x0005U;
constexpr uint16_t kZigbeeZdoMatchDescriptorRequest = 0x0006U;
constexpr uint16_t kZigbeeZdoDeviceAnnounce = 0x0013U;
constexpr uint16_t kZigbeeZdoBindRequest = 0x0021U;
constexpr uint16_t kZigbeeZdoUnbindRequest = 0x0022U;

constexpr uint16_t kZigbeeZdoNodeDescriptorResponse = 0x8002U;
constexpr uint16_t kZigbeeZdoPowerDescriptorResponse = 0x8003U;
constexpr uint16_t kZigbeeZdoSimpleDescriptorResponse = 0x8004U;
constexpr uint16_t kZigbeeZdoActiveEndpointsResponse = 0x8005U;
constexpr uint16_t kZigbeeZdoMatchDescriptorResponse = 0x8006U;
constexpr uint16_t kZigbeeZdoBindResponse = 0x8021U;
constexpr uint16_t kZigbeeZdoUnbindResponse = 0x8022U;

constexpr uint16_t kZigbeeDeviceIdOnOffLight = 0x0100U;
constexpr uint16_t kZigbeeDeviceIdDimmableLight = 0x0101U;
constexpr uint16_t kZigbeeDeviceIdTemperatureSensor = 0x0302U;

constexpr uint8_t kZigbeeApsDeliveryUnicast = 0x00U;
constexpr uint8_t kZigbeeApsDeliveryIndirect = 0x01U;
constexpr uint8_t kZigbeeApsDeliveryBroadcast = 0x02U;
constexpr uint8_t kZigbeeApsDeliveryGroup = 0x03U;

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
  uint8_t frameVersion = 1U;
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

struct ZigbeeMacBeaconPayload {
  bool valid = false;
  uint8_t protocolId = 0U;
  uint8_t stackProfile = 0U;
  uint8_t protocolVersion = 0U;
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

struct ZigbeeAttributeReportRecord {
  uint16_t attributeId = 0U;
  ZigbeeAttributeValue value{};
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

struct ZigbeeConfigureReportingStatusRecord {
  uint8_t status = 0U;
  uint8_t direction = 0U;
  uint16_t attributeId = 0U;
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

  static bool buildNwkFrame(const ZigbeeNetworkFrame& frame,
                            const uint8_t* payload, uint8_t payloadLength,
                            uint8_t* outFrame, uint8_t* outLength);
  static bool parseNwkFrame(const uint8_t* frame, uint8_t length,
                            ZigbeeNetworkFrame* outFrame);

  static bool buildApsDataFrame(const ZigbeeApsDataFrame& frame,
                                const uint8_t* payload, uint8_t payloadLength,
                                uint8_t* outFrame, uint8_t* outLength);
  static bool parseApsDataFrame(const uint8_t* frame, uint8_t length,
                                ZigbeeApsDataFrame* outFrame);

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
  static bool parseConfigureReportingRequest(
      const uint8_t* payload, uint8_t length,
      ZigbeeReportingConfiguration* outConfigurations,
      uint8_t maxConfigurations, uint8_t* outCount);
  static bool buildConfigureReportingRequest(
      const ZigbeeReportingConfiguration* configurations,
      uint8_t configurationCount, uint8_t transactionSequence,
      uint8_t* outFrame, uint8_t* outLength);
  static bool parseAttributeReport(const uint8_t* payload, uint8_t length,
                                   ZigbeeAttributeReportRecord* outRecords,
                                   uint8_t maxRecords, uint8_t* outCount);
  static bool buildReadAttributesResponse(
      const ZigbeeReadAttributeRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildConfigureReportingResponse(
      const ZigbeeConfigureReportingStatusRecord* records, uint8_t recordCount,
      uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength);
  static bool buildAttributeReport(const ZigbeeAttributeReportRecord* records,
                                   uint8_t recordCount,
                                   uint8_t transactionSequence,
                                   uint8_t* outFrame, uint8_t* outLength);
  static bool buildDefaultResponse(uint8_t transactionSequence,
                                   bool directionToClient, uint8_t commandId,
                                   uint8_t status, uint8_t* outFrame,
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
  static bool parseZdoStatusResponse(const uint8_t* payload, uint8_t length,
                                     uint8_t* outTransactionSequence,
                                     uint8_t* outStatus);
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
  bool resolveBindingDestination(uint8_t sourceEndpoint, uint16_t clusterId,
                                 uint64_t* outDestinationIeee,
                                 uint8_t* outDestinationEndpoint) const;
  bool isInGroup(uint16_t groupId) const;

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
  bool buildMatchDescriptorResponse(uint8_t transactionSequence,
                                    uint16_t requestNwkAddress,
                                    uint16_t profileId,
                                    const uint16_t* inputClusters,
                                    uint8_t inputClusterCount,
                                    const uint16_t* outputClusters,
                                    uint8_t outputClusterCount,
                                    uint8_t* outPayload,
                                    uint8_t* outLength) const;
  bool appendReadRecordForCluster(uint16_t clusterId, uint16_t attributeId,
                                  ZigbeeReadAttributeRecord* outRecord) const;
  bool makeAttributeValueForCluster(uint16_t clusterId, uint16_t attributeId,
                                    ZigbeeAttributeValue* outValue) const;
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
};

}  // namespace xiao_nrf54l15
