#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

extern "C" void nrf54l15_clean_idle_service(void);

// Runtime CPU clock control.
//
// The board package defaults to 64 MHz because that is the best general
// low-current setting. If a sketch has a short compute-heavy section, it can
// temporarily switch to 128 MHz and then explicitly downclock back to 64 MHz.

static constexpr uint32_t kWorkPeriodMs = 3000UL;
static constexpr uint32_t kLedPulseMs = 5UL;

static PowerManager g_power;

static const char* cpuName(CpuFrequency frequency) {
  return frequency == CpuFrequency::k128MHz ? "128 MHz" : "64 MHz";
}

static void printCpu(const char* label) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(cpuName(ClockControl::cpuFrequency()));
}

static void shortFastWork() {
  (void)ClockControl::setCpuFrequency(CpuFrequency::k128MHz);
  printCpu("active");

  volatile uint32_t checksum = 0U;
  for (uint32_t i = 0U; i < 50000UL; ++i) {
    checksum += (i ^ 0x54L);
  }
  (void)checksum;

  (void)ClockControl::setCpuFrequency(CpuFrequency::k64MHz);
  printCpu("downclocked");
}

static void idleWfiUntil(uint32_t deadlineMs) {
  (void)g_power.prepareLowPowerIdle();
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

  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  (void)Gpio::write(kPinUserLed, true);

  (void)ClockControl::setCpuFrequency(CpuFrequency::k64MHz);
  (void)ClockControl::enableIdleCpuScaling(CpuFrequency::k64MHz);
  (void)g_power.prepareLowPowerIdle();

  Serial.println("LowPowerCpuFrequencyControl");
  printCpu("boot");
}

void loop() {
  const uint32_t startMs = millis();

  (void)Gpio::write(kPinUserLed, false);
  delay(kLedPulseMs);
  (void)Gpio::write(kPinUserLed, true);

  shortFastWork();
  idleWfiUntil(startMs + kWorkPeriodMs);
}
