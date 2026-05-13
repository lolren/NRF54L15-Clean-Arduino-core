/*
 * BleAdvertiserRotatingBackground
 *
 * Low-power continuous ADV_NONCONN_IND beacon for current measurements.
 *
 * This uses the background advertiser, but sends only one primary-channel packet
 * per interval and rotates 37 -> 38 -> 39 over successive intervals. That keeps
 * the device visible over time while avoiding the current cost of a three-channel
 * event every interval.
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
constexpr int8_t kTxPowerDbm = -10;
constexpr uint32_t kAdvertisingIntervalMs = 1000UL;
constexpr uint32_t kHfxoLeadUs = 1200UL;

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
    ok = gBle.setAdvertisingName("X54-LP-ROT", true);
  }
  if (ok) {
    ok = gBle.beginBackgroundAdvertising(
        kAdvertisingIntervalMs, BleAdvertisingChannel::k37, kHfxoLeadUs,
        true, true);
  }
  if (!ok) {
    failStop();
  }
}

void loop() {
  __asm volatile("wfi");
}
