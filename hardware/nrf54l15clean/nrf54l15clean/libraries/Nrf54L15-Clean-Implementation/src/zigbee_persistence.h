#pragma once

#include <Preferences.h>

#include "zigbee_stack.h"

namespace xiao_nrf54l15 {

struct ZigbeePersistentState {
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

class ZigbeePersistentStateStore {
 public:
  ZigbeePersistentStateStore();

  bool begin(const char* name = "zigbee");
  void end();
  bool load(ZigbeePersistentState* outState);
  bool save(const ZigbeePersistentState& state);
  bool clear();

  static void initialize(ZigbeePersistentState* state);
  static bool isValid(const ZigbeePersistentState& state);

 private:
  Preferences prefs_;
  bool open_;
};

}  // namespace xiao_nrf54l15
