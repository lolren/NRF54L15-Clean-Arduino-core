#include <Arduino.h>
#include <string.h>

#include "nrf54l15_hal.h"
#include "zigbee_commissioning.h"
#include "zigbee_persistence.h"
#include "zigbee_security.h"
#include "zigbee_stack.h"

#if defined(NRF54L15_CLEAN_ZIGBEE_ENABLED) && (NRF54L15_CLEAN_ZIGBEE_ENABLED == 0)
#error "Enable Tools > Zigbee Support to build ZigbeeSleepyOnOffButton."
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_CHANNEL
#define NRF54L15_CLEAN_ZIGBEE_CHANNEL 15
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PAN_ID
#define NRF54L15_CLEAN_ZIGBEE_PAN_ID 0x1234
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK
#define NRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK 0x07FFF800UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK
#define NRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK 0U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ACTIVE_SCAN_WINDOW_MS
#define NRF54L15_CLEAN_ZIGBEE_ACTIVE_SCAN_WINDOW_MS 400UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_RESPONSE_TIMEOUT_MS
#define NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_RESPONSE_TIMEOUT_MS 4000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_LISTEN_MS
#define NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_LISTEN_MS 120UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_RETRY_DELAY_MS
#define NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_RETRY_DELAY_MS 40UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PARENT_POLL_FOLLOWUP_LISTEN_US
#define NRF54L15_CLEAN_ZIGBEE_PARENT_POLL_FOLLOWUP_LISTEN_US 80000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_COORDINATOR_REALIGNMENT_TIMEOUT_MS
#define NRF54L15_CLEAN_ZIGBEE_COORDINATOR_REALIGNMENT_TIMEOUT_MS 400UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_NWK_REJOIN_RESPONSE_TIMEOUT_MS
#define NRF54L15_CLEAN_ZIGBEE_NWK_REJOIN_RESPONSE_TIMEOUT_MS 1500UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE
#define NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE 1
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY
#define NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY 1
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY
#define NRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY 1
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS
#define NRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS 0
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE
#define NRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE 0ULL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PREFERRED_EXTENDED_PAN_ID
#define NRF54L15_CLEAN_ZIGBEE_PREFERRED_EXTENDED_PAN_ID 0ULL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_COORDINATOR_SHORT
#define NRF54L15_CLEAN_ZIGBEE_COORDINATOR_SHORT 0x0000U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_LOCAL_SHORT
#define NRF54L15_CLEAN_ZIGBEE_LOCAL_SHORT 0x7E11U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_LOCAL_IEEE
#define NRF54L15_CLEAN_ZIGBEE_LOCAL_IEEE 0ULL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_INSTALL_CODE_BYTES
#define NRF54L15_CLEAN_ZIGBEE_INSTALL_CODE_BYTES                                  \
  0x10U, 0xACU, 0x51U, 0x01U, 0x24U, 0x4BU, 0x00U, 0x57U, 0x68U, 0x79U, 0x8AU, \
      0x9BU, 0xACU, 0xBDU, 0xCEU, 0xDFU, 0x1FU, 0x7FU
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BASIC_MANUFACTURER_NAME
#define NRF54L15_CLEAN_ZIGBEE_BASIC_MANUFACTURER_NAME "CleanCore"
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BASIC_MODEL_IDENTIFIER
#define NRF54L15_CLEAN_ZIGBEE_BASIC_MODEL_IDENTIFIER "X54-BUTTON-REMOTE"
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BASIC_SW_BUILD_ID
#define NRF54L15_CLEAN_ZIGBEE_BASIC_SW_BUILD_ID "0.3.8"
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BUTTON_PAIR_HOLD_MS
#define NRF54L15_CLEAN_ZIGBEE_BUTTON_PAIR_HOLD_MS 3000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BUTTON_DEBOUNCE_MS
#define NRF54L15_CLEAN_ZIGBEE_BUTTON_DEBOUNCE_MS 35UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_PAIRING_WINDOW_MS
#define NRF54L15_CLEAN_ZIGBEE_PAIRING_WINDOW_MS 90000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MIN_S
#define NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MIN_S 5U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MAX_S
#define NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MAX_S 21600U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BATTERY_VOLTAGE_REPORT_CHANGE
#define NRF54L15_CLEAN_ZIGBEE_BATTERY_VOLTAGE_REPORT_CHANGE 1U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BATTERY_PERCENT_REPORT_CHANGE
#define NRF54L15_CLEAN_ZIGBEE_BATTERY_PERCENT_REPORT_CHANGE 2U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_MIN_S
#define NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_MIN_S 0U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_MAX_S
#define NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_MAX_S 3600U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_CHANGE
#define NRF54L15_CLEAN_ZIGBEE_ONOFF_REPORT_CHANGE 1U
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS
#define NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS 6000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS
#define NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS 180000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BOOT_AWAKE_WINDOW_MS
#define NRF54L15_CLEAN_ZIGBEE_BOOT_AWAKE_WINDOW_MS 12000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_STATUS_INTERVAL_MS
#define NRF54L15_CLEAN_ZIGBEE_STATUS_INTERVAL_MS 1000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_BOOT_REPORT_DELAY_MS
#define NRF54L15_CLEAN_ZIGBEE_BOOT_REPORT_DELAY_MS 1200UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_INTERVIEW_GRACE_MS
#define NRF54L15_CLEAN_ZIGBEE_INTERVIEW_GRACE_MS 45000UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_SYSTEM_OFF_ENTRY_DELAY_MS
#define NRF54L15_CLEAN_ZIGBEE_SYSTEM_OFF_ENTRY_DELAY_MS 350UL
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_DEBUG_KEEP_AWAKE
#define NRF54L15_CLEAN_ZIGBEE_DEBUG_KEEP_AWAKE 1
#endif

#ifndef NRF54L15_CLEAN_ZIGBEE_DIRECT_LOOKUP_TIMEOUT_MS
#define NRF54L15_CLEAN_ZIGBEE_DIRECT_LOOKUP_TIMEOUT_MS 1000UL
#endif

using namespace xiao_nrf54l15;

namespace {

static PowerManager g_powerManager;
static ZigbeeRadio g_radio;
static ZigbeeHomeAutomationDevice g_device;
static ZigbeePersistentStateStore g_store;

static uint8_t g_macSequence = 1U;
static ZigbeeEndDeviceCommonState g_network{};
static uint8_t& g_nwkSequence = g_network.nwkSequence;
static uint32_t& g_nwkSecurityFrameCounter = g_network.nwkSecurityFrameCounter;
static uint32_t& g_lastInboundSecurityFrameCounter =
    g_network.incomingNwkFrameCounter;
static uint32_t& g_lastInboundApsFrameCounter =
    g_network.incomingApsFrameCounter;
static uint8_t& g_apsCounter = g_network.apsCounter;
static uint8_t g_zdoSequence = 1U;
static uint32_t& g_lastJoinAttemptMs = g_network.lastJoinAttemptMs;
static uint32_t g_lastStatusMs = 0U;
static uint32_t g_lastPollMs = 0U;
static uint32_t g_bootMs = 0U;
static uint32_t g_awakeUntilMs = 0U;
static uint32_t g_pairingWindowUntilMs = 0U;
static bool g_pendingButtonAction = false;
static bool g_wokeFromButton = false;
static uint8_t (&g_activeNetworkKey)[16] = g_network.activeNetworkKey;
static uint8_t& g_activeNetworkKeySequence = g_network.activeNetworkKeySequence;
static uint8_t (&g_alternateNetworkKey)[16] = g_network.alternateNetworkKey;
static uint8_t& g_alternateNetworkKeySequence =
    g_network.alternateNetworkKeySequence;
static bool& g_joined = g_network.joined;
static bool& g_rejoinPending = g_network.rejoinPending;
static bool& g_securityEnabled = g_network.securityEnabled;
static bool& g_haveActiveNetworkKey = g_network.haveActiveNetworkKey;
static bool& g_haveAlternateNetworkKey = g_network.haveAlternateNetworkKey;
static uint64_t& g_trustCenterIeee = g_network.trustCenterIeee;
static ZigbeePreconfiguredKeyMode& g_preconfiguredKeyMode =
    g_network.preconfiguredKeyMode;
static uint32_t& g_parentPollIntervalMs = g_network.parentPollIntervalMs;

static ZigbeePersistentState g_restoredState{};
static bool g_haveRestoredState = false;
static bool g_bootReportsPending = true;

static constexpr uint8_t kPreferredChannel =
    static_cast<uint8_t>(NRF54L15_CLEAN_ZIGBEE_CHANNEL);
static constexpr uint16_t kPreferredPanId =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_PAN_ID);
static constexpr uint16_t kCoordinatorShort =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_COORDINATOR_SHORT);
static constexpr uint16_t kButtonShort =
    static_cast<uint16_t>(NRF54L15_CLEAN_ZIGBEE_LOCAL_SHORT);
static constexpr uint8_t kLocalEndpoint = 1U;
static constexpr uint8_t kCoordinatorEndpoint = 1U;
static constexpr uint8_t kOnOffCommandOff = 0x00U;
static constexpr uint8_t kOnOffCommandOn = 0x01U;
static constexpr uint8_t kOnOffCommandToggle = 0x02U;
static constexpr uint8_t kSystemOffMagic = 0xB7U;
static constexpr uint64_t kConfiguredPreferredExtendedPanId =
    static_cast<uint64_t>(NRF54L15_CLEAN_ZIGBEE_PREFERRED_EXTENDED_PAN_ID);
static uint64_t g_localIeee =
    static_cast<uint64_t>(NRF54L15_CLEAN_ZIGBEE_LOCAL_IEEE);
static uint64_t& kIeeeAddress = g_localIeee;
static constexpr uint8_t kStateFlagJoined = 0x01U;
static constexpr uint8_t kStateFlagSecurityEnabled = 0x02U;
static const uint8_t kInstallCode[18] = {
    NRF54L15_CLEAN_ZIGBEE_INSTALL_CODE_BYTES};

static uint8_t& g_channel = g_network.channel;
static uint16_t& g_panId = g_network.panId;
static uint16_t& g_localShort = g_network.localShort;
static uint16_t& g_parentShort = g_network.parentShort;
static uint64_t& g_extendedPanId = g_network.extendedPanId;

struct ScanResult {
  bool valid = false;
  uint8_t channel = 0U;
  int8_t rssiDbm = -127;
  int16_t score = 0;
  ZigbeeMacBeaconView beacon{};
};

struct PendingApsAck {
  bool active = false;
  uint16_t destinationShort = 0U;
  uint8_t counter = 0U;
  uint16_t clusterId = 0U;
  uint16_t profileId = 0U;
  uint8_t destinationEndpoint = 0U;
  uint8_t sourceEndpoint = 0U;
  uint8_t retriesRemaining = 0U;
  uint8_t payloadLength = 0U;
  uint8_t payload[96] = {0U};
  uint32_t deadlineMs = 0U;
};

struct RecentInboundAps {
  bool valid = false;
  uint16_t sourceShort = 0U;
  uint8_t counter = 0U;
  uint16_t clusterId = 0U;
  uint16_t profileId = 0U;
  uint8_t destinationEndpoint = 0U;
  uint8_t sourceEndpoint = 0U;
  uint8_t deliveryMode = 0U;
  uint32_t expiresMs = 0U;
};

static constexpr uint8_t kPendingApsAckSlots = 3U;
static PendingApsAck g_pendingApsAcks[kPendingApsAckSlots]{};
static RecentInboundAps g_recentInboundAps{};
static constexpr uint32_t kApsAckTimeoutMs = 900U;
static constexpr uint32_t kRecentInboundApsWindowMs = 4000U;
static constexpr uint8_t kApsAckRetryLimit = 2U;
static constexpr uint32_t kParentRxAcknowledgeDelayUs = 3000U;
static constexpr uint32_t kParentRxResponseDelayUs = 25000U;

void extendAwakeWindow(uint32_t durationMs);
void clearPendingApsAck();
void clearPendingApsAckSlot(uint8_t slot);
void clearRecentInboundAps();
bool hasPendingApsAcks();
bool inInterviewGracePeriod(uint32_t nowMs);
void clearJoinState(bool clearStore);
bool triggerButtonAction(uint8_t commandId);
bool sendButtonOnOffCommand(uint8_t commandId);

constexpr bool lowPowerCycleEnabled() {
  return static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_SYSTEM_OFF_ENTRY_DELAY_MS) >
         0UL;
}

ZigbeeCommissioningPolicy commissioningPolicy() {
  ZigbeeCommissioningPolicy policy{};
  policy.primaryChannelMask =
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK);
  policy.secondaryChannelMask =
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK);
  policy.activeScanWindowMs =
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_ACTIVE_SCAN_WINDOW_MS);
  policy.associationResponseTimeoutMs = static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_RESPONSE_TIMEOUT_MS);
  policy.associationPollListenMs =
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_LISTEN_MS);
  policy.associationPollRetryDelayMs = static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_RETRY_DELAY_MS);
  policy.coordinatorRealignmentTimeoutMs = static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_COORDINATOR_REALIGNMENT_TIMEOUT_MS);
  policy.nwkRejoinResponseTimeoutMs = static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_NWK_REJOIN_RESPONSE_TIMEOUT_MS);
  policy.initialPollIntervalMs = 100UL;
  policy.preferredPanId = kPreferredPanId;
  policy.preferredExtendedPanId =
      (g_extendedPanId != 0U) ? g_extendedPanId
                              : kConfiguredPreferredExtendedPanId;
  policy.pinnedTrustCenterIeee =
      static_cast<uint64_t>(NRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE);
  policy.allowWellKnownKey = (NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY != 0);
  policy.allowInstallCodeKey = (NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE != 0);
  policy.installCodeOnly =
      (NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE != 0) &&
      (NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY == 0);
  policy.requireEncryptedTransportKey =
      (NRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY != 0);
  policy.requireEncryptedUpdateDevice =
      (NRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS == 0);
  policy.requireEncryptedSwitchKey =
      (NRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS == 0);
  policy.requirePanCoordinator = true;
  policy.requireUniqueTrustCenterForRejoin = true;
  return policy;
}

void refreshCommissioningState() {
  g_network.policy = commissioningPolicy();
  g_network.preferredChannel = kPreferredChannel;
  g_network.preferredPanId = kPreferredPanId;
  g_network.defaultShort = kButtonShort;
  g_network.coordinatorShort = kCoordinatorShort;
}

bool loadInstallCodeLinkKey(uint8_t outKey[16]) {
#if NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE
  return ZigbeeSecurity::deriveInstallCodeLinkKey(kInstallCode,
                                                  sizeof(kInstallCode), outKey);
#else
  (void)outKey;
  return false;
#endif
}

uint64_t expectedTrustCenterIeee() {
  refreshCommissioningState();
  return ZigbeeCommissioning::expectedTrustCenterIeee(g_network);
}

void applyDefaultReporting() {
  g_device.configureReporting(kZigbeeClusterPowerConfiguration, 0x0020U,
                              ZigbeeZclDataType::kUint8,
                              static_cast<uint16_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MIN_S),
                              static_cast<uint16_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MAX_S),
                              static_cast<uint32_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_VOLTAGE_REPORT_CHANGE));
  g_device.configureReporting(kZigbeeClusterPowerConfiguration, 0x0021U,
                              ZigbeeZclDataType::kUint8,
                              static_cast<uint16_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MIN_S),
                              static_cast<uint16_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_REPORT_MAX_S),
                              static_cast<uint32_t>(
                                  NRF54L15_CLEAN_ZIGBEE_BATTERY_PERCENT_REPORT_CHANGE));
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

void sampleSensors() {
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

void applyJoinLed() {
  const uint32_t nowMs = millis();
  g_device.updateIdentify(nowMs);
  const bool pairingBlink =
      !g_joined && static_cast<int32_t>(g_pairingWindowUntilMs - nowMs) > 0;
  const bool ledOn = g_device.identifying()
                         ? identifyIndicatorOn(nowMs, g_device.identifyEffect())
                         : (pairingBlink ? (((nowMs / 120U) & 0x01UL) == 0U)
                                         : g_joined);
  Gpio::write(kPinUserLed, !ledOn);
}

void clearActiveNetworkKey() {
  memset(g_activeNetworkKey, 0, sizeof(g_activeNetworkKey));
  g_activeNetworkKeySequence = 0U;
  g_haveActiveNetworkKey = false;
}

uint8_t readGpregret0() {
  return static_cast<uint8_t>(NRF_POWER->GPREGRET[0] &
                              POWER_GPREGRET_GPREGRET_Msk);
}

void writeGpregret0(uint8_t value) {
  NRF_POWER->GPREGRET[0] = static_cast<uint32_t>(value);
}

bool buttonPressed() {
  bool high = true;
  if (!Gpio::read(kPinUserButton, &high)) {
    return false;
  }
  return !high;
}

void configureButtonSenseLowWake() {
  (void)Gpio::configure(kPinUserButton, GpioDirection::kInput,
                        GpioPull::kDisabled);
  if (kPinUserButton.port != 0U) {
    return;
  }

  uint32_t cnf = NRF_P0->PIN_CNF[kPinUserButton.pin];
  cnf &= ~GPIO_PIN_CNF_SENSE_Msk;
  cnf |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
  NRF_P0->PIN_CNF[kPinUserButton.pin] = cnf;
}

uint32_t measureButtonHoldMs(uint32_t thresholdMs) {
  if (!buttonPressed()) {
    return 0U;
  }

  const uint32_t startMs = millis();
  while (buttonPressed()) {
    const uint32_t heldMs = millis() - startMs;
    if (heldMs >= thresholdMs) {
      return heldMs;
    }
    delay(10);
  }
  return millis() - startMs;
}

void extendAwakeWindow(uint32_t durationMs) {
  const uint32_t targetMs = millis() + durationMs;
  if (static_cast<int32_t>(targetMs - g_awakeUntilMs) > 0) {
    g_awakeUntilMs = targetMs;
  }
}

void beginPairingWindow(bool factoryResetFirst) {
  if (factoryResetFirst) {
    clearJoinState(true);
  }
  g_pairingWindowUntilMs = millis() +
                           static_cast<uint32_t>(
                               NRF54L15_CLEAN_ZIGBEE_PAIRING_WINDOW_MS);
  extendAwakeWindow(static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_PAIRING_WINDOW_MS));
  if (!g_joined && !g_rejoinPending) {
    ZigbeeCommissioning::requestNetworkSteering(&g_network);
  }
}

void enterSystemOffIfPossible() {
#if NRF54L15_CLEAN_ZIGBEE_DEBUG_KEEP_AWAKE
  return;
#endif
  if (!lowPowerCycleEnabled() || hasPendingApsAcks() || g_device.identifying()) {
    return;
  }
  if (static_cast<int32_t>(g_awakeUntilMs - millis()) > 0) {
    return;
  }
  if (!g_joined && static_cast<int32_t>(g_pairingWindowUntilMs - millis()) > 0) {
    return;
  }

  Serial.print("system_off wait_button\r\n");
  Serial.flush();
  delay(20);
  configureButtonSenseLowWake();
  writeGpregret0(kSystemOffMagic);
  g_powerManager.systemOffNoRetention();
}

void configureDeviceForCurrentNetwork() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = NRF54L15_CLEAN_ZIGBEE_BASIC_MANUFACTURER_NAME;
  basic.modelIdentifier = NRF54L15_CLEAN_ZIGBEE_BASIC_MODEL_IDENTIFIER;
  basic.swBuildId = NRF54L15_CLEAN_ZIGBEE_BASIC_SW_BUILD_ID;
  basic.powerSource = 0x03U;
  g_device.configureOnOffLightSwitch(kLocalEndpoint, kIeeeAddress, g_localShort,
                                     g_panId, basic, 0x0000U);
  applyReportingState();
  applyBindingState();
  sampleSensors();
}

bool persistState() {
  refreshCommissioningState();
  ZigbeePersistentState state{};
  ZigbeeCommissioning::populatePersistentState(
      g_network, kIeeeAddress, ZigbeeLogicalType::kEndDevice,
      g_device.config().manufacturerCode, &state);

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
  ZigbeeCommissioning::initializeEndDeviceState(&g_network,
                                                commissioningPolicy(),
                                                kPreferredChannel,
                                                kPreferredPanId, kButtonShort,
                                                kCoordinatorShort);

  ZigbeePersistentState state{};
  if (g_store.load(&state) && state.ieeeAddress == kIeeeAddress) {
    g_restoredState = state;
    g_haveRestoredState = true;
    ZigbeeCommissioning::restoreEndDeviceState(&g_network, state, kIeeeAddress);
  }

  configureDeviceForCurrentNetwork();
  applyJoinLed();
}

void clearJoinState(bool clearStore) {
  refreshCommissioningState();
  ZigbeeCommissioning::clearEndDeviceState(&g_network, clearStore);
  clearPendingApsAck();
  clearRecentInboundAps();
  g_pairingWindowUntilMs = 0U;
  g_pendingButtonAction = false;
  if (clearStore) {
    memset(&g_restoredState, 0, sizeof(g_restoredState));
    g_haveRestoredState = false;
  }
  configureDeviceForCurrentNetwork();
  applyJoinLed();
  if (clearStore) {
    g_store.clear();
  } else {
    persistState();
  }
}

ZigbeeCommissioningStartRequest requestCommissioningStart() {
  refreshCommissioningState();
  return ZigbeeCommissioning::requestRejoinOrSteering(&g_network);
}

void handleAcceptedLeaveRequest(uint8_t leaveFlags) {
  const ZigbeeAcceptedLeaveDisposition disposition =
      ZigbeeCommissioning::applyAcceptedLeaveRequest(&g_network, leaveFlags);

  if (disposition == ZigbeeAcceptedLeaveDisposition::kClearState) {
    Serial.print("mgmt_leave accepted clear\r\n");
    clearJoinState(true);
    return;
  }

  clearPendingApsAck();
  clearRecentInboundAps();
  if (disposition == ZigbeeAcceptedLeaveDisposition::kPersistRejoin) {
    persistState();
    Serial.print("mgmt_leave accepted rejoin\r\n");
    return;
  }

  Serial.print("mgmt_leave accepted rejoin_unavailable failure=");
  Serial.print(ZigbeeCommissioning::failureName(g_network.lastFailure));
  Serial.print("\r\n");
  clearJoinState(true);
}

void clearPendingApsAck() {
  memset(g_pendingApsAcks, 0, sizeof(g_pendingApsAcks));
}

void clearPendingApsAckSlot(uint8_t slot) {
  if (slot >= kPendingApsAckSlots) {
    return;
  }
  memset(&g_pendingApsAcks[slot], 0, sizeof(g_pendingApsAcks[slot]));
}

void clearRecentInboundAps() {
  memset(&g_recentInboundAps, 0, sizeof(g_recentInboundAps));
}

bool hasPendingApsAcks() {
  for (uint8_t i = 0U; i < kPendingApsAckSlots; ++i) {
    if (g_pendingApsAcks[i].active) {
      return true;
    }
  }
  return false;
}

bool matchesPendingApsAck(const PendingApsAck& pending,
                          const ZigbeeApsAcknowledgementFrame& ack,
                          uint16_t sourceShort) {
  return pending.active && ack.valid && !ack.ackFormatCommand &&
         sourceShort == pending.destinationShort &&
         ack.counter == pending.counter &&
         ack.clusterId == pending.clusterId &&
         ack.profileId == pending.profileId &&
         ack.destinationEndpoint == pending.sourceEndpoint &&
         ack.sourceEndpoint == pending.destinationEndpoint;
}

bool findPendingApsAckSlot(const ZigbeeApsAcknowledgementFrame& ack,
                           uint16_t sourceShort, uint8_t* outSlot) {
  if (outSlot == nullptr) {
    return false;
  }

  for (uint8_t i = 0U; i < kPendingApsAckSlots; ++i) {
    if (matchesPendingApsAck(g_pendingApsAcks[i], ack, sourceShort)) {
      *outSlot = i;
      return true;
    }
  }
  return false;
}

void rememberPendingApsAck(uint16_t destinationShort,
                           const ZigbeeApsDataFrame& aps,
                           const uint8_t* payload, uint8_t payloadLength) {
  uint8_t slot = kPendingApsAckSlots;
  if (aps.deliveryMode != kZigbeeApsDeliveryUnicast || !aps.ackRequested ||
      payloadLength > sizeof(g_pendingApsAcks[0].payload) ||
      (payloadLength > 0U && payload == nullptr)) {
    return;
  }
  const uint32_t nowMs = millis();
  uint8_t oldestActiveSlot = 0U;
  bool haveOldestActiveSlot = false;
  for (uint8_t i = 0U; i < kPendingApsAckSlots; ++i) {
    PendingApsAck& pending = g_pendingApsAcks[i];
    if (pending.active &&
        pending.destinationShort == destinationShort &&
        pending.counter == aps.counter &&
        pending.clusterId == aps.clusterId &&
        pending.profileId == aps.profileId &&
        pending.destinationEndpoint == aps.destinationEndpoint &&
        pending.sourceEndpoint == aps.sourceEndpoint) {
      slot = i;
      break;
    }
    if (!pending.active ||
        static_cast<int32_t>(nowMs - pending.deadlineMs) >= 0) {
      slot = i;
      break;
    }
    if (!haveOldestActiveSlot ||
        static_cast<int32_t>(pending.deadlineMs -
                             g_pendingApsAcks[oldestActiveSlot].deadlineMs) < 0) {
      oldestActiveSlot = i;
      haveOldestActiveSlot = true;
    }
  }
  if (slot >= kPendingApsAckSlots) {
    slot = oldestActiveSlot;
  }

  PendingApsAck& pending = g_pendingApsAcks[slot];
  memset(&pending, 0, sizeof(pending));
  pending.active = true;
  pending.destinationShort = destinationShort;
  pending.counter = aps.counter;
  pending.clusterId = aps.clusterId;
  pending.profileId = aps.profileId;
  pending.destinationEndpoint = aps.destinationEndpoint;
  pending.sourceEndpoint = aps.sourceEndpoint;
  pending.retriesRemaining = kApsAckRetryLimit;
  pending.payloadLength = payloadLength;
  if (payloadLength > 0U) {
    memcpy(pending.payload, payload, payloadLength);
  }
  pending.deadlineMs = nowMs + kApsAckTimeoutMs;
}

void rememberRecentInboundAps(uint16_t sourceShort,
                              const ZigbeeApsDataFrame& aps) {
  g_recentInboundAps.valid = true;
  g_recentInboundAps.sourceShort = sourceShort;
  g_recentInboundAps.counter = aps.counter;
  g_recentInboundAps.clusterId = aps.clusterId;
  g_recentInboundAps.profileId = aps.profileId;
  g_recentInboundAps.destinationEndpoint = aps.destinationEndpoint;
  g_recentInboundAps.sourceEndpoint = aps.sourceEndpoint;
  g_recentInboundAps.deliveryMode = aps.deliveryMode;
  g_recentInboundAps.expiresMs = millis() + kRecentInboundApsWindowMs;
}

bool isRecentInboundApsDuplicate(uint16_t sourceShort,
                                 const ZigbeeApsDataFrame& aps,
                                 uint32_t nowMs) {
  if (!g_recentInboundAps.valid ||
      static_cast<int32_t>(nowMs - g_recentInboundAps.expiresMs) >= 0) {
    clearRecentInboundAps();
    return false;
  }
  return g_recentInboundAps.sourceShort == sourceShort &&
         g_recentInboundAps.counter == aps.counter &&
         g_recentInboundAps.clusterId == aps.clusterId &&
         g_recentInboundAps.profileId == aps.profileId &&
         g_recentInboundAps.destinationEndpoint == aps.destinationEndpoint &&
         g_recentInboundAps.sourceEndpoint == aps.sourceEndpoint &&
         g_recentInboundAps.deliveryMode == aps.deliveryMode;
}

bool sendNwkCommand(uint16_t destinationShort, const uint8_t* payload,
                    uint8_t payloadLength) {
  if (!g_joined || payload == nullptr || payloadLength == 0U) {
    return false;
  }
  const bool useSecurity = g_securityEnabled && g_haveActiveNetworkKey;

  ZigbeeNetworkFrame nwk{};
  nwk.frameType = ZigbeeNwkFrameType::kCommand;
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
                                              payload, payloadLength, nwkFrame,
                                              &nwkLength)) {
      return false;
    }
  } else if (!ZigbeeCodec::buildNwkFrame(nwk, payload, payloadLength, nwkFrame,
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

bool sendApsFrameExtendedWithCounter(
    uint16_t destinationShort, uint8_t deliveryMode,
    uint16_t destinationGroup, uint8_t destinationEndpoint,
    uint16_t clusterId, uint16_t profileId, uint8_t sourceEndpoint,
    const uint8_t* payload, uint8_t payloadLength, uint8_t apsCounterValue,
    bool trackAck) {
  if (!g_joined) {
    return false;
  }
  const bool useSecurity = g_securityEnabled && g_haveActiveNetworkKey;

  ZigbeeApsDataFrame aps{};
  aps.frameType = ZigbeeApsFrameType::kData;
  aps.deliveryMode = deliveryMode;
  aps.ackRequested = trackAck && (deliveryMode == kZigbeeApsDeliveryUnicast);
  aps.destinationEndpoint = destinationEndpoint;
  aps.destinationGroup = destinationGroup;
  aps.clusterId = clusterId;
  aps.profileId = profileId;
  aps.sourceEndpoint = sourceEndpoint;
  aps.counter = apsCounterValue;

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

  const bool sent = g_radio.transmit(psdu, psduLength, false, 1200000UL);
  if (sent && trackAck && aps.deliveryMode == kZigbeeApsDeliveryUnicast) {
    rememberPendingApsAck(destinationShort, aps, payload, payloadLength);
  }
  return sent;
}

bool sendApsFrameWithCounter(uint16_t destinationShort,
                             uint8_t destinationEndpoint, uint16_t clusterId,
                             uint16_t profileId, uint8_t sourceEndpoint,
                             const uint8_t* payload, uint8_t payloadLength,
                             uint8_t apsCounterValue, bool trackAck) {
  return sendApsFrameExtendedWithCounter(
      destinationShort, kZigbeeApsDeliveryUnicast, 0U, destinationEndpoint,
      clusterId, profileId, sourceEndpoint, payload, payloadLength,
      apsCounterValue, trackAck);
}

bool resendPendingApsFrame(uint8_t slot) {
  if (slot >= kPendingApsAckSlots || !g_pendingApsAcks[slot].active) {
    return false;
  }
  PendingApsAck& pending = g_pendingApsAcks[slot];
  const bool sent = sendApsFrameWithCounter(
      pending.destinationShort, pending.destinationEndpoint,
      pending.clusterId, pending.profileId, pending.sourceEndpoint,
      pending.payload, pending.payloadLength, pending.counter, false);
  if (sent) {
    pending.deadlineMs = millis() + kApsAckTimeoutMs;
  }
  return sent;
}

void maybeExpirePendingApsAck(uint32_t nowMs) {
  for (uint8_t i = 0U; i < kPendingApsAckSlots; ++i) {
    PendingApsAck& pending = g_pendingApsAcks[i];
    if (!pending.active ||
        static_cast<int32_t>(nowMs - pending.deadlineMs) < 0) {
      continue;
    }
    if (pending.retriesRemaining > 0U) {
      --pending.retriesRemaining;
      const bool resent = resendPendingApsFrame(i);
      Serial.print("aps_ack retry ctr=0x");
      Serial.print(pending.counter, HEX);
      Serial.print(" cluster=0x");
      Serial.print(pending.clusterId, HEX);
      Serial.print(" remaining=");
      Serial.print(pending.retriesRemaining);
      Serial.print(" sent=");
      Serial.print(resent ? "yes" : "no");
      Serial.print("\r\n");
      if (resent) {
        continue;
      }
    }

    Serial.print("aps_ack miss ctr=0x");
    Serial.print(pending.counter, HEX);
    Serial.print(" cluster=0x");
    Serial.print(pending.clusterId, HEX);
    Serial.print("\r\n");
    clearPendingApsAckSlot(i);
  }
}

bool sendApsAcknowledgement(uint16_t destinationShort,
                            const ZigbeeApsDataFrame& request) {
  if (!g_joined || request.deliveryMode != kZigbeeApsDeliveryUnicast ||
      !request.ackRequested) {
    return false;
  }
  const bool useSecurity = g_securityEnabled && g_haveActiveNetworkKey;

  uint8_t apsFrame[127] = {0U};
  uint8_t apsLength = 0U;
  if (!ZigbeeCodec::buildApsDataAcknowledgement(request, apsFrame,
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

bool sendApsFrame(uint16_t destinationShort, uint8_t destinationEndpoint,
                  uint16_t clusterId, uint16_t profileId,
                  uint8_t sourceEndpoint, const uint8_t* payload,
                  uint8_t payloadLength) {
  return sendApsFrameWithCounter(destinationShort, destinationEndpoint,
                                 clusterId, profileId, sourceEndpoint, payload,
                                 payloadLength, g_apsCounter++, true);
}

bool sendGroupApsFrame(uint16_t destinationShort, uint16_t destinationGroup,
                       uint16_t clusterId, uint16_t profileId,
                       uint8_t sourceEndpoint, const uint8_t* payload,
                       uint8_t payloadLength) {
  return sendApsFrameExtendedWithCounter(
      destinationShort, kZigbeeApsDeliveryGroup, destinationGroup, 0U,
      clusterId, profileId, sourceEndpoint, payload, payloadLength,
      g_apsCounter++, false);
}

bool sendEndDeviceTimeoutRequest() {
  refreshCommissioningState();
  if (!ZigbeeCommissioning::shouldRequestEndDeviceTimeout(g_network)) {
    return false;
  }
  ZigbeeCommissioning::recordEndDeviceTimeoutRequest(&g_network, millis());

  uint8_t command[8] = {0U};
  uint8_t commandLength = 0U;
  if (!ZigbeeCodec::buildNwkEndDeviceTimeoutRequestCommand(
          g_network.endDeviceTimeoutIndex, g_network.endDeviceConfiguration,
          command, &commandLength)) {
    return false;
  }

  return sendNwkCommand(g_parentShort, command, commandLength);
}

bool sendDeviceAnnounce() {
  ZigbeeCommissioning::recordDeviceAnnounceAttempt(&g_network, millis());
  uint8_t payload[127] = {0U};
  uint8_t payloadLength = 0U;
  if (!g_device.buildDeviceAnnounce(g_zdoSequence++, payload, &payloadLength)) {
    return false;
  }
  const bool sent = sendApsFrame(g_parentShort, 0U, kZigbeeZdoDeviceAnnounce,
                                 kZigbeeProfileZdo, 0U, payload,
                                 payloadLength);
  if (sent) {
    ZigbeeCommissioning::completeDeviceAnnounce(&g_network);
  }
  return sent;
}

bool sendAttributeReport(uint16_t clusterId) {
  uint8_t zclFrame[127] = {0U};
  uint8_t zclLength = 0U;
  if (!g_device.buildAttributeReport(clusterId, g_zdoSequence++, zclFrame,
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
          g_parentShort, destination.groupId, clusterId,
          kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame, zclLength);
    }
    if (destination.addressMode == ZigbeeBindingAddressMode::kExtended &&
        destination.endpoint != 0U) {
      return sendApsFrame(g_parentShort, destination.endpoint, clusterId,
                          kZigbeeProfileHomeAutomation, kLocalEndpoint,
                          zclFrame, zclLength);
    }
  }
  return sendApsFrame(g_parentShort, kCoordinatorEndpoint, clusterId,
                      kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                      zclLength);
}

bool sendButtonOnOffCommand(uint8_t commandId) {
  if (!g_joined) {
    return false;
  }

  uint8_t zclFrame[32] = {0U};
  uint8_t zclLength = 0U;
  ZigbeeZclFrame frame{};
  frame.frameType = ZigbeeZclFrameType::kClusterSpecific;
  frame.disableDefaultResponse = false;
  frame.transactionSequence = g_zdoSequence++;
  frame.commandId = commandId;
  if (!ZigbeeCodec::buildZclFrame(frame, nullptr, 0U, zclFrame, &zclLength)) {
    return false;
  }

  ZigbeeResolvedBindingDestination destination{};
  if (g_device.resolveBindingDestination(kLocalEndpoint, kZigbeeClusterOnOff,
                                         &destination)) {
    if (destination.addressMode == ZigbeeBindingAddressMode::kGroup &&
        destination.groupId != 0U) {
      return sendGroupApsFrame(g_parentShort, destination.groupId,
                               kZigbeeClusterOnOff,
                               kZigbeeProfileHomeAutomation, kLocalEndpoint,
                               zclFrame, zclLength);
    }
    if (destination.addressMode == ZigbeeBindingAddressMode::kExtended &&
        destination.endpoint != 0U) {
      return sendApsFrame(g_parentShort, destination.endpoint,
                          kZigbeeClusterOnOff, kZigbeeProfileHomeAutomation,
                          kLocalEndpoint, zclFrame, zclLength);
    }
  }

  return sendApsFrame(g_parentShort, kCoordinatorEndpoint, kZigbeeClusterOnOff,
                      kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                      zclLength);
}

bool sendDueAttributeReport(uint32_t nowMs, uint16_t* outClusterId) {
  if (outClusterId != nullptr) {
    *outClusterId = 0U;
  }

  uint8_t zclFrame[127] = {0U};
  uint8_t zclLength = 0U;
  uint16_t clusterId = 0U;
  if (!g_device.buildDueAttributeReport(nowMs, g_zdoSequence++, &clusterId,
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
          g_parentShort, destination.groupId, clusterId,
          kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame, zclLength);
    } else if (destination.addressMode ==
                   ZigbeeBindingAddressMode::kExtended &&
               destination.endpoint != 0U) {
      ok = sendApsFrame(g_parentShort, destination.endpoint, clusterId,
                        kZigbeeProfileHomeAutomation, kLocalEndpoint, zclFrame,
                        zclLength);
    }
  }
  if (!ok) {
    ok = sendApsFrame(g_parentShort, kCoordinatorEndpoint, clusterId,
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

bool triggerButtonAction(uint8_t commandId) {
  if (!g_joined) {
    return false;
  }

  bool targetOn = g_device.onOff();
  if (commandId == kOnOffCommandOn) {
    targetOn = true;
  } else if (commandId == kOnOffCommandOff) {
    targetOn = false;
  } else {
    targetOn = !targetOn;
  }

  (void)g_device.setOnOff(targetOn);
  sampleSensors();
  const bool commandOk = sendButtonOnOffCommand(commandId);
  const bool powerOk = sendAttributeReport(kZigbeeClusterPowerConfiguration);
  persistState();
  extendAwakeWindow(
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS));
  Serial.print("button_action cmd=");
  if (commandId == kOnOffCommandOn) {
    Serial.print("on");
  } else if (commandId == kOnOffCommandOff) {
    Serial.print("off");
  } else {
    Serial.print("toggle");
  }
  Serial.print(" local_onoff=");
  Serial.print(targetOn ? "on" : "off");
  Serial.print(" zcl_cmd=");
  Serial.print(commandOk ? "OK" : "FAIL");
  Serial.print(" power_report=");
  Serial.print(powerOk ? "OK" : "FAIL");
  Serial.print("\r\n");
  return commandOk || powerOk;
}

bool handleApsCommand(const uint8_t* frame, uint8_t length, uint16_t sourceShort,
                      uint64_t securedSourceIeee, bool nwkSecured) {
  refreshCommissioningState();

  uint8_t installCodeKey[16] = {0U};
  const bool haveInstallCodeKey = loadInstallCodeLinkKey(installCodeKey);
  ZigbeeTransportKeyInstallResult transportInstall{};
  if (ZigbeeCommissioning::acceptTransportKeyCommand(
          g_network, kIeeeAddress, sourceShort, securedSourceIeee, nwkSecured,
          frame, length, installCodeKey,
          haveInstallCodeKey, &transportInstall)) {
    ZigbeeCommissioning::applyTransportKeyInstall(&g_network, transportInstall);
    if (transportInstall.activatesNetworkKey) {
      g_lastInboundSecurityFrameCounter = 0U;
    }
    persistState();
    if (transportInstall.stagesAlternateKey) {
      Serial.print("transport_key_update seq=");
    } else if (transportInstall.refreshesAlternateKey) {
      Serial.print("transport_key_update_refresh seq=");
    } else {
      Serial.print("transport_key seq=");
    }
    Serial.print(transportInstall.transportKey.keySequence);
    Serial.print(" install=");
    if (transportInstall.activatesNetworkKey) {
      Serial.print("active");
    } else if (transportInstall.stagesAlternateKey) {
      Serial.print("staged");
    } else if (transportInstall.refreshesActiveNetworkKey) {
      Serial.print("active_refresh");
    } else if (transportInstall.refreshesAlternateKey) {
      Serial.print("staged_refresh");
    } else {
      Serial.print("noop");
    }
    Serial.print(" tc=0x");
    Serial.print(static_cast<uint32_t>(g_trustCenterIeee >> 32U), HEX);
    Serial.print(static_cast<uint32_t>(g_trustCenterIeee & 0xFFFFFFFFUL), HEX);
    Serial.print(" lk=");
    Serial.print(ZigbeeCommissioning::keyModeName(g_preconfiguredKeyMode));
    Serial.print(" ctr=");
    Serial.print(transportInstall.counter);
    if (transportInstall.apsSecurity.valid) {
      Serial.print(" aps_sec_fc=");
      Serial.print(transportInstall.apsSecurity.frameCounter);
    }
    Serial.print("\r\n");
    extendAwakeWindow(static_cast<uint32_t>(
        NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS));
    return true;
  }

  ZigbeeUpdateDeviceAcceptance updateDevice{};
  if (ZigbeeCommissioning::acceptUpdateDeviceCommand(
          g_network, kIeeeAddress, sourceShort, securedSourceIeee, nwkSecured,
#if NRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS
          true,
#else
          false,
#endif
          frame, length, installCodeKey, haveInstallCodeKey, &updateDevice)) {
    ZigbeeCommissioning::applyUpdateDevice(&g_network, updateDevice);
    if (updateDevice.updateDevice.status ==
        kZigbeeApsUpdateDeviceStatusStandardSecureRejoin) {
      configureDeviceForCurrentNetwork();
      applyJoinLed();
      persistState();
    }
    Serial.print("update_device short=0x");
    Serial.print(updateDevice.updateDevice.deviceShort, HEX);
    Serial.print(" status=0x");
    Serial.print(updateDevice.updateDevice.status, HEX);
    Serial.print(" nwk_sec=");
    Serial.print(nwkSecured ? "1" : "0");
    if (updateDevice.apsSecurity.valid) {
      Serial.print(" aps_sec_fc=");
      Serial.print(updateDevice.apsSecurity.frameCounter);
    }
    Serial.print("\r\n");
    extendAwakeWindow(static_cast<uint32_t>(
        NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS));
    return true;
  }

  ZigbeeSwitchKeyAcceptance switchKey{};
  if (ZigbeeCommissioning::acceptSwitchKeyCommand(
          g_network, sourceShort, securedSourceIeee, nwkSecured, false, frame,
          length, installCodeKey, haveInstallCodeKey, &switchKey)) {
    ZigbeeCommissioning::applySwitchKey(&g_network, switchKey);
    g_lastInboundSecurityFrameCounter = 0U;
    configureDeviceForCurrentNetwork();
    applyJoinLed();
    persistState();
    Serial.print("switch_key seq=");
    Serial.print(g_activeNetworkKeySequence);
    Serial.print(" ctr=");
    Serial.print(switchKey.counter);
    if (switchKey.apsSecurity.valid) {
      Serial.print(" aps_sec_fc=");
      Serial.print(switchKey.apsSecurity.frameCounter);
    }
    Serial.print("\r\n");
    extendAwakeWindow(static_cast<uint32_t>(
        NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS));
    return true;
  }

  const ZigbeeCommissioningFailure rejectedFailure =
      ZigbeeCommissioning::classifyRejectedTrustCenterCommand(
          g_network, frame, length, installCodeKey, haveInstallCodeKey);
  if (rejectedFailure != ZigbeeCommissioningFailure::kNone) {
    g_network.lastFailure = rejectedFailure;
    Serial.print("tc_cmd reject=");
    Serial.print(ZigbeeCommissioning::failureName(rejectedFailure));
    Serial.print(" mode=");
    Serial.print(ZigbeeCommissioning::stateName(g_network.state));
    Serial.print("\r\n");
    return true;
  }

  return false;
}

bool activeScan(ScanResult* outResult) {
  if (outResult == nullptr) {
    return false;
  }
  memset(outResult, 0, sizeof(*outResult));
  refreshCommissioningState();
  ZigbeeBeaconCandidate candidate{};
  if (!ZigbeeCommissioning::activeScan(g_radio, &g_macSequence, &g_network,
                                       &candidate) ||
      !candidate.valid) {
    return false;
  }

  outResult->valid = candidate.valid;
  outResult->channel = candidate.channel;
  outResult->rssiDbm = candidate.rssiDbm;
  outResult->score = candidate.score;
  outResult->beacon = candidate.beacon;
  return true;
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
  refreshCommissioningState();
  if (!ZigbeeCommissioning::performJoin(g_radio, &g_macSequence, kIeeeAddress,
                                        0xC0U, &g_network)) {
    return false;
  }
  configureDeviceForCurrentNetwork();
  applyJoinLed();
  persistState();
  extendAwakeWindow(static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS));
  return true;
}

bool performSecureRejoin() {
  refreshCommissioningState();
  if (!ZigbeeCommissioning::performSecureRejoin(
          g_radio, &g_macSequence, kIeeeAddress, 0xC0U, &g_network)) {
    return false;
  }
  configureDeviceForCurrentNetwork();
  applyJoinLed();
  persistState();
  extendAwakeWindow(static_cast<uint32_t>(
      NRF54L15_CLEAN_ZIGBEE_COMMISSIONING_AWAKE_MS));
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
  // Trust Center transport-key delivery is APS-secured over an unsecured NWK
  // frame immediately after association, before the active network key exists.
  const bool requireSecuredNwk = g_haveActiveNetworkKey;
  if (g_haveActiveNetworkKey) {
    const uint64_t expectedTc = expectedTrustCenterIeee();
    nwkValid = ZigbeeSecurity::parseSecuredNwkFrame(
        macData.payload, macData.payloadLength, g_activeNetworkKey, &nwk,
        &security, decryptedPayload, &decryptedPayloadLength, expectedTc);
    if (nwkValid &&
        (!security.valid ||
         (expectedTc != 0U && security.sourceIeee != expectedTc) ||
         security.keySequence != g_activeNetworkKeySequence ||
         security.frameCounter <= g_lastInboundSecurityFrameCounter)) {
      return;
    }
  }
  if (!nwkValid && !requireSecuredNwk) {
    nwkValid =
        ZigbeeCodec::parseNwkFrame(macData.payload, macData.payloadLength, &nwk);
  }
  if (!nwkValid || !nwk.valid || nwk.destinationShort != g_localShort) {
    return;
  }
  if (security.valid) {
    g_lastInboundSecurityFrameCounter = security.frameCounter;
  }

  if (nwk.frameType == ZigbeeNwkFrameType::kCommand) {
    ZigbeeNwkEndDeviceTimeoutResponse timeoutResponse{};
    if (ZigbeeCommissioning::acceptEndDeviceTimeoutResponse(
            g_network, nwk.payload, nwk.payloadLength, &timeoutResponse)) {
      ZigbeeCommissioning::applyEndDeviceTimeoutResponse(&g_network,
                                                         timeoutResponse);
      if (g_parentPollIntervalMs > 1000UL) {
        g_parentPollIntervalMs = 1000UL;
      }
      persistState();
      Serial.print("end_device_timeout status=0x");
      Serial.print(timeoutResponse.status, HEX);
      Serial.print(" parent_info=0x");
      Serial.print(timeoutResponse.parentInformation, HEX);
      Serial.print(" poll_ms=");
      Serial.print(g_parentPollIntervalMs);
      Serial.print("\r\n");
    }
    return;
  }

  ZigbeeApsDataFrame aps{};
  ZigbeeApsAcknowledgementFrame ack{};
  if (ZigbeeCodec::parseApsAcknowledgementFrame(nwk.payload, nwk.payloadLength,
                                                &ack) &&
      ack.valid) {
    uint8_t ackSlot = 0U;
    if (findPendingApsAckSlot(ack, nwk.sourceShort, &ackSlot)) {
      Serial.print("aps_ack rx ctr=0x");
      Serial.print(ack.counter, HEX);
      Serial.print(" cluster=0x");
      Serial.print(ack.clusterId, HEX);
      Serial.print("\r\n");
      clearPendingApsAckSlot(ackSlot);
    }
    return;
  }
  if (!ZigbeeCodec::parseApsDataFrame(nwk.payload, nwk.payloadLength, &aps) ||
      !aps.valid) {
    (void)handleApsCommand(nwk.payload, nwk.payloadLength, nwk.sourceShort,
                           security.valid ? security.sourceIeee : 0U,
                           security.valid);
    return;
  }

  const uint32_t nowMs = millis();
  const bool duplicateAps =
      isRecentInboundApsDuplicate(nwk.sourceShort, aps, nowMs);

  if (aps.profileId == kZigbeeProfileZdo) {
    Serial.print("zdo_req cluster=0x");
    Serial.print(aps.clusterId, HEX);
    Serial.print(" len=");
    Serial.print(aps.payloadLength);
    Serial.print(" ack=");
    Serial.print(aps.ackRequested ? "yes" : "no");
    Serial.print(" dup=");
    Serial.print(duplicateAps ? "yes" : "no");
    Serial.print("\r\n");
    if (aps.deliveryMode == kZigbeeApsDeliveryUnicast && aps.ackRequested) {
      delayMicroseconds(kParentRxAcknowledgeDelayUs);
      (void)sendApsAcknowledgement(nwk.sourceShort, aps);
    }
    if (duplicateAps) {
      Serial.print("aps_dup cluster=0x");
      Serial.print(aps.clusterId, HEX);
      Serial.print(" src=0x");
      Serial.print(nwk.sourceShort, HEX);
      Serial.print(" replay=yes\r\n");
    } else {
      rememberRecentInboundAps(nwk.sourceShort, aps);
    }
    uint16_t responseClusterId = 0U;
    uint8_t responsePayload[127] = {0U};
    uint8_t responseLength = 0U;
    if (g_device.handleZdoRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                  &responseClusterId, responsePayload,
                                  &responseLength) &&
        responseLength > 0U) {
      delayMicroseconds(kParentRxResponseDelayUs);
      const bool sent = sendApsFrame(nwk.sourceShort, 0U, responseClusterId,
                                     kZigbeeProfileZdo, 0U, responsePayload,
                                     responseLength);
      Serial.print("zdo_rsp cluster=0x");
      Serial.print(responseClusterId, HEX);
      Serial.print(" len=");
      Serial.print(responseLength);
      Serial.print(" sent=");
      Serial.print(sent ? "yes" : "no");
      Serial.print("\r\n");
      uint8_t leaveFlags = 0U;
      if (g_device.consumeLeaveRequest(&leaveFlags)) {
        handleAcceptedLeaveRequest(leaveFlags);
      }
    }
    return;
  }

  if (aps.profileId != kZigbeeProfileHomeAutomation ||
      aps.destinationEndpoint != kLocalEndpoint) {
    return;
  }
  if (aps.deliveryMode == kZigbeeApsDeliveryUnicast && aps.ackRequested) {
    delayMicroseconds(kParentRxAcknowledgeDelayUs);
    (void)sendApsAcknowledgement(nwk.sourceShort, aps);
  }
  if (duplicateAps) {
    Serial.print("aps_dup cluster=0x");
    Serial.print(aps.clusterId, HEX);
    Serial.print(" src=0x");
    Serial.print(nwk.sourceShort, HEX);
    Serial.print(" replay=yes\r\n");
  } else {
    rememberRecentInboundAps(nwk.sourceShort, aps);
  }

  uint8_t responseFrame[127] = {0U};
  uint8_t responseLength = 0U;
  if (!g_device.handleZclRequest(aps.clusterId, aps.payload, aps.payloadLength,
                                 responseFrame, &responseLength)) {
    return;
  }

  persistState();
  Serial.print("zcl cluster=0x");
  Serial.print(aps.clusterId, HEX);
  Serial.print("\r\n");
  if (responseLength > 0U) {
    delayMicroseconds(kParentRxResponseDelayUs);
    (void)sendApsFrame(nwk.sourceShort, aps.sourceEndpoint, aps.clusterId,
                       aps.profileId, aps.destinationEndpoint, responseFrame,
                       responseLength);
  }
}

void pollCoordinator() {
  refreshCommissioningState();
  if (!ZigbeeCommissioning::shouldPollParent(g_network)) {
    return;
  }

  uint8_t request[127] = {0U};
  uint8_t requestLength = 0U;
  const bool joinedPoll =
      g_joined || (g_localShort != 0U && g_localShort != kButtonShort);
  const bool built =
      joinedPoll
          ? ZigbeeCodec::buildDataRequestShort(g_macSequence++, g_panId,
                                               g_parentShort, g_localShort,
                                               request, &requestLength)
          : ZigbeeCodec::buildDataRequest(g_macSequence++, g_panId,
                                          g_parentShort, kIeeeAddress, request,
                                          &requestLength);
  if (!built) {
    return;
  }
  ZigbeeFrame frame{};
  if (g_radio.transmitThenReceive(request, requestLength, &frame, 12000U, false,
                                  1200000UL)) {
    processIncomingFrame(frame);
  }

  const uint32_t followupWindowStartUs = micros();
  while (static_cast<uint32_t>(micros() - followupWindowStartUs) <
         static_cast<uint32_t>(
             NRF54L15_CLEAN_ZIGBEE_PARENT_POLL_FOLLOWUP_LISTEN_US)) {
    const uint32_t elapsedUs =
        static_cast<uint32_t>(micros() - followupWindowStartUs);
    const uint32_t remainingUs =
        (elapsedUs <
         static_cast<uint32_t>(
             NRF54L15_CLEAN_ZIGBEE_PARENT_POLL_FOLLOWUP_LISTEN_US))
            ? (static_cast<uint32_t>(
                   NRF54L15_CLEAN_ZIGBEE_PARENT_POLL_FOLLOWUP_LISTEN_US) -
               elapsedUs)
            : 0U;
    if (remainingUs == 0U) {
      break;
    }
    if (!g_radio.receive(&frame, remainingUs, 350000UL)) {
      continue;
    }
    processIncomingFrame(frame);
  }
}

void maybeSendScheduledReports(uint32_t nowMs) {
  if (!g_joined || inInterviewGracePeriod(nowMs)) {
    return;
  }

  while (sendDueAttributeReport(nowMs, nullptr)) {
  }
}

bool inInterviewGracePeriod(uint32_t nowMs) {
  return g_joined && g_lastJoinAttemptMs != 0U &&
         (nowMs - g_lastJoinAttemptMs) <
             static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_INTERVIEW_GRACE_MS);
}

void maybeSendBootReports(uint32_t nowMs) {
  if (!g_joined || !g_bootReportsPending) {
    return;
  }
  if (inInterviewGracePeriod(nowMs)) {
    return;
  }
  if ((nowMs - g_bootMs) <
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_BOOT_REPORT_DELAY_MS)) {
    return;
  }

  const bool onOffOk = sendAttributeReport(kZigbeeClusterOnOff);
  const bool powerOk = sendAttributeReport(kZigbeeClusterPowerConfiguration);
  g_bootReportsPending = false;
  Serial.print("boot_report onoff=");
  Serial.print(onOffOk ? "OK" : "FAIL");
  Serial.print(" power=");
  Serial.print(powerOk ? "OK" : "FAIL");
  Serial.print("\r\n");
}

void maybeEnterLowPowerCycle(uint32_t nowMs) {
  (void)nowMs;
  enterSystemOffIfPossible();
}

void debugListenCurrentChannel(uint32_t durationMs) {
  const uint32_t deadline = millis() + durationMs;
  Serial.print("listen ch=");
  Serial.print(g_channel);
  Serial.print(" ms=");
  Serial.print(durationMs);
  Serial.print("\r\n");
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    ZigbeeFrame frame{};
    if (!g_radio.receive(&frame, 10000U, 400000UL)) {
      continue;
    }
    Serial.print("listen_rx len=");
    Serial.print(frame.length);
    Serial.print(" rssi=");
    Serial.print(frame.rssiDbm);
    ZigbeeMacBeaconView beacon{};
    if (ZigbeeCodec::parseBeaconFrame(frame.psdu, frame.length, &beacon) &&
        beacon.valid) {
      Serial.print(" beacon pan=0x");
      Serial.print(beacon.panId, HEX);
      Serial.print(" parent=0x");
      Serial.print(beacon.sourceShort, HEX);
      Serial.print(" permit=");
      Serial.print(beacon.associationPermit ? "yes" : "no");
    }
    Serial.print("\r\n");
  }
  Serial.print("listen done\r\n");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    extendAwakeWindow(static_cast<uint32_t>(
        NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS));
    if (ch == 'r') {
      const bool powerOk = sendAttributeReport(kZigbeeClusterPowerConfiguration);
      Serial.print("report power=");
      Serial.print(powerOk ? "OK" : "FAIL");
      Serial.print("\r\n");
    } else if (ch == 't') {
      g_pendingButtonAction = true;
      extendAwakeWindow(
          static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS));
      Serial.print("toggle requested\r\n");
    } else if (ch == 'o') {
      (void)triggerButtonAction(kOnOffCommandOn);
    } else if (ch == 'f') {
      (void)triggerButtonAction(kOnOffCommandOff);
    } else if (ch == 'j') {
      beginPairingWindow(false);
      Serial.print("pairing window opened\r\n");
    } else if (ch == 'b') {
      ScanResult scan{};
      if (activeScan(&scan) && scan.valid) {
        Serial.print("scan_candidate ch=");
        Serial.print(scan.channel);
        Serial.print(" pan=0x");
        Serial.print(scan.beacon.panId, HEX);
        Serial.print(" parent=0x");
        Serial.print(scan.beacon.sourceShort, HEX);
        Serial.print(" rssi=");
        Serial.print(scan.rssiDbm);
        Serial.print(" permit=");
        Serial.print(scan.beacon.associationPermit ? "yes" : "no");
        Serial.print("\r\n");
      } else {
        Serial.print("scan_candidate none failure=");
        Serial.print(ZigbeeCommissioning::failureName(g_network.lastFailure));
        Serial.print("\r\n");
      }
    } else if (ch == 'y') {
      debugListenCurrentChannel(500U);
    } else if (ch == 'c') {
      clearJoinState(true);
      Serial.print("state cleared\r\n");
    } else if (ch == 'p') {
      beginPairingWindow(true);
      Serial.print("factory reset + pairing window\r\n");
    } else if (ch == 's') {
      Serial.print("state joined=");
      Serial.print(g_joined ? "yes" : "no");
      Serial.print(" mode=");
      Serial.print(ZigbeeCommissioning::stateName(g_network.state));
      Serial.print(" failure=");
      Serial.print(ZigbeeCommissioning::failureName(g_network.lastFailure));
      Serial.print(" join_attempts=");
      Serial.print(g_network.joinAttempts);
      Serial.print(" rejoin_attempts=");
      Serial.print(g_network.rejoinAttempts);
      Serial.print(" ch=");
      Serial.print(g_channel);
      Serial.print(" pan=0x");
      Serial.print(g_panId, HEX);
      Serial.print(" short=0x");
      Serial.print(g_localShort, HEX);
      Serial.print(" nwk_seq=");
      Serial.print(g_haveActiveNetworkKey ? g_activeNetworkKeySequence : 0U);
      Serial.print(" alt_seq=");
      Serial.print(g_haveAlternateNetworkKey ? g_alternateNetworkKeySequence
                                             : 0U);
      Serial.print(" poll_ms=");
      Serial.print(g_parentPollIntervalMs);
      Serial.print(" onoff=");
      Serial.print(g_device.onOff() ? "on" : "off");
      Serial.print(" pair_ms_left=");
      const uint32_t nowMs = millis();
      const uint32_t pairRemaining =
          static_cast<int32_t>(g_pairingWindowUntilMs - nowMs) > 0
              ? (g_pairingWindowUntilMs - nowMs)
              : 0U;
      Serial.print(pairRemaining);
      Serial.print(" reports=");
      Serial.print(g_device.reportingConfigurationCount());
      Serial.print("\r\n");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.print("\r\nbtn_setup stage=serial\r\n");
  Serial.flush();

  g_bootMs = millis();
  g_bootReportsPending = true;
  g_pendingButtonAction = false;
  g_pairingWindowUntilMs = 0U;
  g_awakeUntilMs = millis() +
                   static_cast<uint32_t>(
                       NRF54L15_CLEAN_ZIGBEE_BOOT_AWAKE_WINDOW_MS);

  if (g_localIeee == 0U) {
    g_localIeee = zigbeeFactoryEui64();
  }

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);
  Gpio::configure(kPinUserButton, GpioDirection::kInput, GpioPull::kDisabled);

  const uint32_t resetReason = NRF_RESET->RESETREAS;
  NRF_RESET->RESETREAS = resetReason;
  const uint8_t gpregret0 = readGpregret0();
  writeGpregret0(0U);
  g_wokeFromButton =
      ((resetReason & RESET_RESETREAS_OFF_Msk) != 0U) &&
      gpregret0 == kSystemOffMagic;
  Serial.print("btn_setup stage=reset woke=");
  Serial.print(g_wokeFromButton ? "yes" : "no");
  Serial.print("\r\n");
  Serial.flush();

  g_store.begin("zbjoints");
  Serial.print("btn_setup stage=store\r\n");
  Serial.flush();
  restoreState();
  Serial.print("btn_setup stage=restore joined=");
  Serial.print(g_joined ? "yes" : "no");
  Serial.print("\r\n");
  Serial.flush();

  const bool ok = g_radio.begin(g_channel, 8);
  Serial.print("btn_setup stage=radio ok=");
  Serial.print(ok ? "yes" : "no");
  Serial.print("\r\n");
  Serial.flush();
  Serial.print("\r\nZigbeeSleepyOnOffButton start\r\n");
  Serial.print("radio=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" preferred_channel=");
  Serial.print(kPreferredChannel);
  Serial.print(" ieee=0x");
  Serial.print(static_cast<uint32_t>(kIeeeAddress >> 32U), HEX);
  Serial.print(static_cast<uint32_t>(kIeeeAddress & 0xFFFFFFFFUL), HEX);
  Serial.print(" joined=");
  Serial.print(g_joined ? "yes" : "no");
  Serial.print(" woke_button=");
  Serial.print(g_wokeFromButton ? "yes" : "no");
  Serial.print("\r\n");
  Serial.print("serial commands: r=report s=status t=toggle o=on f=off j=pair b=scan y=listen c=clear p=factory_reset_pair\r\n");

  if (g_joined) {
    g_radio.setChannel(g_channel);
  }

  if (g_wokeFromButton) {
    delay(static_cast<unsigned long>(
        NRF54L15_CLEAN_ZIGBEE_BUTTON_DEBOUNCE_MS));
    const uint32_t heldMs = measureButtonHoldMs(static_cast<uint32_t>(
        NRF54L15_CLEAN_ZIGBEE_BUTTON_PAIR_HOLD_MS));
    if (heldMs >=
        static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_BUTTON_PAIR_HOLD_MS)) {
      beginPairingWindow(g_joined);
      Serial.print("button_hold pairing held_ms=");
      Serial.print(heldMs);
      Serial.print("\r\n");
    } else if (g_joined) {
      g_pendingButtonAction = true;
      extendAwakeWindow(static_cast<uint32_t>(
          NRF54L15_CLEAN_ZIGBEE_ACTION_AWAKE_WINDOW_MS));
      Serial.print("button_press short held_ms=");
      Serial.print(heldMs);
      Serial.print("\r\n");
    } else {
      Serial.print("button_press ignored_not_joined held_ms=");
      Serial.print(heldMs);
      Serial.print(" hold_3s_to_pair\r\n");
    }
  }
}

void loop() {
  handleSerialCommands();

  const uint32_t now = millis();
  const ZigbeeCommissioningAction action =
      ZigbeeCommissioning::nextAction(&g_network, now);
  if (!g_joined) {
    clearPendingApsAck();
    clearRecentInboundAps();
    const bool pairingActive =
        static_cast<int32_t>(g_pairingWindowUntilMs - now) > 0 ||
        g_rejoinPending ||
        action != ZigbeeCommissioningAction::kNone;
    if (!pairingActive) {
      applyJoinLed();
      maybeEnterLowPowerCycle(now);
      delay(1);
      return;
    }
    if (action == ZigbeeCommissioningAction::kPollParent &&
        (now - g_lastPollMs) >= 250U) {
      g_lastPollMs = now;
      pollCoordinator();
    } else if (action == ZigbeeCommissioningAction::kSecureRejoin) {
      Serial.print("secure_rejoin start\r\n");
      if (performSecureRejoin()) {
        Serial.print("secure_rejoin OK ch=");
      } else {
        Serial.print("secure_rejoin MISS failure=");
        Serial.print(ZigbeeCommissioning::failureName(g_network.lastFailure));
        Serial.print("\r\n");
        delay(1);
        return;
      }
      Serial.print(g_channel);
      Serial.print(" pan=0x");
      Serial.print(g_panId, HEX);
      Serial.print(" short=0x");
      Serial.print(g_localShort, HEX);
      Serial.print("\r\n");
      g_pairingWindowUntilMs = 0U;
    } else if (action == ZigbeeCommissioningAction::kJoin) {
      Serial.print("scan_join start\r\n");
      if (!performJoin()) {
        Serial.print("join MISS failure=");
        Serial.print(ZigbeeCommissioning::failureName(g_network.lastFailure));
        Serial.print("\r\n");
        delay(1);
        return;
      }
      Serial.print("join OK ch=");
      Serial.print(g_channel);
      Serial.print(" pan=0x");
      Serial.print(g_panId, HEX);
      Serial.print(" short=0x");
      Serial.print(g_localShort, HEX);
      Serial.print("\r\n");
      g_pairingWindowUntilMs = 0U;
    }
    applyJoinLed();
    delay(1);
    return;
  }

  if (action == ZigbeeCommissioningAction::kRequestEndDeviceTimeout &&
      !inInterviewGracePeriod(now)) {
    const bool ok = sendEndDeviceTimeoutRequest();
    Serial.print("end_device_timeout_req ");
    Serial.print(ok ? "OK" : "FAIL");
    Serial.print("\r\n");
  } else if (action == ZigbeeCommissioningAction::kSendDeviceAnnounce) {
    const bool ok = sendDeviceAnnounce();
    Serial.print("device_announce ");
    Serial.print(ok ? "OK" : "FAIL");
    Serial.print("\r\n");
  }

  if ((now - g_lastPollMs) >= g_parentPollIntervalMs) {
    g_lastPollMs = now;
    pollCoordinator();
  }
  if ((now - g_bootMs) >= 2000UL) {
    sampleSensors();
  }
  if (g_pendingButtonAction) {
    g_pendingButtonAction = false;
    (void)triggerButtonAction(kOnOffCommandToggle);
  }
  maybeExpirePendingApsAck(now);
  maybeSendScheduledReports(now);
  maybeSendBootReports(now);
  applyJoinLed();

  if ((now - g_lastStatusMs) >=
      static_cast<uint32_t>(NRF54L15_CLEAN_ZIGBEE_STATUS_INTERVAL_MS)) {
    g_lastStatusMs = now;
    Serial.print("alive ch=");
    Serial.print(g_channel);
    Serial.print(" pan=0x");
    Serial.print(g_panId, HEX);
    Serial.print(" short=0x");
    Serial.print(g_localShort, HEX);
    Serial.print(" nwk_seq=");
    Serial.print(g_haveActiveNetworkKey ? g_activeNetworkKeySequence : 0U);
    Serial.print(" poll_ms=");
    Serial.print(g_parentPollIntervalMs);
    Serial.print(" onoff=");
    Serial.print(g_device.onOff() ? "on" : "off");
    Serial.print(" reports=");
    Serial.print(g_device.reportingConfigurationCount());
    Serial.print("\r\n");
  }

  maybeEnterLowPowerCycle(now);

  delay(1);
}
