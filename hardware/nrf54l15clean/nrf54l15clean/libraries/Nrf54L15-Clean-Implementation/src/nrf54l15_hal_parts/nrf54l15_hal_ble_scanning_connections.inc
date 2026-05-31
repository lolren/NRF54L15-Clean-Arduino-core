static uint32_t bleBudgetedSpinLimit(uint32_t budgetUs, uint32_t multiplier,
                                     uint32_t minSpin, uint32_t maxSpin) {
  uint64_t spin = static_cast<uint64_t>(budgetUs) * multiplier;
  if (spin < minSpin) {
    spin = minSpin;
  } else if (spin > maxSpin) {
    spin = maxSpin;
  }
  return static_cast<uint32_t>(spin);
}

static uint32_t bleBudgetedListenSpinLimit(uint32_t budgetUs) {
  return bleBudgetedSpinLimit(budgetUs, 4U, 12000UL, 1500000UL);
}

static uint32_t bleBudgetedDisableSpinLimit(uint32_t budgetUs) {
  return bleBudgetedSpinLimit(budgetUs, 4U, 12000UL, 120000UL);
}

static bool bleWaitDisabledBudgeted(NRF_RADIO_Type* radio, uint32_t budgetUs) {
  if (radio == nullptr) {
    return false;
  }

  if (radio->EVENTS_DISABLED != 0U) {
    radio->EVENTS_DISABLED = 0U;
    return true;
  }

  radio->EVENTS_DISABLED = 0U;
  const bool disabled =
      waitRadioDisabledBudgeted(radio, budgetUs,
                                bleBudgetedDisableSpinLimit(budgetUs));
  if (radio->EVENTS_DISABLED != 0U) {
    radio->EVENTS_DISABLED = 0U;
  }
  return disabled;
}

struct BleActiveScanReqCacheEntry {
  uint8_t address[6];
  uint32_t lastUs;
  uint8_t random;
  uint8_t valid;
};

// In busy 2.4 GHz environments the scanner can otherwise re-issue SCAN_REQ
// frames to the same background advertisers every second while the target peer
// is absent. A larger cache and longer hold cut that repeated RF work without
// delaying first discovery of new devices.
static constexpr uint8_t kBleActiveScanReqCacheSlots = 32U;
static constexpr uint32_t kBleActiveScanReqCacheHoldUs = 5000000UL;
static BleActiveScanReqCacheEntry
    g_bleActiveScanReqCache[kBleActiveScanReqCacheSlots] = {};
volatile uint32_t __attribute__((used)) g_ble_scan_active_scan_req_count = 0U;
volatile uint32_t __attribute__((used)) g_ble_scan_active_scan_req_skip_count = 0U;

static bool bleActiveScanReqCacheMatch(const BleActiveScanReqCacheEntry& entry,
                                       const uint8_t* address,
                                       bool random,
                                       uint32_t nowUs) {
  if (address == nullptr || entry.valid == 0U ||
      ((entry.random != 0U) != random) ||
      !bleAddressEqual(entry.address, address)) {
    return false;
  }

  return static_cast<uint32_t>(nowUs - entry.lastUs) <
         kBleActiveScanReqCacheHoldUs;
}

static bool bleActiveScanReqRecentlySent(const uint8_t* address, bool random,
                                         uint32_t nowUs) {
  for (uint8_t i = 0U; i < kBleActiveScanReqCacheSlots; ++i) {
    if (bleActiveScanReqCacheMatch(g_bleActiveScanReqCache[i], address,
                                   random, nowUs)) {
      g_bleActiveScanReqCache[i].lastUs = nowUs;
      return true;
    }
  }

  return false;
}

static void bleActiveScanReqRemember(const uint8_t* address, bool random,
                                     uint32_t nowUs) {
  if (address == nullptr) {
    return;
  }

  uint8_t slot = 0U;
  uint32_t oldestAge = 0U;
  for (uint8_t i = 0U; i < kBleActiveScanReqCacheSlots; ++i) {
    if (g_bleActiveScanReqCache[i].valid == 0U) {
      slot = i;
      oldestAge = 0xFFFFFFFFUL;
      break;
    }
    const uint32_t age = static_cast<uint32_t>(nowUs -
                                              g_bleActiveScanReqCache[i].lastUs);
    if (age >= oldestAge) {
      oldestAge = age;
      slot = i;
    }
  }

  memcpy(g_bleActiveScanReqCache[slot].address, address,
         sizeof(g_bleActiveScanReqCache[slot].address));
  g_bleActiveScanReqCache[slot].lastUs = nowUs;
  g_bleActiveScanReqCache[slot].random = random ? 1U : 0U;
  g_bleActiveScanReqCache[slot].valid = 1U;
}

bool BleRadio::scanOnce(BleAdvertisingChannel channel, BleScanPacket* packet,
                        uint32_t spinLimit) {
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }
  if (!initialized_ || radio_ == nullptr || connected_) {
    endUnconnectedRadioActivity();
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    endUnconnectedRadioActivity();
    return false;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));

  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const bool endSeen = waitForEnd(spinLimit);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const bool disabled = waitDisabled(spinLimit / 2U + 1U);
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);

  radio_->SHORTS = 0U;
  if (!disabled || !crcOk) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const uint8_t payloadLen = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  if (packet != nullptr) {
    packet->channel = channel;
    packet->rssiDbm = radioRssiDbm(radio_);
    packet->pduHeader = rxPacket_[0];
    packet->length = payloadLen;
    packet->payload = &rxPacket_[2];
  }

  clearRadioCoreEvents(radio_);
  endUnconnectedRadioActivity();
  return true;
}

bool BleRadio::scanCycle(BleScanPacket* packet, uint32_t perChannelSpinLimit) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint8_t idx = static_cast<uint8_t>(scanCycleStartIndex_ % 3U);
  scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
  return scanOnce(kChannels[idx], packet, perChannelSpinLimit);
}

bool BleRadio::scanOnceBudgeted(BleAdvertisingChannel channel,
                                BleScanPacket* packet,
                                uint32_t listenBudgetUs) {
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }
  if (!initialized_ || radio_ == nullptr || connected_) {
    endUnconnectedRadioActivity();
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    endUnconnectedRadioActivity();
    return false;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));

  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const bool endSeen =
      waitRadioEndBudgetedSleep(radio_, listenBudgetUs);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    bleWaitDisabledBudgeted(radio_, 3000U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const bool disabled = bleWaitDisabledBudgeted(radio_, 3000U);
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);

  radio_->SHORTS = 0U;
  if (!disabled || !crcOk) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const uint8_t payloadLen = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  if (packet != nullptr) {
    packet->channel = channel;
    packet->rssiDbm = radioRssiDbm(radio_);
    packet->pduHeader = rxPacket_[0];
    packet->length = payloadLen;
    packet->payload = &rxPacket_[2];
  }

  clearRadioCoreEvents(radio_);
  endUnconnectedRadioActivity();
  return true;
}

bool BleRadio::scanCycleBudgeted(BleScanPacket* packet,
                                 uint32_t perChannelListenBudgetUs) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint8_t idx = static_cast<uint8_t>(scanCycleStartIndex_ % 3U);
  scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
  return scanOnceBudgeted(kChannels[idx], packet, perChannelListenBudgetUs);
}

bool BleRadio::receivePacketOnCurrentChannel(uint32_t listenSpinLimit,
                                             uint32_t spinLimit,
                                             uint8_t* outHeader,
                                             uint8_t* outPayloadLength,
                                             const uint8_t** outPayload,
                                             int8_t* outRssiDbm,
                                             uint32_t* outStartUs,
                                             uint32_t* outEndUs) {
  if (!initialized_ || radio_ == nullptr || connected_) {
    return false;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  bool addressSeen = false;
  uint32_t packetStartUs = 0U;
  uint32_t packetEndUs = 0U;
  const bool endSeen =
      waitRadioReceiveEndBudgeted(radio_, 0U, listenSpinLimit, &addressSeen,
                                  &packetStartUs, &packetEndUs);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    return false;
  }
  if ((outStartUs != nullptr) && addressSeen) {
    *outStartUs = packetStartUs;
  }
  if (outEndUs != nullptr) {
    *outEndUs = (packetEndUs != 0U) ? packetEndUs : bleTimingUs();
  }

  bool disabled = waitDisabled(spinLimit / 2U + 1U);
  if (!disabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    disabled = waitDisabled(spinLimit / 2U + 1U);
  }
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const int8_t rssiDbm = radioRssiDbm(radio_);
  radio_->SHORTS = 0U;
  if (!disabled || !crcOk) {
    clearRadioCoreEvents(radio_);
    return false;
  }

  if (outHeader != nullptr) {
    *outHeader = rxPacket_[0];
  }
  if (outPayloadLength != nullptr) {
    *outPayloadLength = rxPacket_[1];
  }
  if (outPayload != nullptr) {
    *outPayload = &rxPacket_[2];
  }
  if (outRssiDbm != nullptr) {
    *outRssiDbm = rssiDbm;
  }

  clearRadioCoreEvents(radio_);
  return true;
}

bool BleRadio::receivePacketOnCurrentChannelBudgeted(
    uint32_t listenBudgetUs, uint8_t* outHeader, uint8_t* outPayloadLength,
    const uint8_t** outPayload, int8_t* outRssiDbm, uint32_t* outStartUs,
    uint32_t* outEndUs) {
  if (!initialized_ || radio_ == nullptr || connected_) {
    return false;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  bool addressSeen = false;
  uint32_t packetStartUs = 0U;
  uint32_t packetEndUs = 0U;
  const bool endSeen = waitRadioReceiveEndBudgetedSleep(
      radio_, listenBudgetUs, &addressSeen, &packetStartUs, &packetEndUs);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    bleWaitDisabledBudgeted(radio_, 3000U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    return false;
  }
  if ((outStartUs != nullptr) && addressSeen) {
    *outStartUs = packetStartUs;
  }
  if (outEndUs != nullptr) {
    *outEndUs = (packetEndUs != 0U) ? packetEndUs : bleTimingUs();
  }

  bool disabled = bleWaitDisabledBudgeted(radio_, 3000U);
  if (!disabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    disabled = bleWaitDisabledBudgeted(radio_, 3000U);
  }
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const int8_t rssiDbm = radioRssiDbm(radio_);
  radio_->SHORTS = 0U;
  if (!disabled || !crcOk) {
    clearRadioCoreEvents(radio_);
    return false;
  }

  if (outHeader != nullptr) {
    *outHeader = rxPacket_[0];
  }
  if (outPayloadLength != nullptr) {
    *outPayloadLength = rxPacket_[1];
  }
  if (outPayload != nullptr) {
    *outPayload = &rxPacket_[2];
  }
  if (outRssiDbm != nullptr) {
    *outRssiDbm = rssiDbm;
  }

  clearRadioCoreEvents(radio_);
  return true;
}

bool BleRadio::scanActiveOnce(BleAdvertisingChannel channel,
                              BleActiveScanResult* result,
                              uint32_t advListenSpinLimit,
                              uint32_t scanRspListenSpinLimit,
                              uint32_t spinLimit) {
  if (!serviceResolvablePrivateAddressRotation()) {
    return false;
  }
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }
  if (!initialized_ || radio_ == nullptr || connected_) {
    endUnconnectedRadioActivity();
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    endUnconnectedRadioActivity();
    return false;
  }

  if (result != nullptr) {
    memset(result, 0, sizeof(*result));
    result->channel = channel;
    result->scanRspRssiDbm = -127;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const bool endSeen = waitForEnd(advListenSpinLimit);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }
  const uint32_t advEndUs = bleTimingUs();

  bool disabled = waitDisabled(spinLimit / 2U + 1U);
  if (!disabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    disabled = waitDisabled(spinLimit / 2U + 1U);
  }
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const int8_t advRssi = radioRssiDbm(radio_);
  radio_->SHORTS = 0U;
  if (!disabled || !crcOk) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const uint8_t advHeader = rxPacket_[0];
  const uint8_t advPduType = static_cast<uint8_t>(advHeader & 0x0FU);
  const uint8_t advPayloadLen = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  const bool advertiserAddressRandom = ((advHeader >> 6U) & 0x1U) != 0U;
  const uint8_t* advPayload = &rxPacket_[2];

  const bool isAdvertisingPdu =
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvDirectInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvNonConnInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvScanInd));
  if (!isAdvertisingPdu || (advPayloadLen < 6U)) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return false;
  }

  const uint8_t copyAdvLen =
      (advPayloadLen <= static_cast<uint8_t>(sizeof(result->advPayload)))
          ? advPayloadLen
          : static_cast<uint8_t>(sizeof(result->advPayload));
  if (result != nullptr) {
    result->advRssiDbm = advRssi;
    result->advHeader = advHeader;
    result->advPayloadLength = copyAdvLen;
    result->advertiserAddressRandom = advertiserAddressRandom;
    memcpy(result->advertiserAddress, advPayload, sizeof(result->advertiserAddress));
    if (copyAdvLen > 0U) {
      memcpy(result->advPayload, advPayload, copyAdvLen);
    }
  }

  const bool scannable =
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvScanInd));
  if (!scannable) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return true;
  }

  // Active scan request: [ScanA (local)] [AdvA (remote)]
  const bool scannerAddressRandom = bleAddressTypeIsRandom(addressType_);
  txPacket_[0] = static_cast<uint8_t>(kBlePduScanReq |
                                      ((scannerAddressRandom ? 1U : 0U) << 6U) |
                                      ((advertiserAddressRandom ? 1U : 0U) << 7U));
  txPacket_[1] = 12U;
  memcpy(&txPacket_[2], address_, 6U);
  memcpy(&txPacket_[8], advPayload, 6U);

  clearRadioCoreEvents(radio_);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&txPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  const uint32_t txTriggerTargetUs = advEndUs + kBleConnTxenAfterRxUs;
  bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
  bool txDisabled = waitDisabled(spinLimit / 2U + 1U);
  if (!txDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    txDisabled = waitDisabled(spinLimit / 2U + 1U);
  }
  radio_->SHORTS = 0U;
  if (!txDisabled) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return true;
  }

  // Wait for SCAN_RSP on the same channel.
  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);
  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const bool rspEndSeen = waitRadioEndBudgeted(
      radio_, kBleScanRspListenMaxUs, scanRspListenSpinLimit);
  if (!rspEndSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return true;
  }

  bool rspDisabled = waitDisabled(spinLimit / 2U + 1U);
  if (!rspDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    rspDisabled = waitDisabled(spinLimit / 2U + 1U);
  }
  const uint32_t rspCrcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool rspCrcOk = (rspCrcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const int8_t rspRssi = radioRssiDbm(radio_);
  radio_->SHORTS = 0U;
  if (!rspDisabled || !rspCrcOk) {
    clearRadioCoreEvents(radio_);
    endUnconnectedRadioActivity();
    return true;
  }

  const uint8_t rspHeader = rxPacket_[0];
  const uint8_t rspPduType = static_cast<uint8_t>(rspHeader & 0x0FU);
  const uint8_t rspPayloadLen = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  const bool rspAddrRandom = ((rspHeader >> 6U) & 0x1U) != 0U;
  const uint8_t* rspPayload = &rxPacket_[2];

  const bool looksLikeMatchingScanRsp =
      (rspPduType == kBlePduScanRsp) &&
      (rspPayloadLen >= 6U) &&
      (rspAddrRandom == advertiserAddressRandom) &&
      bleAddressEqual(rspPayload, advPayload);
  if (looksLikeMatchingScanRsp && (result != nullptr)) {
    const uint8_t copyRspLen =
        (rspPayloadLen <= static_cast<uint8_t>(sizeof(result->scanRspPayload)))
            ? rspPayloadLen
            : static_cast<uint8_t>(sizeof(result->scanRspPayload));
    result->scanResponseReceived = true;
    result->scanRspRssiDbm = rspRssi;
    result->scanRspHeader = rspHeader;
    result->scanRspPayloadLength = copyRspLen;
    if (copyRspLen > 0U) {
      memcpy(result->scanRspPayload, rspPayload, copyRspLen);
    }
  }

  clearRadioCoreEvents(radio_);
  endUnconnectedRadioActivity();
  return true;
}

bool BleRadio::scanActiveOnceBudgeted(BleAdvertisingChannel channel,
                                      BleActiveScanResult* result,
                                      uint32_t advListenBudgetUs,
                                      uint32_t scanRspListenBudgetUs) {
  auto finish = [&](bool value, uint8_t code) {
    g_ble_scan_active_last_return_value = value ? 1U : 0U;
    g_ble_scan_active_last_return_code = code;
    endUnconnectedRadioActivity();
    return value;
  };
  if (!serviceResolvablePrivateAddressRotation()) {
    return false;
  }
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }
  if (!initialized_ || radio_ == nullptr || connected_) {
    return finish(false, 1U);
  }
  if (!setAdvertisingChannel(channel)) {
    return finish(false, 2U);
  }

  if (result != nullptr) {
    memset(result, 0, sizeof(*result));
    result->channel = channel;
    result->scanRspRssiDbm = -127;
  }

  uint8_t advHeader = 0U;
  uint8_t advPayloadLenRaw = 0U;
  const uint8_t* advPayload = nullptr;
  int8_t advRssi = -127;
  uint32_t advEndUs = 0U;
  if (!receivePacketOnCurrentChannelBudgeted(advListenBudgetUs, &advHeader,
                                             &advPayloadLenRaw, &advPayload,
                                             &advRssi, nullptr, &advEndUs)) {
    return finish(false, 3U);
  }

  ++g_ble_scan_active_adv_rx_count;
  g_ble_scan_active_last_channel = static_cast<uint8_t>(channel);
  g_ble_scan_active_last_header = advHeader;
  g_ble_scan_active_last_length = advPayloadLenRaw;
  g_ble_scan_active_last_flags = 0U;
  if (advPayload != nullptr) {
    memcpy(const_cast<uint8_t*>(g_ble_scan_active_last_address), advPayload, 6U);
    const uint8_t advDataLen =
        (advPayloadLenRaw > 6U)
            ? static_cast<uint8_t>(((advPayloadLenRaw - 6U) < 24U)
                                       ? (advPayloadLenRaw - 6U)
                                       : 24U)
            : 0U;
    if (advDataLen > 0U) {
      memcpy(const_cast<uint8_t*>(g_ble_scan_active_last_adv_data),
             &advPayload[6], advDataLen);
    }
    if (advDataLen < 24U) {
      memset(const_cast<uint8_t*>(&g_ble_scan_active_last_adv_data[advDataLen]),
             0, 24U - advDataLen);
    }
  } else {
    memset(const_cast<uint8_t*>(g_ble_scan_active_last_address), 0, 6U);
    memset(const_cast<uint8_t*>(g_ble_scan_active_last_adv_data), 0, 24U);
  }

  const uint8_t advPduType = static_cast<uint8_t>(advHeader & 0x0FU);
  const uint8_t advPayloadLen = static_cast<uint8_t>(advPayloadLenRaw & 0x3FU);
  const bool advertiserAddressRandom = ((advHeader >> 6U) & 0x1U) != 0U;
  if (advertiserAddressRandom) {
    g_ble_scan_active_last_flags |= 0x01U;
  }

  const bool isAdvertisingPdu =
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvDirectInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvNonConnInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvScanInd));
  if (isAdvertisingPdu) {
    g_ble_scan_active_last_flags |= 0x02U;
  }
  if (!isAdvertisingPdu || (advPayloadLen < 6U)) {
    return finish(false, 4U);
  }

  const uint8_t copyAdvLen =
      (advPayloadLen <= static_cast<uint8_t>(sizeof(result->advPayload)))
          ? advPayloadLen
          : static_cast<uint8_t>(sizeof(result->advPayload));
  if (result != nullptr) {
    result->advRssiDbm = advRssi;
    result->advHeader = advHeader;
    result->advPayloadLength = copyAdvLen;
    result->advertiserAddressRandom = advertiserAddressRandom;
    memcpy(result->advertiserAddress, advPayload, sizeof(result->advertiserAddress));
    if (copyAdvLen > 0U) {
      memcpy(result->advPayload, advPayload, copyAdvLen);
    }
  }

  const bool scannable =
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd)) ||
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvScanInd));
  if (scannable) {
    g_ble_scan_active_last_flags |= 0x04U;
  }
  if (!scannable) {
    endUnconnectedRadioActivity();
    return true;
  }

  const uint32_t scanReqNowUs = bleTimingUs();
  if (bleActiveScanReqRecentlySent(advPayload, advertiserAddressRandom,
                                   scanReqNowUs)) {
    g_ble_scan_active_last_flags |= 0x10U;
    ++g_ble_scan_active_scan_req_skip_count;
    return finish(true, 10U);
  }
  bleActiveScanReqRemember(advPayload, advertiserAddressRandom, scanReqNowUs);
  ++g_ble_scan_active_scan_req_count;

  txPacket_[0] = static_cast<uint8_t>(kBlePduScanReq |
                                      ((bleAddressTypeIsRandom(addressType_) ? 1U : 0U)
                                       << 6U) |
                                      ((advertiserAddressRandom ? 1U : 0U) << 7U));
  txPacket_[1] = 12U;
  memcpy(&txPacket_[2], address_, 6U);
  memcpy(&txPacket_[8], advPayload, 6U);

  clearRadioCoreEvents(radio_);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&txPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  const uint32_t txTriggerTargetUs = advEndUs + kBleConnTxenAfterRxUs;
  bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
  bool txDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  if (!txDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    txDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  }
  radio_->SHORTS = 0U;
  if (!txDisabled) {
    clearRadioCoreEvents(radio_);
    return finish(true, 5U);
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);
  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const uint32_t scanRspBudgetUs =
      (scanRspListenBudgetUs == 0U)
          ? kBleScanRspListenMaxUs
          : ((scanRspListenBudgetUs < kBleScanRspListenMaxUs)
                 ? scanRspListenBudgetUs
                 : kBleScanRspListenMaxUs);
  const bool rspEndSeen =
      waitRadioEndBudgetedSleep(radio_, scanRspBudgetUs);
  if (!rspEndSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    bleWaitDisabledBudgeted(radio_, 3000U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    return finish(true, 6U);
  }

  bool rspDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  if (!rspDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    rspDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  }
  const uint32_t rspCrcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool rspCrcOk = (rspCrcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const int8_t rspRssi = radioRssiDbm(radio_);
  radio_->SHORTS = 0U;
  if (!rspDisabled || !rspCrcOk) {
    clearRadioCoreEvents(radio_);
    return finish(true, 7U);
  }

  const uint8_t rspHeader = rxPacket_[0];
  const uint8_t rspPduType = static_cast<uint8_t>(rspHeader & 0x0FU);
  const uint8_t rspPayloadLen = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  const bool rspAddrRandom = ((rspHeader >> 6U) & 0x1U) != 0U;
  const uint8_t* rspPayload = &rxPacket_[2];

  const bool looksLikeMatchingScanRsp =
      (rspPduType == kBlePduScanRsp) &&
      (rspPayloadLen >= 6U) &&
      (rspAddrRandom == advertiserAddressRandom) &&
      bleAddressEqual(rspPayload, advPayload);
  if (looksLikeMatchingScanRsp) {
    g_ble_scan_active_last_flags |= 0x08U;
  }
  if (looksLikeMatchingScanRsp && (result != nullptr)) {
    const uint8_t copyRspLen =
        (rspPayloadLen <= static_cast<uint8_t>(sizeof(result->scanRspPayload)))
            ? rspPayloadLen
            : static_cast<uint8_t>(sizeof(result->scanRspPayload));
    result->scanResponseReceived = true;
    result->scanRspRssiDbm = rspRssi;
    result->scanRspHeader = rspHeader;
    result->scanRspPayloadLength = copyRspLen;
    if (copyRspLen > 0U) {
      memcpy(result->scanRspPayload, rspPayload, copyRspLen);
    }
  }

  clearRadioCoreEvents(radio_);
  return finish(true, looksLikeMatchingScanRsp ? 9U : 8U);
}

bool BleRadio::scanActiveCycle(BleActiveScanResult* result,
                               uint32_t perChannelAdvListenSpinLimit,
                               uint32_t scanRspListenSpinLimit) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint8_t idx = static_cast<uint8_t>(scanCycleStartIndex_ % 3U);
  scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
  return scanActiveOnce(kChannels[idx], result, perChannelAdvListenSpinLimit,
                        scanRspListenSpinLimit,
                        perChannelAdvListenSpinLimit + scanRspListenSpinLimit +
                            200000UL);
}

bool BleRadio::scanExtendedOnce(BleAdvertisingChannel channel,
                                BleExtendedScanResult* result,
                                uint32_t primaryListenSpinLimit,
                                uint32_t secondaryListenSpinLimit,
                                uint32_t spinLimit) {
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }

  auto finish = [&](bool value) {
    if (radio_ != nullptr) {
      radio_->SHORTS = 0U;
      clearRadioCoreEvents(radio_);
    }
    endUnconnectedRadioActivity();
    return value;
  };

  if (!initialized_ || radio_ == nullptr || connected_) {
    return finish(false);
  }
  if (!setAdvertisingChannel(channel)) {
    return finish(false);
  }

  BleExtendedScanResult scratch{};
  BleExtendedScanResult* out = (result != nullptr) ? result : &scratch;
  initBleExtendedScanResult(out, channel);
  bool haveAdvertiserAddress = false;

  auto receiveChain = [&](BleExtendedAuxPtrView nextAux,
                          uint8_t* packetCount,
                          uint8_t* dataBuffer,
                          uint16_t* dataLength) {
    bool haveNextAux = true;
    while (haveNextAux) {
      if (packetCount == nullptr || *packetCount >= kBleExtendedSecondaryPacketCountMax) {
        break;
      }
      if (!setDataChannel(nextAux.channel)) {
        return false;
      }

      uint8_t header = 0U;
      uint8_t payloadLenRaw = 0U;
      const uint8_t* payload = nullptr;
      if (!receivePacketOnCurrentChannel(secondaryListenSpinLimit,
                                         spinLimit,
                                         &header,
                                         &payloadLenRaw,
                                         &payload,
                                         nullptr,
                                         nullptr,
                                         nullptr)) {
        return false;
      }

      if ((header & 0x0FU) != kBlePduAdvExtInd) {
        return false;
      }

      BleExtendedHeaderView view{};
      const uint8_t payloadLen = payloadLenRaw;
      if (!parseBleExtendedHeader(payload, payloadLen, &view) || !view.valid) {
        return false;
      }

      ++(*packetCount);
      updateBleExtendedScanIdentity(out, view, &haveAdvertiserAddress);
      appendBleExtendedScanBytes(dataBuffer, dataLength, view.advData, view.advDataLength);

      if (!view.hasAuxPtr) {
        haveNextAux = false;
      } else {
        nextAux = view.auxPtr;
      }
    }
    return true;
  };

  uint8_t primaryHeader = 0U;
  uint8_t primaryPayloadLenRaw = 0U;
  const uint8_t* primaryPayload = nullptr;
  int8_t primaryRssi = -127;
  if (!receivePacketOnCurrentChannel(primaryListenSpinLimit,
                                     spinLimit,
                                     &primaryHeader,
                                     &primaryPayloadLenRaw,
                                     &primaryPayload,
                                     &primaryRssi,
                                     nullptr,
                                     nullptr)) {
    return finish(false);
  }

  const uint8_t primaryPayloadLen = primaryPayloadLenRaw;
  if ((primaryHeader & 0x0FU) != kBlePduAdvExtInd) {
    return finish(false);
  }

  BleExtendedHeaderView primaryView{};
  if (!parseBleExtendedHeader(primaryPayload, primaryPayloadLen, &primaryView) ||
      !primaryView.valid) {
    return finish(false);
  }

  out->primaryHeader = primaryHeader;
  out->primaryPayloadLength = primaryPayloadLen;
  out->primaryRssiDbm = primaryRssi;
  out->advMode = primaryView.advMode;
  out->advertiserAddressRandom = ((primaryHeader >> 6U) & 0x1U) != 0U;
  updateBleExtendedScanIdentity(out, primaryView, &haveAdvertiserAddress);
  appendBleExtendedScanBytes(out->data, &out->dataLength,
                             primaryView.advData, primaryView.advDataLength);

  if (!primaryView.hasAuxPtr) {
    return finish(out->dataLength > 0U);
  }

  out->auxChannel = primaryView.auxPtr.channel;
  out->auxPhy = primaryView.auxPtr.phy;
  out->auxOffsetUs = primaryView.auxPtr.offsetUs;

  if (!receiveChain(primaryView.auxPtr, &out->secondaryPacketCount,
                    out->data, &out->dataLength)) {
    return finish(false);
  }

  return finish((out->secondaryPacketCount > 0U) || (out->dataLength > 0U));
}

bool BleRadio::scanExtendedCycle(BleExtendedScanResult* result,
                                 uint32_t perChannelPrimaryListenSpinLimit,
                                 uint32_t secondaryListenSpinLimit) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint32_t firstPassSpin = perChannelPrimaryListenSpinLimit;
  const uint32_t secondPassSpin =
      (perChannelPrimaryListenSpinLimit > 1U)
          ? (perChannelPrimaryListenSpinLimit / 2U)
          : perChannelPrimaryListenSpinLimit;

  for (uint8_t pass = 0U; pass < 2U; ++pass) {
    const uint32_t dwell = (pass == 0U) ? firstPassSpin : secondPassSpin;
    for (uint8_t i = 0U; i < 3U; ++i) {
      const uint8_t idx = static_cast<uint8_t>((scanCycleStartIndex_ + i) % 3U);
      if (scanExtendedOnce(kChannels[idx],
                           result,
                           dwell,
                           secondaryListenSpinLimit,
                           dwell + secondaryListenSpinLimit + 200000UL)) {
        scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
        return true;
      }
    }
  }

  scanCycleStartIndex_ = static_cast<uint8_t>((scanCycleStartIndex_ + 1U) % 3U);
  return false;
}

bool BleRadio::scanActiveCycleBudgeted(BleActiveScanResult* result,
                                       uint32_t perChannelAdvListenBudgetUs,
                                       uint32_t scanRspListenBudgetUs) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint8_t idx = static_cast<uint8_t>(scanCycleStartIndex_ % 3U);
  scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
  return scanActiveOnceBudgeted(kChannels[idx], result,
                                perChannelAdvListenBudgetUs,
                                scanRspListenBudgetUs);
}

bool BleRadio::scanExtendedActiveOnce(BleAdvertisingChannel channel,
                                      BleExtendedScanResult* result,
                                      uint32_t primaryListenSpinLimit,
                                      uint32_t secondaryListenSpinLimit,
                                      uint32_t scanRspListenSpinLimit,
                                      uint32_t spinLimit) {
  if (!beginUnconnectedRadioActivity(1500000UL)) {
    return false;
  }

  auto finish = [&](bool value) {
    if (radio_ != nullptr) {
      radio_->SHORTS = 0U;
      clearRadioCoreEvents(radio_);
    }
    endUnconnectedRadioActivity();
    return value;
  };

  if (!initialized_ || radio_ == nullptr || connected_) {
    return finish(false);
  }
  if (!setAdvertisingChannel(channel)) {
    return finish(false);
  }

  BleExtendedScanResult scratch{};
  BleExtendedScanResult* out = (result != nullptr) ? result : &scratch;
  initBleExtendedScanResult(out, channel);
  bool haveAdvertiserAddress = false;

  auto receiveChain = [&](BleExtendedAuxPtrView nextAux,
                          uint8_t* packetCount,
                          uint8_t* dataBuffer,
                          uint16_t* dataLength) {
    bool haveNextAux = true;
    while (haveNextAux) {
      if (packetCount == nullptr || *packetCount >= kBleExtendedSecondaryPacketCountMax) {
        break;
      }
      if (!setDataChannel(nextAux.channel)) {
        return false;
      }

      uint8_t header = 0U;
      uint8_t payloadLenRaw = 0U;
      const uint8_t* payload = nullptr;
      int8_t rssi = -127;
      if (!receivePacketOnCurrentChannel(secondaryListenSpinLimit,
                                         spinLimit,
                                         &header,
                                         &payloadLenRaw,
                                         &payload,
                                         &rssi,
                                         nullptr,
                                         nullptr)) {
        return false;
      }

      if ((header & 0x0FU) != kBlePduAdvExtInd) {
        return false;
      }

      BleExtendedHeaderView view{};
      const uint8_t payloadLen = payloadLenRaw;
      if (!parseBleExtendedHeader(payload, payloadLen, &view) || !view.valid) {
        return false;
      }

      ++(*packetCount);
      updateBleExtendedScanIdentity(out, view, &haveAdvertiserAddress);
      appendBleExtendedScanBytes(dataBuffer, dataLength, view.advData, view.advDataLength);

      if (!view.hasAuxPtr) {
        haveNextAux = false;
      } else {
        nextAux = view.auxPtr;
      }
    }
    return true;
  };

  uint8_t primaryHeader = 0U;
  uint8_t primaryPayloadLenRaw = 0U;
  const uint8_t* primaryPayload = nullptr;
  int8_t primaryRssi = -127;
  if (!receivePacketOnCurrentChannel(primaryListenSpinLimit,
                                     spinLimit,
                                     &primaryHeader,
                                     &primaryPayloadLenRaw,
                                     &primaryPayload,
                                     &primaryRssi,
                                     nullptr,
                                     nullptr)) {
    return finish(false);
  }

  const uint8_t primaryPayloadLen = primaryPayloadLenRaw;
  if ((primaryHeader & 0x0FU) != kBlePduAdvExtInd) {
    return finish(false);
  }

  BleExtendedHeaderView primaryView{};
  if (!parseBleExtendedHeader(primaryPayload, primaryPayloadLen, &primaryView) ||
      !primaryView.valid) {
    return finish(false);
  }

  out->primaryHeader = primaryHeader;
  out->primaryPayloadLength = primaryPayloadLen;
  out->primaryRssiDbm = primaryRssi;
  out->advMode = primaryView.advMode;
  out->advertiserAddressRandom = ((primaryHeader >> 6U) & 0x1U) != 0U;
  updateBleExtendedScanIdentity(out, primaryView, &haveAdvertiserAddress);
  appendBleExtendedScanBytes(out->data, &out->dataLength,
                             primaryView.advData, primaryView.advDataLength);

  if (!primaryView.hasAuxPtr) {
    return finish(out->dataLength > 0U);
  }

  out->auxChannel = primaryView.auxPtr.channel;
  out->auxPhy = primaryView.auxPtr.phy;
  out->auxOffsetUs = primaryView.auxPtr.offsetUs;

  if (!setDataChannel(primaryView.auxPtr.channel)) {
    return finish(false);
  }

  uint8_t secondaryHeader = 0U;
  uint8_t secondaryPayloadLenRaw = 0U;
  const uint8_t* secondaryPayload = nullptr;
  int8_t secondaryRssi = -127;
  uint32_t secondaryEndUs = 0U;
  if (!receivePacketOnCurrentChannel(secondaryListenSpinLimit,
                                     spinLimit,
                                     &secondaryHeader,
                                     &secondaryPayloadLenRaw,
                                     &secondaryPayload,
                                     &secondaryRssi,
                                     nullptr,
                                     &secondaryEndUs)) {
    return finish(false);
  }

  const uint8_t secondaryPayloadLen = secondaryPayloadLenRaw;
  if ((secondaryHeader & 0x0FU) != kBlePduAdvExtInd) {
    return finish(false);
  }

  BleExtendedHeaderView secondaryView{};
  if (!parseBleExtendedHeader(secondaryPayload, secondaryPayloadLen, &secondaryView) ||
      !secondaryView.valid) {
    return finish(false);
  }

  ++out->secondaryPacketCount;
  updateBleExtendedScanIdentity(out, secondaryView, &haveAdvertiserAddress);
  appendBleExtendedScanBytes(out->data, &out->dataLength,
                             secondaryView.advData, secondaryView.advDataLength);

  if (primaryView.advMode != kBleExtendedAdvModeNonConnScan) {
    if (secondaryView.hasAuxPtr &&
        !receiveChain(secondaryView.auxPtr, &out->secondaryPacketCount,
                      out->data, &out->dataLength)) {
      return finish(false);
    }
    return finish(true);
  }

  if (!haveAdvertiserAddress) {
    return finish(true);
  }

  const bool scannerAddressRandom = bleAddressTypeIsRandom(addressType_);
  rxPacket_[0] = static_cast<uint8_t>(kBlePduScanReq |
                                      ((scannerAddressRandom ? 1U : 0U) << 6U) |
                                      ((out->advertiserAddressRandom ? 1U : 0U) << 7U));
  rxPacket_[1] = 12U;
  memcpy(&rxPacket_[2], address_, 6U);
  memcpy(&rxPacket_[8], out->advertiserAddress, 6U);

  clearRadioCoreEvents(radio_);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk) |
      ((RADIO_SHORTS_DISABLED_RXEN_Enabled << RADIO_SHORTS_DISABLED_RXEN_Pos) &
       RADIO_SHORTS_DISABLED_RXEN_Msk) |
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk);

  const uint32_t txTriggerTargetUs = secondaryEndUs + kBleConnTxenAfterRxUs;
  bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
  const bool txEndSeen = waitForEnd(spinLimit);
  if (!txEndSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    return finish(false);
  }

  bool txDisabled = waitDisabled(spinLimit / 2U + 1U);
  if (!txDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    txDisabled = waitDisabled(spinLimit / 2U + 1U);
  }
  if (!txDisabled) {
    return finish(false);
  }

  radio_->EVENTS_DISABLED = 0U;
  radio_->EVENTS_END = 0U;
  radio_->EVENTS_PHYEND = 0U;
  radio_->EVENTS_CRCOK = 0U;
  radio_->EVENTS_CRCERROR = 0U;
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  const uint32_t state =
      (radio_->STATE & RADIO_STATE_STATE_Msk) >> RADIO_STATE_STATE_Pos;
  if (state == RADIO_STATE_STATE_Disabled) {
    radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;
  }

  const bool rspEndSeen = waitRadioEndBudgeted(
      radio_, kBleScanRspListenMaxUs, scanRspListenSpinLimit);
  if (!rspEndSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    return finish(true);
  }

  const int8_t scanRspRssi = radioRssiDbm(radio_);
  const uint8_t scanRspHeader = rxPacket_[0];
  const uint8_t scanRspPayloadLenRaw = rxPacket_[1];
  const uint8_t* scanRspPayload = &rxPacket_[2];
  const uint32_t rspCrcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool rspCrcOk = (rspCrcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);

  bool rspDisabled = waitDisabled(spinLimit / 2U + 1U);
  if (!rspDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    rspDisabled = waitDisabled(spinLimit / 2U + 1U);
  }
  if (!rspDisabled || !rspCrcOk || ((scanRspHeader & 0x0FU) != kBlePduAdvExtInd)) {
    return finish(true);
  }

  BleExtendedHeaderView scanRspView{};
  if (!parseBleExtendedHeader(scanRspPayload, scanRspPayloadLenRaw, &scanRspView) ||
      !scanRspView.valid) {
    return finish(true);
  }

  out->scanResponseReceived = true;
  out->scanRspRssiDbm = scanRspRssi;
  ++out->scanRspSecondaryPacketCount;
  updateBleExtendedScanIdentity(out, scanRspView, &haveAdvertiserAddress);
  appendBleExtendedScanBytes(out->scanRspData, &out->scanRspDataLength,
                             scanRspView.advData, scanRspView.advDataLength);

  if (scanRspView.hasAuxPtr &&
      !receiveChain(scanRspView.auxPtr, &out->scanRspSecondaryPacketCount,
                    out->scanRspData, &out->scanRspDataLength)) {
    return finish(false);
  }

  return finish(true);
}

bool BleRadio::scanExtendedActiveCycle(BleExtendedScanResult* result,
                                       uint32_t perChannelPrimaryListenSpinLimit,
                                       uint32_t secondaryListenSpinLimit,
                                       uint32_t scanRspListenSpinLimit) {
  static constexpr BleAdvertisingChannel kChannels[3] = {
      BleAdvertisingChannel::k37,
      BleAdvertisingChannel::k38,
      BleAdvertisingChannel::k39,
  };

  const uint32_t firstPassSpin = perChannelPrimaryListenSpinLimit;
  const uint32_t secondPassSpin =
      (perChannelPrimaryListenSpinLimit > 1U)
          ? (perChannelPrimaryListenSpinLimit / 2U)
          : perChannelPrimaryListenSpinLimit;

  for (uint8_t pass = 0U; pass < 2U; ++pass) {
    const uint32_t dwell = (pass == 0U) ? firstPassSpin : secondPassSpin;
    for (uint8_t i = 0U; i < 3U; ++i) {
      const uint8_t idx = static_cast<uint8_t>((scanCycleStartIndex_ + i) % 3U);
      if (scanExtendedActiveOnce(kChannels[idx],
                                 result,
                                 dwell,
                                 secondaryListenSpinLimit,
                                 scanRspListenSpinLimit,
                                 dwell + secondaryListenSpinLimit +
                                     scanRspListenSpinLimit + 200000UL)) {
        scanCycleStartIndex_ = static_cast<uint8_t>((idx + 1U) % 3U);
        return true;
      }
    }
  }

  scanCycleStartIndex_ = static_cast<uint8_t>((scanCycleStartIndex_ + 1U) % 3U);
  return false;
}

bool BleRadio::initiateConnectionOnce(BleAdvertisingChannel channel,
                                      const uint8_t peerAddress[6],
                                      bool peerAddressRandom,
                                      uint16_t intervalUnits,
                                      uint16_t supervisionTimeoutUnits,
                                      uint8_t hopIncrement,
                                      uint32_t advListenSpinLimit,
                                      uint32_t spinLimit) {
  g_ble_central_connect_debug_stage = 1U;
  g_ble_central_connect_debug_header = 0U;
  g_ble_central_connect_debug_length = 0U;
  g_ble_central_connect_debug_flags = 0U;
  memset(const_cast<uint8_t*>(g_ble_central_connect_debug_address), 0,
         sizeof(g_ble_central_connect_debug_address));
  if (!initialized_ || radio_ == nullptr || connected_ || peerAddress == nullptr) {
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    return false;
  }

  uint8_t advHeader = 0U;
  uint8_t advPayloadLenRaw = 0U;
  const uint8_t* advPayload = nullptr;
  uint32_t advEndUs = 0U;
  const bool packetReceived =
      receivePacketOnCurrentChannel(advListenSpinLimit, spinLimit, &advHeader,
                                    &advPayloadLenRaw, &advPayload, nullptr,
                                    nullptr, &advEndUs);
  if (!packetReceived) {
    emitBleTrace("CENTRAL_CONNECT_SCAN_TIMEOUT");
    return false;
  }
  g_ble_central_connect_debug_stage = 2U;
  const uint8_t advPduType = static_cast<uint8_t>(advHeader & 0x0FU);
  const uint8_t advPayloadLen = static_cast<uint8_t>(advPayloadLenRaw & 0x3FU);
  const bool advChSel2 = ((advHeader >> 5U) & 0x1U) != 0U;
  const bool advertiserAddressRandom = ((advHeader >> 6U) & 0x1U) != 0U;
  g_ble_central_connect_debug_header = advHeader;
  g_ble_central_connect_debug_length = advPayloadLen;
  if (advertiserAddressRandom) {
    g_ble_central_connect_debug_flags |= 0x10U;
  }
  if (advPayloadLen >= sizeof(g_ble_central_connect_debug_address)) {
    memcpy(const_cast<uint8_t*>(g_ble_central_connect_debug_address), advPayload,
           sizeof(g_ble_central_connect_debug_address));
  }
  if (advPayloadLen < 6U) {
    emitBleTrace("CENTRAL_CONNECT_SHORT_ADV");
    return false;
  }
  g_ble_central_connect_debug_flags |= 0x01U;
  g_ble_central_connect_debug_stage = 3U;

  const bool addressMatch =
      (advertiserAddressRandom == peerAddressRandom) &&
      bleAddressEqual(advPayload, peerAddress);
  const bool connectable =
      (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd));
  if (addressMatch) {
    g_ble_central_connect_debug_flags |= 0x02U;
  }
  if (connectable) {
    g_ble_central_connect_debug_flags |= 0x04U;
  }
  if (!addressMatch) {
    emitBleTrace("CENTRAL_CONNECT_ADDR_MISS");
    return false;
  }
  if (!connectable) {
    emitBleTrace("CENTRAL_CONNECT_NOT_CONN");
    return false;
  }
  g_ble_central_connect_debug_stage = 4U;

  const bool initiatorAddressRandom = bleAddressTypeIsRandom(addressType_);
  const bool useChSel2 = useChSel2_ && advChSel2;
  uint8_t entropy[7] = {0};
  const uint32_t seed =
      bleTimingUs() ^ (static_cast<uint32_t>(address_[0]) << 24U) ^
      (static_cast<uint32_t>(peerAddress[0]) << 16U) ^
      (static_cast<uint32_t>(peerAddress[5]) << 8U) ^
      static_cast<uint32_t>(intervalUnits);
  fillPseudoRandomBytes(entropy, sizeof(entropy), seed);

  const uint32_t accessAddressSeed =
      readLe32(&entropy[0]) ^
      (static_cast<uint32_t>(peerAddress[1]) << 24U) ^
      (static_cast<uint32_t>(peerAddress[4]) << 16U) ^
      (static_cast<uint32_t>(intervalUnits) << 4U);
  const uint32_t accessAddress =
      generateBleConnectionAccessAddress(accessAddressSeed);
  uint32_t crcInit = readLe24(&entropy[4]);
  if (crcInit == 0U || crcInit == 0x00FFFFFFUL) {
    crcInit ^= 0x005A96A5UL;
  }
  crcInit &= 0x00FFFFFFUL;

  txPacket_[0] = static_cast<uint8_t>(
      kBlePduConnectInd |
      ((useChSel2 ? 1U : 0U) << 5U) |
      ((initiatorAddressRandom ? 1U : 0U) << 6U) |
      ((peerAddressRandom ? 1U : 0U) << 7U));
  txPacket_[1] = 34U;
  memcpy(&txPacket_[2], address_, 6U);
  memcpy(&txPacket_[8], peerAddress, 6U);

  uint8_t* llData = &txPacket_[14];
  writeLe32(&llData[0], accessAddress);
  writeLe24(&llData[4], crcInit);
  llData[7] = 1U;
  writeLe16(&llData[8], 0U);
  writeLe16(&llData[10], intervalUnits);
  writeLe16(&llData[12], 0U);
  writeLe16(&llData[14], supervisionTimeoutUnits);
  llData[16] = 0xFFU;
  llData[17] = 0xFFU;
  llData[18] = 0xFFU;
  llData[19] = 0xFFU;
  llData[20] = 0x1FU;
  llData[21] = static_cast<uint8_t>((3U << 5U) | (hopIncrement & 0x1FU));

  clearRadioCoreEvents(radio_);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&txPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  g_ble_central_connect_debug_stage = 5U;
  const uint32_t txTriggerTargetUs = advEndUs + kBleConnTxenAfterRxUs;
  bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
  bool txDisabled = waitDisabled(spinLimit / 2U + 1U);
  if (!txDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    txDisabled = waitDisabled(spinLimit / 2U + 1U);
  }
  const uint32_t connectIndEndUs = bleTimingUs();
  radio_->SHORTS = 0U;
  clearRadioCoreEvents(radio_);
  if (!txDisabled) {
    emitBleTrace("CENTRAL_CONNECT_TX_TIMEOUT");
    return false;
  }
  g_ble_central_connect_debug_flags |= 0x08U;
  g_ble_central_connect_debug_stage = 6U;

  return startCentralConnection(peerAddress, peerAddressRandom, accessAddress,
                                crcInit, intervalUnits,
                                supervisionTimeoutUnits, hopIncrement,
                                useChSel2, connectIndEndUs);
}

bool BleRadio::initiateConnectionOnceBudgeted(BleAdvertisingChannel channel,
                                              const uint8_t peerAddress[6],
                                              bool peerAddressRandom,
                                              uint16_t intervalUnits,
                                              uint16_t supervisionTimeoutUnits,
                                              uint8_t hopIncrement,
                                              uint32_t advListenBudgetUs) {
  g_ble_central_connect_debug_stage = 1U;
  g_ble_central_connect_debug_header = 0U;
  g_ble_central_connect_debug_length = 0U;
  g_ble_central_connect_debug_flags = 0U;
  memset(const_cast<uint8_t*>(g_ble_central_connect_debug_address), 0,
         sizeof(g_ble_central_connect_debug_address));
  if (!initialized_ || radio_ == nullptr || connected_ || peerAddress == nullptr) {
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    return false;
  }

  uint8_t advHeader = 0U;
  uint8_t advPayloadLenRaw = 0U;
  const uint8_t* advPayload = nullptr;
  uint32_t advEndUs = 0U;
  bool advChSel2 = false;
  const uint32_t listenStartUs = bleTimingUs();
  while (true) {
    const uint32_t elapsedUs =
        static_cast<uint32_t>(bleTimingUs() - listenStartUs);
    if ((advListenBudgetUs > 0U) && (elapsedUs >= advListenBudgetUs)) {
      emitBleTrace("CENTRAL_CONNECT_SCAN_TIMEOUT");
      return false;
    }
    const uint32_t remainingBudgetUs =
        (advListenBudgetUs > 0U) ? (advListenBudgetUs - elapsedUs) : 0U;
    const bool packetReceived = receivePacketOnCurrentChannelBudgeted(
        remainingBudgetUs, &advHeader, &advPayloadLenRaw, &advPayload, nullptr,
        nullptr, &advEndUs);
    if (!packetReceived) {
      emitBleTrace("CENTRAL_CONNECT_SCAN_TIMEOUT");
      return false;
    }

    g_ble_central_connect_debug_stage = 2U;
    const uint8_t advPduType = static_cast<uint8_t>(advHeader & 0x0FU);
    const uint8_t advPayloadLen =
        static_cast<uint8_t>(advPayloadLenRaw & 0x3FU);
    advChSel2 = ((advHeader >> 5U) & 0x1U) != 0U;
    const bool advertiserAddressRandom = ((advHeader >> 6U) & 0x1U) != 0U;
    g_ble_central_connect_debug_header = advHeader;
    g_ble_central_connect_debug_length = advPayloadLen;
    g_ble_central_connect_debug_flags = 0U;
    if (advertiserAddressRandom) {
      g_ble_central_connect_debug_flags |= 0x10U;
    }
    if (advPayloadLen >= sizeof(g_ble_central_connect_debug_address)) {
      memcpy(const_cast<uint8_t*>(g_ble_central_connect_debug_address),
             advPayload, sizeof(g_ble_central_connect_debug_address));
    }
    if (advPayloadLen < 6U) {
      continue;
    }
    g_ble_central_connect_debug_flags |= 0x01U;
    g_ble_central_connect_debug_stage = 3U;

    const bool addressMatch =
        (advertiserAddressRandom == peerAddressRandom) &&
        bleAddressEqual(advPayload, peerAddress);
    const bool connectable =
        (advPduType == static_cast<uint8_t>(BleAdvPduType::kAdvInd));
    if (addressMatch) {
      g_ble_central_connect_debug_flags |= 0x02U;
    }
    if (connectable) {
      g_ble_central_connect_debug_flags |= 0x04U;
    }
    if (addressMatch && connectable) {
      break;
    }
  }
  g_ble_central_connect_debug_stage = 4U;

  const bool initiatorAddressRandom = bleAddressTypeIsRandom(addressType_);
  const bool useChSel2 = useChSel2_ && advChSel2;
  uint8_t entropy[7] = {0};
  const uint32_t seed =
      bleTimingUs() ^ (static_cast<uint32_t>(address_[0]) << 24U) ^
      (static_cast<uint32_t>(peerAddress[0]) << 16U) ^
      (static_cast<uint32_t>(peerAddress[5]) << 8U) ^
      static_cast<uint32_t>(intervalUnits);
  fillPseudoRandomBytes(entropy, sizeof(entropy), seed);

  const uint32_t accessAddressSeed =
      readLe32(&entropy[0]) ^
      (static_cast<uint32_t>(peerAddress[1]) << 24U) ^
      (static_cast<uint32_t>(peerAddress[4]) << 16U) ^
      (static_cast<uint32_t>(intervalUnits) << 4U);
  const uint32_t accessAddress =
      generateBleConnectionAccessAddress(accessAddressSeed);
  uint32_t crcInit = readLe24(&entropy[4]);
  if (crcInit == 0U || crcInit == 0x00FFFFFFUL) {
    crcInit ^= 0x005A96A5UL;
  }
  crcInit &= 0x00FFFFFFUL;

  txPacket_[0] = static_cast<uint8_t>(
      kBlePduConnectInd |
      ((useChSel2 ? 1U : 0U) << 5U) |
      ((initiatorAddressRandom ? 1U : 0U) << 6U) |
      ((peerAddressRandom ? 1U : 0U) << 7U));
  txPacket_[1] = 34U;
  memcpy(&txPacket_[2], address_, 6U);
  memcpy(&txPacket_[8], peerAddress, 6U);

  uint8_t* llData = &txPacket_[14];
  writeLe32(&llData[0], accessAddress);
  writeLe24(&llData[4], crcInit);
  llData[7] = 1U;
  writeLe16(&llData[8], 0U);
  writeLe16(&llData[10], intervalUnits);
  writeLe16(&llData[12], 0U);
  writeLe16(&llData[14], supervisionTimeoutUnits);
  llData[16] = 0xFFU;
  llData[17] = 0xFFU;
  llData[18] = 0xFFU;
  llData[19] = 0xFFU;
  llData[20] = 0x1FU;
  llData[21] = static_cast<uint8_t>((3U << 5U) | (hopIncrement & 0x1FU));

  clearRadioCoreEvents(radio_);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&txPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  g_ble_central_connect_debug_stage = 5U;
  const uint32_t txTriggerTargetUs = advEndUs + kBleConnTxenAfterRxUs;
  bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
  bool txDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  if (!txDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    txDisabled = bleWaitDisabledBudgeted(radio_, 3000U);
  }
  const uint32_t connectIndEndUs = bleTimingUs();
  radio_->SHORTS = 0U;
  clearRadioCoreEvents(radio_);
  if (!txDisabled) {
    emitBleTrace("CENTRAL_CONNECT_TX_TIMEOUT");
    return false;
  }
  g_ble_central_connect_debug_flags |= 0x08U;
  g_ble_central_connect_debug_stage = 6U;

  return startCentralConnection(peerAddress, peerAddressRandom, accessAddress,
                                crcInit, intervalUnits,
                                supervisionTimeoutUnits, hopIncrement,
                                useChSel2, connectIndEndUs);
}

bool BleRadio::handleRequestAndMaybeRespond(BleAdvertisingChannel channel,
                                            BleAdvInteraction* interaction,
                                            uint32_t requestListenSpinLimit,
                                            uint32_t spinLimit) {
  if (!initialized_ || radio_ == nullptr) {
    return false;
  }
  if (!setAdvertisingChannel(channel)) {
    return false;
  }

  clearRadioCoreEvents(radio_);
  memset(rxPacket_, 0, sizeof(rxPacket_));
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;

  const bool endSeen = waitRadioEndBudgeted(
      radio_, kBleAdvRequestListenMaxUs, requestListenSpinLimit);
  if (!endSeen) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitDisabled(spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    return true;
  }
  const uint32_t rxEndUs = bleTimingUs();

  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  const uint8_t hdr = rxPacket_[0];
  const uint8_t pduType = static_cast<uint8_t>(hdr & 0x0FU);
  const bool pduChSel2 = ((hdr >> 5U) & 0x1U) != 0U;
  const uint8_t length = static_cast<uint8_t>(rxPacket_[1] & 0x3FU);
  const bool txAddrRandom = ((hdr >> 6U) & 0x1U) != 0U;
  const bool rxAddrRandom = ((hdr >> 7U) & 0x1U) != 0U;
  const uint8_t* payload = &rxPacket_[2];

  bool disabled = waitDisabled(spinLimit / 2U + 1U);
  if (!disabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    disabled = waitDisabled(spinLimit / 2U + 1U);
  }
  radio_->SHORTS = 0U;
  if (!disabled) {
    clearRadioCoreEvents(radio_);
    return false;
  }

  bool hasScanReq = false;
  bool hasConnectInd = false;
  bool connectAccepted = false;
  bool addressMatch = false;
  const bool connectable = (pduType_ == BleAdvPduType::kAdvInd);
  if (crcOk && length >= 12U) {
    const uint8_t* scannerOrInitiator = payload;
    const uint8_t* advertiserAddress = &payload[6];
    const bool advertiserAddressMatch =
        bleAddressEqual(advertiserAddress, &address_[0]);
    const bool advertiserAddressTypeMatch =
        (rxAddrRandom == bleAddressTypeIsRandom(addressType_));
    addressMatch = advertiserAddressMatch && advertiserAddressTypeMatch;
    // Keep the Qualcomm/Sony RxAdd fallback consistent with the other ADV_IND
    // interaction path. Some centrals reuse the correct address bytes while
    // misreporting the RxAdd bit in SCAN_REQ/CONNECT_IND.
    if (!addressMatch && advertiserAddressMatch &&
        (pduType == kBlePduScanReq || pduType == kBlePduConnectInd)) {
      addressMatch = true;
    }

    if (addressMatch && pduType == kBlePduScanReq) {
      hasScanReq = true;
      if (interaction != nullptr) {
        memcpy(&interaction->peerAddress[0], scannerOrInitiator, 6U);
      }
    } else if (addressMatch && pduType == kBlePduConnectInd && connectable &&
               (length >= 34U)) {
      hasConnectInd = true;
      if (interaction != nullptr) {
        memcpy(&interaction->peerAddress[0], scannerOrInitiator, 6U);
      }
      connectAccepted = startConnectionFromConnectInd(payload, length, txAddrRandom,
                                                      pduChSel2, rxEndUs);
    }
  }

  if (interaction != nullptr) {
    interaction->channel = channel;
    interaction->rssiDbm = radioRssiDbm(radio_);
    interaction->peerAddressRandom = txAddrRandom;
    interaction->receivedScanRequest = hasScanReq;
    interaction->receivedConnectInd = hasConnectInd;
    interaction->connectIndChSel2 = hasConnectInd && useChSel2_ && pduChSel2;
  }

  bool txScanRspOk = false;
  const bool scannable = (pduType_ == BleAdvPduType::kAdvInd) ||
                         (pduType_ == BleAdvPduType::kAdvScanInd);
  if (hasScanReq && scannable && !connectAccepted) {
    clearRadioCoreEvents(radio_);
    radio_->PACKETPTR =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&scanRspPacket_[0]));
    radio_->SHORTS =
        ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
         RADIO_SHORTS_TXREADY_START_Msk) |
        ((RADIO_SHORTS_PHYEND_DISABLE_Enabled
          << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
         RADIO_SHORTS_PHYEND_DISABLE_Msk);

    // Wait for T_IFS turnaround (150us nominal).
    const uint32_t txTriggerTargetUs = rxEndUs + kBleScanRspTxenAfterRxUs;
    bleTriggerTxAtTargetUsCritical(radio_, txTriggerTargetUs);
    txScanRspOk = waitDisabled(spinLimit);
    radio_->SHORTS = 0U;
  }

  if (interaction != nullptr) {
    interaction->scanResponseTransmitted = txScanRspOk;
    if (hasConnectInd && !connectAccepted) {
      interaction->receivedConnectInd = false;
      interaction->connectIndChSel2 = false;
    }
  }

  clearRadioCoreEvents(radio_);
  return true;
}

bool BleRadio::startConnectionFromConnectInd(const uint8_t* payload, uint8_t length,
                                             bool peerAddressRandom, bool useChSel2,
                                             uint32_t connectIndEndUs) {
  if (!initialized_ || radio_ == nullptr || payload == nullptr || length < 34U) {
    return false;
  }

  memcpy(connectionPeerAddress_, payload, sizeof(connectionPeerAddress_));
  connectionPeerAddressRandom_ = peerAddressRandom;

  const uint8_t* llData = &payload[12];
  connectionAccessAddress_ = readLe32(&llData[0]);
  connectionCrcInit_ = readLe24(&llData[4]);

  const uint8_t winSize = llData[7];
  const uint16_t winOffset = readLe16(&llData[8]);
  g_ble_periph_connect_win_size = winSize;
  g_ble_periph_connect_win_offset = winOffset;
  blePeriphConnTraceReset();
  blePeriphRxTraceReset();
  blePeriphTxTraceReset();
  blePeriphConnTracePush(0U, 0U, 1U, static_cast<uint8_t>(llData[21] & 0x1FU));
  connectionIntervalUnits_ = readLe16(&llData[10]);
  connectionLatency_ = readLe16(&llData[12]);
  connectionTimeoutUnits_ = readLe16(&llData[14]);
  memcpy(connectionChannelMap_, &llData[16], sizeof(connectionChannelMap_));
  connectionChannelMap_[4] &= 0x1FU;
  connectionSca_ = (llData[21] >> 5U) & 0x07U;
  connectionHop_ = llData[21] & 0x1FU;
  // CSA#2 is used only when both sides indicate support via ChSel bit.
  connectionUseChSel2_ = useChSel2_ && useChSel2;
  connectionChannelId_ = static_cast<uint16_t>(
      (connectionAccessAddress_ & 0xFFFFU) ^ ((connectionAccessAddress_ >> 16U) & 0xFFFFU));

  connectionChannelCount_ = bitCount37(connectionChannelMap_);
  if (connectionIntervalUnits_ < 6U || connectionIntervalUnits_ > 3200U) {
    return false;
  }
  if (connectionTimeoutUnits_ < 10U) {
    return false;
  }
  if (connectionChannelCount_ < 2U) {
    return false;
  }
  if (connectionHop_ < 5U || connectionHop_ > 16U) {
    return false;
  }
  if (connectionAccessAddress_ == kBleAccessAddress) {
    return false;
  }

  connectionChanUse_ = 0U;
  connectionExpectedRxSn_ = 0U;
  connectionTxSn_ = 0U;
  connectionLastTxSn_ = 0U;
  connectionTxHistoryValid_ = false;
  connectionFreshTxAllowed_ = true;
  connectionEventCounter_ = 0U;
  connectionMissedEventCount_ = 0U;
  {
    const uint32_t intervalUs =
        static_cast<uint32_t>(connectionIntervalUnits_) * 1250UL;
    const uint32_t winSizeUs =
        static_cast<uint32_t>((winSize > 0U) ? winSize : 1U) * 1250UL;
    if (intervalUs > (kBleConnRxStartLeadUs + 2000UL)) {
      const uint32_t maxSyncListenUs = intervalUs - kBleConnRxStartLeadUs - 2000UL;
      connectionFirstEventListenUs_ =
          (maxSyncListenUs > winSizeUs) ? maxSyncListenUs : winSizeUs;
    } else {
      connectionFirstEventListenUs_ = winSizeUs;
    }
  }
  connectionSyncAttemptsRemaining_ = 8U;
  connectionCurrentTxPhy_ = kBlePhy1M;
  connectionCurrentRxPhy_ = kBlePhy1M;
  connectionPreferredTxPhyMask_ =
      static_cast<uint8_t>(kBlePhy1M | kBlePhy2M | kBlePhyCoded);
  connectionPreferredRxPhyMask_ =
      static_cast<uint8_t>(kBlePhy1M | kBlePhy2M | kBlePhyCoded);
  connectionPhyRequestPending_ = false;
  connectionPhyRequestInFlight_ = false;
  connectionPhyUpdatePending_ = false;
  connectionPhyUpdateInstant_ = 0U;
  connectionPendingTxPhy_ = kBlePhy1M;
  connectionPendingRxPhy_ = kBlePhy1M;
  connectionMaxTxPayloadLength_ = kBleDefaultDataPduMaxPayload;
  connectionMaxRxPayloadLength_ = kBleDefaultDataPduMaxPayload;
  connectionDataLengthUpdatePending_ = false;
  connectionDataLengthUpdateInFlight_ = false;
  connectionDataLengthUpdateComplete_ = false;
  connectionAttMtuExchangePending_ = false;
  connectionAttMtuExchangeInFlight_ = false;
  connectionAttMtuExchangeComplete_ = false;
  connectionConnParamUpdatePending_ = false;
  connectionConnParamUpdateInFlight_ = false;
  connectionConnParamUpdateIdentifier_ = 1U;
  connectionRole_ = BleConnectionRole::kPeripheral;
  connectionRequestedAttMtu_ =
      preferredAttMtuForRole(BleConnectionRole::kPeripheral);
  connectionAttMtu_ = kBleDefaultAttMtu;
  connectionCentralLinkSetupNotBeforeMs_ = millis() + 1000UL;
  connectionLastTxLlid_ = 0x01U;
  connectionLastTxLength_ = 0U;
  connectionLastTxSn_ = 0U;
  connectionLastTxPlainLlid_ = 0x01U;
  connectionLastTxPlainLength_ = 0U;
  memset(connectionLastTxPlainPayload_, 0, sizeof(connectionLastTxPlainPayload_));
  connectionPendingTxLlid_ = 0x01U;
  connectionPendingTxLength_ = 0U;
  connectionPendingTxValid_ = false;
  memset(connectionPendingTxPayload_, 0, sizeof(connectionPendingTxPayload_));
  connectionUpdatePending_ = false;
  connectionUpdateInstant_ = 0U;
  connectionPendingWinSize_ = 0U;
  connectionPendingWinOffset_ = 0U;
  connectionPendingIntervalUnits_ = 0U;
  connectionPendingLatency_ = 0U;
  connectionPendingTimeoutUnits_ = 0U;
  connectionCentralConnParamIndPending_ = false;
  connectionCentralConnParamIntervalUnits_ = 0U;
  connectionCentralConnParamLatency_ = 0U;
  connectionCentralConnParamTimeoutUnits_ = 0U;
  connectionChannelMapPending_ = false;
  connectionChannelMapInstant_ = 0U;
  memset(connectionPendingChannelMap_, 0, sizeof(connectionPendingChannelMap_));
  connectionPendingChannelCount_ = 0U;
  connectionServiceChangedIndicationsEnabled_ = false;
  // Do not emit Service Changed on every connection. The current core does not
  // track a database-change epoch, and spurious indications force needless
  // rediscovery right in the window where Android apps are trying to enable
  // NUS notifications.
  connectionServiceChangedIndicationPending_ = false;
  connectionServiceChangedIndicationAwaitingConfirm_ = false;
  connectionBatteryNotificationsEnabled_ = false;
  connectionBatteryNotificationPending_ = false;
  connectionSmpSecurityRequestPending_ = false;
  clearPreparedWriteState();
  connectionServiceBusy_ = false;
  clearQueuedConnectionEvents();
  clearCustomGattConnectionState();
  clearConnectionSecurityState();
  smpPairingFailureReasonPending_ = false;
  smpLastPairingFailureReason_ = 0U;
  smpPrefetchedLocalRandomValid_ = false;
  memset(smpPrefetchedLocalRandom_, 0, sizeof(smpPrefetchedLocalRandom_));
  connectionPrefetchedEncMaterialValid_ = false;
  memset(connectionPrefetchedSkds_, 0, sizeof(connectionPrefetchedSkds_));
  memset(connectionPrefetchedIvs_, 0, sizeof(connectionPrefetchedIvs_));
  if (primeBondForCurrentPeer()) {
    emitBleTrace("BOND_PRIMED");
  }
  memset(connectionTxPayload_, 0, sizeof(connectionTxPayload_));

  radio_->BASE0 = bleAccessAddressBase(connectionAccessAddress_);
  radio_->PREFIX0 = (radio_->PREFIX0 & ~RADIO_PREFIX0_AP0_Msk) |
                    ((bleAccessAddressPrefix(connectionAccessAddress_)
                      << RADIO_PREFIX0_AP0_Pos) &
                     RADIO_PREFIX0_AP0_Msk);
  radio_->CRCINIT = (connectionCrcInit_ & RADIO_CRCINIT_CRCINIT_Msk);
  radio_->TXADDRESS =
      (0UL << RADIO_TXADDRESS_TXADDRESS_Pos) & RADIO_TXADDRESS_TXADDRESS_Msk;
  radio_->RXADDRESSES =
      (RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos) &
      RADIO_RXADDRESSES_ADDR0_Msk;

  const uint32_t nowUs =
      (connectIndEndUs != 0U) ? connectIndEndUs : bleTimingUs();
  // The first peripheral data event starts 1.25 ms after CONNECT_IND end plus
  // the transmit window offset. Starting one interval-unit early can leave RX
  // just late enough to miss the preamble/address on strict centrals.
  const uint32_t offsetUs =
      1250UL + (static_cast<uint32_t>(winOffset) * 1250UL);
  connectionNextEventUs_ = nowUs + ((offsetUs > 0U) ? offsetUs : kBleConnFirstEventFallbackUs);

  quiesceBackgroundAdvertisingForConnectionHandoff();
  connected_ = true;
  endUnconnectedRadioActivity();
  emitBleTrace("CONNECTED");
  return true;
}

bool BleRadio::startCentralConnection(const uint8_t peerAddress[6],
                                      bool peerAddressRandom,
                                      uint32_t accessAddress, uint32_t crcInit,
                                      uint16_t intervalUnits,
                                      uint16_t supervisionTimeoutUnits,
                                      uint8_t hopIncrement, bool useChSel2,
                                      uint32_t connectIndEndUs) {
  if (!initialized_ || radio_ == nullptr || peerAddress == nullptr) {
    return false;
  }
  if (intervalUnits < 6U || intervalUnits > 3200U ||
      supervisionTimeoutUnits < 10U ||
      hopIncrement < 5U || hopIncrement > 16U ||
      accessAddress == kBleAccessAddress) {
    return false;
  }

  memcpy(connectionPeerAddress_, peerAddress, sizeof(connectionPeerAddress_));
  connectionPeerAddressRandom_ = peerAddressRandom;
  connectionAccessAddress_ = accessAddress;
  connectionCrcInit_ = (crcInit & 0x00FFFFFFUL);
  connectionIntervalUnits_ = intervalUnits;
  connectionLatency_ = 0U;
  connectionTimeoutUnits_ = supervisionTimeoutUnits;
  memset(connectionChannelMap_, 0, sizeof(connectionChannelMap_));
  connectionChannelMap_[0] = 0xFFU;
  connectionChannelMap_[1] = 0xFFU;
  connectionChannelMap_[2] = 0xFFU;
  connectionChannelMap_[3] = 0xFFU;
  connectionChannelMap_[4] = 0x1FU;
  connectionChannelCount_ = 37U;
  connectionHop_ = hopIncrement;
  connectionUseChSel2_ = useChSel2;
  connectionChannelId_ = static_cast<uint16_t>(
      (connectionAccessAddress_ & 0xFFFFU) ^
      ((connectionAccessAddress_ >> 16U) & 0xFFFFU));
  connectionSca_ = 3U;
  connectionChanUse_ = 0U;
  connectionExpectedRxSn_ = 0U;
  connectionTxSn_ = 0U;
  connectionTxHistoryValid_ = false;
  connectionFreshTxAllowed_ = true;
  connectionEventCounter_ = 0U;
  connectionMissedEventCount_ = 0U;
  connectionFirstEventListenUs_ = 0U;
  connectionSyncAttemptsRemaining_ = 0U;
  connectionCurrentTxPhy_ = kBlePhy1M;
  connectionCurrentRxPhy_ = kBlePhy1M;
  connectionPreferredTxPhyMask_ =
      static_cast<uint8_t>(kBlePhy1M | kBlePhy2M | kBlePhyCoded);
  connectionPreferredRxPhyMask_ =
      static_cast<uint8_t>(kBlePhy1M | kBlePhy2M | kBlePhyCoded);
  connectionPhyRequestPending_ = false;
  connectionPhyRequestInFlight_ = false;
  connectionPhyUpdatePending_ = false;
  connectionPhyUpdateInstant_ = 0U;
  connectionPendingTxPhy_ = kBlePhy1M;
  connectionPendingRxPhy_ = kBlePhy1M;
  connectionMaxTxPayloadLength_ = kBleDefaultDataPduMaxPayload;
  connectionMaxRxPayloadLength_ = kBleDefaultDataPduMaxPayload;
  connectionDataLengthUpdatePending_ = false;
  connectionDataLengthUpdateInFlight_ = false;
  connectionDataLengthUpdateComplete_ = false;
  connectionAttMtuExchangePending_ = false;
  connectionAttMtuExchangeInFlight_ = false;
  connectionAttMtuExchangeComplete_ = false;
  connectionConnParamUpdatePending_ = false;
  connectionConnParamUpdateInFlight_ = false;
  connectionConnParamUpdateIdentifier_ = 1U;
  connectionRole_ = BleConnectionRole::kCentral;
  connectionRequestedAttMtu_ =
      preferredAttMtuForRole(BleConnectionRole::kCentral);
  connectionAttMtu_ = kBleDefaultAttMtu;
  connectionCentralLinkSetupNotBeforeMs_ = millis() + 1000UL;
  connectionLastTxLlid_ = 0x01U;
  connectionLastTxLength_ = 0U;
  connectionLastTxPlainLlid_ = 0x01U;
  connectionLastTxPlainLength_ = 0U;
  memset(connectionLastTxPlainPayload_, 0, sizeof(connectionLastTxPlainPayload_));
  connectionPendingTxLlid_ = 0x01U;
  connectionPendingTxLength_ = 0U;
  connectionPendingTxValid_ = false;
  memset(connectionPendingTxPayload_, 0, sizeof(connectionPendingTxPayload_));
  connectionUpdatePending_ = false;
  connectionUpdateInstant_ = 0U;
  connectionPendingWinSize_ = 0U;
  connectionPendingWinOffset_ = 0U;
  connectionPendingIntervalUnits_ = 0U;
  connectionPendingLatency_ = 0U;
  connectionPendingTimeoutUnits_ = 0U;
  connectionCentralConnParamIndPending_ = false;
  connectionCentralConnParamIntervalUnits_ = 0U;
  connectionCentralConnParamLatency_ = 0U;
  connectionCentralConnParamTimeoutUnits_ = 0U;
  connectionChannelMapPending_ = false;
  connectionChannelMapInstant_ = 0U;
  memset(connectionPendingChannelMap_, 0, sizeof(connectionPendingChannelMap_));
  connectionPendingChannelCount_ = 0U;
  connectionServiceChangedIndicationsEnabled_ = false;
  connectionServiceChangedIndicationPending_ = false;
  connectionServiceChangedIndicationAwaitingConfirm_ = false;
  connectionBatteryNotificationsEnabled_ = false;
  connectionBatteryNotificationPending_ = false;
  clearPreparedWriteState();
  connectionServiceBusy_ = false;
  clearQueuedConnectionEvents();
  clearCustomGattConnectionState();
  clearConnectionSecurityState();
  smpPairingFailureReasonPending_ = false;
  smpLastPairingFailureReason_ = 0U;
  smpPrefetchedLocalRandomValid_ = false;
  memset(smpPrefetchedLocalRandom_, 0, sizeof(smpPrefetchedLocalRandom_));
  connectionPrefetchedEncMaterialValid_ = false;
  memset(connectionPrefetchedSkds_, 0, sizeof(connectionPrefetchedSkds_));
  memset(connectionPrefetchedIvs_, 0, sizeof(connectionPrefetchedIvs_));
  if (primeBondForCurrentPeer()) {
    emitBleTrace("BOND_PRIMED");
  }
  memset(connectionTxPayload_, 0, sizeof(connectionTxPayload_));

  radio_->BASE0 = bleAccessAddressBase(connectionAccessAddress_);
  radio_->PREFIX0 = (radio_->PREFIX0 & ~RADIO_PREFIX0_AP0_Msk) |
                    ((bleAccessAddressPrefix(connectionAccessAddress_)
                      << RADIO_PREFIX0_AP0_Pos) &
                     RADIO_PREFIX0_AP0_Msk);
  radio_->CRCINIT = (connectionCrcInit_ & RADIO_CRCINIT_CRCINIT_Msk);
  radio_->TXADDRESS =
      (0UL << RADIO_TXADDRESS_TXADDRESS_Pos) & RADIO_TXADDRESS_TXADDRESS_Msk;
  radio_->RXADDRESSES =
      (RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos) &
      RADIO_RXADDRESSES_ADDR0_Msk;

  const uint32_t nowUs =
      (connectIndEndUs != 0U) ? connectIndEndUs : bleTimingUs();
  connectionNextEventUs_ = nowUs + 1250UL;
  quiesceBackgroundAdvertisingForConnectionHandoff();
  connected_ = true;
  endUnconnectedRadioActivity();
  maybeQueueCentralLinkSetupRequest();
  emitBleTrace("CONNECTED_CENTRAL");
  return true;
}
