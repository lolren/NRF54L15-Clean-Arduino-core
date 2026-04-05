#include "zigbee_persistence.h"

#include <stdio.h>
#include <string.h>

namespace xiao_nrf54l15 {

namespace {

constexpr uint32_t kZigbeeStateMagic = 0x5A425330UL;
constexpr uint16_t kZigbeeStateVersion = 6U;
constexpr char kPrefsKeyState[] = "state";
constexpr char kPrefsKeyStateLen[] = "stlen";
constexpr char kPrefsKeyStateChunkPrefix[] = "st";
constexpr size_t kPrefsChunkValueLen = 48U;
constexpr size_t kPrefsChunkKeyCapacity = 5U;
constexpr size_t kPrefsMaxChunkCount = 27U;

struct ZigbeePersistentStateV1 {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint8_t channel = 0U;
  uint8_t logicalType = 0U;
  uint16_t panId = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t parentShort = 0U;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint64_t extendedPanId = 0U;
  uint8_t networkKey[16] = {0};
  uint32_t nwkFrameCounter = 0U;
  uint32_t apsFrameCounter = 0U;
  uint8_t keySequence = 0U;
  uint8_t flags = 0U;
  bool onOffState = false;
  uint8_t reportingCount = 0U;
  ZigbeeReportingConfiguration reporting[8] = {};
};

struct ZigbeePersistentStateV2 {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint8_t channel = 0U;
  uint8_t logicalType = 0U;
  uint16_t panId = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t parentShort = 0U;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint64_t extendedPanId = 0U;
  uint8_t networkKey[16] = {0};
  uint32_t nwkFrameCounter = 0U;
  uint32_t apsFrameCounter = 0U;
  uint8_t keySequence = 0U;
  uint8_t flags = 0U;
  bool onOffState = false;
  uint8_t levelState = 0U;
  uint8_t reportingCount = 0U;
  ZigbeeReportingConfiguration reporting[8] = {};
};

struct ZigbeePersistentStateV3 {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint8_t channel = 0U;
  uint8_t logicalType = 0U;
  uint16_t panId = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t parentShort = 0U;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint64_t extendedPanId = 0U;
  uint8_t networkKey[16] = {0};
  uint32_t nwkFrameCounter = 0U;
  uint32_t apsFrameCounter = 0U;
  uint8_t keySequence = 0U;
  uint8_t flags = 0U;
  bool onOffState = false;
  uint8_t levelState = 0U;
  uint8_t reportingCount = 0U;
  ZigbeeReportingConfiguration reporting[8] = {};
  uint8_t bindingCount = 0U;
  ZigbeeBindingEntry bindings[8] = {};
};

struct ZigbeePersistentStateV4 {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint8_t channel = 0U;
  uint8_t logicalType = 0U;
  uint16_t panId = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t parentShort = 0U;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint64_t extendedPanId = 0U;
  uint8_t networkKey[16] = {0};
  uint32_t nwkFrameCounter = 0U;
  uint32_t apsFrameCounter = 0U;
  uint8_t keySequence = 0U;
  uint8_t flags = 0U;
  bool onOffState = false;
  uint8_t levelState = 0U;
  uint8_t reportingCount = 0U;
  ZigbeeReportingConfiguration reporting[8] = {};
  uint8_t bindingCount = 0U;
  ZigbeeBindingEntry bindings[8] = {};
  uint32_t incomingNwkFrameCounter = 0U;
};

struct ZigbeePersistentStateV5 {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint8_t channel = 0U;
  uint8_t logicalType = 0U;
  uint16_t panId = 0U;
  uint16_t nwkAddress = 0U;
  uint16_t parentShort = 0U;
  uint16_t manufacturerCode = 0U;
  uint64_t ieeeAddress = 0U;
  uint64_t extendedPanId = 0U;
  uint8_t networkKey[16] = {0};
  uint32_t nwkFrameCounter = 0U;
  uint32_t apsFrameCounter = 0U;
  uint8_t keySequence = 0U;
  uint8_t flags = 0U;
  uint8_t preconfiguredKeyMode = 0U;
  bool onOffState = false;
  uint8_t levelState = 0U;
  uint64_t trustCenterIeee = 0U;
  uint8_t reportingCount = 0U;
  ZigbeeReportingConfiguration reporting[8] = {};
  uint8_t bindingCount = 0U;
  ZigbeeBindingEntry bindings[8] = {};
  uint32_t incomingNwkFrameCounter = 0U;
  uint32_t incomingApsFrameCounter = 0U;
};

bool isValidV1(const ZigbeePersistentStateV1& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == 1U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) / sizeof(state.reporting[0]));
}

bool isValidV2(const ZigbeePersistentStateV2& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == 2U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) / sizeof(state.reporting[0]));
}

bool isValidV3(const ZigbeePersistentStateV3& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == 3U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) / sizeof(state.reporting[0])) &&
         state.bindingCount <=
             static_cast<uint8_t>(sizeof(state.bindings) / sizeof(state.bindings[0]));
}

bool isValidV4(const ZigbeePersistentStateV4& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == 4U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) /
                                  sizeof(state.reporting[0])) &&
         state.bindingCount <=
             static_cast<uint8_t>(sizeof(state.bindings) /
                                  sizeof(state.bindings[0]));
}

bool isValidV5(const ZigbeePersistentStateV5& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == 5U &&
         state.preconfiguredKeyMode <= 2U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) /
                                  sizeof(state.reporting[0])) &&
         state.bindingCount <=
             static_cast<uint8_t>(sizeof(state.bindings) /
                                  sizeof(state.bindings[0]));
}

size_t chunkCountForLength(size_t len) {
  return (len + kPrefsChunkValueLen - 1U) / kPrefsChunkValueLen;
}

bool formatChunkKey(size_t chunkIndex, char outKey[kPrefsChunkKeyCapacity]) {
  if (outKey == nullptr || chunkIndex > 0xFFU) {
    return false;
  }
  const int written =
      snprintf(outKey, kPrefsChunkKeyCapacity, "%s%02X",
               kPrefsKeyStateChunkPrefix, static_cast<unsigned>(chunkIndex));
  return written > 0 && static_cast<size_t>(written) < kPrefsChunkKeyCapacity;
}

void clearChunkedKeys(Preferences* prefs) {
  if (prefs == nullptr) {
    return;
  }
  (void)prefs->remove(kPrefsKeyStateLen);
  for (size_t i = 0U; i < kPrefsMaxChunkCount; ++i) {
    char key[kPrefsChunkKeyCapacity] = {0};
    if (!formatChunkKey(i, key)) {
      break;
    }
    (void)prefs->remove(key);
  }
}

bool loadChunkedState(Preferences* prefs, ZigbeePersistentState* outState) {
  if (prefs == nullptr || outState == nullptr) {
    return false;
  }

  const uint32_t storedLen = prefs->getUInt(kPrefsKeyStateLen, 0U);
  if (storedLen != sizeof(*outState)) {
    return false;
  }

  const size_t chunkCount = chunkCountForLength(storedLen);
  if (chunkCount == 0U || chunkCount > kPrefsMaxChunkCount) {
    return false;
  }

  memset(outState, 0, sizeof(*outState));
  uint8_t* dst = reinterpret_cast<uint8_t*>(outState);
  size_t offset = 0U;
  for (size_t i = 0U; i < chunkCount; ++i) {
    char key[kPrefsChunkKeyCapacity] = {0};
    if (!formatChunkKey(i, key)) {
      return false;
    }
    const size_t expectedLen =
        ((storedLen - offset) < kPrefsChunkValueLen) ? (storedLen - offset)
                                                     : kPrefsChunkValueLen;
    if (prefs->getBytesLength(key) != expectedLen) {
      return false;
    }
    if (prefs->getBytes(key, dst + offset, expectedLen) != expectedLen) {
      return false;
    }
    offset += expectedLen;
  }

  return ZigbeePersistentStateStore::isValid(*outState);
}

bool saveChunkedState(Preferences* prefs, const ZigbeePersistentState& state) {
  if (prefs == nullptr) {
    return false;
  }

  const size_t totalLen = sizeof(state);
  const size_t chunkCount = chunkCountForLength(totalLen);
  if (chunkCount == 0U || chunkCount > kPrefsMaxChunkCount) {
    return false;
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(&state);
  (void)prefs->remove(kPrefsKeyState);
  (void)prefs->remove(kPrefsKeyStateLen);
  clearChunkedKeys(prefs);

  size_t offset = 0U;
  for (size_t i = 0U; i < chunkCount; ++i) {
    char key[kPrefsChunkKeyCapacity] = {0};
    if (!formatChunkKey(i, key)) {
      return false;
    }
    const size_t chunkLen =
        ((totalLen - offset) < kPrefsChunkValueLen) ? (totalLen - offset)
                                                    : kPrefsChunkValueLen;
    if (prefs->putBytes(key, src + offset, chunkLen) != chunkLen) {
      (void)prefs->remove(kPrefsKeyStateLen);
      return false;
    }
    offset += chunkLen;
  }

  return prefs->putUInt(kPrefsKeyStateLen, static_cast<uint32_t>(totalLen)) ==
         sizeof(uint32_t);
}

}  // namespace

ZigbeePersistentStateStore::ZigbeePersistentStateStore()
    : prefs_(), open_(false) {}

bool ZigbeePersistentStateStore::begin(const char* name) {
  if (open_) {
    return true;
  }
  open_ = prefs_.begin(name, false);
  return open_;
}

void ZigbeePersistentStateStore::end() {
  if (!open_) {
    return;
  }
  prefs_.end();
  open_ = false;
}

void ZigbeePersistentStateStore::initialize(ZigbeePersistentState* state) {
  if (state == nullptr) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->magic = kZigbeeStateMagic;
  state->version = kZigbeeStateVersion;
}

bool ZigbeePersistentStateStore::isValid(const ZigbeePersistentState& state) {
  return state.magic == kZigbeeStateMagic &&
         state.version == kZigbeeStateVersion &&
         state.preconfiguredKeyMode <= 2U &&
         state.reportingCount <=
             static_cast<uint8_t>(sizeof(state.reporting) /
                                  sizeof(state.reporting[0])) &&
         state.bindingCount <=
             static_cast<uint8_t>(sizeof(state.bindings) /
                                  sizeof(state.bindings[0]));
}

bool ZigbeePersistentStateStore::load(ZigbeePersistentState* outState) {
  if (!open_ || outState == nullptr) {
    return false;
  }
  initialize(outState);
  const size_t len = prefs_.getBytesLength(kPrefsKeyState);
  if (len == sizeof(*outState)) {
    return prefs_.getBytes(kPrefsKeyState, outState, sizeof(*outState)) ==
               sizeof(*outState) &&
           isValid(*outState);
  }
  if (loadChunkedState(&prefs_, outState)) {
    return true;
  }
  if (len == sizeof(ZigbeePersistentStateV5)) {
    ZigbeePersistentStateV5 legacy{};
    if (prefs_.getBytes(kPrefsKeyState, &legacy, sizeof(legacy)) !=
            sizeof(legacy) ||
        !isValidV5(legacy)) {
      return false;
    }

    outState->magic = legacy.magic;
    outState->version = kZigbeeStateVersion;
    outState->channel = legacy.channel;
    outState->logicalType = legacy.logicalType;
    outState->panId = legacy.panId;
    outState->nwkAddress = legacy.nwkAddress;
    outState->parentShort = legacy.parentShort;
    outState->manufacturerCode = legacy.manufacturerCode;
    outState->ieeeAddress = legacy.ieeeAddress;
    outState->extendedPanId = legacy.extendedPanId;
    memcpy(outState->networkKey, legacy.networkKey,
           sizeof(outState->networkKey));
    outState->nwkFrameCounter = legacy.nwkFrameCounter;
    outState->apsFrameCounter = legacy.apsFrameCounter;
    outState->keySequence = legacy.keySequence;
    memset(outState->alternateNetworkKey, 0,
           sizeof(outState->alternateNetworkKey));
    outState->alternateKeySequence = 0U;
    outState->flags = legacy.flags;
    outState->preconfiguredKeyMode = legacy.preconfiguredKeyMode;
    outState->onOffState = legacy.onOffState;
    outState->levelState = legacy.levelState;
    outState->trustCenterIeee = legacy.trustCenterIeee;
    outState->reportingCount = legacy.reportingCount;
    memcpy(outState->reporting, legacy.reporting, sizeof(outState->reporting));
    outState->bindingCount = legacy.bindingCount;
    memcpy(outState->bindings, legacy.bindings, sizeof(outState->bindings));
    outState->incomingNwkFrameCounter = legacy.incomingNwkFrameCounter;
    outState->incomingApsFrameCounter = legacy.incomingApsFrameCounter;
    return true;
  }
  if (len == sizeof(ZigbeePersistentStateV4)) {
    ZigbeePersistentStateV4 legacy{};
    if (prefs_.getBytes(kPrefsKeyState, &legacy, sizeof(legacy)) !=
            sizeof(legacy) ||
        !isValidV4(legacy)) {
      return false;
    }

    outState->magic = legacy.magic;
    outState->version = kZigbeeStateVersion;
    outState->channel = legacy.channel;
    outState->logicalType = legacy.logicalType;
    outState->panId = legacy.panId;
    outState->nwkAddress = legacy.nwkAddress;
    outState->parentShort = legacy.parentShort;
    outState->manufacturerCode = legacy.manufacturerCode;
    outState->ieeeAddress = legacy.ieeeAddress;
    outState->extendedPanId = legacy.extendedPanId;
    memcpy(outState->networkKey, legacy.networkKey,
           sizeof(outState->networkKey));
    outState->nwkFrameCounter = legacy.nwkFrameCounter;
    outState->apsFrameCounter = legacy.apsFrameCounter;
    outState->keySequence = legacy.keySequence;
    outState->flags = legacy.flags;
    outState->preconfiguredKeyMode = 0U;
    outState->onOffState = legacy.onOffState;
    outState->levelState = legacy.levelState;
    outState->trustCenterIeee = 0U;
    outState->reportingCount = legacy.reportingCount;
    memcpy(outState->reporting, legacy.reporting, sizeof(outState->reporting));
    outState->bindingCount = legacy.bindingCount;
    memcpy(outState->bindings, legacy.bindings, sizeof(outState->bindings));
    outState->incomingNwkFrameCounter = legacy.incomingNwkFrameCounter;
    outState->incomingApsFrameCounter = 0U;
    return true;
  }
  if (len == sizeof(ZigbeePersistentStateV3)) {
    ZigbeePersistentStateV3 legacy{};
    if (prefs_.getBytes(kPrefsKeyState, &legacy, sizeof(legacy)) !=
            sizeof(legacy) ||
        !isValidV3(legacy)) {
      return false;
    }

    outState->magic = legacy.magic;
    outState->version = kZigbeeStateVersion;
    outState->channel = legacy.channel;
    outState->logicalType = legacy.logicalType;
    outState->panId = legacy.panId;
    outState->nwkAddress = legacy.nwkAddress;
    outState->parentShort = legacy.parentShort;
    outState->manufacturerCode = legacy.manufacturerCode;
    outState->ieeeAddress = legacy.ieeeAddress;
    outState->extendedPanId = legacy.extendedPanId;
    memcpy(outState->networkKey, legacy.networkKey,
           sizeof(outState->networkKey));
    outState->nwkFrameCounter = legacy.nwkFrameCounter;
    outState->apsFrameCounter = legacy.apsFrameCounter;
    outState->keySequence = legacy.keySequence;
    outState->flags = legacy.flags;
    outState->preconfiguredKeyMode = 0U;
    outState->onOffState = legacy.onOffState;
    outState->levelState = legacy.levelState;
    outState->trustCenterIeee = 0U;
    outState->reportingCount = legacy.reportingCount;
    memcpy(outState->reporting, legacy.reporting, sizeof(outState->reporting));
    outState->bindingCount = legacy.bindingCount;
    memcpy(outState->bindings, legacy.bindings, sizeof(outState->bindings));
    outState->incomingNwkFrameCounter = 0U;
    outState->incomingApsFrameCounter = 0U;
    return true;
  }
  if (len == sizeof(ZigbeePersistentStateV2)) {
    ZigbeePersistentStateV2 legacy{};
    if (prefs_.getBytes(kPrefsKeyState, &legacy, sizeof(legacy)) !=
            sizeof(legacy) ||
        !isValidV2(legacy)) {
      return false;
    }

    outState->magic = legacy.magic;
    outState->version = kZigbeeStateVersion;
    outState->channel = legacy.channel;
    outState->logicalType = legacy.logicalType;
    outState->panId = legacy.panId;
    outState->nwkAddress = legacy.nwkAddress;
    outState->parentShort = legacy.parentShort;
    outState->manufacturerCode = legacy.manufacturerCode;
    outState->ieeeAddress = legacy.ieeeAddress;
    outState->extendedPanId = legacy.extendedPanId;
    memcpy(outState->networkKey, legacy.networkKey,
           sizeof(outState->networkKey));
    outState->nwkFrameCounter = legacy.nwkFrameCounter;
    outState->apsFrameCounter = legacy.apsFrameCounter;
    outState->keySequence = legacy.keySequence;
    outState->flags = legacy.flags;
    outState->preconfiguredKeyMode = 0U;
    outState->onOffState = legacy.onOffState;
    outState->levelState = legacy.levelState;
    outState->trustCenterIeee = 0U;
    outState->reportingCount = legacy.reportingCount;
    memcpy(outState->reporting, legacy.reporting, sizeof(outState->reporting));
    outState->incomingNwkFrameCounter = 0U;
    outState->incomingApsFrameCounter = 0U;
    return true;
  }

  if (len != sizeof(ZigbeePersistentStateV1)) {
    return false;
  }

  ZigbeePersistentStateV1 legacy{};
  if (prefs_.getBytes(kPrefsKeyState, &legacy, sizeof(legacy)) != sizeof(legacy) ||
      !isValidV1(legacy)) {
    return false;
  }

  outState->magic = legacy.magic;
  outState->version = kZigbeeStateVersion;
  outState->channel = legacy.channel;
  outState->logicalType = legacy.logicalType;
  outState->panId = legacy.panId;
  outState->nwkAddress = legacy.nwkAddress;
  outState->parentShort = legacy.parentShort;
  outState->manufacturerCode = legacy.manufacturerCode;
  outState->ieeeAddress = legacy.ieeeAddress;
  outState->extendedPanId = legacy.extendedPanId;
  memcpy(outState->networkKey, legacy.networkKey, sizeof(outState->networkKey));
  outState->nwkFrameCounter = legacy.nwkFrameCounter;
  outState->apsFrameCounter = legacy.apsFrameCounter;
  outState->keySequence = legacy.keySequence;
  outState->flags = legacy.flags;
  outState->preconfiguredKeyMode = 0U;
  outState->onOffState = legacy.onOffState;
  outState->levelState = legacy.onOffState ? 0xFEU : 0U;
  outState->trustCenterIeee = 0U;
  outState->reportingCount = legacy.reportingCount;
  memcpy(outState->reporting, legacy.reporting, sizeof(outState->reporting));
  outState->incomingNwkFrameCounter = 0U;
  outState->incomingApsFrameCounter = 0U;
  return true;
}

bool ZigbeePersistentStateStore::save(const ZigbeePersistentState& state) {
  if (!open_ || !isValid(state)) {
    return false;
  }
  return saveChunkedState(&prefs_, state);
}

bool ZigbeePersistentStateStore::clear() {
  if (!open_) {
    return false;
  }
  const bool removedState = prefs_.remove(kPrefsKeyState);
  const bool removedLength = prefs_.remove(kPrefsKeyStateLen);
  clearChunkedKeys(&prefs_);
  return removedState || removedLength;
}

}  // namespace xiao_nrf54l15
