#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static Grtc g_grtc;

static constexpr uint8_t kCompareChannel = 0U;
static constexpr uint32_t kAlarmPeriodUs = 250000UL;
static constexpr uint8_t kWakeLeadLfclkCycles = 4U;

static uint64_t g_nextAlarmUs = 0ULL;
static uint32_t g_alarmCount = 0UL;
static bool g_ledOn = false;

static void printU64(uint64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%llu",
           static_cast<unsigned long long>(value));
  Serial.print(buffer);
}

static void printI64(int64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
  Serial.print(buffer);
}

static void armNextAlarm(uint64_t nowUs) {
  do {
    g_nextAlarmUs += static_cast<uint64_t>(kAlarmPeriodUs);
  } while (g_nextAlarmUs <= nowUs);

  (void)g_grtc.setCompareAbsoluteUs(kCompareChannel, g_nextAlarmUs, true);
}

void setup() {
  Serial.begin(115200);
  delay(250);

  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  (void)Gpio::write(kPinUserLed, true);

  Serial.println("GrtcCompareAlarmTicker");
  Serial.println("Polling a GRTC compare channel as a periodic alarm source.");

  if (!g_grtc.begin(GrtcClockSource::kLfxo)) {
    Serial.println("GRTC begin failed");
    while (true) {
      delay(1000);
    }
  }

  (void)g_grtc.setWakeLeadLfclk(kWakeLeadLfclkCycles);
  g_nextAlarmUs = g_grtc.counter();
  armNextAlarm(g_nextAlarmUs);
}

void loop() {
  if (!g_grtc.pollCompare(kCompareChannel, true)) {
    delay(1);
    return;
  }

  const uint64_t nowUs = g_grtc.counter();
  const int64_t lateUs =
      static_cast<int64_t>(nowUs) - static_cast<int64_t>(g_nextAlarmUs);

  g_ledOn = !g_ledOn;
  (void)Gpio::write(kPinUserLed, !g_ledOn);

  ++g_alarmCount;
  Serial.print("alarm ");
  Serial.print(g_alarmCount);
  Serial.print(" at ");
  printU64(nowUs);
  Serial.print(" us, scheduling error ");
  printI64(lateUs);
  Serial.println(" us");

  armNextAlarm(nowUs);
}
