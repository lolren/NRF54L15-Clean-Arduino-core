#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

extern "C" void nrf54l15_clean_idle_service(void);

// Demonstrates the consolidated low-power idle preparation helper.
//
// The board package defaults to the low-power WFI path.
// The helper is explicit: it does not run automatically because radio, serial,
// and latency-sensitive sketches may intentionally keep clocks or board rails on.

static PowerManager g_power;

static constexpr uint32_t kPulsePeriodMs = 5000UL;
static constexpr uint32_t kPulseMs = 5UL;

static void sleepUntilMs(uint32_t deadlineMs) {
  while (static_cast<int32_t>(millis() - deadlineMs) < 0) {
    nrf54l15_clean_idle_service();
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
    __asm volatile("wfi");
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("LowPowerPrepareIdleBudget");
  Serial.println("Preparing low-power latency, DCDC, idle CPU scaling, and board load collapse.");
  Serial.flush();

  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  (void)Gpio::write(kPinUserLed, true);

  const bool ok = g_power.prepareLowPowerIdle(
      true,   // collapse board loads: RF switch, VBAT divider, IMU/mic rail where present
      true,   // enable automatic 64 MHz CPU scaling during core idle paths
      false); // leave HFXO under radio/serial ownership by default

  Serial.print("prepareLowPowerIdle=");
  Serial.println(ok ? "ok" : "partial");
  Serial.flush();
}

void loop() {
  const uint32_t now = millis();

  (void)Gpio::write(kPinUserLed, false);
  delay(kPulseMs);
  (void)Gpio::write(kPinUserLed, true);

  // Reapply after any sketch code that may have used board rails or constant
  // latency. This is cheap and keeps the idle budget deterministic.
  (void)g_power.prepareLowPowerIdle();
  sleepUntilMs(now + kPulsePeriodMs);
}
