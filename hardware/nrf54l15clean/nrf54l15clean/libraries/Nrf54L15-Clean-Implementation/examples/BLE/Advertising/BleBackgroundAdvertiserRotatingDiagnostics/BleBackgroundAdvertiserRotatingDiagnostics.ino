/*
 * BleBackgroundAdvertiserRotatingDiagnostics
 *
 * Diagnostic sketch for the low-power rotating background advertiser.
 * By default it runs shadow-only so SWD can inspect counters without USB CDC
 * disturbing the current profile. Define NRF54L15_BG_DIAG_ENABLE_SERIAL=1 to
 * print the same counters once per second over Serial.
 */

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

constexpr BoardAntennaPath kAntennaPath = BoardAntennaPath::kCeramic;
constexpr int8_t kTxPowerDbm = -10;
constexpr uint32_t kAdvertisingIntervalMs = 1000UL;
#if defined(NRF54L15_BG_EXAMPLE_HFXO_LEAD_US)
constexpr uint32_t kHfxoLeadUs =
    static_cast<uint32_t>(NRF54L15_BG_EXAMPLE_HFXO_LEAD_US);
#else
// Match the core default background-advertising lead time.
constexpr uint32_t kHfxoLeadUs = 1200UL;
#endif
#if !defined(NRF54L15_BG_DIAG_ENABLE_SERIAL)
constexpr bool kEnableSerial = false;
#elif (NRF54L15_BG_DIAG_ENABLE_SERIAL != 0)
constexpr bool kEnableSerial = true;
#else
constexpr bool kEnableSerial = false;
#endif
constexpr unsigned long kSerialBaud = 115200UL;
constexpr unsigned long kPrintPeriodMs = 1000UL;
constexpr uint32_t kDiagMagic = 0x524F5442UL;

BleRadio gBle;
PowerManager gPower;
volatile uint32_t gDiagShadow[32] = {0U};

[[noreturn]] void failStop(uint32_t step) {
  gDiagShadow[0] = kDiagMagic;
  gDiagShadow[1] = 0xFFFFFFFFUL;
  gDiagShadow[2] = step;
  if (kEnableSerial) {
    Serial.print(F("fail step="));
    Serial.println(step);
  }
  while (true) {
    __asm volatile("wfi");
  }
}

void configureBoardForBleLowPower() {
  BoardControl::setBatterySenseEnabled(false);
  BoardControl::setImuMicEnabled(false);
  BoardControl::collapseRfPathIdle();
}

uint32_t xoRunning() {
  return (((NRF_CLOCK->XO.STAT & CLOCK_XO_STAT_STATE_Msk) >>
           CLOCK_XO_STAT_STATE_Pos) == CLOCK_XO_STAT_STATE_Running)
             ? 1U
             : 0U;
}

uint32_t xoRunRequested() {
  return (((NRF_CLOCK->XO.RUN & CLOCK_XO_RUN_STATUS_Msk) >>
           CLOCK_XO_RUN_STATUS_Pos) == CLOCK_XO_RUN_STATUS_Triggered)
             ? 1U
             : 0U;
}

}  // namespace

void setup() {
  if (kEnableSerial) {
    Serial.begin(kSerialBaud);
    const unsigned long serialWaitStartMs = millis();
    while (!Serial && (millis() - serialWaitStartMs) < 1500UL) {
      __NOP();
    }
  }

  configureBoardForBleLowPower();

  (void)BoardControl::enableRfPath(kAntennaPath);
  bool ok = gBle.begin(kTxPowerDbm);
  uint32_t failStep = 1U;
  if (ok) {
    gPower.setLatencyMode(PowerLatencyMode::kLowPower);
  }
  if (ok) {
    failStep = 2U;
    ok = gBle.setAdvertisingPduType(BleAdvPduType::kAdvNonConnInd);
  }
  if (ok) {
    failStep = 3U;
    ok = gBle.setAdvertisingName("X54-BG-ROT", true);
  }
  if (ok) {
    failStep = 4U;
    ok = gBle.beginBackgroundAdvertising(kAdvertisingIntervalMs,
                                         BleAdvertisingChannel::k37,
                                         kHfxoLeadUs, true, true);
  }
  if (!ok) {
    failStop(failStep);
  }

  gDiagShadow[0] = kDiagMagic;
  gDiagShadow[1] = 1U;
  gDiagShadow[2] = 0U;
  if (kEnableSerial) {
    Serial.println(F("rotating background advertiser running"));
  }
}

void loop() {
  static unsigned long nextPrintMs = 0UL;
  const unsigned long nowMs = millis();
  if (nowMs < nextPrintMs) {
    __asm volatile("wfi");
    return;
  }
  nextPrintMs = nowMs + kPrintPeriodMs;

  BleBackgroundAdvertisingDebugCounters counters{};
  gBle.getBackgroundAdvertisingDebugCounters(&counters);
  gDiagShadow[0] = kDiagMagic;
  gDiagShadow[1] = 2U;
  gDiagShadow[2] = nowMs;
  gDiagShadow[3] = counters.enabled;
  gDiagShadow[4] = counters.rotatingChannel;
  gDiagShadow[5] = counters.eventArmCount;
  gDiagShadow[6] = counters.eventCompleteCount;
  gDiagShadow[7] = counters.serviceRunCount;
  gDiagShadow[8] = counters.rfPathManaged;
  gDiagShadow[9] = counters.rfPathActive;
  gDiagShadow[10] = BoardControl::rfSwitchPowerEnabled() ? 1U : 0U;
  gDiagShadow[11] = static_cast<uint32_t>(BoardControl::antennaPath());
  gDiagShadow[12] = counters.rfPathPrewarmRestoreCount;
  gDiagShadow[13] = counters.rfPathIdleCollapseCount;
  gDiagShadow[14] = counters.constlatActive;
  gDiagShadow[15] = counters.lowPowerReleaseCount;
  gDiagShadow[16] = xoRunning();
  gDiagShadow[17] = xoRunRequested();
  gDiagShadow[18] = counters.txSettleTimeoutCount;
  gDiagShadow[19] = gBle.getBackgroundAdvertisingLastStopReason();
  gDiagShadow[20] = counters.irqCompareCount;
  gDiagShadow[21] = counters.txReadyCount;
  gDiagShadow[22] = counters.txStartKickCount;
  gDiagShadow[23] = counters.txKickRetryCount;
  gDiagShadow[24] = counters.txKickFallbackCount;
  gDiagShadow[25] = counters.txPhyendCount;
  gDiagShadow[26] = counters.txDisabledCount;
  gDiagShadow[27] = counters.lastChannel;
  gDiagShadow[28] = counters.lastRandomDelayUs;
  gDiagShadow[29] = counters.clockIrqCount;
  gDiagShadow[30] = counters.clockXotunedCount;
  gDiagShadow[31] = counters.clockXotuneFailedCount;

  if (kEnableSerial) {
    Serial.print(F("ms="));
    Serial.print(nowMs);
    Serial.print(F(" enabled="));
    Serial.print(counters.enabled);
    Serial.print(F(" rotate="));
    Serial.print(counters.rotatingChannel);
    Serial.print(F(" arm="));
    Serial.print(counters.eventArmCount);
    Serial.print(F(" complete="));
    Serial.print(counters.eventCompleteCount);
    Serial.print(F(" irq="));
    Serial.print(counters.irqCompareCount);
    Serial.print(F(" service="));
    Serial.print(counters.serviceRunCount);
    Serial.print(F(" rf_managed="));
    Serial.print(counters.rfPathManaged);
    Serial.print(F(" rf_dbg_active="));
    Serial.print(counters.rfPathActive);
    Serial.print(F(" rf_live_power="));
    Serial.print(BoardControl::rfSwitchPowerEnabled() ? 1 : 0);
    Serial.print(F(" rf_live_path="));
    Serial.print(static_cast<uint32_t>(BoardControl::antennaPath()));
    Serial.print(F(" rf_restore="));
    Serial.print(counters.rfPathPrewarmRestoreCount);
    Serial.print(F(" rf_collapse="));
    Serial.print(counters.rfPathIdleCollapseCount);
    Serial.print(F(" lowpwr_release="));
    Serial.print(counters.lowPowerReleaseCount);
    Serial.print(F(" constlat_now="));
    Serial.print(counters.constlatActive);
    Serial.print(F(" clk_tuned="));
    Serial.print(counters.clockXotunedCount);
    Serial.print(F(" clk_fail="));
    Serial.print(counters.clockXotuneFailedCount);
    Serial.print(F(" xo_running="));
    Serial.print(xoRunning());
    Serial.print(F(" xo_run_req="));
    Serial.print(xoRunRequested());
    Serial.print(F(" tx_ready="));
    Serial.print(counters.txReadyCount);
    Serial.print(F(" tx_retry="));
    Serial.print(counters.txKickRetryCount);
    Serial.print(F(" tx_fallback="));
    Serial.print(counters.txKickFallbackCount);
    Serial.print(F(" tx_settle_to="));
    Serial.print(counters.txSettleTimeoutCount);
    Serial.print(F(" stop_reason="));
    Serial.print(gBle.getBackgroundAdvertisingLastStopReason());
    Serial.print(F(" ch="));
    Serial.print(counters.lastChannel);
    Serial.print(F(" last_rand_us="));
    Serial.println(counters.lastRandomDelayUs);
  }

  __asm volatile("wfi");
}
