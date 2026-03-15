#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static Grtc g_grtc;

static constexpr uint32_t kPrintPeriodUs = 1000000UL;
static constexpr uint32_t kLedPulseMs = 20UL;

static uint64_t g_nextPrintUs = 0ULL;

static void printU64(uint64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%llu",
           static_cast<unsigned long long>(value));
  Serial.print(buffer);
}

static void printPadded2(uint32_t value) {
  if (value < 10U) {
    Serial.print('0');
  }
  Serial.print(value);
}

static void printPadded3(uint32_t value) {
  if (value < 100U) {
    Serial.print('0');
  }
  if (value < 10U) {
    Serial.print('0');
  }
  Serial.print(value);
}

static void pulseLed() {
  (void)Gpio::write(kPinUserLed, false);
  delay(kLedPulseMs);
  (void)Gpio::write(kPinUserLed, true);
}

static void printUptime(uint64_t uptimeUs) {
  const uint64_t totalMs = uptimeUs / 1000ULL;
  const uint32_t millisPart = static_cast<uint32_t>(totalMs % 1000ULL);
  const uint64_t totalSeconds = totalMs / 1000ULL;
  const uint32_t secondsPart = static_cast<uint32_t>(totalSeconds % 60ULL);
  const uint64_t totalMinutes = totalSeconds / 60ULL;
  const uint32_t minutesPart = static_cast<uint32_t>(totalMinutes % 60ULL);
  const uint64_t hoursPart = totalMinutes / 60ULL;

  Serial.print("GRTC uptime ");
  printU64(hoursPart);
  Serial.print(':');
  printPadded2(minutesPart);
  Serial.print(':');
  printPadded2(secondsPart);
  Serial.print('.');
  printPadded3(millisPart);
  Serial.print(" (");
  printU64(uptimeUs);
  Serial.println(" us since boot)");
}

void setup() {
  Serial.begin(115200);
  delay(250);

  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  (void)Gpio::write(kPinUserLed, true);

  Serial.println("GrtcUptimeClock");
  Serial.println("nRF54L15 has GRTC, which is a monotonic counter/alarm block.");
  Serial.println("This is RTC-like uptime, not a battery-backed calendar clock.");

  if (!g_grtc.begin(GrtcClockSource::kLfxo)) {
    Serial.println("GRTC begin failed");
    while (true) {
      delay(1000);
    }
  }

  g_nextPrintUs = g_grtc.counter() + static_cast<uint64_t>(kPrintPeriodUs);
}

void loop() {
  const uint64_t nowUs = g_grtc.counter();
  if (nowUs < g_nextPrintUs) {
    delay(10);
    return;
  }

  printUptime(nowUs);
  pulseLed();

  do {
    g_nextPrintUs += static_cast<uint64_t>(kPrintPeriodUs);
  } while (g_nextPrintUs <= nowUs);
}
