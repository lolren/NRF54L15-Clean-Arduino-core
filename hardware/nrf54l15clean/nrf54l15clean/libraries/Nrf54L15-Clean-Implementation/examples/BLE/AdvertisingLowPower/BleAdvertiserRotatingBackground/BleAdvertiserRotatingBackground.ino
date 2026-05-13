/*
 * BleAdvertiserRotatingBackground
 *
 * Low-power continuous ADV_NONCONN_IND beacon for current measurements.
 *
 * This keeps the Zephyr-style legacy advertising event shape (all three
 * primary channels in each event) while stretching the interval beyond the
 * usual fast advertising presets to keep current low.
 *
 * Board settings for lowest current:
 *   - Power: Low power
 *   - Disable VPR / Thread / Matter / Zigbee
 */

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

constexpr BoardAntennaPath kAntennaPath = BoardAntennaPath::kCeramic;
constexpr int8_t kTxPowerDbm = 0;
constexpr uint32_t kAdvertisingIntervalMs = 500UL;
constexpr uint32_t kInterChannelDelayUs = 350UL;
#if defined(NRF54L15_BG_EXAMPLE_HFXO_LEAD_US)
constexpr uint32_t kHfxoLeadUs =
    static_cast<uint32_t>(NRF54L15_BG_EXAMPLE_HFXO_LEAD_US);
#else
// Match the core default background-advertising lead time.
constexpr uint32_t kHfxoLeadUs = 1200UL;
#endif

BleRadio gBle;
PowerManager gPower;

[[noreturn]] void failStop() {
  BoardControl::collapseRfPathIdle();
  while (true) {
    __asm volatile("wfi");
  }
}

void configureBoardForBleLowPower() {
  BoardControl::setBatterySenseEnabled(false);
  BoardControl::setImuMicEnabled(false);
  BoardControl::collapseRfPathIdle();
}

}  // namespace

void setup() {
  configureBoardForBleLowPower();

  (void)BoardControl::enableRfPath(kAntennaPath);
  bool ok = gBle.begin(kTxPowerDbm);
  if (ok) {
    gPower.setLatencyMode(PowerLatencyMode::kLowPower);
  }
  if (ok) {
    ok = gBle.setAdvertisingPduType(BleAdvPduType::kAdvNonConnInd);
  }
  if (ok) {
    ok = gBle.setAdvertisingName("X54-LP-3CH", true);
  }
  if (ok) {
    ok = gBle.beginBackgroundAdvertising3Channel(
        kAdvertisingIntervalMs, kInterChannelDelayUs, kHfxoLeadUs, true);
  }
  if (!ok) {
    failStop();
  }
}

void loop() {
  __asm volatile("wfi");
}
