#include "zigbee_stack.h"

#include <Arduino.h>

#include <string.h>

namespace xiao_nrf54l15 {

namespace {

constexpr uint8_t kNwkProtocolVersion = 2U;

constexpr uint8_t kZclCommandReadAttributes = 0x00U;
constexpr uint8_t kZclCommandReadAttributesResponse = 0x01U;
constexpr uint8_t kZclCommandConfigureReporting = 0x06U;
constexpr uint8_t kZclCommandConfigureReportingResponse = 0x07U;
constexpr uint8_t kZclCommandReportAttributes = 0x0AU;
constexpr uint8_t kZclCommandDefaultResponse = 0x0BU;

constexpr uint8_t kZclStatusSuccess = 0x00U;
constexpr uint8_t kZclStatusUnsupportedClusterCommand = 0x81U;
constexpr uint8_t kZclStatusUnsupportedGeneralCommand = 0x82U;
constexpr uint8_t kZclStatusInvalidField = 0x85U;
constexpr uint8_t kZclStatusUnsupportedAttribute = 0x86U;
constexpr uint8_t kZclStatusInsufficientSpace = 0x89U;
constexpr uint8_t kZclStatusNotFound = 0x8BU;
constexpr uint8_t kZclStatusUnsupportedReporting = 0x8CU;

constexpr uint8_t kZdoStatusSuccess = 0x00U;
constexpr uint8_t kZdoStatusDeviceNotFound = 0x81U;
constexpr uint8_t kZdoStatusInvalidEndpoint = 0x82U;
constexpr uint8_t kZdoStatusNotSupported = 0x84U;
constexpr uint8_t kZdoStatusNoEntry = 0x88U;
constexpr uint8_t kZdoStatusInsufficientSpace = 0x8AU;
constexpr uint8_t kZdoStatusTableFull = 0x8CU;

constexpr uint8_t kBasicPowerSourceMainsSinglePhase = 0x01U;
constexpr uint8_t kBasicPowerSourceBattery = 0x03U;

constexpr uint8_t kOnOffCommandOff = 0x00U;
constexpr uint8_t kOnOffCommandOn = 0x01U;
constexpr uint8_t kOnOffCommandToggle = 0x02U;
constexpr uint8_t kIdentifyCommandIdentify = 0x00U;
constexpr uint8_t kIdentifyCommandIdentifyQuery = 0x01U;
constexpr uint8_t kIdentifyCommandTriggerEffect = 0x40U;
constexpr uint8_t kGroupsCommandAddGroup = 0x00U;
constexpr uint8_t kGroupsCommandViewGroup = 0x01U;
constexpr uint8_t kGroupsCommandGetGroupMembership = 0x02U;
constexpr uint8_t kGroupsCommandRemoveGroup = 0x03U;
constexpr uint8_t kGroupsCommandRemoveAllGroups = 0x04U;
constexpr uint8_t kGroupsCommandAddGroupIfIdentifying = 0x05U;
constexpr uint8_t kScenesCommandAddScene = 0x00U;
constexpr uint8_t kScenesCommandViewScene = 0x01U;
constexpr uint8_t kScenesCommandRemoveScene = 0x02U;
constexpr uint8_t kScenesCommandRemoveAllScenes = 0x03U;
constexpr uint8_t kScenesCommandStoreScene = 0x04U;
constexpr uint8_t kScenesCommandRecallScene = 0x05U;
constexpr uint8_t kScenesCommandGetSceneMembership = 0x06U;
constexpr uint8_t kLevelControlCommandMoveToLevel = 0x00U;
constexpr uint8_t kLevelControlCommandMove = 0x01U;
constexpr uint8_t kLevelControlCommandStep = 0x02U;
constexpr uint8_t kLevelControlCommandStop = 0x03U;
constexpr uint8_t kLevelControlCommandMoveToLevelWithOnOff = 0x04U;
constexpr uint8_t kLevelControlCommandMoveWithOnOff = 0x05U;
constexpr uint8_t kLevelControlCommandStepWithOnOff = 0x06U;
constexpr uint8_t kLevelControlCommandStopWithOnOff = 0x07U;

struct ParsedSceneExtensionData {
  bool hasOnOff = false;
  bool onOff = false;
  bool hasLevel = false;
  uint8_t level = 0U;
};

uint16_t readLe16(const uint8_t* src) {
  return static_cast<uint16_t>(src[0]) |
         (static_cast<uint16_t>(src[1]) << 8U);
}

void writeLe16(uint8_t* dst, uint16_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFFU);
  dst[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

uint32_t readLe24(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8U) |
         (static_cast<uint32_t>(src[2]) << 16U);
}

void writeLe24(uint8_t* dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFFU);
  dst[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  dst[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
}

uint64_t readLe64(const uint8_t* src) {
  uint64_t value = 0U;
  for (uint8_t i = 0U; i < 8U; ++i) {
    value |= (static_cast<uint64_t>(src[i]) << (8U * i));
  }
  return value;
}

void writeLe64(uint8_t* dst, uint64_t value) {
  for (uint8_t i = 0U; i < 8U; ++i) {
    dst[i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
  }
}

uint8_t boundedStringLength(const char* text) {
  if (text == nullptr) {
    return 0U;
  }
  const size_t len = strnlen(text, 32U);
  return static_cast<uint8_t>(len > 31U ? 31U : len);
}

bool appendBytes(uint8_t* outBuffer, uint8_t maxLength, uint8_t* ioOffset,
                 const uint8_t* data, uint8_t dataLength) {
  if (outBuffer == nullptr || ioOffset == nullptr) {
    return false;
  }
  if (dataLength > 0U && data == nullptr) {
    return false;
  }
  if ((static_cast<uint16_t>(*ioOffset) + dataLength) > maxLength) {
    return false;
  }
  if (dataLength > 0U) {
    memcpy(&outBuffer[*ioOffset], data, dataLength);
  }
  *ioOffset = static_cast<uint8_t>(*ioOffset + dataLength);
  return true;
}

bool appendLe16(uint8_t* outBuffer, uint8_t maxLength, uint8_t* ioOffset,
                uint16_t value) {
  uint8_t bytes[2];
  writeLe16(bytes, value);
  return appendBytes(outBuffer, maxLength, ioOffset, bytes, sizeof(bytes));
}

bool appendLe64(uint8_t* outBuffer, uint8_t maxLength, uint8_t* ioOffset,
                uint64_t value) {
  uint8_t bytes[8];
  writeLe64(bytes, value);
  return appendBytes(outBuffer, maxLength, ioOffset, bytes, sizeof(bytes));
}

bool appendCharStringPayload(uint8_t* outBuffer, uint8_t maxLength,
                             uint8_t* ioOffset, const char* text,
                             uint8_t textLength) {
  if (!appendBytes(outBuffer, maxLength, ioOffset, &textLength, 1U)) {
    return false;
  }
  if (textLength == 0U) {
    return true;
  }
  return appendBytes(outBuffer, maxLength, ioOffset,
                     reinterpret_cast<const uint8_t*>(text), textLength);
}

bool buildClusterSpecificResponseFrame(uint8_t transactionSequence,
                                       uint8_t commandId,
                                       const uint8_t* payload,
                                       uint8_t payloadLength,
                                       uint8_t* outFrame,
                                       uint8_t* outLength) {
  ZigbeeZclFrame response{};
  response.frameType = ZigbeeZclFrameType::kClusterSpecific;
  response.directionToClient = true;
  response.disableDefaultResponse = false;
  response.transactionSequence = transactionSequence;
  response.commandId = commandId;
  return ZigbeeCodec::buildZclFrame(response, payload, payloadLength, outFrame,
                                    outLength);
}

bool buildSimpleZdoStatusResponse(uint8_t transactionSequence, uint8_t status,
                                  uint8_t* outPayload, uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }
  outPayload[0] = transactionSequence;
  outPayload[1] = status;
  *outLength = 2U;
  return true;
}

bool buildZdoAddressResponse(uint8_t transactionSequence, uint8_t status,
                             uint64_t ieeeAddress, uint16_t nwkAddress,
                             bool includeAssociatedList, uint8_t* outPayload,
                             uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &status, 1U)) {
    return false;
  }
  if (status != kZdoStatusSuccess) {
    *outLength = offset;
    return true;
  }

  const uint8_t associatedDeviceCount = 0U;
  const uint8_t startIndex = 0U;
  if (!appendLe64(outPayload, 127U, &offset, ieeeAddress) ||
      !appendLe16(outPayload, 127U, &offset, nwkAddress)) {
    return false;
  }
  if (includeAssociatedList &&
      (!appendBytes(outPayload, 127U, &offset, &associatedDeviceCount, 1U) ||
       !appendBytes(outPayload, 127U, &offset, &startIndex, 1U))) {
    return false;
  }

  *outLength = offset;
  return true;
}

ZigbeeGroupEntry* findGroupEntry(ZigbeeGroupsState* groups, uint16_t groupId) {
  if (groups == nullptr || !groups->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(groups->entries) /
                                                sizeof(groups->entries[0]));
       ++i) {
    if (groups->entries[i].used && groups->entries[i].groupId == groupId) {
      return &groups->entries[i];
    }
  }
  return nullptr;
}

const ZigbeeGroupEntry* findGroupEntry(const ZigbeeGroupsState* groups,
                                       uint16_t groupId) {
  if (groups == nullptr || !groups->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(groups->entries) /
                                                sizeof(groups->entries[0]));
       ++i) {
    if (groups->entries[i].used && groups->entries[i].groupId == groupId) {
      return &groups->entries[i];
    }
  }
  return nullptr;
}

ZigbeeGroupEntry* allocateGroupEntry(ZigbeeGroupsState* groups, uint16_t groupId) {
  ZigbeeGroupEntry* existing = findGroupEntry(groups, groupId);
  if (existing != nullptr) {
    return existing;
  }
  if (groups == nullptr || !groups->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(groups->entries) /
                                                sizeof(groups->entries[0]));
       ++i) {
    if (!groups->entries[i].used) {
      memset(&groups->entries[i], 0, sizeof(groups->entries[i]));
      groups->entries[i].used = true;
      groups->entries[i].groupId = groupId;
      return &groups->entries[i];
    }
  }
  return nullptr;
}

bool upsertGroup(ZigbeeGroupsState* groups, uint16_t groupId, const char* name,
                 uint8_t nameLength) {
  ZigbeeGroupEntry* entry = allocateGroupEntry(groups, groupId);
  if (entry == nullptr) {
    return false;
  }
  entry->nameLength = static_cast<uint8_t>(nameLength > 15U ? 15U : nameLength);
  memset(entry->name, 0, sizeof(entry->name));
  if (entry->nameLength > 0U && name != nullptr) {
    memcpy(entry->name, name, entry->nameLength);
  }
  return true;
}

bool removeGroupEntry(ZigbeeGroupsState* groups, uint16_t groupId) {
  ZigbeeGroupEntry* entry = findGroupEntry(groups, groupId);
  if (entry == nullptr) {
    return false;
  }
  memset(entry, 0, sizeof(*entry));
  return true;
}

uint8_t countUsedGroups(const ZigbeeGroupsState& groups) {
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(groups.entries) /
                                                sizeof(groups.entries[0]));
       ++i) {
    if (groups.entries[i].used) {
      ++count;
    }
  }
  return count;
}

ZigbeeSceneEntry* findSceneEntry(ZigbeeScenesState* scenes, uint16_t groupId,
                                 uint8_t sceneId) {
  if (scenes == nullptr || !scenes->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(scenes->entries) /
                                                sizeof(scenes->entries[0]));
       ++i) {
    if (scenes->entries[i].used && scenes->entries[i].groupId == groupId &&
        scenes->entries[i].sceneId == sceneId) {
      return &scenes->entries[i];
    }
  }
  return nullptr;
}

const ZigbeeSceneEntry* findSceneEntry(const ZigbeeScenesState* scenes,
                                       uint16_t groupId, uint8_t sceneId) {
  if (scenes == nullptr || !scenes->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(scenes->entries) /
                                                sizeof(scenes->entries[0]));
       ++i) {
    if (scenes->entries[i].used && scenes->entries[i].groupId == groupId &&
        scenes->entries[i].sceneId == sceneId) {
      return &scenes->entries[i];
    }
  }
  return nullptr;
}

ZigbeeSceneEntry* allocateSceneEntry(ZigbeeScenesState* scenes, uint16_t groupId,
                                     uint8_t sceneId) {
  ZigbeeSceneEntry* existing = findSceneEntry(scenes, groupId, sceneId);
  if (existing != nullptr) {
    return existing;
  }
  if (scenes == nullptr || !scenes->enabled) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(scenes->entries) /
                                                sizeof(scenes->entries[0]));
       ++i) {
    if (!scenes->entries[i].used) {
      memset(&scenes->entries[i], 0, sizeof(scenes->entries[i]));
      scenes->entries[i].used = true;
      scenes->entries[i].groupId = groupId;
      scenes->entries[i].sceneId = sceneId;
      return &scenes->entries[i];
    }
  }
  return nullptr;
}

bool removeSceneEntry(ZigbeeScenesState* scenes, uint16_t groupId,
                      uint8_t sceneId) {
  ZigbeeSceneEntry* entry = findSceneEntry(scenes, groupId, sceneId);
  if (entry == nullptr) {
    return false;
  }
  memset(entry, 0, sizeof(*entry));
  return true;
}

void removeAllScenesForGroup(ZigbeeScenesState* scenes, uint16_t groupId) {
  if (scenes == nullptr || !scenes->enabled) {
    return;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(scenes->entries) /
                                                sizeof(scenes->entries[0]));
       ++i) {
    if (scenes->entries[i].used && scenes->entries[i].groupId == groupId) {
      memset(&scenes->entries[i], 0, sizeof(scenes->entries[i]));
    }
  }
  if (scenes->currentGroupId == groupId) {
    scenes->sceneValid = false;
    scenes->currentSceneId = 0U;
  }
}

uint8_t countScenesForGroup(const ZigbeeScenesState& scenes, uint16_t groupId) {
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(scenes.entries) /
                                                sizeof(scenes.entries[0]));
       ++i) {
    if (scenes.entries[i].used && scenes.entries[i].groupId == groupId) {
      ++count;
    }
  }
  return count;
}

bool parseSceneExtensionSets(const uint8_t* payload, uint8_t length,
                             uint8_t offset,
                             ParsedSceneExtensionData* outExtension) {
  if (outExtension == nullptr) {
    return false;
  }
  memset(outExtension, 0, sizeof(*outExtension));
  while (offset < length) {
    if (length < static_cast<uint8_t>(offset + 3U)) {
      return false;
    }
    const uint16_t clusterId = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    const uint8_t extensionLength = payload[offset++];
    if (length < static_cast<uint8_t>(offset + extensionLength)) {
      return false;
    }
    if (clusterId == kZigbeeClusterOnOff && extensionLength >= 1U) {
      outExtension->hasOnOff = true;
      outExtension->onOff = payload[offset] != 0U;
    } else if (clusterId == kZigbeeClusterLevelControl &&
               extensionLength >= 1U) {
      outExtension->hasLevel = true;
      outExtension->level = payload[offset];
    }
    offset = static_cast<uint8_t>(offset + extensionLength);
  }
  return true;
}

bool storeSceneState(ZigbeeHomeAutomationConfig* config, uint16_t groupId,
                     uint8_t sceneId, uint16_t transitionTimeDeciseconds,
                     const char* name, uint8_t nameLength,
                     const ParsedSceneExtensionData* extension,
                     bool captureCurrentState) {
  if (config == nullptr || !config->scenes.enabled) {
    return false;
  }
  if (config->groups.enabled && findGroupEntry(&config->groups, groupId) == nullptr &&
      !upsertGroup(&config->groups, groupId, nullptr, 0U)) {
    return false;
  }

  ZigbeeSceneEntry* scene = allocateSceneEntry(&config->scenes, groupId, sceneId);
  if (scene == nullptr) {
    return false;
  }

  scene->transitionTimeDeciseconds = transitionTimeDeciseconds;
  scene->nameLength = static_cast<uint8_t>(nameLength > 15U ? 15U : nameLength);
  memset(scene->name, 0, sizeof(scene->name));
  if (scene->nameLength > 0U && name != nullptr) {
    memcpy(scene->name, name, scene->nameLength);
  }

  if (captureCurrentState) {
    scene->hasOnOff = config->onOff.enabled;
    scene->onOff = config->onOff.on;
    scene->hasLevel = config->level.enabled;
    scene->level = config->level.currentLevel;
  } else if (extension != nullptr) {
    scene->hasOnOff = extension->hasOnOff;
    scene->onOff = extension->onOff;
    scene->hasLevel = extension->hasLevel;
    scene->level = extension->level;
  } else {
    scene->hasOnOff = false;
    scene->onOff = false;
    scene->hasLevel = false;
    scene->level = 0U;
  }

  config->scenes.currentGroupId = groupId;
  config->scenes.currentSceneId = sceneId;
  config->scenes.sceneValid = true;
  return true;
}

void invalidateCurrentScene(ZigbeeHomeAutomationConfig* config) {
  if (config != nullptr && config->scenes.enabled) {
    config->scenes.sceneValid = false;
  }
}


bool clusterListContains(const uint16_t* clusters, uint8_t clusterCount,
                         uint16_t clusterId) {
  if (clusters == nullptr && clusterCount > 0U) {
    return false;
  }
  for (uint8_t i = 0U; i < clusterCount; ++i) {
    if (clusters[i] == clusterId) {
      return true;
    }
  }
  return false;
}

uint8_t attributeValueLength(const ZigbeeAttributeValue& value) {
  switch (value.type) {
    case ZigbeeZclDataType::kBoolean:
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      return 1U;
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
    case ZigbeeZclDataType::kInt16:
      return 2U;
    case ZigbeeZclDataType::kUint32:
      return 4U;
    case ZigbeeZclDataType::kCharString:
      return static_cast<uint8_t>(1U + value.stringLength);
    default:
      break;
  }
  return 0U;
}

bool zclDataTypeHasReportableChange(ZigbeeZclDataType type) {
  switch (type) {
    case ZigbeeZclDataType::kUint8:
    case ZigbeeZclDataType::kUint16:
    case ZigbeeZclDataType::kUint32:
    case ZigbeeZclDataType::kInt16:
      return true;
    default:
      break;
  }
  return false;
}

uint8_t zclDataTypeStorageLength(ZigbeeZclDataType type) {
  switch (type) {
    case ZigbeeZclDataType::kBoolean:
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      return 1U;
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
    case ZigbeeZclDataType::kInt16:
      return 2U;
    case ZigbeeZclDataType::kUint32:
      return 4U;
    default:
      break;
  }
  return 0U;
}

bool attributeValueEquals(const ZigbeeAttributeValue& left,
                          const ZigbeeAttributeValue& right) {
  if (left.type != right.type) {
    return false;
  }

  switch (left.type) {
    case ZigbeeZclDataType::kBoolean:
      return left.data.boolValue == right.data.boolValue;
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      return left.data.u8 == right.data.u8;
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
      return left.data.u16 == right.data.u16;
    case ZigbeeZclDataType::kUint32:
      return left.data.u32 == right.data.u32;
    case ZigbeeZclDataType::kInt16:
      return left.data.i16 == right.data.i16;
    case ZigbeeZclDataType::kCharString:
      return left.stringLength == right.stringLength &&
             (left.stringLength == 0U ||
              memcmp(left.stringValue, right.stringValue,
                     left.stringLength) == 0);
    default:
      break;
  }

  return false;
}

bool attributeValueMeetsReportableChange(const ZigbeeAttributeValue& current,
                                         const ZigbeeAttributeValue& baseline,
                                         uint32_t reportableChange) {
  if (!attributeValueEquals(current, baseline) &&
      !zclDataTypeHasReportableChange(current.type)) {
    return true;
  }
  if (current.type != baseline.type || attributeValueEquals(current, baseline)) {
    return current.type != baseline.type;
  }

  if (reportableChange == 0U) {
    return true;
  }

  switch (current.type) {
    case ZigbeeZclDataType::kUint8: {
      const uint8_t delta = current.data.u8 > baseline.data.u8
                                ? static_cast<uint8_t>(current.data.u8 -
                                                       baseline.data.u8)
                                : static_cast<uint8_t>(baseline.data.u8 -
                                                       current.data.u8);
      return delta >= reportableChange;
    }
    case ZigbeeZclDataType::kUint16:
    case ZigbeeZclDataType::kBitmap16: {
      const uint16_t delta = current.data.u16 > baseline.data.u16
                                 ? static_cast<uint16_t>(current.data.u16 -
                                                         baseline.data.u16)
                                 : static_cast<uint16_t>(baseline.data.u16 -
                                                         current.data.u16);
      return delta >= reportableChange;
    }
    case ZigbeeZclDataType::kUint32: {
      const uint32_t delta = current.data.u32 > baseline.data.u32
                                 ? current.data.u32 - baseline.data.u32
                                 : baseline.data.u32 - current.data.u32;
      return delta >= reportableChange;
    }
    case ZigbeeZclDataType::kInt16: {
      const int32_t delta = static_cast<int32_t>(current.data.i16) -
                            static_cast<int32_t>(baseline.data.i16);
      const uint32_t magnitude =
          static_cast<uint32_t>(delta < 0 ? -delta : delta);
      return magnitude >= reportableChange;
    }
    default:
      break;
  }

  return true;
}

bool readAttributeValue(const uint8_t* payload, uint8_t length, uint8_t* ioOffset,
                        ZigbeeZclDataType type, ZigbeeAttributeValue* outValue) {
  if (payload == nullptr || ioOffset == nullptr || outValue == nullptr) {
    return false;
  }

  memset(outValue, 0, sizeof(*outValue));
  outValue->type = type;

  switch (type) {
    case ZigbeeZclDataType::kBoolean:
      if (length < static_cast<uint8_t>(*ioOffset + 1U)) {
        return false;
      }
      outValue->data.boolValue = payload[*ioOffset] != 0U;
      *ioOffset = static_cast<uint8_t>(*ioOffset + 1U);
      return true;
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      if (length < static_cast<uint8_t>(*ioOffset + 1U)) {
        return false;
      }
      outValue->data.u8 = payload[*ioOffset];
      *ioOffset = static_cast<uint8_t>(*ioOffset + 1U);
      return true;
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
      if (length < static_cast<uint8_t>(*ioOffset + 2U)) {
        return false;
      }
      outValue->data.u16 = readLe16(&payload[*ioOffset]);
      *ioOffset = static_cast<uint8_t>(*ioOffset + 2U);
      return true;
    case ZigbeeZclDataType::kInt16:
      if (length < static_cast<uint8_t>(*ioOffset + 2U)) {
        return false;
      }
      outValue->data.i16 = static_cast<int16_t>(readLe16(&payload[*ioOffset]));
      *ioOffset = static_cast<uint8_t>(*ioOffset + 2U);
      return true;
    case ZigbeeZclDataType::kUint32:
      if (length < static_cast<uint8_t>(*ioOffset + 4U)) {
        return false;
      }
      outValue->data.u32 =
          static_cast<uint32_t>(payload[*ioOffset]) |
          (static_cast<uint32_t>(payload[*ioOffset + 1U]) << 8U) |
          (static_cast<uint32_t>(payload[*ioOffset + 2U]) << 16U) |
          (static_cast<uint32_t>(payload[*ioOffset + 3U]) << 24U);
      *ioOffset = static_cast<uint8_t>(*ioOffset + 4U);
      return true;
    case ZigbeeZclDataType::kCharString:
      if (length < static_cast<uint8_t>(*ioOffset + 1U)) {
        return false;
      }
      outValue->stringLength = payload[*ioOffset];
      *ioOffset = static_cast<uint8_t>(*ioOffset + 1U);
      if (length < static_cast<uint8_t>(*ioOffset + outValue->stringLength)) {
        return false;
      }
      outValue->stringValue =
          reinterpret_cast<const char*>(&payload[*ioOffset]);
      *ioOffset = static_cast<uint8_t>(*ioOffset + outValue->stringLength);
      return true;
    default:
      break;
  }

  return false;
}

bool appendReportableChange(uint8_t* outBuffer, uint8_t maxLength, uint8_t* ioOffset,
                            ZigbeeZclDataType type, uint32_t reportableChange) {
  if (outBuffer == nullptr || ioOffset == nullptr) {
    return false;
  }

  if (!zclDataTypeHasReportableChange(type)) {
    return true;
  }

  const uint8_t storageLength = zclDataTypeStorageLength(type);
  if (storageLength == 0U) {
    return false;
  }

  uint8_t bytes[4] = {0U, 0U, 0U, 0U};
  bytes[0] = static_cast<uint8_t>(reportableChange & 0xFFU);
  bytes[1] = static_cast<uint8_t>((reportableChange >> 8U) & 0xFFU);
  bytes[2] = static_cast<uint8_t>((reportableChange >> 16U) & 0xFFU);
  bytes[3] = static_cast<uint8_t>((reportableChange >> 24U) & 0xFFU);
  return appendBytes(outBuffer, maxLength, ioOffset, bytes, storageLength);
}

bool encodeAddressField(const ZigbeeMacAddress& address, uint8_t* outFrame,
                        uint8_t* ioOffset) {
  if (outFrame == nullptr || ioOffset == nullptr) {
    return false;
  }
  switch (address.mode) {
    case ZigbeeMacAddressMode::kNone:
      return true;
    case ZigbeeMacAddressMode::kShort:
      return appendLe16(outFrame, 127U, ioOffset, address.panId) &&
             appendLe16(outFrame, 127U, ioOffset, address.shortAddress);
    case ZigbeeMacAddressMode::kExtended:
      return appendLe16(outFrame, 127U, ioOffset, address.panId) &&
             appendLe64(outFrame, 127U, ioOffset, address.extendedAddress);
    default:
      break;
  }
  return false;
}

bool appendAddressValue(const ZigbeeMacAddress& address, uint8_t* outFrame,
                        uint8_t* ioOffset) {
  if (outFrame == nullptr || ioOffset == nullptr) {
    return false;
  }
  switch (address.mode) {
    case ZigbeeMacAddressMode::kShort:
      return appendLe16(outFrame, 127U, ioOffset, address.shortAddress);
    case ZigbeeMacAddressMode::kExtended:
      return appendLe64(outFrame, 127U, ioOffset, address.extendedAddress);
    case ZigbeeMacAddressMode::kNone:
      return true;
    default:
      break;
  }
  return false;
}

bool parseAddressField(const uint8_t* frame, uint8_t length, uint8_t* ioOffset,
                       ZigbeeMacAddressMode mode, bool includePanId,
                       uint16_t implicitPanId, ZigbeeMacAddress* outAddress) {
  if (frame == nullptr || ioOffset == nullptr || outAddress == nullptr) {
    return false;
  }

  outAddress->mode = mode;
  outAddress->panId = includePanId ? 0U : implicitPanId;
  outAddress->shortAddress = 0U;
  outAddress->extendedAddress = 0U;

  if (mode == ZigbeeMacAddressMode::kNone) {
    return true;
  }
  if (includePanId) {
    if (length < static_cast<uint8_t>(*ioOffset + 2U)) {
      return false;
    }
    outAddress->panId = readLe16(&frame[*ioOffset]);
    *ioOffset = static_cast<uint8_t>(*ioOffset + 2U);
  }

  if (mode == ZigbeeMacAddressMode::kShort) {
    if (length < static_cast<uint8_t>(*ioOffset + 2U)) {
      return false;
    }
    outAddress->shortAddress = readLe16(&frame[*ioOffset]);
    *ioOffset = static_cast<uint8_t>(*ioOffset + 2U);
    return true;
  }
  if (mode == ZigbeeMacAddressMode::kExtended) {
    if (length < static_cast<uint8_t>(*ioOffset + 8U)) {
      return false;
    }
    outAddress->extendedAddress = readLe64(&frame[*ioOffset]);
    *ioOffset = static_cast<uint8_t>(*ioOffset + 8U);
    return true;
  }
  return false;
}

bool writeAttributeValue(uint8_t* outBuffer, uint8_t maxLength, uint8_t* ioOffset,
                         const ZigbeeAttributeValue& value) {
  if (outBuffer == nullptr || ioOffset == nullptr) {
    return false;
  }

  if (!appendBytes(outBuffer, maxLength, ioOffset,
                   reinterpret_cast<const uint8_t*>(&value.type), 1U)) {
    return false;
  }

  switch (value.type) {
    case ZigbeeZclDataType::kBoolean: {
      const uint8_t encoded = value.data.boolValue ? 1U : 0U;
      return appendBytes(outBuffer, maxLength, ioOffset, &encoded, 1U);
    }
    case ZigbeeZclDataType::kBitmap8:
    case ZigbeeZclDataType::kUint8:
      return appendBytes(outBuffer, maxLength, ioOffset, &value.data.u8, 1U);
    case ZigbeeZclDataType::kBitmap16:
    case ZigbeeZclDataType::kUint16:
      return appendLe16(outBuffer, maxLength, ioOffset, value.data.u16);
    case ZigbeeZclDataType::kInt16: {
      const uint16_t encoded = static_cast<uint16_t>(value.data.i16);
      return appendLe16(outBuffer, maxLength, ioOffset, encoded);
    }
    case ZigbeeZclDataType::kUint32: {
      uint8_t bytes[4];
      bytes[0] = static_cast<uint8_t>(value.data.u32 & 0xFFU);
      bytes[1] = static_cast<uint8_t>((value.data.u32 >> 8U) & 0xFFU);
      bytes[2] = static_cast<uint8_t>((value.data.u32 >> 16U) & 0xFFU);
      bytes[3] = static_cast<uint8_t>((value.data.u32 >> 24U) & 0xFFU);
      return appendBytes(outBuffer, maxLength, ioOffset, bytes, sizeof(bytes));
    }
    case ZigbeeZclDataType::kCharString: {
      const uint8_t len = value.stringLength;
      if (!appendBytes(outBuffer, maxLength, ioOffset, &len, 1U)) {
        return false;
      }
      if (len == 0U) {
        return true;
      }
      return appendBytes(outBuffer, maxLength, ioOffset,
                         reinterpret_cast<const uint8_t*>(value.stringValue), len);
    }
    default:
      break;
  }

  return false;
}

bool buildReadResponseForRecords(const ZigbeeReadAttributeRecord* records,
                                 uint8_t recordCount, uint8_t transactionSequence,
                                 uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (recordCount > 0U && records == nullptr) {
    return false;
  }

  uint8_t payload[127] = {0};
  uint8_t payloadLength = 0U;
  for (uint8_t i = 0U; i < recordCount; ++i) {
    if (!appendLe16(payload, sizeof(payload), &payloadLength,
                    records[i].attributeId)) {
      return false;
    }
    if (!appendBytes(payload, sizeof(payload), &payloadLength,
                     &records[i].status, 1U)) {
      return false;
    }
    if (records[i].status == kZclStatusSuccess) {
      if (!writeAttributeValue(payload, sizeof(payload), &payloadLength,
                               records[i].value)) {
        return false;
      }
    }
  }

  ZigbeeZclFrame response{};
  response.frameType = ZigbeeZclFrameType::kGlobal;
  response.directionToClient = true;
  response.disableDefaultResponse = true;
  response.transactionSequence = transactionSequence;
  response.commandId = kZclCommandReadAttributesResponse;
  return ZigbeeCodec::buildZclFrame(response, payload, payloadLength, outFrame,
                                    outLength);
}

bool buildConfigureReportingResponseForRecords(
    const ZigbeeConfigureReportingStatusRecord* records, uint8_t recordCount,
    uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (recordCount > 0U && records == nullptr) {
    return false;
  }

  bool allSuccess = (recordCount > 0U);
  for (uint8_t i = 0U; i < recordCount; ++i) {
    if (records[i].status != kZclStatusSuccess) {
      allSuccess = false;
      break;
    }
  }

  uint8_t payload[127] = {0};
  uint8_t payloadLength = 0U;
  if (recordCount == 0U || allSuccess) {
    payload[0] = kZclStatusSuccess;
    payloadLength = 1U;
  } else {
    for (uint8_t i = 0U; i < recordCount; ++i) {
      if (records[i].status == kZclStatusSuccess) {
        continue;
      }
      if (!appendBytes(payload, sizeof(payload), &payloadLength,
                       &records[i].status, 1U) ||
          !appendBytes(payload, sizeof(payload), &payloadLength,
                       &records[i].direction, 1U) ||
          !appendLe16(payload, sizeof(payload), &payloadLength,
                      records[i].attributeId)) {
        return false;
      }
    }
  }

  ZigbeeZclFrame response{};
  response.frameType = ZigbeeZclFrameType::kGlobal;
  response.directionToClient = true;
  response.disableDefaultResponse = true;
  response.transactionSequence = transactionSequence;
  response.commandId = kZclCommandConfigureReportingResponse;
  return ZigbeeCodec::buildZclFrame(response, payload, payloadLength, outFrame,
                                    outLength);
}

bool buildAttributeReportForRecords(const ZigbeeAttributeReportRecord* records,
                                    uint8_t recordCount,
                                    uint8_t transactionSequence,
                                    uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (recordCount > 0U && records == nullptr) {
    return false;
  }

  uint8_t payload[127] = {0};
  uint8_t payloadLength = 0U;
  for (uint8_t i = 0U; i < recordCount; ++i) {
    if (!appendLe16(payload, sizeof(payload), &payloadLength,
                    records[i].attributeId) ||
        !writeAttributeValue(payload, sizeof(payload), &payloadLength,
                             records[i].value)) {
      return false;
    }
  }

  ZigbeeZclFrame response{};
  response.frameType = ZigbeeZclFrameType::kGlobal;
  response.directionToClient = false;
  response.disableDefaultResponse = true;
  response.transactionSequence = transactionSequence;
  response.commandId = kZclCommandReportAttributes;
  return ZigbeeCodec::buildZclFrame(response, payload, payloadLength, outFrame,
                                    outLength);
}

bool appendClusterIds(const uint16_t* clusters, uint8_t clusterCount,
                      uint8_t* outPayload, uint8_t maxLength,
                      uint8_t* ioOffset) {
  if ((clusters == nullptr) && (clusterCount > 0U)) {
    return false;
  }
  for (uint8_t i = 0U; i < clusterCount; ++i) {
    if (!appendLe16(outPayload, maxLength, ioOffset, clusters[i])) {
      return false;
    }
  }
  return true;
}

uint8_t descriptorCount(const ZigbeeHomeAutomationConfig& config) {
  return config.endpointCount > 4U ? 4U : config.endpointCount;
}

uint8_t buildMacCapabilityFlags(const ZigbeeHomeAutomationConfig& config) {
  const bool mainsPowered = !config.power.batteryBacked;
  const bool receiverOnWhenIdle = mainsPowered && config.onOff.enabled;
  uint8_t flags = 0U;
  flags |= mainsPowered ? (1U << 2U) : 0U;
  flags |= receiverOnWhenIdle ? (1U << 3U) : 0U;
  flags |= (1U << 6U);  // Security capable.
  flags |= (1U << 7U);  // Allocate address.
  return flags;
}

}  // namespace

bool ZigbeeCodec::buildMacFrame(const ZigbeeMacFrame& frame,
                                const uint8_t* payload, uint8_t payloadLength,
                                uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.securityEnabled) {
    return false;
  }
  if (frame.frameVersion > 0x03U) {
    return false;
  }
  if (frame.panCompression &&
      (frame.destination.mode == ZigbeeMacAddressMode::kNone ||
       frame.source.mode == ZigbeeMacAddressMode::kNone ||
       frame.destination.panId != frame.source.panId)) {
    return false;
  }

  uint8_t offset = 0U;
  uint16_t control = static_cast<uint16_t>(frame.frameType);
  control |= frame.securityEnabled ? (1U << 3U) : 0U;
  control |= frame.framePending ? (1U << 4U) : 0U;
  control |= frame.ackRequested ? (1U << 5U) : 0U;
  control |= frame.panCompression ? (1U << 6U) : 0U;
  control |= (static_cast<uint16_t>(frame.destination.mode) & 0x03U) << 10U;
  control |= (static_cast<uint16_t>(frame.frameVersion) & 0x03U) << 12U;
  control |= (static_cast<uint16_t>(frame.source.mode) & 0x03U) << 14U;

  if (!appendLe16(outFrame, 127U, &offset, control) ||
      !appendBytes(outFrame, 127U, &offset, &frame.sequence, 1U)) {
    return false;
  }

  if (frame.destination.mode != ZigbeeMacAddressMode::kNone) {
    if (!appendLe16(outFrame, 127U, &offset, frame.destination.panId) ||
        !appendAddressValue(frame.destination, outFrame, &offset)) {
      return false;
    }
  }

  if (frame.source.mode != ZigbeeMacAddressMode::kNone) {
    if (!frame.panCompression && frame.destination.mode != ZigbeeMacAddressMode::kNone) {
      if (!appendLe16(outFrame, 127U, &offset, frame.source.panId)) {
        return false;
      }
    } else if (frame.destination.mode == ZigbeeMacAddressMode::kNone) {
      if (!appendLe16(outFrame, 127U, &offset, frame.source.panId)) {
        return false;
      }
    }
    if (!appendAddressValue(frame.source, outFrame, &offset)) {
      return false;
    }
  }

  if (frame.frameType == ZigbeeMacFrameType::kCommand) {
    if (!appendBytes(outFrame, 127U, &offset, &frame.commandId, 1U)) {
      return false;
    }
  }

  if (!appendBytes(outFrame, 127U, &offset, payload, payloadLength)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseMacFrame(const uint8_t* frame, uint8_t length,
                                ZigbeeMacFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 3U) {
    return false;
  }

  const uint16_t control = readLe16(&frame[0]);
  uint8_t offset = 2U;
  outFrame->valid = true;
  outFrame->frameType = static_cast<ZigbeeMacFrameType>(control & 0x07U);
  outFrame->securityEnabled = ((control >> 3U) & 0x1U) != 0U;
  outFrame->framePending = ((control >> 4U) & 0x1U) != 0U;
  outFrame->ackRequested = ((control >> 5U) & 0x1U) != 0U;
  outFrame->panCompression = ((control >> 6U) & 0x1U) != 0U;
  outFrame->frameVersion = static_cast<uint8_t>((control >> 12U) & 0x03U);
  outFrame->sequence = frame[offset++];

  const ZigbeeMacAddressMode destinationMode =
      static_cast<ZigbeeMacAddressMode>((control >> 10U) & 0x03U);
  const ZigbeeMacAddressMode sourceMode =
      static_cast<ZigbeeMacAddressMode>((control >> 14U) & 0x03U);

  if (!parseAddressField(frame, length, &offset, destinationMode,
                         destinationMode != ZigbeeMacAddressMode::kNone, 0U,
                         &outFrame->destination)) {
    return false;
  }

  const bool sourcePanPresent =
      (sourceMode != ZigbeeMacAddressMode::kNone) &&
      !((outFrame->panCompression) &&
        (destinationMode != ZigbeeMacAddressMode::kNone));
  const uint16_t implicitPan = outFrame->destination.panId;
  if (!parseAddressField(frame, length, &offset, sourceMode, sourcePanPresent,
                         implicitPan, &outFrame->source)) {
    return false;
  }

  if (outFrame->frameType == ZigbeeMacFrameType::kCommand) {
    if (length < static_cast<uint8_t>(offset + 1U)) {
      return false;
    }
    outFrame->commandId = frame[offset++];
  }

  if (length < offset) {
    return false;
  }
  outFrame->payload = &frame[offset];
  outFrame->payloadLength = static_cast<uint8_t>(length - offset);
  return true;
}

bool ZigbeeCodec::buildAssociationRequest(uint8_t sequence, uint16_t panId,
                                          uint16_t coordinatorShort,
                                          uint64_t deviceExtended,
                                          uint8_t capabilityInformation,
                                          uint8_t* outFrame,
                                          uint8_t* outLength) {
  const uint8_t payload[1] = {capabilityInformation};
  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.ackRequested = true;
  frame.panCompression = true;
  frame.sequence = sequence;
  frame.destination.mode = ZigbeeMacAddressMode::kShort;
  frame.destination.panId = panId;
  frame.destination.shortAddress = coordinatorShort;
  frame.source.mode = ZigbeeMacAddressMode::kExtended;
  frame.source.panId = panId;
  frame.source.extendedAddress = deviceExtended;
  frame.commandId = kZigbeeMacCommandAssociationRequest;
  return buildMacFrame(frame, payload, sizeof(payload), outFrame, outLength);
}

bool ZigbeeCodec::parseAssociationRequest(
    const uint8_t* frame, uint8_t length,
    ZigbeeMacAssociationRequestView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (frame == nullptr || outView == nullptr) {
    return false;
  }

  ZigbeeMacFrame parsed{};
  if (!parseMacFrame(frame, length, &parsed) || !parsed.valid ||
      parsed.frameType != ZigbeeMacFrameType::kCommand ||
      parsed.commandId != kZigbeeMacCommandAssociationRequest ||
      parsed.destination.mode != ZigbeeMacAddressMode::kShort ||
      parsed.source.mode != ZigbeeMacAddressMode::kExtended ||
      parsed.payloadLength != 1U) {
    return false;
  }

  outView->valid = true;
  outView->sequence = parsed.sequence;
  outView->coordinatorPanId = parsed.destination.panId;
  outView->coordinatorShort = parsed.destination.shortAddress;
  outView->deviceExtended = parsed.source.extendedAddress;
  outView->capabilityInformation = parsed.payload[0];
  return true;
}

bool ZigbeeCodec::buildAssociationResponse(uint8_t sequence, uint16_t panId,
                                           uint64_t destinationExtended,
                                           uint16_t coordinatorShort,
                                           uint16_t assignedShort,
                                           uint8_t status, uint8_t* outFrame,
                                           uint8_t* outLength) {
  uint8_t payload[3];
  writeLe16(&payload[0], assignedShort);
  payload[2] = status;

  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.panCompression = true;
  frame.sequence = sequence;
  frame.destination.mode = ZigbeeMacAddressMode::kExtended;
  frame.destination.panId = panId;
  frame.destination.extendedAddress = destinationExtended;
  frame.source.mode = ZigbeeMacAddressMode::kShort;
  frame.source.panId = panId;
  frame.source.shortAddress = coordinatorShort;
  frame.commandId = kZigbeeMacCommandAssociationResponse;
  return buildMacFrame(frame, payload, sizeof(payload), outFrame, outLength);
}

bool ZigbeeCodec::parseAssociationResponse(
    const uint8_t* frame, uint8_t length,
    ZigbeeMacAssociationResponseView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (frame == nullptr || outView == nullptr) {
    return false;
  }

  ZigbeeMacFrame parsed{};
  if (!parseMacFrame(frame, length, &parsed) || !parsed.valid ||
      parsed.frameType != ZigbeeMacFrameType::kCommand ||
      parsed.commandId != kZigbeeMacCommandAssociationResponse ||
      parsed.destination.mode != ZigbeeMacAddressMode::kExtended ||
      parsed.source.mode != ZigbeeMacAddressMode::kShort ||
      parsed.payloadLength != 3U) {
    return false;
  }

  outView->valid = true;
  outView->sequence = parsed.sequence;
  outView->panId = parsed.destination.panId;
  outView->coordinatorShort = parsed.source.shortAddress;
  outView->destinationExtended = parsed.destination.extendedAddress;
  outView->assignedShort = readLe16(parsed.payload);
  outView->status = parsed.payload[2];
  return true;
}

bool ZigbeeCodec::buildOrphanNotification(uint8_t sequence,
                                          uint64_t deviceExtended,
                                          uint8_t* outFrame,
                                          uint8_t* outLength) {
  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.sequence = sequence;
  frame.panCompression = true;
  frame.destination.mode = ZigbeeMacAddressMode::kShort;
  frame.destination.panId = 0xFFFFU;
  frame.destination.shortAddress = 0xFFFFU;
  frame.source.mode = ZigbeeMacAddressMode::kExtended;
  frame.source.panId = 0xFFFFU;
  frame.source.extendedAddress = deviceExtended;
  frame.commandId = kZigbeeMacCommandOrphanNotification;
  return buildMacFrame(frame, nullptr, 0U, outFrame, outLength);
}

bool ZigbeeCodec::parseOrphanNotification(
    const uint8_t* frame, uint8_t length,
    ZigbeeMacOrphanNotificationView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (frame == nullptr || outView == nullptr) {
    return false;
  }

  ZigbeeMacFrame parsed{};
  if (!parseMacFrame(frame, length, &parsed) || !parsed.valid ||
      parsed.frameType != ZigbeeMacFrameType::kCommand ||
      parsed.commandId != kZigbeeMacCommandOrphanNotification ||
      parsed.destination.mode != ZigbeeMacAddressMode::kShort ||
      parsed.destination.shortAddress != 0xFFFFU ||
      parsed.source.mode != ZigbeeMacAddressMode::kExtended ||
      parsed.payloadLength != 0U) {
    return false;
  }

  outView->valid = true;
  outView->sequence = parsed.sequence;
  outView->panId = parsed.destination.panId;
  outView->deviceExtended = parsed.source.extendedAddress;
  return true;
}

bool ZigbeeCodec::buildCoordinatorRealignment(uint8_t sequence, uint16_t panId,
                                              uint16_t coordinatorShort,
                                              uint8_t channel,
                                              uint16_t assignedShort,
                                              uint64_t destinationExtended,
                                              uint8_t* outFrame,
                                              uint8_t* outLength) {
  uint8_t payload[5] = {0U};
  writeLe16(&payload[0], panId);
  writeLe16(&payload[2], coordinatorShort);
  payload[4] = channel;

  uint8_t tail[2] = {0U};
  writeLe16(&tail[0], assignedShort);

  uint8_t fullPayload[7] = {0U};
  memcpy(fullPayload, payload, sizeof(payload));
  memcpy(&fullPayload[5], tail, sizeof(tail));

  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.sequence = sequence;
  frame.panCompression = true;
  frame.destination.mode = ZigbeeMacAddressMode::kExtended;
  frame.destination.panId = panId;
  frame.destination.extendedAddress = destinationExtended;
  frame.source.mode = ZigbeeMacAddressMode::kShort;
  frame.source.panId = panId;
  frame.source.shortAddress = coordinatorShort;
  frame.commandId = kZigbeeMacCommandCoordinatorRealignment;
  return buildMacFrame(frame, fullPayload, sizeof(fullPayload), outFrame,
                       outLength);
}

bool ZigbeeCodec::parseCoordinatorRealignment(
    const uint8_t* frame, uint8_t length,
    ZigbeeMacCoordinatorRealignmentView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (frame == nullptr || outView == nullptr) {
    return false;
  }

  ZigbeeMacFrame parsed{};
  if (!parseMacFrame(frame, length, &parsed) || !parsed.valid ||
      parsed.frameType != ZigbeeMacFrameType::kCommand ||
      parsed.commandId != kZigbeeMacCommandCoordinatorRealignment ||
      parsed.destination.mode != ZigbeeMacAddressMode::kExtended ||
      parsed.source.mode != ZigbeeMacAddressMode::kShort ||
      parsed.payloadLength < 7U) {
    return false;
  }

  outView->valid = true;
  outView->sequence = parsed.sequence;
  outView->panId = readLe16(&parsed.payload[0]);
  outView->coordinatorShort = readLe16(&parsed.payload[2]);
  outView->channel = parsed.payload[4];
  outView->assignedShort = readLe16(&parsed.payload[5]);
  outView->destinationExtended = parsed.destination.extendedAddress;
  return true;
}

bool ZigbeeCodec::buildBeaconFrame(uint8_t sequence, uint16_t panId,
                                   uint16_t coordinatorShort,
                                   const ZigbeeMacBeaconPayload& payload,
                                   uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t beaconPayload[19] = {0U};
  uint16_t superframeSpecification = 0x0FFFU;
  superframeSpecification |= payload.panCoordinator ? 0x4000U : 0U;
  superframeSpecification |= payload.associationPermit ? 0x8000U : 0U;
  writeLe16(&beaconPayload[0], superframeSpecification);
  beaconPayload[2] = 0U;  // No GTS.
  beaconPayload[3] = 0U;  // No pending-address list.
  beaconPayload[4] = payload.protocolId;
  beaconPayload[5] = static_cast<uint8_t>((payload.stackProfile & 0x0FU) |
                                          ((payload.protocolVersion & 0x0FU) << 4U));
  beaconPayload[6] = static_cast<uint8_t>((payload.routerCapacity ? 0x04U : 0U) |
                                          (payload.endDeviceCapacity ? 0x08U : 0U));
  writeLe64(&beaconPayload[7], payload.extendedPanId);
  writeLe24(&beaconPayload[15], payload.txOffset & 0x00FFFFFFUL);
  beaconPayload[18] = payload.updateId;

  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kBeacon;
  frame.sequence = sequence;
  frame.source.mode = ZigbeeMacAddressMode::kShort;
  frame.source.panId = panId;
  frame.source.shortAddress = coordinatorShort;
  return buildMacFrame(frame, beaconPayload, sizeof(beaconPayload), outFrame,
                       outLength);
}

bool ZigbeeCodec::parseBeaconFrame(const uint8_t* frame, uint8_t length,
                                   ZigbeeMacBeaconView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (frame == nullptr || outView == nullptr) {
    return false;
  }

  ZigbeeMacFrame parsed{};
  if (!parseMacFrame(frame, length, &parsed) || !parsed.valid ||
      parsed.frameType != ZigbeeMacFrameType::kBeacon ||
      parsed.destination.mode != ZigbeeMacAddressMode::kNone ||
      parsed.source.mode != ZigbeeMacAddressMode::kShort ||
      parsed.payloadLength < 19U) {
    return false;
  }

  outView->valid = true;
  outView->sequence = parsed.sequence;
  outView->panId = parsed.source.panId;
  outView->sourceShort = parsed.source.shortAddress;
  outView->superframeSpecification = readLe16(&parsed.payload[0]);
  outView->panCoordinator =
      ((outView->superframeSpecification >> 14U) & 0x01U) != 0U;
  outView->associationPermit =
      ((outView->superframeSpecification >> 15U) & 0x01U) != 0U;

  outView->network.valid = true;
  outView->network.protocolId = parsed.payload[4];
  outView->network.stackProfile = static_cast<uint8_t>(parsed.payload[5] & 0x0FU);
  outView->network.protocolVersion =
      static_cast<uint8_t>((parsed.payload[5] >> 4U) & 0x0FU);
  outView->network.routerCapacity = (parsed.payload[6] & 0x04U) != 0U;
  outView->network.endDeviceCapacity = (parsed.payload[6] & 0x08U) != 0U;
  outView->network.extendedPanId = readLe64(&parsed.payload[7]);
  outView->network.txOffset = readLe24(&parsed.payload[15]);
  outView->network.updateId = parsed.payload[18];
  return true;
}

bool ZigbeeCodec::buildBeaconRequest(uint8_t sequence, uint8_t* outFrame,
                                     uint8_t* outLength) {
  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.sequence = sequence;
  frame.destination.mode = ZigbeeMacAddressMode::kShort;
  frame.destination.panId = 0xFFFFU;
  frame.destination.shortAddress = 0xFFFFU;
  frame.source.mode = ZigbeeMacAddressMode::kNone;
  frame.commandId = kZigbeeMacCommandBeaconRequest;
  return buildMacFrame(frame, nullptr, 0U, outFrame, outLength);
}

bool ZigbeeCodec::buildDataRequest(uint8_t sequence, uint16_t panId,
                                   uint16_t coordinatorShort,
                                   uint64_t deviceExtended, uint8_t* outFrame,
                                   uint8_t* outLength) {
  ZigbeeMacFrame frame{};
  frame.frameType = ZigbeeMacFrameType::kCommand;
  frame.ackRequested = true;
  frame.panCompression = true;
  frame.sequence = sequence;
  frame.destination.mode = ZigbeeMacAddressMode::kShort;
  frame.destination.panId = panId;
  frame.destination.shortAddress = coordinatorShort;
  frame.source.mode = ZigbeeMacAddressMode::kExtended;
  frame.source.panId = panId;
  frame.source.extendedAddress = deviceExtended;
  frame.commandId = kZigbeeMacCommandDataRequest;
  return buildMacFrame(frame, nullptr, 0U, outFrame, outLength);
}

bool ZigbeeCodec::buildNwkFrame(const ZigbeeNetworkFrame& frame,
                                const uint8_t* payload, uint8_t payloadLength,
                                uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.securityEnabled || frame.multicast || frame.sourceRoute) {
    return false;
  }

  uint8_t offset = 0U;
  uint16_t control = static_cast<uint16_t>(frame.frameType);
  control |= (static_cast<uint16_t>(kNwkProtocolVersion & 0x0FU) << 2U);
  control |= (static_cast<uint16_t>(frame.discoverRoute & 0x03U) << 6U);
  control |= frame.securityEnabled ? (1U << 9U) : 0U;
  control |= frame.sourceRoute ? (1U << 10U) : 0U;
  control |= frame.extendedDestination ? (1U << 11U) : 0U;
  control |= frame.extendedSource ? (1U << 12U) : 0U;

  if (!appendLe16(outFrame, 127U, &offset, control) ||
      !appendLe16(outFrame, 127U, &offset, frame.destinationShort) ||
      !appendLe16(outFrame, 127U, &offset, frame.sourceShort) ||
      !appendBytes(outFrame, 127U, &offset, &frame.radius, 1U) ||
      !appendBytes(outFrame, 127U, &offset, &frame.sequence, 1U)) {
    return false;
  }

  if (frame.extendedDestination &&
      !appendLe64(outFrame, 127U, &offset, frame.destinationExtended)) {
    return false;
  }
  if (frame.extendedSource &&
      !appendLe64(outFrame, 127U, &offset, frame.sourceExtended)) {
    return false;
  }

  if (!appendBytes(outFrame, 127U, &offset, payload, payloadLength)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseNwkFrame(const uint8_t* frame, uint8_t length,
                                ZigbeeNetworkFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 8U) {
    return false;
  }

  const uint16_t control = readLe16(&frame[0]);
  const uint8_t protocolVersion = static_cast<uint8_t>((control >> 2U) & 0x0FU);
  if (protocolVersion != kNwkProtocolVersion) {
    return false;
  }

  const bool securityEnabled = ((control >> 9U) & 0x1U) != 0U;
  const bool sourceRoute = ((control >> 10U) & 0x1U) != 0U;
  const bool extendedDestination = ((control >> 11U) & 0x1U) != 0U;
  const bool extendedSource = ((control >> 12U) & 0x1U) != 0U;
  const bool multicast = ((control >> 8U) & 0x1U) != 0U;
  if (securityEnabled || sourceRoute || multicast) {
    return false;
  }

  uint8_t offset = 2U;
  outFrame->valid = true;
  outFrame->frameType =
      static_cast<ZigbeeNwkFrameType>(control & 0x03U);
  outFrame->discoverRoute = static_cast<uint8_t>((control >> 6U) & 0x03U);
  outFrame->multicast = multicast;
  outFrame->securityEnabled = securityEnabled;
  outFrame->sourceRoute = sourceRoute;
  outFrame->extendedDestination = extendedDestination;
  outFrame->extendedSource = extendedSource;

  if (length < static_cast<uint8_t>(offset + 6U)) {
    return false;
  }

  outFrame->destinationShort = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->sourceShort = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->radius = frame[offset++];
  outFrame->sequence = frame[offset++];

  if (extendedDestination) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outFrame->destinationExtended = readLe64(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }
  if (extendedSource) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outFrame->sourceExtended = readLe64(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }

  if (length < offset) {
    return false;
  }

  outFrame->payload = &frame[offset];
  outFrame->payloadLength = static_cast<uint8_t>(length - offset);
  return true;
}

bool ZigbeeCodec::buildNwkRejoinRequestCommand(uint8_t capabilityInformation,
                                               uint8_t* outFrame,
                                               uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  outFrame[0] = kZigbeeNwkCommandRejoinRequest;
  outFrame[1] = capabilityInformation;
  *outLength = 2U;
  return true;
}

bool ZigbeeCodec::parseNwkRejoinRequestCommand(
    const uint8_t* frame, uint8_t length, ZigbeeNwkRejoinRequest* outRequest) {
  if (outRequest != nullptr) {
    memset(outRequest, 0, sizeof(*outRequest));
  }
  if (frame == nullptr || outRequest == nullptr || length != 2U ||
      frame[0] != kZigbeeNwkCommandRejoinRequest) {
    return false;
  }

  outRequest->valid = true;
  outRequest->capabilityInformation = frame[1];
  return true;
}

bool ZigbeeCodec::buildNwkRejoinResponseCommand(uint16_t networkAddress,
                                                uint8_t status,
                                                uint8_t* outFrame,
                                                uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  outFrame[0] = kZigbeeNwkCommandRejoinResponse;
  writeLe16(&outFrame[1], networkAddress);
  outFrame[3] = status;
  *outLength = 4U;
  return true;
}

bool ZigbeeCodec::parseNwkRejoinResponseCommand(
    const uint8_t* frame, uint8_t length,
    ZigbeeNwkRejoinResponse* outResponse) {
  if (outResponse != nullptr) {
    memset(outResponse, 0, sizeof(*outResponse));
  }
  if (frame == nullptr || outResponse == nullptr || length != 4U ||
      frame[0] != kZigbeeNwkCommandRejoinResponse) {
    return false;
  }

  outResponse->valid = true;
  outResponse->networkAddress = readLe16(&frame[1]);
  outResponse->status = frame[3];
  return true;
}

bool ZigbeeCodec::buildNwkEndDeviceTimeoutRequestCommand(
    uint8_t requestedTimeout, uint8_t endDeviceConfiguration,
    uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  outFrame[0] = kZigbeeNwkCommandEndDeviceTimeoutRequest;
  outFrame[1] = requestedTimeout;
  outFrame[2] = endDeviceConfiguration;
  *outLength = 3U;
  return true;
}

bool ZigbeeCodec::parseNwkEndDeviceTimeoutRequestCommand(
    const uint8_t* frame, uint8_t length,
    ZigbeeNwkEndDeviceTimeoutRequest* outRequest) {
  if (outRequest != nullptr) {
    memset(outRequest, 0, sizeof(*outRequest));
  }
  if (frame == nullptr || outRequest == nullptr || length != 3U ||
      frame[0] != kZigbeeNwkCommandEndDeviceTimeoutRequest) {
    return false;
  }

  outRequest->valid = true;
  outRequest->requestedTimeout = frame[1];
  outRequest->endDeviceConfiguration = frame[2];
  return true;
}

bool ZigbeeCodec::buildNwkEndDeviceTimeoutResponseCommand(
    uint8_t status, uint8_t parentInformation, uint8_t* outFrame,
    uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  outFrame[0] = kZigbeeNwkCommandEndDeviceTimeoutResponse;
  outFrame[1] = status;
  outFrame[2] = parentInformation;
  *outLength = 3U;
  return true;
}

bool ZigbeeCodec::parseNwkEndDeviceTimeoutResponseCommand(
    const uint8_t* frame, uint8_t length,
    ZigbeeNwkEndDeviceTimeoutResponse* outResponse) {
  if (outResponse != nullptr) {
    memset(outResponse, 0, sizeof(*outResponse));
  }
  if (frame == nullptr || outResponse == nullptr || length != 3U ||
      frame[0] != kZigbeeNwkCommandEndDeviceTimeoutResponse) {
    return false;
  }

  outResponse->valid = true;
  outResponse->status = frame[1];
  outResponse->parentInformation = frame[2];
  return true;
}

bool ZigbeeCodec::buildApsDataFrame(const ZigbeeApsDataFrame& frame,
                                    const uint8_t* payload,
                                    uint8_t payloadLength, uint8_t* outFrame,
                                    uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.frameType != ZigbeeApsFrameType::kData ||
      (frame.deliveryMode != kZigbeeApsDeliveryUnicast &&
       frame.deliveryMode != kZigbeeApsDeliveryGroup) ||
      frame.securityEnabled) {
    return false;
  }
  if (frame.deliveryMode == kZigbeeApsDeliveryUnicast &&
      frame.destinationEndpoint == 0U && frame.profileId != kZigbeeProfileZdo) {
    return false;
  }
  if (frame.deliveryMode == kZigbeeApsDeliveryGroup &&
      (frame.destinationGroup == 0U || frame.sourceEndpoint == 0U)) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t control = static_cast<uint8_t>(frame.frameType);
  control |= static_cast<uint8_t>((frame.deliveryMode & 0x03U) << 2U);
  control |= frame.securityEnabled ? (1U << 5U) : 0U;
  control |= frame.ackRequested ? (1U << 6U) : 0U;

  if (!appendBytes(outFrame, 127U, &offset, &control, 1U)) {
    return false;
  }
  if (frame.deliveryMode == kZigbeeApsDeliveryUnicast) {
    if (!appendBytes(outFrame, 127U, &offset, &frame.destinationEndpoint, 1U)) {
      return false;
    }
  } else {
    if (!appendLe16(outFrame, 127U, &offset, frame.destinationGroup)) {
      return false;
    }
  }
  if (!appendLe16(outFrame, 127U, &offset, frame.clusterId) ||
      !appendLe16(outFrame, 127U, &offset, frame.profileId) ||
      !appendBytes(outFrame, 127U, &offset, &frame.sourceEndpoint, 1U) ||
      !appendBytes(outFrame, 127U, &offset, &frame.counter, 1U) ||
      !appendBytes(outFrame, 127U, &offset, payload, payloadLength)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseApsDataFrame(const uint8_t* frame, uint8_t length,
                                    ZigbeeApsDataFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 8U) {
    return false;
  }

  const uint8_t control = frame[0];
  const ZigbeeApsFrameType frameType =
      static_cast<ZigbeeApsFrameType>(control & 0x03U);
  const uint8_t deliveryMode = static_cast<uint8_t>((control >> 2U) & 0x03U);
  const bool securityEnabled = ((control >> 5U) & 0x1U) != 0U;
  const bool extendedHeader = ((control >> 7U) & 0x1U) != 0U;
  if (frameType != ZigbeeApsFrameType::kData ||
      (deliveryMode != kZigbeeApsDeliveryUnicast &&
       deliveryMode != kZigbeeApsDeliveryGroup) ||
      securityEnabled ||
      extendedHeader) {
    return false;
  }

  uint8_t offset = 1U;
  outFrame->valid = true;
  outFrame->frameType = frameType;
  outFrame->deliveryMode = deliveryMode;
  outFrame->securityEnabled = securityEnabled;
  outFrame->ackRequested = ((control >> 6U) & 0x1U) != 0U;
  if (deliveryMode == kZigbeeApsDeliveryUnicast) {
    if (length < 8U) {
      return false;
    }
    outFrame->destinationEndpoint = frame[offset++];
  } else {
    if (length < 9U) {
      return false;
    }
    outFrame->destinationGroup = readLe16(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
  }
  outFrame->clusterId = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->profileId = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->sourceEndpoint = frame[offset++];
  outFrame->counter = frame[offset++];
  outFrame->payload = &frame[offset];
  outFrame->payloadLength = static_cast<uint8_t>(length - offset);
  return true;
}

bool ZigbeeCodec::buildApsCommandFrame(const ZigbeeApsCommandFrame& frame,
                                       const uint8_t* payload,
                                       uint8_t payloadLength,
                                       uint8_t* outFrame,
                                       uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.frameType != ZigbeeApsFrameType::kCommand ||
      frame.deliveryMode == kZigbeeApsDeliveryGroup) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t control = static_cast<uint8_t>(frame.frameType);
  control |= static_cast<uint8_t>((frame.deliveryMode & 0x03U) << 2U);
  control |= frame.securityEnabled ? (1U << 5U) : 0U;
  control |= frame.ackRequested ? (1U << 6U) : 0U;

  if (!appendBytes(outFrame, 127U, &offset, &control, 1U) ||
      !appendBytes(outFrame, 127U, &offset, &frame.counter, 1U) ||
      !appendBytes(outFrame, 127U, &offset, &frame.commandId, 1U) ||
      !appendBytes(outFrame, 127U, &offset, payload, payloadLength)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseApsCommandFrame(const uint8_t* frame, uint8_t length,
                                       ZigbeeApsCommandFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 3U) {
    return false;
  }

  const uint8_t control = frame[0];
  const ZigbeeApsFrameType frameType =
      static_cast<ZigbeeApsFrameType>(control & 0x03U);
  const uint8_t deliveryMode = static_cast<uint8_t>((control >> 2U) & 0x03U);
  const bool extendedHeader = ((control >> 7U) & 0x1U) != 0U;
  if (frameType != ZigbeeApsFrameType::kCommand ||
      deliveryMode == kZigbeeApsDeliveryGroup || extendedHeader) {
    return false;
  }

  outFrame->valid = true;
  outFrame->frameType = frameType;
  outFrame->deliveryMode = deliveryMode;
  outFrame->securityEnabled = ((control >> 5U) & 0x1U) != 0U;
  outFrame->ackRequested = ((control >> 6U) & 0x1U) != 0U;
  outFrame->counter = frame[1];
  outFrame->commandId = frame[2];
  outFrame->payload = &frame[3];
  outFrame->payloadLength = static_cast<uint8_t>(length - 3U);
  return true;
}

bool ZigbeeCodec::buildApsAcknowledgementFrame(
    const ZigbeeApsAcknowledgementFrame& frame, uint8_t* outFrame,
    uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (frame.frameType != ZigbeeApsFrameType::kAcknowledgement ||
      frame.deliveryMode == kZigbeeApsDeliveryGroup) {
    return false;
  }
  if (!frame.ackFormatCommand &&
      ((frame.profileId != kZigbeeProfileZdo &&
        (frame.destinationEndpoint == 0U || frame.sourceEndpoint == 0U)) ||
       frame.profileId == 0U)) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t control = static_cast<uint8_t>(frame.frameType);
  control |= static_cast<uint8_t>((frame.deliveryMode & 0x03U) << 2U);
  control |= frame.ackFormatCommand ? (1U << 4U) : 0U;
  control |= frame.securityEnabled ? (1U << 5U) : 0U;

  if (!appendBytes(outFrame, 127U, &offset, &control, 1U)) {
    return false;
  }
  if (!frame.ackFormatCommand) {
    if (!appendBytes(outFrame, 127U, &offset, &frame.destinationEndpoint, 1U) ||
        !appendLe16(outFrame, 127U, &offset, frame.clusterId) ||
        !appendLe16(outFrame, 127U, &offset, frame.profileId) ||
        !appendBytes(outFrame, 127U, &offset, &frame.sourceEndpoint, 1U)) {
      return false;
    }
  }
  if (!appendBytes(outFrame, 127U, &offset, &frame.counter, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseApsAcknowledgementFrame(
    const uint8_t* frame, uint8_t length,
    ZigbeeApsAcknowledgementFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 2U) {
    return false;
  }

  const uint8_t control = frame[0];
  const ZigbeeApsFrameType frameType =
      static_cast<ZigbeeApsFrameType>(control & 0x03U);
  const uint8_t deliveryMode = static_cast<uint8_t>((control >> 2U) & 0x03U);
  const bool ackFormatCommand = ((control >> 4U) & 0x1U) != 0U;
  const bool securityEnabled = ((control >> 5U) & 0x1U) != 0U;
  const bool extendedHeader = ((control >> 7U) & 0x1U) != 0U;
  if (frameType != ZigbeeApsFrameType::kAcknowledgement ||
      deliveryMode == kZigbeeApsDeliveryGroup || extendedHeader) {
    return false;
  }

  uint8_t offset = 1U;
  outFrame->valid = true;
  outFrame->frameType = frameType;
  outFrame->deliveryMode = deliveryMode;
  outFrame->securityEnabled = securityEnabled;
  outFrame->ackFormatCommand = ackFormatCommand;

  if (!ackFormatCommand) {
    if (length < 8U) {
      return false;
    }
    outFrame->destinationEndpoint = frame[offset++];
    outFrame->clusterId = readLe16(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    outFrame->profileId = readLe16(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    outFrame->sourceEndpoint = frame[offset++];
  }

  outFrame->counter = frame[offset++];
  return offset == length;
}

bool ZigbeeCodec::buildApsDataAcknowledgement(const ZigbeeApsDataFrame& request,
                                              uint8_t* outFrame,
                                              uint8_t* outLength) {
  ZigbeeApsAcknowledgementFrame ack{};
  ack.frameType = ZigbeeApsFrameType::kAcknowledgement;
  ack.deliveryMode = request.deliveryMode;
  ack.destinationEndpoint = request.sourceEndpoint;
  ack.clusterId = request.clusterId;
  ack.profileId = request.profileId;
  ack.sourceEndpoint = request.destinationEndpoint;
  ack.counter = request.counter;
  return buildApsAcknowledgementFrame(ack, outFrame, outLength);
}

bool ZigbeeCodec::buildApsTransportKeyCommand(const ZigbeeApsTransportKey& key,
                                              uint8_t counter,
                                              uint8_t* outFrame,
                                              uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr ||
      key.keyType != kZigbeeApsTransportKeyStandardNetworkKey) {
    return false;
  }

  uint8_t payload[64] = {0U};
  uint8_t payloadLength = 0U;
  if (!appendBytes(payload, sizeof(payload), &payloadLength, &key.keyType, 1U) ||
      !appendBytes(payload, sizeof(payload), &payloadLength, key.key,
                   sizeof(key.key)) ||
      !appendBytes(payload, sizeof(payload), &payloadLength, &key.keySequence,
                   1U) ||
      !appendLe64(payload, sizeof(payload), &payloadLength,
                  key.destinationIeee) ||
      !appendLe64(payload, sizeof(payload), &payloadLength, key.sourceIeee)) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  command.frameType = ZigbeeApsFrameType::kCommand;
  command.deliveryMode = kZigbeeApsDeliveryUnicast;
  command.counter = counter;
  command.commandId = kZigbeeApsCommandTransportKey;
  return buildApsCommandFrame(command, payload, payloadLength, outFrame,
                              outLength);
}

bool ZigbeeCodec::parseApsTransportKeyCommand(const uint8_t* frame,
                                              uint8_t length,
                                              ZigbeeApsTransportKey* outKey,
                                              uint8_t* outCounter) {
  if (outKey != nullptr) {
    memset(outKey, 0, sizeof(*outKey));
  }
  if (outCounter != nullptr) {
    *outCounter = 0U;
  }
  if (frame == nullptr || outKey == nullptr || outCounter == nullptr) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  if (!parseApsCommandFrame(frame, length, &command) || !command.valid ||
      command.commandId != kZigbeeApsCommandTransportKey ||
      command.payloadLength != 34U) {
    return false;
  }

  uint8_t offset = 0U;
  outKey->valid = true;
  outKey->keyType = command.payload[offset++];
  if (outKey->keyType != kZigbeeApsTransportKeyStandardNetworkKey) {
    return false;
  }
  memcpy(outKey->key, &command.payload[offset], sizeof(outKey->key));
  offset = static_cast<uint8_t>(offset + sizeof(outKey->key));
  outKey->keySequence = command.payload[offset++];
  outKey->destinationIeee = readLe64(&command.payload[offset]);
  offset = static_cast<uint8_t>(offset + 8U);
  outKey->sourceIeee = readLe64(&command.payload[offset]);
  *outCounter = command.counter;
  return true;
}

bool ZigbeeCodec::buildApsUpdateDeviceCommand(
    const ZigbeeApsUpdateDevice& device, uint8_t counter, uint8_t* outFrame,
    uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t payload[16] = {0U};
  uint8_t payloadLength = 0U;
  if (!appendLe64(payload, sizeof(payload), &payloadLength, device.deviceIeee) ||
      !appendLe16(payload, sizeof(payload), &payloadLength, device.deviceShort) ||
      !appendBytes(payload, sizeof(payload), &payloadLength, &device.status,
                   1U)) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  command.frameType = ZigbeeApsFrameType::kCommand;
  command.deliveryMode = kZigbeeApsDeliveryUnicast;
  command.counter = counter;
  command.commandId = kZigbeeApsCommandUpdateDevice;
  return buildApsCommandFrame(command, payload, payloadLength, outFrame,
                              outLength);
}

bool ZigbeeCodec::parseApsUpdateDeviceCommand(const uint8_t* frame,
                                              uint8_t length,
                                              ZigbeeApsUpdateDevice* outDevice,
                                              uint8_t* outCounter) {
  if (outDevice != nullptr) {
    memset(outDevice, 0, sizeof(*outDevice));
  }
  if (outCounter != nullptr) {
    *outCounter = 0U;
  }
  if (frame == nullptr || outDevice == nullptr || outCounter == nullptr) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  if (!parseApsCommandFrame(frame, length, &command) || !command.valid ||
      command.commandId != kZigbeeApsCommandUpdateDevice ||
      command.payloadLength != 11U) {
    return false;
  }

  outDevice->valid = true;
  outDevice->deviceIeee = readLe64(&command.payload[0]);
  outDevice->deviceShort = readLe16(&command.payload[8]);
  outDevice->status = command.payload[10];
  *outCounter = command.counter;
  return true;
}

bool ZigbeeCodec::buildApsSwitchKeyCommand(const ZigbeeApsSwitchKey& key,
                                           uint8_t counter, uint8_t* outFrame,
                                           uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  command.frameType = ZigbeeApsFrameType::kCommand;
  command.deliveryMode = kZigbeeApsDeliveryUnicast;
  command.counter = counter;
  command.commandId = kZigbeeApsCommandSwitchKey;
  return buildApsCommandFrame(command, &key.keySequence, 1U, outFrame,
                              outLength);
}

bool ZigbeeCodec::parseApsSwitchKeyCommand(const uint8_t* frame, uint8_t length,
                                           ZigbeeApsSwitchKey* outKey,
                                           uint8_t* outCounter) {
  if (outKey != nullptr) {
    memset(outKey, 0, sizeof(*outKey));
  }
  if (outCounter != nullptr) {
    *outCounter = 0U;
  }
  if (frame == nullptr || outKey == nullptr || outCounter == nullptr) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  if (!parseApsCommandFrame(frame, length, &command) || !command.valid ||
      command.commandId != kZigbeeApsCommandSwitchKey ||
      command.payloadLength != 1U) {
    return false;
  }

  outKey->valid = true;
  outKey->keySequence = command.payload[0];
  *outCounter = command.counter;
  return true;
}

bool ZigbeeCodec::buildZclFrame(const ZigbeeZclFrame& frame,
                                const uint8_t* payload, uint8_t payloadLength,
                                uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t control = static_cast<uint8_t>(frame.frameType);
  control |= frame.manufacturerSpecific ? (1U << 2U) : 0U;
  control |= frame.directionToClient ? (1U << 3U) : 0U;
  control |= frame.disableDefaultResponse ? (1U << 4U) : 0U;
  if (!appendBytes(outFrame, 127U, &offset, &control, 1U)) {
    return false;
  }
  if (frame.manufacturerSpecific &&
      !appendLe16(outFrame, 127U, &offset, frame.manufacturerCode)) {
    return false;
  }
  if (!appendBytes(outFrame, 127U, &offset, &frame.transactionSequence, 1U) ||
      !appendBytes(outFrame, 127U, &offset, &frame.commandId, 1U) ||
      !appendBytes(outFrame, 127U, &offset, payload, payloadLength)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseZclFrame(const uint8_t* frame, uint8_t length,
                                ZigbeeZclFrame* outFrame) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (frame == nullptr || outFrame == nullptr || length < 3U) {
    return false;
  }

  const uint8_t control = frame[0];
  uint8_t offset = 1U;
  const bool manufacturerSpecific = ((control >> 2U) & 0x1U) != 0U;
  if (manufacturerSpecific) {
    if (length < 5U) {
      return false;
    }
    outFrame->manufacturerCode = readLe16(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
  }
  if (length < static_cast<uint8_t>(offset + 2U)) {
    return false;
  }

  outFrame->valid = true;
  outFrame->frameType = static_cast<ZigbeeZclFrameType>(control & 0x03U);
  outFrame->manufacturerSpecific = manufacturerSpecific;
  outFrame->directionToClient = ((control >> 3U) & 0x1U) != 0U;
  outFrame->disableDefaultResponse = ((control >> 4U) & 0x1U) != 0U;
  outFrame->transactionSequence = frame[offset++];
  outFrame->commandId = frame[offset++];
  outFrame->payload = &frame[offset];
  outFrame->payloadLength = static_cast<uint8_t>(length - offset);
  return true;
}

bool ZigbeeCodec::parseReadAttributesRequest(const uint8_t* payload,
                                             uint8_t length,
                                             uint16_t* outAttributeIds,
                                             uint8_t maxAttributeIds,
                                             uint8_t* outCount) {
  if (outCount != nullptr) {
    *outCount = 0U;
  }
  if (payload == nullptr || outAttributeIds == nullptr || outCount == nullptr) {
    return false;
  }
  if ((length == 0U) || ((length & 0x01U) != 0U)) {
    return false;
  }

  const uint8_t count = static_cast<uint8_t>(length / 2U);
  if (count > maxAttributeIds) {
    return false;
  }
  for (uint8_t i = 0U; i < count; ++i) {
    outAttributeIds[i] = readLe16(&payload[i * 2U]);
  }
  *outCount = count;
  return true;
}

bool ZigbeeCodec::buildReadAttributesRequest(const uint16_t* attributeIds,
                                             uint8_t attributeCount,
                                             uint8_t transactionSequence,
                                             uint8_t* outFrame,
                                             uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (attributeCount > 0U && attributeIds == nullptr) {
    return false;
  }

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  for (uint8_t i = 0U; i < attributeCount; ++i) {
    if (!appendLe16(payload, sizeof(payload), &payloadLength, attributeIds[i])) {
      return false;
    }
  }

  ZigbeeZclFrame request{};
  request.frameType = ZigbeeZclFrameType::kGlobal;
  request.disableDefaultResponse = true;
  request.transactionSequence = transactionSequence;
  request.commandId = kZclCommandReadAttributes;
  return ZigbeeCodec::buildZclFrame(request, payload, payloadLength, outFrame,
                                    outLength);
}

bool ZigbeeCodec::parseReadAttributesResponse(
    const uint8_t* payload, uint8_t length, ZigbeeReadAttributeRecord* outRecords,
    uint8_t maxRecords, uint8_t* outCount) {
  if (outCount != nullptr) {
    *outCount = 0U;
  }
  if (payload == nullptr || outRecords == nullptr || outCount == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t count = 0U;
  while (offset < length) {
    if ((length < static_cast<uint8_t>(offset + 3U)) || (count >= maxRecords)) {
      return false;
    }

    outRecords[count].attributeId = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    outRecords[count].status = payload[offset++];
    memset(&outRecords[count].value, 0, sizeof(outRecords[count].value));

    if (outRecords[count].status == kZclStatusSuccess) {
      if (length < static_cast<uint8_t>(offset + 1U)) {
        return false;
      }
      const ZigbeeZclDataType type =
          static_cast<ZigbeeZclDataType>(payload[offset++]);
      if (!readAttributeValue(payload, length, &offset, type,
                              &outRecords[count].value)) {
        return false;
      }
    }

    ++count;
  }

  *outCount = count;
  return true;
}

bool ZigbeeCodec::parseConfigureReportingRequest(
    const uint8_t* payload, uint8_t length,
    ZigbeeReportingConfiguration* outConfigurations,
    uint8_t maxConfigurations, uint8_t* outCount) {
  if (outCount != nullptr) {
    *outCount = 0U;
  }
  if (payload == nullptr || outConfigurations == nullptr || outCount == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t count = 0U;
  while (offset < length) {
    if (count >= maxConfigurations || (length - offset) < 6U) {
      return false;
    }

    const uint8_t direction = payload[offset++];
    if (direction != 0U) {
      return false;
    }

    ZigbeeReportingConfiguration config{};
    config.used = true;
    config.attributeId = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    config.dataType = static_cast<ZigbeeZclDataType>(payload[offset++]);
    config.minimumIntervalSeconds = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    config.maximumIntervalSeconds = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);

    if (zclDataTypeHasReportableChange(config.dataType)) {
      const uint8_t changeLength = zclDataTypeStorageLength(config.dataType);
      if (changeLength == 0U ||
          length < static_cast<uint8_t>(offset + changeLength)) {
        return false;
      }
      if (changeLength == 1U) {
        config.reportableChange = payload[offset];
      } else if (changeLength == 2U) {
        config.reportableChange = readLe16(&payload[offset]);
      } else if (changeLength == 4U) {
        config.reportableChange = static_cast<uint32_t>(payload[offset]) |
                                  (static_cast<uint32_t>(payload[offset + 1U]) << 8U) |
                                  (static_cast<uint32_t>(payload[offset + 2U]) << 16U) |
                                  (static_cast<uint32_t>(payload[offset + 3U]) << 24U);
      } else {
        return false;
      }
      offset = static_cast<uint8_t>(offset + changeLength);
    } else {
      config.reportableChange = 0U;
    }

    outConfigurations[count++] = config;
  }

  *outCount = count;
  return true;
}

bool ZigbeeCodec::buildConfigureReportingRequest(
    const ZigbeeReportingConfiguration* configurations,
    uint8_t configurationCount, uint8_t transactionSequence, uint8_t* outFrame,
    uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (configurationCount > 0U && configurations == nullptr) {
    return false;
  }

  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  for (uint8_t i = 0U; i < configurationCount; ++i) {
    const uint8_t direction = 0U;
    if (!appendBytes(payload, sizeof(payload), &payloadLength, &direction, 1U) ||
        !appendLe16(payload, sizeof(payload), &payloadLength,
                    configurations[i].attributeId) ||
        !appendBytes(payload, sizeof(payload), &payloadLength,
                     reinterpret_cast<const uint8_t*>(&configurations[i].dataType),
                     1U) ||
        !appendLe16(payload, sizeof(payload), &payloadLength,
                    configurations[i].minimumIntervalSeconds) ||
        !appendLe16(payload, sizeof(payload), &payloadLength,
                    configurations[i].maximumIntervalSeconds) ||
        !appendReportableChange(payload, sizeof(payload), &payloadLength,
                                configurations[i].dataType,
                                configurations[i].reportableChange)) {
      return false;
    }
  }

  ZigbeeZclFrame request{};
  request.frameType = ZigbeeZclFrameType::kGlobal;
  request.disableDefaultResponse = true;
  request.transactionSequence = transactionSequence;
  request.commandId = kZclCommandConfigureReporting;
  return ZigbeeCodec::buildZclFrame(request, payload, payloadLength, outFrame,
                                    outLength);
}

bool ZigbeeCodec::parseAttributeReport(const uint8_t* payload, uint8_t length,
                                       ZigbeeAttributeReportRecord* outRecords,
                                       uint8_t maxRecords, uint8_t* outCount) {
  if (outCount != nullptr) {
    *outCount = 0U;
  }
  if (payload == nullptr || outRecords == nullptr || outCount == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  uint8_t count = 0U;
  while (offset < length) {
    if ((length < static_cast<uint8_t>(offset + 3U)) || (count >= maxRecords)) {
      return false;
    }

    outRecords[count].attributeId = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
    const ZigbeeZclDataType type =
        static_cast<ZigbeeZclDataType>(payload[offset++]);
    if (!readAttributeValue(payload, length, &offset, type,
                            &outRecords[count].value)) {
      return false;
    }
    ++count;
  }

  *outCount = count;
  return true;
}

bool ZigbeeCodec::buildReadAttributesResponse(
    const ZigbeeReadAttributeRecord* records, uint8_t recordCount,
    uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength) {
  return buildReadResponseForRecords(records, recordCount, transactionSequence,
                                     outFrame, outLength);
}

bool ZigbeeCodec::buildConfigureReportingResponse(
    const ZigbeeConfigureReportingStatusRecord* records, uint8_t recordCount,
    uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength) {
  return buildConfigureReportingResponseForRecords(records, recordCount,
                                                   transactionSequence,
                                                   outFrame, outLength);
}

bool ZigbeeCodec::buildAttributeReport(
    const ZigbeeAttributeReportRecord* records, uint8_t recordCount,
    uint8_t transactionSequence, uint8_t* outFrame, uint8_t* outLength) {
  return buildAttributeReportForRecords(records, recordCount,
                                        transactionSequence, outFrame,
                                        outLength);
}

bool ZigbeeCodec::buildDefaultResponse(uint8_t transactionSequence,
                                       bool directionToClient,
                                       uint8_t commandId, uint8_t status,
                                       uint8_t* outFrame, uint8_t* outLength) {
  uint8_t payload[2] = {commandId, status};
  ZigbeeZclFrame response{};
  response.frameType = ZigbeeZclFrameType::kGlobal;
  response.directionToClient = directionToClient;
  response.disableDefaultResponse = true;
  response.transactionSequence = transactionSequence;
  response.commandId = kZclCommandDefaultResponse;
  return buildZclFrame(response, payload, sizeof(payload), outFrame, outLength);
}

bool ZigbeeCodec::buildZdoNetworkAddressRequest(uint8_t transactionSequence,
                                                uint64_t ieeeAddressOfInterest,
                                                bool requestExtendedResponse,
                                                uint8_t startIndex,
                                                uint8_t* outPayload,
                                                uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  const uint8_t requestType = requestExtendedResponse ? 0x01U : 0x00U;
  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe64(outPayload, 127U, &offset, ieeeAddressOfInterest) ||
      !appendBytes(outPayload, 127U, &offset, &requestType, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &startIndex, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::buildZdoIeeeAddressRequest(uint8_t transactionSequence,
                                             uint16_t nwkAddressOfInterest,
                                             bool requestExtendedResponse,
                                             uint8_t startIndex,
                                             uint8_t* outPayload,
                                             uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  const uint8_t requestType = requestExtendedResponse ? 0x01U : 0x00U;
  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe16(outPayload, 127U, &offset, nwkAddressOfInterest) ||
      !appendBytes(outPayload, 127U, &offset, &requestType, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &startIndex, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::buildZdoNodeDescriptorRequest(uint8_t transactionSequence,
                                                uint16_t nwkAddressOfInterest,
                                                uint8_t* outPayload,
                                                uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }
  outPayload[0] = transactionSequence;
  writeLe16(&outPayload[1], nwkAddressOfInterest);
  *outLength = 3U;
  return true;
}

bool ZigbeeCodec::buildZdoPowerDescriptorRequest(uint8_t transactionSequence,
                                                 uint16_t nwkAddressOfInterest,
                                                 uint8_t* outPayload,
                                                 uint8_t* outLength) {
  return buildZdoNodeDescriptorRequest(transactionSequence, nwkAddressOfInterest,
                                       outPayload, outLength);
}

bool ZigbeeCodec::buildZdoActiveEndpointsRequest(uint8_t transactionSequence,
                                                 uint16_t nwkAddressOfInterest,
                                                 uint8_t* outPayload,
                                                 uint8_t* outLength) {
  return buildZdoNodeDescriptorRequest(transactionSequence, nwkAddressOfInterest,
                                       outPayload, outLength);
}

bool ZigbeeCodec::buildZdoSimpleDescriptorRequest(uint8_t transactionSequence,
                                                  uint16_t nwkAddressOfInterest,
                                                  uint8_t endpoint,
                                                  uint8_t* outPayload,
                                                  uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }
  outPayload[0] = transactionSequence;
  writeLe16(&outPayload[1], nwkAddressOfInterest);
  outPayload[3] = endpoint;
  *outLength = 4U;
  return true;
}

bool ZigbeeCodec::buildZdoMatchDescriptorRequest(
    uint8_t transactionSequence, uint16_t nwkAddressOfInterest,
    uint16_t profileId, const uint16_t* inputClusters,
    uint8_t inputClusterCount, const uint16_t* outputClusters,
    uint8_t outputClusterCount, uint8_t* outPayload, uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }
  if ((inputClusterCount > 0U && inputClusters == nullptr) ||
      (outputClusterCount > 0U && outputClusters == nullptr)) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe16(outPayload, 127U, &offset, nwkAddressOfInterest) ||
      !appendLe16(outPayload, 127U, &offset, profileId) ||
      !appendBytes(outPayload, 127U, &offset, &inputClusterCount, 1U) ||
      !appendClusterIds(inputClusters, inputClusterCount, outPayload, 127U,
                        &offset) ||
      !appendBytes(outPayload, 127U, &offset, &outputClusterCount, 1U) ||
      !appendClusterIds(outputClusters, outputClusterCount, outPayload, 127U,
                        &offset)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::buildZdoBindRequest(
    uint8_t transactionSequence, uint64_t sourceIeeeAddress,
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeeeAddress, uint8_t destinationEndpoint,
    uint8_t* outPayload, uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr || sourceEndpoint == 0U) {
    return false;
  }
  if (destinationMode == ZigbeeBindingAddressMode::kExtended &&
      destinationEndpoint == 0U) {
    return false;
  }

  uint8_t offset = 0U;
  const uint8_t destinationModeValue =
      static_cast<uint8_t>(destinationMode);
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe64(outPayload, 127U, &offset, sourceIeeeAddress) ||
      !appendBytes(outPayload, 127U, &offset, &sourceEndpoint, 1U) ||
      !appendLe16(outPayload, 127U, &offset, clusterId) ||
      !appendBytes(outPayload, 127U, &offset, &destinationModeValue, 1U)) {
    return false;
  }

  if (destinationMode == ZigbeeBindingAddressMode::kGroup) {
    if (!appendLe16(outPayload, 127U, &offset, destinationGroup)) {
      return false;
    }
  } else if (destinationMode == ZigbeeBindingAddressMode::kExtended) {
    if (!appendLe64(outPayload, 127U, &offset, destinationIeeeAddress) ||
        !appendBytes(outPayload, 127U, &offset, &destinationEndpoint, 1U)) {
      return false;
    }
  } else {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::buildZdoUnbindRequest(
    uint8_t transactionSequence, uint64_t sourceIeeeAddress,
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeeeAddress, uint8_t destinationEndpoint,
    uint8_t* outPayload, uint8_t* outLength) {
  return buildZdoBindRequest(transactionSequence, sourceIeeeAddress,
                             sourceEndpoint, clusterId, destinationMode,
                             destinationGroup, destinationIeeeAddress,
                             destinationEndpoint, outPayload, outLength);
}

bool ZigbeeCodec::buildZdoMgmtLeaveRequest(uint8_t transactionSequence,
                                           uint64_t deviceIeeeAddress,
                                           uint8_t flags,
                                           uint8_t* outPayload,
                                           uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe64(outPayload, 127U, &offset, deviceIeeeAddress) ||
      !appendBytes(outPayload, 127U, &offset, &flags, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseZdoMgmtLeaveRequest(const uint8_t* payload,
                                           uint8_t length,
                                           uint8_t* outTransactionSequence,
                                           uint64_t* outDeviceIeeeAddress,
                                           uint8_t* outFlags) {
  if (outTransactionSequence != nullptr) {
    *outTransactionSequence = 0U;
  }
  if (outDeviceIeeeAddress != nullptr) {
    *outDeviceIeeeAddress = 0U;
  }
  if (outFlags != nullptr) {
    *outFlags = 0U;
  }
  if (payload == nullptr || outTransactionSequence == nullptr ||
      outDeviceIeeeAddress == nullptr || outFlags == nullptr || length < 10U) {
    return false;
  }

  *outTransactionSequence = payload[0];
  *outDeviceIeeeAddress = readLe64(&payload[1]);
  *outFlags = payload[9];
  return true;
}

bool ZigbeeCodec::buildZdoMgmtPermitJoinRequest(
    uint8_t transactionSequence, uint8_t permitDurationSeconds,
    bool trustCenterSignificance, uint8_t* outPayload, uint8_t* outLength) {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  const uint8_t tcSignificance = trustCenterSignificance ? 0x01U : 0x00U;
  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &permitDurationSeconds, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &tcSignificance, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeCodec::parseZdoMgmtPermitJoinRequest(
    const uint8_t* payload, uint8_t length, uint8_t* outTransactionSequence,
    uint8_t* outPermitDurationSeconds, bool* outTrustCenterSignificance) {
  if (outTransactionSequence != nullptr) {
    *outTransactionSequence = 0U;
  }
  if (outPermitDurationSeconds != nullptr) {
    *outPermitDurationSeconds = 0U;
  }
  if (outTrustCenterSignificance != nullptr) {
    *outTrustCenterSignificance = false;
  }
  if (payload == nullptr || outTransactionSequence == nullptr ||
      outPermitDurationSeconds == nullptr ||
      outTrustCenterSignificance == nullptr || length < 3U) {
    return false;
  }

  *outTransactionSequence = payload[0];
  *outPermitDurationSeconds = payload[1];
  *outTrustCenterSignificance = (payload[2] & 0x01U) != 0U;
  return true;
}

bool ZigbeeCodec::parseZdoStatusResponse(const uint8_t* payload, uint8_t length,
                                         uint8_t* outTransactionSequence,
                                         uint8_t* outStatus) {
  if (outTransactionSequence != nullptr) {
    *outTransactionSequence = 0U;
  }
  if (outStatus != nullptr) {
    *outStatus = 0U;
  }
  if (payload == nullptr || outTransactionSequence == nullptr ||
      outStatus == nullptr || length < 2U) {
    return false;
  }
  *outTransactionSequence = payload[0];
  *outStatus = payload[1];
  return true;
}

bool ZigbeeCodec::parseZdoAddressResponse(const uint8_t* payload, uint8_t length,
                                          ZigbeeZdoAddressResponseView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (payload == nullptr || outView == nullptr || length < 2U) {
    return false;
  }

  outView->transactionSequence = payload[0];
  outView->status = payload[1];
  if (outView->status != kZdoStatusSuccess) {
    outView->valid = true;
    return true;
  }
  if (length < 12U) {
    return false;
  }

  outView->ieeeAddress = readLe64(&payload[2]);
  outView->nwkAddress = readLe16(&payload[10]);
  if (length > 12U) {
    if (length < 14U || ((length - 14U) % 2U) != 0U) {
      return false;
    }

    outView->associatedDeviceCount = payload[12];
    outView->startIndex = payload[13];
    outView->associatedDeviceListCount =
        static_cast<uint8_t>((length - 14U) / 2U);
    if (outView->associatedDeviceListCount > 8U) {
      return false;
    }
    for (uint8_t i = 0U; i < outView->associatedDeviceListCount; ++i) {
      outView->associatedDevices[i] = readLe16(&payload[14U + (i * 2U)]);
    }
  }
  outView->valid = true;
  return true;
}

bool ZigbeeCodec::parseZdoActiveEndpointsResponse(
    const uint8_t* payload, uint8_t length,
    ZigbeeZdoActiveEndpointsResponseView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (payload == nullptr || outView == nullptr || length < 5U) {
    return false;
  }

  outView->transactionSequence = payload[0];
  outView->status = payload[1];
  outView->nwkAddressOfInterest = readLe16(&payload[2]);
  outView->endpointCount = payload[4];
  if (outView->endpointCount > 8U ||
      length < static_cast<uint8_t>(5U + outView->endpointCount)) {
    return false;
  }

  for (uint8_t i = 0U; i < outView->endpointCount; ++i) {
    outView->endpoints[i] = payload[5U + i];
  }
  outView->valid = true;
  return true;
}

bool ZigbeeCodec::parseZdoSimpleDescriptorResponse(
    const uint8_t* payload, uint8_t length,
    ZigbeeZdoSimpleDescriptorResponseView* outView) {
  if (outView != nullptr) {
    memset(outView, 0, sizeof(*outView));
  }
  if (payload == nullptr || outView == nullptr || length < 6U) {
    return false;
  }

  outView->transactionSequence = payload[0];
  outView->status = payload[1];
  outView->nwkAddressOfInterest = readLe16(&payload[2]);
  const uint8_t descriptorLength = payload[4];
  if (outView->status != kZdoStatusSuccess) {
    outView->valid = true;
    return true;
  }
  if (length < static_cast<uint8_t>(5U + descriptorLength) || descriptorLength < 8U) {
    return false;
  }

  uint8_t offset = 5U;
  outView->endpoint = payload[offset++];
  outView->profileId = readLe16(&payload[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outView->deviceId = readLe16(&payload[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outView->deviceVersion = static_cast<uint8_t>(payload[offset++] & 0x0FU);
  outView->inputClusterCount = payload[offset++];
  if (outView->inputClusterCount > 8U ||
      length < static_cast<uint8_t>(offset + (outView->inputClusterCount * 2U) + 1U)) {
    return false;
  }

  for (uint8_t i = 0U; i < outView->inputClusterCount; ++i) {
    outView->inputClusters[i] = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
  }

  outView->outputClusterCount = payload[offset++];
  if (outView->outputClusterCount > 8U ||
      length < static_cast<uint8_t>(offset + (outView->outputClusterCount * 2U))) {
    return false;
  }
  for (uint8_t i = 0U; i < outView->outputClusterCount; ++i) {
    outView->outputClusters[i] = readLe16(&payload[offset]);
    offset = static_cast<uint8_t>(offset + 2U);
  }

  outView->valid = true;
  return true;
}

ZigbeeHomeAutomationDevice::ZigbeeHomeAutomationDevice()
    : config_(),
      onOffLightInputClusters_{kZigbeeClusterBasic, kZigbeeClusterIdentify,
                               kZigbeeClusterGroups, kZigbeeClusterScenes,
                               kZigbeeClusterOnOff},
      onOffLightOutputClusters_{0U},
      dimmableLightInputClusters_{kZigbeeClusterBasic, kZigbeeClusterIdentify,
                                  kZigbeeClusterGroups, kZigbeeClusterScenes,
                                  kZigbeeClusterOnOff,
                                  kZigbeeClusterLevelControl},
      dimmableLightOutputClusters_{0U},
      temperatureSensorInputClusters_{kZigbeeClusterBasic,
                                      kZigbeeClusterPowerConfiguration,
                                      kZigbeeClusterIdentify,
                                      kZigbeeClusterTemperatureMeasurement},
      temperatureSensorOutputClusters_{0U},
      leaveRequested_(false) {}

bool ZigbeeHomeAutomationDevice::configureOnOffLight(
    uint8_t endpoint, uint64_t ieeeAddress, uint16_t nwkAddress,
    uint16_t panId, const ZigbeeBasicClusterConfig& basic,
    uint16_t manufacturerCode) {
  if (endpoint == 0U) {
    return false;
  }

  memset(&config_, 0, sizeof(config_));
  memset(reporting_, 0, sizeof(reporting_));
  memset(reportingState_, 0, sizeof(reportingState_));
  leaveRequested_ = false;
  config_.logicalType = ZigbeeLogicalType::kEndDevice;
  config_.manufacturerCode = manufacturerCode;
  config_.ieeeAddress = ieeeAddress;
  config_.nwkAddress = nwkAddress;
  config_.panId = panId;
  config_.endpointCount = 1U;
  config_.endpointDescriptors[0].endpoint = endpoint;
  config_.endpointDescriptors[0].profileId = kZigbeeProfileHomeAutomation;
  config_.endpointDescriptors[0].deviceId = kZigbeeDeviceIdOnOffLight;
  config_.endpointDescriptors[0].deviceVersion = 1U;
  config_.endpointDescriptors[0].inputClusters = onOffLightInputClusters_;
  config_.endpointDescriptors[0].inputClusterCount =
      static_cast<uint8_t>(sizeof(onOffLightInputClusters_) /
                           sizeof(onOffLightInputClusters_[0]));
  config_.endpointDescriptors[0].outputClusters = onOffLightOutputClusters_;
  config_.endpointDescriptors[0].outputClusterCount = 0U;
  config_.basic = basic;
  if (config_.basic.powerSource == 0U) {
    config_.basic.powerSource = kBasicPowerSourceMainsSinglePhase;
  }
  config_.power.batteryBacked = false;
  config_.temperature.enabled = false;
  config_.identify.enabled = true;
  config_.identify.identifyTimeSeconds = 0U;
  config_.groups.enabled = true;
  config_.scenes.enabled = true;
  config_.scenes.currentGroupId = 0U;
  config_.scenes.currentSceneId = 0U;
  config_.scenes.sceneValid = false;
  config_.onOff.enabled = true;
  config_.onOff.on = false;
  config_.level.enabled = false;
  config_.level.currentLevel = 0U;
  config_.level.minLevel = 1U;
  config_.level.maxLevel = 0xFEU;
  return true;
}

bool ZigbeeHomeAutomationDevice::configureDimmableLight(
    uint8_t endpoint, uint64_t ieeeAddress, uint16_t nwkAddress,
    uint16_t panId, const ZigbeeBasicClusterConfig& basic,
    uint16_t manufacturerCode) {
  if (endpoint == 0U) {
    return false;
  }

  memset(&config_, 0, sizeof(config_));
  memset(reporting_, 0, sizeof(reporting_));
  memset(reportingState_, 0, sizeof(reportingState_));
  leaveRequested_ = false;
  config_.logicalType = ZigbeeLogicalType::kEndDevice;
  config_.manufacturerCode = manufacturerCode;
  config_.ieeeAddress = ieeeAddress;
  config_.nwkAddress = nwkAddress;
  config_.panId = panId;
  config_.endpointCount = 1U;
  config_.endpointDescriptors[0].endpoint = endpoint;
  config_.endpointDescriptors[0].profileId = kZigbeeProfileHomeAutomation;
  config_.endpointDescriptors[0].deviceId = kZigbeeDeviceIdDimmableLight;
  config_.endpointDescriptors[0].deviceVersion = 1U;
  config_.endpointDescriptors[0].inputClusters = dimmableLightInputClusters_;
  config_.endpointDescriptors[0].inputClusterCount =
      static_cast<uint8_t>(sizeof(dimmableLightInputClusters_) /
                           sizeof(dimmableLightInputClusters_[0]));
  config_.endpointDescriptors[0].outputClusters = dimmableLightOutputClusters_;
  config_.endpointDescriptors[0].outputClusterCount = 0U;
  config_.basic = basic;
  if (config_.basic.powerSource == 0U) {
    config_.basic.powerSource = kBasicPowerSourceMainsSinglePhase;
  }
  config_.power.batteryBacked = false;
  config_.temperature.enabled = false;
  config_.identify.enabled = true;
  config_.identify.identifyTimeSeconds = 0U;
  config_.groups.enabled = true;
  config_.scenes.enabled = true;
  config_.scenes.currentGroupId = 0U;
  config_.scenes.currentSceneId = 0U;
  config_.scenes.sceneValid = false;
  config_.onOff.enabled = true;
  config_.onOff.on = false;
  config_.level.enabled = true;
  config_.level.currentLevel = 0xFEU;
  config_.level.minLevel = 1U;
  config_.level.maxLevel = 0xFEU;
  return true;
}

bool ZigbeeHomeAutomationDevice::configureTemperatureSensor(
    uint8_t endpoint, uint64_t ieeeAddress, uint16_t nwkAddress,
    uint16_t panId, const ZigbeeBasicClusterConfig& basic,
    uint16_t manufacturerCode) {
  if (endpoint == 0U) {
    return false;
  }

  memset(&config_, 0, sizeof(config_));
  memset(reporting_, 0, sizeof(reporting_));
  memset(reportingState_, 0, sizeof(reportingState_));
  leaveRequested_ = false;
  config_.logicalType = ZigbeeLogicalType::kEndDevice;
  config_.manufacturerCode = manufacturerCode;
  config_.ieeeAddress = ieeeAddress;
  config_.nwkAddress = nwkAddress;
  config_.panId = panId;
  config_.endpointCount = 1U;
  config_.endpointDescriptors[0].endpoint = endpoint;
  config_.endpointDescriptors[0].profileId = kZigbeeProfileHomeAutomation;
  config_.endpointDescriptors[0].deviceId = kZigbeeDeviceIdTemperatureSensor;
  config_.endpointDescriptors[0].deviceVersion = 1U;
  config_.endpointDescriptors[0].inputClusters = temperatureSensorInputClusters_;
  config_.endpointDescriptors[0].inputClusterCount =
      static_cast<uint8_t>(sizeof(temperatureSensorInputClusters_) /
                           sizeof(temperatureSensorInputClusters_[0]));
  config_.endpointDescriptors[0].outputClusters = temperatureSensorOutputClusters_;
  config_.endpointDescriptors[0].outputClusterCount = 0U;
  config_.basic = basic;
  if (config_.basic.powerSource == 0U) {
    config_.basic.powerSource = kBasicPowerSourceBattery;
  }
  config_.power.batteryBacked = true;
  config_.power.batteryVoltageDecivolts = 30U;
  config_.power.batteryPercentageRemainingHalf = 200U;
  config_.temperature.enabled = true;
  config_.identify.enabled = true;
  config_.identify.identifyTimeSeconds = 0U;
  config_.groups.enabled = false;
  config_.scenes.enabled = false;
  config_.temperature.measuredValueCentiDegrees = 2150;
  config_.temperature.minMeasuredValueCentiDegrees = -4000;
  config_.temperature.maxMeasuredValueCentiDegrees = 12500;
  config_.temperature.toleranceCentiDegrees = 50U;
  config_.onOff.enabled = false;
  config_.level.enabled = false;
  config_.level.currentLevel = 0U;
  config_.level.minLevel = 1U;
  config_.level.maxLevel = 0xFEU;
  return true;
}

bool ZigbeeHomeAutomationDevice::setBatteryStatus(
    uint8_t batteryVoltageDecivolts,
    uint8_t batteryPercentageRemainingHalf) {
  config_.power.batteryBacked = true;
  config_.power.batteryVoltageDecivolts = batteryVoltageDecivolts;
  config_.power.batteryPercentageRemainingHalf =
      batteryPercentageRemainingHalf;
  return true;
}

bool ZigbeeHomeAutomationDevice::setTemperatureState(
    int16_t measuredValueCentiDegrees, int16_t minMeasuredValueCentiDegrees,
    int16_t maxMeasuredValueCentiDegrees, uint16_t toleranceCentiDegrees) {
  if (!config_.temperature.enabled) {
    return false;
  }
  config_.temperature.measuredValueCentiDegrees = measuredValueCentiDegrees;
  config_.temperature.minMeasuredValueCentiDegrees =
      minMeasuredValueCentiDegrees;
  config_.temperature.maxMeasuredValueCentiDegrees =
      maxMeasuredValueCentiDegrees;
  config_.temperature.toleranceCentiDegrees = toleranceCentiDegrees;
  return true;
}

bool ZigbeeHomeAutomationDevice::setOnOff(bool on) {
  if (!config_.onOff.enabled) {
    return false;
  }
  config_.onOff.on = on;
  if (config_.level.enabled) {
    if (on && config_.level.currentLevel == 0U) {
      config_.level.currentLevel = config_.level.maxLevel;
    }
  }
  invalidateCurrentScene(&config_);
  return true;
}

bool ZigbeeHomeAutomationDevice::setLevel(uint8_t level) {
  if (!config_.level.enabled) {
    return false;
  }

  if (level == 0xFFU) {
    level = config_.level.maxLevel;
  }
  if (level > config_.level.maxLevel) {
    level = config_.level.maxLevel;
  }
  if (level > 0U && level < config_.level.minLevel) {
    level = config_.level.minLevel;
  }
  config_.level.currentLevel = level;
  if (config_.onOff.enabled) {
    config_.onOff.on = (level > 0U);
  }
  invalidateCurrentScene(&config_);
  return true;
}

bool ZigbeeHomeAutomationDevice::onOff() const { return config_.onOff.on; }

uint8_t ZigbeeHomeAutomationDevice::level() const {
  return config_.level.currentLevel;
}

const ZigbeeHomeAutomationConfig& ZigbeeHomeAutomationDevice::config() const {
  return config_;
}

const ZigbeeSimpleDescriptor* ZigbeeHomeAutomationDevice::findEndpoint(
    uint8_t endpoint) const {
  for (uint8_t i = 0U; i < descriptorCount(config_); ++i) {
    if (config_.endpointDescriptors[i].endpoint == endpoint) {
      return &config_.endpointDescriptors[i];
    }
  }
  return nullptr;
}

bool ZigbeeHomeAutomationDevice::endpointMatches(
    const ZigbeeSimpleDescriptor& descriptor, uint16_t profileId,
    const uint16_t* inputClusters, uint8_t inputClusterCount,
    const uint16_t* outputClusters, uint8_t outputClusterCount) const {
  if (descriptor.profileId != profileId) {
    return false;
  }
  for (uint8_t i = 0U; i < inputClusterCount; ++i) {
    if (!clusterListContains(descriptor.inputClusters, descriptor.inputClusterCount,
                             inputClusters[i])) {
      return false;
    }
  }
  for (uint8_t i = 0U; i < outputClusterCount; ++i) {
    if (!clusterListContains(descriptor.outputClusters,
                             descriptor.outputClusterCount,
                             outputClusters[i])) {
      return false;
    }
  }
  return true;
}

bool ZigbeeHomeAutomationDevice::buildNodeDescriptorResponse(
    uint8_t transactionSequence, uint16_t requestNwkAddress,
    uint8_t* outPayload, uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  const uint8_t logicalType =
      static_cast<uint8_t>(config_.logicalType) & 0x07U;
  const uint8_t frequencyBand = 0x08U;
  const uint8_t macCapability = buildMacCapabilityFlags(config_);
  const uint16_t incomingTransferSize = 82U;
  const uint16_t outgoingTransferSize = 82U;

  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>(&kZdoStatusSuccess), 1U) ||
      !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
      !appendBytes(outPayload, 127U, &offset, &logicalType, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &frequencyBand, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &macCapability, 1U) ||
      !appendLe16(outPayload, 127U, &offset, config_.manufacturerCode) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>("\x52"), 1U) ||
      !appendLe16(outPayload, 127U, &offset, incomingTransferSize) ||
      !appendLe16(outPayload, 127U, &offset, 0U) ||
      !appendLe16(outPayload, 127U, &offset, outgoingTransferSize) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>("\x00"), 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeHomeAutomationDevice::buildPowerDescriptorResponse(
    uint8_t transactionSequence, uint16_t requestNwkAddress,
    uint8_t* outPayload, uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  const uint8_t availableSources = config_.power.batteryBacked ? 0x08U : 0x04U;
  const uint8_t currentSource = availableSources;
  const uint8_t powerLevel = config_.power.batteryBacked ? 0x0CU : 0x0FU;
  const uint8_t descriptor0 = availableSources;
  const uint8_t descriptor1 =
      static_cast<uint8_t>((currentSource & 0x0FU) |
                           static_cast<uint8_t>((powerLevel & 0x0FU) << 4U));
  uint8_t offset = 0U;

  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>(&kZdoStatusSuccess), 1U) ||
      !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
      !appendBytes(outPayload, 127U, &offset, &descriptor0, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &descriptor1, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeHomeAutomationDevice::buildActiveEndpointsResponse(
    uint8_t transactionSequence, uint16_t requestNwkAddress,
    uint8_t* outPayload, uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  const uint8_t count = descriptorCount(config_);
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>(&kZdoStatusSuccess), 1U) ||
      !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
      !appendBytes(outPayload, 127U, &offset, &count, 1U)) {
    return false;
  }
  for (uint8_t i = 0U; i < count; ++i) {
    const uint8_t endpoint = config_.endpointDescriptors[i].endpoint;
    if (!appendBytes(outPayload, 127U, &offset, &endpoint, 1U)) {
      return false;
    }
  }

  *outLength = offset;
  return true;
}

bool ZigbeeHomeAutomationDevice::buildSimpleDescriptorResponse(
    uint8_t transactionSequence, uint16_t requestNwkAddress, uint8_t endpoint,
    uint8_t* outPayload, uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  const ZigbeeSimpleDescriptor* descriptor = findEndpoint(endpoint);
  if (descriptor == nullptr) {
    uint8_t offset = 0U;
    if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
        !appendBytes(outPayload, 127U, &offset,
                     reinterpret_cast<const uint8_t*>(&kZdoStatusNotSupported),
                     1U) ||
        !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
        !appendBytes(outPayload, 127U, &offset,
                     reinterpret_cast<const uint8_t*>("\x00"), 1U)) {
      return false;
    }
    *outLength = offset;
    return true;
  }

  const uint8_t simpleLength =
      static_cast<uint8_t>(8U + descriptor->inputClusterCount * 2U +
                           descriptor->outputClusterCount * 2U);
  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>(&kZdoStatusSuccess), 1U) ||
      !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
      !appendBytes(outPayload, 127U, &offset, &simpleLength, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &descriptor->endpoint, 1U) ||
      !appendLe16(outPayload, 127U, &offset, descriptor->profileId) ||
      !appendLe16(outPayload, 127U, &offset, descriptor->deviceId)) {
    return false;
  }

  const uint8_t deviceVersion = static_cast<uint8_t>(descriptor->deviceVersion & 0x0FU);
  if (!appendBytes(outPayload, 127U, &offset, &deviceVersion, 1U) ||
      !appendBytes(outPayload, 127U, &offset, &descriptor->inputClusterCount, 1U) ||
      !appendClusterIds(descriptor->inputClusters, descriptor->inputClusterCount,
                        outPayload, 127U, &offset) ||
      !appendBytes(outPayload, 127U, &offset,
                   &descriptor->outputClusterCount, 1U) ||
      !appendClusterIds(descriptor->outputClusters, descriptor->outputClusterCount,
                        outPayload, 127U, &offset)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeHomeAutomationDevice::buildMatchDescriptorResponse(
    uint8_t transactionSequence, uint16_t requestNwkAddress,
    uint16_t profileId, const uint16_t* inputClusters,
    uint8_t inputClusterCount, const uint16_t* outputClusters,
    uint8_t outputClusterCount, uint8_t* outPayload,
    uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t matches[4] = {0};
  uint8_t matchCount = 0U;
  for (uint8_t i = 0U; i < descriptorCount(config_); ++i) {
    const ZigbeeSimpleDescriptor& descriptor = config_.endpointDescriptors[i];
    if (endpointMatches(descriptor, profileId, inputClusters, inputClusterCount,
                        outputClusters, outputClusterCount)) {
      matches[matchCount++] = descriptor.endpoint;
    }
  }

  uint8_t offset = 0U;
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendBytes(outPayload, 127U, &offset,
                   reinterpret_cast<const uint8_t*>(&kZdoStatusSuccess), 1U) ||
      !appendLe16(outPayload, 127U, &offset, requestNwkAddress) ||
      !appendBytes(outPayload, 127U, &offset, &matchCount, 1U) ||
      !appendBytes(outPayload, 127U, &offset, matches, matchCount)) {
    return false;
  }

  *outLength = offset;
  return true;
}

bool ZigbeeHomeAutomationDevice::makeAttributeValueForCluster(
    uint16_t clusterId, uint16_t attributeId,
    ZigbeeAttributeValue* outValue) const {
  if (outValue == nullptr) {
    return false;
  }
  memset(outValue, 0, sizeof(*outValue));
  switch (clusterId) {
    case kZigbeeClusterBasic:
      switch (attributeId) {
        case 0x0000U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.basic.zclVersion;
          return true;
        case 0x0001U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.basic.applicationVersion;
          return true;
        case 0x0002U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.basic.stackVersion;
          return true;
        case 0x0003U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.basic.hwVersion;
          return true;
        case 0x0004U:
          outValue->type = ZigbeeZclDataType::kCharString;
          outValue->stringValue = config_.basic.manufacturerName;
          outValue->stringLength =
              boundedStringLength(config_.basic.manufacturerName);
          return true;
        case 0x0005U:
          outValue->type = ZigbeeZclDataType::kCharString;
          outValue->stringValue = config_.basic.modelIdentifier;
          outValue->stringLength =
              boundedStringLength(config_.basic.modelIdentifier);
          return true;
        case 0x0007U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.basic.powerSource;
          return true;
        case 0x4000U:
          outValue->type = ZigbeeZclDataType::kCharString;
          outValue->stringValue = config_.basic.swBuildId;
          outValue->stringLength =
              boundedStringLength(config_.basic.swBuildId);
          return true;
        default:
          break;
      }
      break;
    case kZigbeeClusterPowerConfiguration:
      if (!config_.power.batteryBacked) {
        break;
      }
      switch (attributeId) {
        case 0x0020U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.power.batteryVoltageDecivolts;
          return true;
        case 0x0021U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 =
              config_.power.batteryPercentageRemainingHalf;
          return true;
        default:
          break;
      }
      break;
    case kZigbeeClusterIdentify:
      if (!config_.identify.enabled) {
        break;
      }
      if (attributeId == 0x0000U) {
        outValue->type = ZigbeeZclDataType::kUint16;
        outValue->data.u16 = config_.identify.identifyTimeSeconds;
        return true;
      }
      break;
    case kZigbeeClusterGroups:
      if (!config_.groups.enabled) {
        break;
      }
      if (attributeId == 0x0000U) {
        outValue->type = ZigbeeZclDataType::kBitmap8;
        outValue->data.u8 = 0x80U;
        return true;
      }
      break;
    case kZigbeeClusterScenes:
      if (!config_.scenes.enabled) {
        break;
      }
      switch (attributeId) {
        case 0x0000U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = countScenesForGroup(
              config_.scenes, config_.scenes.currentGroupId);
          return true;
        case 0x0001U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.scenes.currentSceneId;
          return true;
        case 0x0002U:
          outValue->type = ZigbeeZclDataType::kUint16;
          outValue->data.u16 = config_.scenes.currentGroupId;
          return true;
        case 0x0003U:
          outValue->type = ZigbeeZclDataType::kBoolean;
          outValue->data.boolValue = config_.scenes.sceneValid;
          return true;
        case 0x0004U:
          outValue->type = ZigbeeZclDataType::kBitmap8;
          outValue->data.u8 = 0x80U;
          return true;
        default:
          break;
      }
      break;
    case kZigbeeClusterTemperatureMeasurement:
      if (!config_.temperature.enabled) {
        break;
      }
      switch (attributeId) {
        case 0x0000U:
          outValue->type = ZigbeeZclDataType::kInt16;
          outValue->data.i16 =
              config_.temperature.measuredValueCentiDegrees;
          return true;
        case 0x0001U:
          outValue->type = ZigbeeZclDataType::kInt16;
          outValue->data.i16 =
              config_.temperature.minMeasuredValueCentiDegrees;
          return true;
        case 0x0002U:
          outValue->type = ZigbeeZclDataType::kInt16;
          outValue->data.i16 =
              config_.temperature.maxMeasuredValueCentiDegrees;
          return true;
        case 0x0003U:
          outValue->type = ZigbeeZclDataType::kUint16;
          outValue->data.u16 =
              config_.temperature.toleranceCentiDegrees;
          return true;
        default:
          break;
      }
      break;
    case kZigbeeClusterOnOff:
      if (!config_.onOff.enabled) {
        break;
      }
      if (attributeId == 0x0000U) {
        outValue->type = ZigbeeZclDataType::kBoolean;
        outValue->data.boolValue = config_.onOff.on;
        return true;
      }
      break;
    case kZigbeeClusterLevelControl:
      if (!config_.level.enabled) {
        break;
      }
      switch (attributeId) {
        case 0x0000U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.level.currentLevel;
          return true;
        case 0x0002U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.level.minLevel;
          return true;
        case 0x0003U:
          outValue->type = ZigbeeZclDataType::kUint8;
          outValue->data.u8 = config_.level.maxLevel;
          return true;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return false;
}

bool ZigbeeHomeAutomationDevice::appendReadRecordForCluster(
    uint16_t clusterId, uint16_t attributeId,
    ZigbeeReadAttributeRecord* outRecord) const {
  if (outRecord == nullptr) {
    return false;
  }

  memset(outRecord, 0, sizeof(*outRecord));
  outRecord->attributeId = attributeId;
  outRecord->status = kZclStatusSuccess;
  if (!makeAttributeValueForCluster(clusterId, attributeId, &outRecord->value)) {
    outRecord->status = kZclStatusUnsupportedAttribute;
  }
  return true;
}

void ZigbeeHomeAutomationDevice::resetReportingState(uint8_t index) {
  if (index >= static_cast<uint8_t>(sizeof(reportingState_) /
                                    sizeof(reportingState_[0]))) {
    return;
  }
  memset(&reportingState_[index], 0, sizeof(reportingState_[index]));
}

void ZigbeeHomeAutomationDevice::seedReportingState(uint8_t index) {
  if (index >= static_cast<uint8_t>(sizeof(reportingState_) /
                                    sizeof(reportingState_[0])) ||
      !reporting_[index].used) {
    return;
  }

  resetReportingState(index);
  if (makeAttributeValueForCluster(reporting_[index].clusterId,
                                   reporting_[index].attributeId,
                                   &reportingState_[index].lastReportedValue)) {
    reportingState_[index].baselineValid = true;
    reportingState_[index].lastReportMs = millis();
  }
}

bool ZigbeeHomeAutomationDevice::configureReporting(
    uint16_t clusterId, uint16_t attributeId, ZigbeeZclDataType dataType,
    uint16_t minimumIntervalSeconds, uint16_t maximumIntervalSeconds,
    uint32_t reportableChange) {
  ZigbeeAttributeValue value{};
  if (!makeAttributeValueForCluster(clusterId, attributeId, &value) ||
      value.type != dataType) {
    return false;
  }

  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(reporting_) / sizeof(reporting_[0])); ++i) {
    if (!reporting_[i].used) {
      continue;
    }
    if (reporting_[i].clusterId == clusterId &&
        reporting_[i].attributeId == attributeId) {
      if (maximumIntervalSeconds == 0xFFFFU) {
        reporting_[i].used = false;
        resetReportingState(i);
        return true;
      }
      reporting_[i].dataType = dataType;
      reporting_[i].minimumIntervalSeconds = minimumIntervalSeconds;
      reporting_[i].maximumIntervalSeconds = maximumIntervalSeconds;
      reporting_[i].reportableChange = reportableChange;
      seedReportingState(i);
      return true;
    }
  }

  if (maximumIntervalSeconds == 0xFFFFU) {
    return true;
  }

  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(reporting_) / sizeof(reporting_[0])); ++i) {
    if (reporting_[i].used) {
      continue;
    }
    reporting_[i].used = true;
    reporting_[i].clusterId = clusterId;
    reporting_[i].attributeId = attributeId;
    reporting_[i].dataType = dataType;
    reporting_[i].minimumIntervalSeconds = minimumIntervalSeconds;
    reporting_[i].maximumIntervalSeconds = maximumIntervalSeconds;
    reporting_[i].reportableChange = reportableChange;
    seedReportingState(i);
    return true;
  }

  return false;
}

bool ZigbeeHomeAutomationDevice::buildAttributeReport(
    uint16_t clusterId, uint8_t transactionSequence, uint8_t* outFrame,
    uint8_t* outLength) const {
  if (outLength != nullptr) {
    *outLength = 0U;
  }
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  ZigbeeAttributeReportRecord records[8];
  uint8_t recordCount = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(reporting_) / sizeof(reporting_[0])); ++i) {
    if (!reporting_[i].used || reporting_[i].clusterId != clusterId) {
      continue;
    }
    if (!makeAttributeValueForCluster(clusterId, reporting_[i].attributeId,
                                      &records[recordCount].value)) {
      continue;
    }
    records[recordCount].attributeId = reporting_[i].attributeId;
    ++recordCount;
  }

  if (recordCount == 0U) {
    return false;
  }
  return ZigbeeCodec::buildAttributeReport(records, recordCount,
                                           transactionSequence, outFrame,
                                           outLength);
}

bool ZigbeeHomeAutomationDevice::buildDueAttributeReport(
    uint32_t nowMs, uint8_t transactionSequence, uint16_t* outClusterId,
    uint8_t* outFrame, uint8_t* outLength) {
  if (outClusterId != nullptr) {
    *outClusterId = 0U;
  }
  if (outLength != nullptr) {
    *outLength = 0U;
  }
  if (outClusterId == nullptr || outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  discardDueAttributeReport();

  ZigbeeAttributeReportRecord records[8];
  ZigbeeAttributeValue pendingValues[8];
  uint8_t pendingIndices[8] = {0U};
  uint8_t recordCount = 0U;
  uint16_t selectedClusterId = 0U;
  bool selectedCluster = false;

  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(reporting_) / sizeof(reporting_[0]));
       ++i) {
    if (!reporting_[i].used) {
      continue;
    }

    ZigbeeAttributeValue currentValue{};
    if (!makeAttributeValueForCluster(reporting_[i].clusterId,
                                      reporting_[i].attributeId,
                                      &currentValue)) {
      continue;
    }

    if (!reportingState_[i].baselineValid) {
      reportingState_[i].baselineValid = true;
      reportingState_[i].lastReportedValue = currentValue;
      reportingState_[i].lastReportMs = nowMs;
      continue;
    }

    const uint32_t elapsedMs = nowMs - reportingState_[i].lastReportMs;
    const uint32_t minimumMs =
        static_cast<uint32_t>(reporting_[i].minimumIntervalSeconds) * 1000UL;
    const uint16_t maximumIntervalSeconds = reporting_[i].maximumIntervalSeconds;
    const bool maximumExpired =
        maximumIntervalSeconds != 0U && maximumIntervalSeconds != 0xFFFFU &&
        elapsedMs >= static_cast<uint32_t>(maximumIntervalSeconds) * 1000UL;
    const bool minimumExpired = elapsedMs >= minimumMs;
    const bool thresholdReached =
        attributeValueMeetsReportableChange(currentValue,
                                            reportingState_[i].lastReportedValue,
                                            reporting_[i].reportableChange);
    if (!maximumExpired && !(minimumExpired && thresholdReached)) {
      continue;
    }

    if (!selectedCluster) {
      selectedClusterId = reporting_[i].clusterId;
      selectedCluster = true;
    }
    if (reporting_[i].clusterId != selectedClusterId) {
      continue;
    }

    records[recordCount].attributeId = reporting_[i].attributeId;
    records[recordCount].value = currentValue;
    pendingValues[recordCount] = currentValue;
    pendingIndices[recordCount] = i;
    ++recordCount;
  }

  if (recordCount == 0U ||
      !ZigbeeCodec::buildAttributeReport(records, recordCount,
                                         transactionSequence, outFrame,
                                         outLength)) {
    return false;
  }

  *outClusterId = selectedClusterId;
  for (uint8_t i = 0U; i < recordCount; ++i) {
    reportingState_[pendingIndices[i]].pending = true;
    reportingState_[pendingIndices[i]].pendingValue = pendingValues[i];
  }
  return true;
}

bool ZigbeeHomeAutomationDevice::commitDueAttributeReport(uint32_t nowMs) {
  bool committed = false;
  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(reportingState_) /
                                sizeof(reportingState_[0]));
       ++i) {
    if (!reportingState_[i].pending) {
      continue;
    }
    reportingState_[i].pending = false;
    reportingState_[i].baselineValid = true;
    reportingState_[i].lastReportedValue = reportingState_[i].pendingValue;
    reportingState_[i].lastReportMs = nowMs;
    memset(&reportingState_[i].pendingValue, 0,
           sizeof(reportingState_[i].pendingValue));
    committed = true;
  }
  return committed;
}

void ZigbeeHomeAutomationDevice::discardDueAttributeReport() {
  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(reportingState_) /
                                sizeof(reportingState_[0]));
       ++i) {
    reportingState_[i].pending = false;
    memset(&reportingState_[i].pendingValue, 0,
           sizeof(reportingState_[i].pendingValue));
  }
}

uint8_t ZigbeeHomeAutomationDevice::reportingConfigurationCount() const {
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(reporting_) / sizeof(reporting_[0])); ++i) {
    if (reporting_[i].used) {
      ++count;
    }
  }
  return count;
}

const ZigbeeReportingConfiguration*
ZigbeeHomeAutomationDevice::reportingConfigurations() const {
  return reporting_;
}

bool ZigbeeHomeAutomationDevice::addBinding(
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeee, uint8_t destinationEndpoint) {
  return setBinding(sourceEndpoint, clusterId, destinationMode,
                    destinationGroup, destinationIeee, destinationEndpoint);
}

bool ZigbeeHomeAutomationDevice::removeBinding(
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeee, uint8_t destinationEndpoint) {
  return clearBinding(sourceEndpoint, clusterId, destinationMode,
                      destinationGroup, destinationIeee, destinationEndpoint);
}

uint8_t ZigbeeHomeAutomationDevice::bindingCount() const {
  uint8_t count = 0U;
  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(config_.bindings) /
                                sizeof(config_.bindings[0]));
       ++i) {
    if (config_.bindings[i].used) {
      ++count;
    }
  }
  return count;
}

const ZigbeeBindingEntry* ZigbeeHomeAutomationDevice::bindings() const {
  return config_.bindings;
}

bool ZigbeeHomeAutomationDevice::resolveBindingDestination(
    uint8_t sourceEndpoint, uint16_t clusterId, uint64_t* outDestinationIeee,
    uint8_t* outDestinationEndpoint) const {
  if (outDestinationIeee != nullptr) {
    *outDestinationIeee = 0U;
  }
  if (outDestinationEndpoint != nullptr) {
    *outDestinationEndpoint = 0U;
  }
  if (outDestinationIeee == nullptr || outDestinationEndpoint == nullptr) {
    return false;
  }

  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(config_.bindings) /
                                sizeof(config_.bindings[0]));
       ++i) {
    const ZigbeeBindingEntry& entry = config_.bindings[i];
    if (!entry.used || entry.sourceEndpoint != sourceEndpoint ||
        entry.clusterId != clusterId ||
        entry.destinationAddressMode != ZigbeeBindingAddressMode::kExtended ||
        entry.destinationEndpoint == 0U) {
      continue;
    }
    *outDestinationIeee = entry.destinationIeee;
    *outDestinationEndpoint = entry.destinationEndpoint;
    return true;
  }
  return false;
}

bool ZigbeeHomeAutomationDevice::isInGroup(uint16_t groupId) const {
  return findGroupEntry(&config_.groups, groupId) != nullptr;
}

bool ZigbeeHomeAutomationDevice::leaveRequested() const {
  return leaveRequested_;
}

bool ZigbeeHomeAutomationDevice::consumeLeaveRequest() {
  const bool requested = leaveRequested_;
  leaveRequested_ = false;
  return requested;
}

bool ZigbeeHomeAutomationDevice::setBinding(
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeee, uint8_t destinationEndpoint) {
  if (findEndpoint(sourceEndpoint) == nullptr) {
    return false;
  }
  if (destinationMode == ZigbeeBindingAddressMode::kExtended &&
      destinationEndpoint == 0U) {
    return false;
  }

  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(config_.bindings) /
                                sizeof(config_.bindings[0]));
       ++i) {
    ZigbeeBindingEntry& entry = config_.bindings[i];
    if (!entry.used) {
      continue;
    }
    if (entry.sourceEndpoint == sourceEndpoint && entry.clusterId == clusterId &&
        entry.destinationAddressMode == destinationMode &&
        entry.destinationGroup == destinationGroup &&
        entry.destinationIeee == destinationIeee &&
        entry.destinationEndpoint == destinationEndpoint) {
      return true;
    }
  }

  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(config_.bindings) /
                                sizeof(config_.bindings[0]));
       ++i) {
    ZigbeeBindingEntry& entry = config_.bindings[i];
    if (entry.used) {
      continue;
    }
    memset(&entry, 0, sizeof(entry));
    entry.used = true;
    entry.sourceEndpoint = sourceEndpoint;
    entry.clusterId = clusterId;
    entry.destinationAddressMode = destinationMode;
    entry.destinationGroup = destinationGroup;
    entry.destinationIeee = destinationIeee;
    entry.destinationEndpoint = destinationEndpoint;
    return true;
  }

  return false;
}

bool ZigbeeHomeAutomationDevice::clearBinding(
    uint8_t sourceEndpoint, uint16_t clusterId,
    ZigbeeBindingAddressMode destinationMode, uint16_t destinationGroup,
    uint64_t destinationIeee, uint8_t destinationEndpoint) {
  for (uint8_t i = 0U;
       i < static_cast<uint8_t>(sizeof(config_.bindings) /
                                sizeof(config_.bindings[0]));
       ++i) {
    ZigbeeBindingEntry& entry = config_.bindings[i];
    if (!entry.used || entry.sourceEndpoint != sourceEndpoint ||
        entry.clusterId != clusterId ||
        entry.destinationAddressMode != destinationMode ||
        entry.destinationGroup != destinationGroup ||
        entry.destinationIeee != destinationIeee ||
        entry.destinationEndpoint != destinationEndpoint) {
      continue;
    }
    memset(&entry, 0, sizeof(entry));
    return true;
  }
  return false;
}

bool ZigbeeHomeAutomationDevice::handleZdoRequest(
    uint16_t clusterId, const uint8_t* request, uint8_t requestLength,
    uint16_t* outResponseClusterId, uint8_t* outPayload,
    uint8_t* outLength) {
  if (outResponseClusterId != nullptr) {
    *outResponseClusterId = 0U;
  }
  if (outLength != nullptr) {
    *outLength = 0U;
  }
  if (request == nullptr || outResponseClusterId == nullptr ||
      outPayload == nullptr || outLength == nullptr || requestLength < 1U) {
    return false;
  }

  const uint8_t transactionSequence = request[0];
  switch (clusterId) {
    case kZigbeeZdoNetworkAddressRequest: {
      if (requestLength < 11U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoNetworkAddressResponse;
      const uint64_t ieeeAddress = readLe64(&request[1]);
      const bool extendedResponse = request[9] != 0U;
      if (ieeeAddress != config_.ieeeAddress) {
        return buildZdoAddressResponse(transactionSequence,
                                       kZdoStatusDeviceNotFound, 0U, 0U, false,
                                       outPayload, outLength);
      }
      return buildZdoAddressResponse(transactionSequence, kZdoStatusSuccess,
                                     config_.ieeeAddress, config_.nwkAddress,
                                     extendedResponse, outPayload, outLength);
    }
    case kZigbeeZdoIeeeAddressRequest: {
      if (requestLength < 5U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoIeeeAddressResponse;
      const uint16_t nwkAddress = readLe16(&request[1]);
      const bool extendedResponse = request[3] != 0U;
      if (nwkAddress != config_.nwkAddress) {
        return buildZdoAddressResponse(transactionSequence,
                                       kZdoStatusDeviceNotFound, 0U, 0U, false,
                                       outPayload, outLength);
      }
      return buildZdoAddressResponse(transactionSequence, kZdoStatusSuccess,
                                     config_.ieeeAddress, config_.nwkAddress,
                                     extendedResponse, outPayload, outLength);
    }
    case kZigbeeZdoNodeDescriptorRequest:
      if (requestLength < 3U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoNodeDescriptorResponse;
      return buildNodeDescriptorResponse(transactionSequence,
                                         readLe16(&request[1]), outPayload,
                                         outLength);
    case kZigbeeZdoPowerDescriptorRequest:
      if (requestLength < 3U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoPowerDescriptorResponse;
      return buildPowerDescriptorResponse(transactionSequence,
                                          readLe16(&request[1]), outPayload,
                                          outLength);
    case kZigbeeZdoSimpleDescriptorRequest:
      if (requestLength < 4U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoSimpleDescriptorResponse;
      return buildSimpleDescriptorResponse(transactionSequence,
                                           readLe16(&request[1]), request[3],
                                           outPayload, outLength);
    case kZigbeeZdoActiveEndpointsRequest:
      if (requestLength < 3U) {
        return false;
      }
      *outResponseClusterId = kZigbeeZdoActiveEndpointsResponse;
      return buildActiveEndpointsResponse(transactionSequence,
                                          readLe16(&request[1]), outPayload,
                                          outLength);
    case kZigbeeZdoMatchDescriptorRequest: {
      if (requestLength < 6U) {
        return false;
      }
      const uint16_t requestNwkAddress = readLe16(&request[1]);
      const uint16_t profileId = readLe16(&request[3]);
      uint8_t offset = 5U;
      const uint8_t inputCount = request[offset++];
      if (requestLength < static_cast<uint8_t>(offset + inputCount * 2U + 1U)) {
        return false;
      }
      uint16_t inputClusters[8] = {0};
      if (inputCount > 8U) {
        return false;
      }
      for (uint8_t i = 0U; i < inputCount; ++i) {
        inputClusters[i] = readLe16(&request[offset]);
        offset = static_cast<uint8_t>(offset + 2U);
      }
      const uint8_t outputCount = request[offset++];
      if (outputCount > 8U ||
          requestLength < static_cast<uint8_t>(offset + outputCount * 2U)) {
        return false;
      }
      uint16_t outputClusters[8] = {0};
      for (uint8_t i = 0U; i < outputCount; ++i) {
        outputClusters[i] = readLe16(&request[offset]);
        offset = static_cast<uint8_t>(offset + 2U);
      }
      *outResponseClusterId = kZigbeeZdoMatchDescriptorResponse;
      return buildMatchDescriptorResponse(
          transactionSequence, requestNwkAddress, profileId, inputClusters,
          inputCount, outputClusters, outputCount, outPayload, outLength);
    }
    case kZigbeeZdoBindRequest:
    case kZigbeeZdoUnbindRequest: {
      if (requestLength < 13U) {
        return false;
      }
      const uint64_t sourceIeee = readLe64(&request[1]);
      const uint8_t sourceEndpoint = request[9];
      const uint16_t bindClusterId = readLe16(&request[10]);
      const ZigbeeBindingAddressMode destinationMode =
          static_cast<ZigbeeBindingAddressMode>(request[12]);
      uint8_t offset = 13U;
      uint16_t destinationGroup = 0U;
      uint64_t destinationIeee = 0U;
      uint8_t destinationEndpoint = 0U;

      if (sourceIeee != config_.ieeeAddress) {
        *outResponseClusterId =
            (clusterId == kZigbeeZdoBindRequest) ? kZigbeeZdoBindResponse
                                                 : kZigbeeZdoUnbindResponse;
        return buildSimpleZdoStatusResponse(transactionSequence,
                                            kZdoStatusDeviceNotFound,
                                            outPayload, outLength);
      }
      if (findEndpoint(sourceEndpoint) == nullptr) {
        *outResponseClusterId =
            (clusterId == kZigbeeZdoBindRequest) ? kZigbeeZdoBindResponse
                                                 : kZigbeeZdoUnbindResponse;
        return buildSimpleZdoStatusResponse(transactionSequence,
                                            kZdoStatusInvalidEndpoint,
                                            outPayload, outLength);
      }

      if (destinationMode == ZigbeeBindingAddressMode::kGroup) {
        if (requestLength < static_cast<uint8_t>(offset + 2U)) {
          return false;
        }
        destinationGroup = readLe16(&request[offset]);
      } else if (destinationMode == ZigbeeBindingAddressMode::kExtended) {
        if (requestLength < static_cast<uint8_t>(offset + 9U)) {
          return false;
        }
        destinationIeee = readLe64(&request[offset]);
        offset = static_cast<uint8_t>(offset + 8U);
        destinationEndpoint = request[offset];
      } else {
        *outResponseClusterId =
            (clusterId == kZigbeeZdoBindRequest) ? kZigbeeZdoBindResponse
                                                 : kZigbeeZdoUnbindResponse;
        return buildSimpleZdoStatusResponse(transactionSequence,
                                            kZdoStatusNotSupported,
                                            outPayload, outLength);
      }

      *outResponseClusterId =
          (clusterId == kZigbeeZdoBindRequest) ? kZigbeeZdoBindResponse
                                               : kZigbeeZdoUnbindResponse;
      if (clusterId == kZigbeeZdoBindRequest) {
        const bool added = setBinding(sourceEndpoint, bindClusterId,
                                      destinationMode, destinationGroup,
                                      destinationIeee, destinationEndpoint);
        return buildSimpleZdoStatusResponse(
            transactionSequence,
            added ? kZdoStatusSuccess : kZdoStatusTableFull, outPayload,
            outLength);
      }

      const bool removed = clearBinding(sourceEndpoint, bindClusterId,
                                        destinationMode, destinationGroup,
                                        destinationIeee, destinationEndpoint);
      return buildSimpleZdoStatusResponse(transactionSequence,
                                          removed ? kZdoStatusSuccess
                                                  : kZdoStatusNoEntry,
                                          outPayload, outLength);
    }
    case kZigbeeZdoMgmtLeaveRequest: {
      uint8_t ignoredTransactionSequence = 0U;
      uint64_t deviceIeeeAddress = 0U;
      uint8_t flags = 0U;
      *outResponseClusterId = kZigbeeZdoMgmtLeaveResponse;
      if (!ZigbeeCodec::parseZdoMgmtLeaveRequest(
              request, requestLength, &ignoredTransactionSequence,
              &deviceIeeeAddress, &flags)) {
        return false;
      }
      (void)ignoredTransactionSequence;
      if (deviceIeeeAddress != config_.ieeeAddress) {
        return buildSimpleZdoStatusResponse(transactionSequence,
                                            kZdoStatusDeviceNotFound,
                                            outPayload, outLength);
      }
      leaveRequested_ = true;
      (void)flags;
      return buildSimpleZdoStatusResponse(transactionSequence, kZdoStatusSuccess,
                                          outPayload, outLength);
    }
    case kZigbeeZdoMgmtPermitJoinRequest:
      *outResponseClusterId = kZigbeeZdoMgmtPermitJoinResponse;
      return buildSimpleZdoStatusResponse(transactionSequence,
                                          kZdoStatusNotSupported, outPayload,
                                          outLength);
    default:
      break;
  }

  return false;
}

bool ZigbeeHomeAutomationDevice::handleZclRequest(
    uint16_t clusterId, const uint8_t* request, uint8_t requestLength,
    uint8_t* outFrame, uint8_t* outLength) {
  if (outLength != nullptr) {
    *outLength = 0U;
  }
  if (request == nullptr || outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  ZigbeeZclFrame frame{};
  if (!ZigbeeCodec::parseZclFrame(request, requestLength, &frame) || !frame.valid) {
    return false;
  }

  if (frame.frameType == ZigbeeZclFrameType::kGlobal) {
    if (frame.commandId == kZclCommandReadAttributes) {
      uint16_t attributeIds[16] = {0};
      uint8_t attributeCount = 0U;
      if (!ZigbeeCodec::parseReadAttributesRequest(frame.payload,
                                                   frame.payloadLength,
                                                   attributeIds,
                                                   static_cast<uint8_t>(
                                                       sizeof(attributeIds) /
                                                       sizeof(attributeIds[0])),
                                                   &attributeCount)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      ZigbeeReadAttributeRecord records[16];
      for (uint8_t i = 0U; i < attributeCount; ++i) {
        if (!appendReadRecordForCluster(clusterId, attributeIds[i], &records[i])) {
          return false;
        }
      }
      return ZigbeeCodec::buildReadAttributesResponse(
          records, attributeCount, frame.transactionSequence, outFrame,
          outLength);
    }

    if (frame.commandId == kZclCommandConfigureReporting) {
      ZigbeeReportingConfiguration requested[8];
      uint8_t requestedCount = 0U;
      if (!ZigbeeCodec::parseConfigureReportingRequest(
              frame.payload, frame.payloadLength, requested,
              static_cast<uint8_t>(sizeof(requested) / sizeof(requested[0])),
              &requestedCount)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      ZigbeeConfigureReportingStatusRecord statuses[8];
      for (uint8_t i = 0U; i < requestedCount; ++i) {
        statuses[i].status = kZclStatusSuccess;
        statuses[i].direction = 0U;
        statuses[i].attributeId = requested[i].attributeId;

        ZigbeeAttributeValue value{};
        if (!makeAttributeValueForCluster(clusterId, requested[i].attributeId,
                                          &value)) {
          statuses[i].status = kZclStatusUnsupportedAttribute;
          continue;
        }
        if (value.type != requested[i].dataType) {
          statuses[i].status = kZclStatusInvalidField;
          continue;
        }
        if (!configureReporting(clusterId, requested[i].attributeId,
                                requested[i].dataType,
                                requested[i].minimumIntervalSeconds,
                                requested[i].maximumIntervalSeconds,
                                requested[i].reportableChange)) {
          statuses[i].status = kZclStatusUnsupportedReporting;
        }
      }
      return ZigbeeCodec::buildConfigureReportingResponse(
          statuses, requestedCount, frame.transactionSequence, outFrame,
          outLength);
    }

    if (frame.commandId == kZclCommandDefaultResponse) {
      *outLength = 0U;
      return true;
    }

    return ZigbeeCodec::buildDefaultResponse(
        frame.transactionSequence, true, frame.commandId,
        kZclStatusUnsupportedGeneralCommand, outFrame, outLength);
  }

  if (clusterId == kZigbeeClusterOnOff && config_.onOff.enabled) {
    bool handled = true;
    switch (frame.commandId) {
      case kOnOffCommandOff:
        (void)setOnOff(false);
        break;
      case kOnOffCommandOn:
        (void)setOnOff(true);
        break;
      case kOnOffCommandToggle:
        (void)setOnOff(!config_.onOff.on);
        break;
      default:
        handled = false;
        break;
    }
    if (!handled) {
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(
          frame.transactionSequence, true, frame.commandId,
          kZclStatusUnsupportedClusterCommand, outFrame, outLength);
    }
    if (frame.disableDefaultResponse) {
      *outLength = 0U;
      return true;
    }
    return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                             frame.commandId,
                                             kZclStatusSuccess, outFrame,
                                             outLength);
  }

  if (clusterId == kZigbeeClusterLevelControl && config_.level.enabled) {
    bool handled = true;
    bool invalidField = false;
    switch (frame.commandId) {
      case kLevelControlCommandMoveToLevel:
      case kLevelControlCommandMoveToLevelWithOnOff:
        if (frame.payloadLength < 1U) {
          invalidField = true;
          break;
        }
        if (!setLevel(frame.payload[0])) {
          handled = false;
        }
        if (frame.commandId == kLevelControlCommandMoveToLevelWithOnOff &&
            config_.onOff.enabled) {
          config_.onOff.on = (config_.level.currentLevel > 0U);
        }
        break;
      case kLevelControlCommandMove:
      case kLevelControlCommandMoveWithOnOff:
        if (frame.payloadLength < 1U) {
          invalidField = true;
          break;
        }
        if (frame.payload[0] == 0x00U) {
          (void)setLevel(config_.level.maxLevel);
        } else if (frame.payload[0] == 0x01U) {
          (void)setLevel(0U);
        } else if (frame.payload[0] != 0x02U) {
          invalidField = true;
        }
        if (frame.commandId == kLevelControlCommandMoveWithOnOff &&
            config_.onOff.enabled) {
          config_.onOff.on = (config_.level.currentLevel > 0U);
        }
        break;
      case kLevelControlCommandStep:
      case kLevelControlCommandStepWithOnOff:
        if (frame.payloadLength < 2U) {
          invalidField = true;
          break;
        }
        if (frame.payload[0] == 0x00U) {
          const uint16_t next =
              static_cast<uint16_t>(config_.level.currentLevel) + frame.payload[1];
          (void)setLevel(static_cast<uint8_t>(next > 0xFEU ? 0xFEU : next));
        } else if (frame.payload[0] == 0x01U) {
          const uint8_t next =
              (config_.level.currentLevel > frame.payload[1])
                  ? static_cast<uint8_t>(config_.level.currentLevel -
                                         frame.payload[1])
                  : 0U;
          (void)setLevel(next);
        } else {
          invalidField = true;
        }
        if (frame.commandId == kLevelControlCommandStepWithOnOff &&
            config_.onOff.enabled) {
          config_.onOff.on = (config_.level.currentLevel > 0U);
        }
        break;
      case kLevelControlCommandStop:
      case kLevelControlCommandStopWithOnOff:
        break;
      default:
        handled = false;
        break;
    }
    if (invalidField) {
      return ZigbeeCodec::buildDefaultResponse(
          frame.transactionSequence, true, frame.commandId,
          kZclStatusInvalidField, outFrame, outLength);
    }
    if (!handled) {
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(
          frame.transactionSequence, true, frame.commandId,
          kZclStatusUnsupportedClusterCommand, outFrame, outLength);
    }
    if (frame.disableDefaultResponse) {
      *outLength = 0U;
      return true;
    }
    return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                             frame.commandId,
                                             kZclStatusSuccess, outFrame,
                                             outLength);
  }

  if (clusterId == kZigbeeClusterIdentify && config_.identify.enabled) {
    if (frame.commandId == kIdentifyCommandIdentify) {
      if (frame.payloadLength < 2U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      config_.identify.identifyTimeSeconds = readLe16(frame.payload);
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                               frame.commandId,
                                               kZclStatusSuccess, outFrame,
                                               outLength);
    }

    if (frame.commandId == kIdentifyCommandIdentifyQuery) {
      uint8_t payload[2] = {0U, 0U};
      uint8_t payloadLength = 0U;
      if (config_.identify.identifyTimeSeconds > 0U) {
        writeLe16(payload, config_.identify.identifyTimeSeconds);
        payloadLength = 2U;
      }
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kIdentifyCommandIdentify,
                                               payload, payloadLength, outFrame,
                                               outLength);
    }

    if (frame.commandId == kIdentifyCommandTriggerEffect) {
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                               frame.commandId,
                                               kZclStatusSuccess, outFrame,
                                               outLength);
    }
  }

  if (clusterId == kZigbeeClusterGroups && config_.groups.enabled) {
    if (frame.commandId == kGroupsCommandAddGroup ||
        frame.commandId == kGroupsCommandAddGroupIfIdentifying) {
      if (frame.payloadLength < 3U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t nameLength = frame.payload[2];
      if (frame.payloadLength < static_cast<uint8_t>(3U + nameLength)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      uint8_t status = kZclStatusSuccess;
      if (frame.commandId == kGroupsCommandAddGroup ||
          config_.identify.identifyTimeSeconds > 0U) {
        if (!upsertGroup(&config_.groups, groupId,
                         reinterpret_cast<const char*>(&frame.payload[3]),
                         nameLength)) {
          status = kZclStatusInsufficientSpace;
        }
      }

      if (frame.commandId == kGroupsCommandAddGroupIfIdentifying) {
        if (frame.disableDefaultResponse) {
          *outLength = 0U;
          return true;
        }
        return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                                 frame.commandId, status,
                                                 outFrame, outLength);
      }

      uint8_t payload[3] = {status, 0U, 0U};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kGroupsCommandAddGroup, payload,
                                               sizeof(payload), outFrame,
                                               outLength);
    }

    if (frame.commandId == kGroupsCommandViewGroup) {
      if (frame.payloadLength < 2U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const ZigbeeGroupEntry* entry = findGroupEntry(&config_.groups, groupId);
      uint8_t payload[32] = {0U};
      uint8_t offset = 0U;
      const uint8_t status =
          (entry != nullptr) ? kZclStatusSuccess : kZclStatusNotFound;
      if (!appendBytes(payload, sizeof(payload), &offset, &status, 1U) ||
          !appendLe16(payload, sizeof(payload), &offset, groupId)) {
        return false;
      }
      if (entry != nullptr &&
          !appendCharStringPayload(payload, sizeof(payload), &offset, entry->name,
                                   entry->nameLength)) {
        return false;
      }
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kGroupsCommandViewGroup, payload,
                                               offset, outFrame, outLength);
    }

    if (frame.commandId == kGroupsCommandGetGroupMembership) {
      if (frame.payloadLength < 1U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint8_t requestedCount = frame.payload[0];
      if (frame.payloadLength <
          static_cast<uint8_t>(1U + requestedCount * 2U)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      uint16_t responseGroups[8] = {0U};
      uint8_t responseCount = 0U;
      if (requestedCount == 0U) {
        for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(config_.groups.entries) /
                                                      sizeof(config_.groups.entries[0]));
             ++i) {
          if (!config_.groups.entries[i].used) {
            continue;
          }
          responseGroups[responseCount++] = config_.groups.entries[i].groupId;
        }
      } else {
        for (uint8_t i = 0U; i < requestedCount && responseCount < 8U; ++i) {
          const uint16_t groupId = readLe16(&frame.payload[1U + i * 2U]);
          if (findGroupEntry(&config_.groups, groupId) != nullptr) {
            responseGroups[responseCount++] = groupId;
          }
        }
      }

      uint8_t payload[32] = {0U};
      uint8_t offset = 0U;
      const uint8_t capacity = static_cast<uint8_t>(
          (sizeof(config_.groups.entries) / sizeof(config_.groups.entries[0])) -
          countUsedGroups(config_.groups));
      if (!appendBytes(payload, sizeof(payload), &offset, &capacity, 1U) ||
          !appendBytes(payload, sizeof(payload), &offset, &responseCount, 1U)) {
        return false;
      }
      for (uint8_t i = 0U; i < responseCount; ++i) {
        if (!appendLe16(payload, sizeof(payload), &offset, responseGroups[i])) {
          return false;
        }
      }
      return buildClusterSpecificResponseFrame(
          frame.transactionSequence, kGroupsCommandGetGroupMembership, payload,
          offset, outFrame, outLength);
    }

    if (frame.commandId == kGroupsCommandRemoveGroup) {
      if (frame.payloadLength < 2U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const bool removed = removeGroupEntry(&config_.groups, groupId);
      removeAllScenesForGroup(&config_.scenes, groupId);
      uint8_t payload[3] = {removed ? kZclStatusSuccess : kZclStatusNotFound,
                            0U, 0U};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kGroupsCommandRemoveGroup,
                                               payload, sizeof(payload),
                                               outFrame, outLength);
    }

    if (frame.commandId == kGroupsCommandRemoveAllGroups) {
      memset(&config_.groups.entries, 0, sizeof(config_.groups.entries));
      memset(&config_.scenes.entries, 0, sizeof(config_.scenes.entries));
      config_.scenes.sceneValid = false;
      config_.scenes.currentGroupId = 0U;
      config_.scenes.currentSceneId = 0U;
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                               frame.commandId,
                                               kZclStatusSuccess, outFrame,
                                               outLength);
    }
  }

  if (clusterId == kZigbeeClusterScenes && config_.scenes.enabled) {
    if (frame.commandId == kScenesCommandAddScene) {
      if (frame.payloadLength < 6U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t sceneId = frame.payload[2];
      const uint16_t transitionTimeDeciseconds = readLe16(&frame.payload[3]);
      const uint8_t nameLength = frame.payload[5];
      const uint8_t nameOffset = 6U;
      if (frame.payloadLength < static_cast<uint8_t>(nameOffset + nameLength)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      ParsedSceneExtensionData extension{};
      const uint8_t extensionOffset =
          static_cast<uint8_t>(nameOffset + nameLength);
      if (!parseSceneExtensionSets(frame.payload, frame.payloadLength,
                                   extensionOffset, &extension)) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }

      const bool stored = storeSceneState(
          &config_, groupId, sceneId, transitionTimeDeciseconds,
          reinterpret_cast<const char*>(&frame.payload[nameOffset]), nameLength,
          &extension, !extension.hasOnOff && !extension.hasLevel);
      uint8_t payload[4] = {stored ? kZclStatusSuccess : kZclStatusInsufficientSpace,
                            0U, 0U, sceneId};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kScenesCommandAddScene, payload,
                                               sizeof(payload), outFrame,
                                               outLength);
    }

    if (frame.commandId == kScenesCommandViewScene) {
      if (frame.payloadLength < 3U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t sceneId = frame.payload[2];
      const ZigbeeSceneEntry* scene = findSceneEntry(&config_.scenes, groupId,
                                                     sceneId);
      uint8_t payload[64] = {0U};
      uint8_t offset = 0U;
      const uint8_t status =
          (scene != nullptr) ? kZclStatusSuccess : kZclStatusNotFound;
      if (!appendBytes(payload, sizeof(payload), &offset, &status, 1U) ||
          !appendLe16(payload, sizeof(payload), &offset, groupId) ||
          !appendBytes(payload, sizeof(payload), &offset, &sceneId, 1U)) {
        return false;
      }
      if (scene != nullptr) {
        if (!appendLe16(payload, sizeof(payload), &offset,
                        scene->transitionTimeDeciseconds) ||
            !appendCharStringPayload(payload, sizeof(payload), &offset,
                                     scene->name, scene->nameLength)) {
          return false;
        }
        if (scene->hasOnOff) {
          const uint8_t extensionLength = 1U;
          const uint8_t onOff = scene->onOff ? 1U : 0U;
          if (!appendLe16(payload, sizeof(payload), &offset, kZigbeeClusterOnOff) ||
              !appendBytes(payload, sizeof(payload), &offset, &extensionLength,
                           1U) ||
              !appendBytes(payload, sizeof(payload), &offset, &onOff, 1U)) {
            return false;
          }
        }
        if (scene->hasLevel) {
          const uint8_t extensionLength = 1U;
          if (!appendLe16(payload, sizeof(payload), &offset,
                          kZigbeeClusterLevelControl) ||
              !appendBytes(payload, sizeof(payload), &offset, &extensionLength,
                           1U) ||
              !appendBytes(payload, sizeof(payload), &offset, &scene->level, 1U)) {
            return false;
          }
        }
      }
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kScenesCommandViewScene, payload,
                                               offset, outFrame, outLength);
    }

    if (frame.commandId == kScenesCommandRemoveScene) {
      if (frame.payloadLength < 3U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t sceneId = frame.payload[2];
      const bool removed = removeSceneEntry(&config_.scenes, groupId, sceneId);
      uint8_t payload[4] = {removed ? kZclStatusSuccess : kZclStatusNotFound,
                            0U, 0U, sceneId};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kScenesCommandRemoveScene,
                                               payload, sizeof(payload),
                                               outFrame, outLength);
    }

    if (frame.commandId == kScenesCommandRemoveAllScenes) {
      if (frame.payloadLength < 2U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      removeAllScenesForGroup(&config_.scenes, groupId);
      uint8_t payload[3] = {kZclStatusSuccess, 0U, 0U};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kScenesCommandRemoveAllScenes,
                                               payload, sizeof(payload),
                                               outFrame, outLength);
    }

    if (frame.commandId == kScenesCommandStoreScene) {
      if (frame.payloadLength < 3U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t sceneId = frame.payload[2];
      const bool stored =
          storeSceneState(&config_, groupId, sceneId, 0U, nullptr, 0U, nullptr,
                          true);
      uint8_t payload[4] = {stored ? kZclStatusSuccess : kZclStatusInsufficientSpace,
                            0U, 0U, sceneId};
      writeLe16(&payload[1], groupId);
      return buildClusterSpecificResponseFrame(frame.transactionSequence,
                                               kScenesCommandStoreScene,
                                               payload, sizeof(payload),
                                               outFrame, outLength);
    }

    if (frame.commandId == kScenesCommandRecallScene) {
      if (frame.payloadLength < 3U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      const uint8_t sceneId = frame.payload[2];
      const ZigbeeSceneEntry* scene = findSceneEntry(&config_.scenes, groupId,
                                                     sceneId);
      if (scene == nullptr) {
        return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                                 frame.commandId,
                                                 kZclStatusNotFound, outFrame,
                                                 outLength);
      }
      if (scene->hasLevel) {
        (void)setLevel(scene->level);
      }
      if (scene->hasOnOff) {
        (void)setOnOff(scene->onOff);
      }
      config_.scenes.currentGroupId = groupId;
      config_.scenes.currentSceneId = sceneId;
      config_.scenes.sceneValid = true;
      if (frame.disableDefaultResponse) {
        *outLength = 0U;
        return true;
      }
      return ZigbeeCodec::buildDefaultResponse(frame.transactionSequence, true,
                                               frame.commandId,
                                               kZclStatusSuccess, outFrame,
                                               outLength);
    }

    if (frame.commandId == kScenesCommandGetSceneMembership) {
      if (frame.payloadLength < 2U) {
        return ZigbeeCodec::buildDefaultResponse(
            frame.transactionSequence, true, frame.commandId,
            kZclStatusInvalidField, outFrame, outLength);
      }
      const uint16_t groupId = readLe16(frame.payload);
      uint8_t payload[32] = {0U};
      uint8_t offset = 0U;
      const uint8_t sceneCount = countScenesForGroup(config_.scenes, groupId);
      const uint8_t capacity = static_cast<uint8_t>(
          (sizeof(config_.scenes.entries) / sizeof(config_.scenes.entries[0])) -
          sceneCount);
      const uint8_t status =
          (sceneCount > 0U || findGroupEntry(&config_.groups, groupId) != nullptr)
              ? kZclStatusSuccess
              : kZclStatusNotFound;
      if (!appendBytes(payload, sizeof(payload), &offset, &status, 1U) ||
          !appendBytes(payload, sizeof(payload), &offset, &capacity, 1U) ||
          !appendLe16(payload, sizeof(payload), &offset, groupId) ||
          !appendBytes(payload, sizeof(payload), &offset, &sceneCount, 1U)) {
        return false;
      }
      for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(config_.scenes.entries) /
                                                    sizeof(config_.scenes.entries[0]));
           ++i) {
        if (!config_.scenes.entries[i].used ||
            config_.scenes.entries[i].groupId != groupId) {
          continue;
        }
        if (!appendBytes(payload, sizeof(payload), &offset,
                         &config_.scenes.entries[i].sceneId, 1U)) {
          return false;
        }
      }
      return buildClusterSpecificResponseFrame(
          frame.transactionSequence, kScenesCommandGetSceneMembership, payload,
          offset, outFrame, outLength);
    }
  }

  if (frame.disableDefaultResponse) {
    *outLength = 0U;
    return true;
  }
  return ZigbeeCodec::buildDefaultResponse(
      frame.transactionSequence, true, frame.commandId,
      kZclStatusUnsupportedClusterCommand, outFrame, outLength);
}

bool ZigbeeHomeAutomationDevice::buildDeviceAnnounce(
    uint8_t transactionSequence, uint8_t* outPayload,
    uint8_t* outLength) const {
  if (outPayload == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  const uint8_t capability = buildMacCapabilityFlags(config_);
  if (!appendBytes(outPayload, 127U, &offset, &transactionSequence, 1U) ||
      !appendLe16(outPayload, 127U, &offset, config_.nwkAddress) ||
      !appendLe64(outPayload, 127U, &offset, config_.ieeeAddress) ||
      !appendBytes(outPayload, 127U, &offset, &capability, 1U)) {
    return false;
  }

  *outLength = offset;
  return true;
}

}  // namespace xiao_nrf54l15
