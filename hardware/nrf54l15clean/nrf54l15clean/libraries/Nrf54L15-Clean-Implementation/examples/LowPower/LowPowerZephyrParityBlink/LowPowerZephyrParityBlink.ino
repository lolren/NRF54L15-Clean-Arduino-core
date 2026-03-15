#include <Arduino.h>
#include <nrf54l15_hal.h>
#include <variant.h>

using namespace xiao_nrf54l15;

#if defined(NRF_TRUSTZONE_NONSECURE)
#error "LowPowerZephyrParityBlink requires Security Domain = Secure."
#endif

namespace {

// Closest Arduino-core reproduction of the Zephyr low-power blink path:
// - raw P2.0 LED
// - secure build only
// - timed SYSTEM OFF wake
// - RAM retention cleared for the lowest current
//
// This is a meter-oriented floor test, not a good visual blink test.
// The LED is only on for 5 ms every 1000 ms, so use
// `LowPowerDelaySystemOff` if you want an easier visible confirmation that the
// timed SYSTEM OFF wake loop is running.
constexpr uint32_t kBlinkOnUs = 5000UL;
constexpr uint32_t kSystemOffUs = 1000000UL;
constexpr uint32_t kPostUploadGraceMs = 400UL;
constexpr uint32_t kPostUploadVisibleBlinkUs = 30000UL;
constexpr uint8_t kLedPin = 0U;  // XIAO nRF54L15 LED = P2.0, active low.

PowerManager gPower;

void ledInit() {
  NRF_P2->DIRSET = (1UL << kLedPin);
  NRF_P2->OUTSET = (1UL << kLedPin);
}

void ledOn() { NRF_P2->OUTCLR = (1UL << kLedPin); }

void ledOff() { NRF_P2->OUTSET = (1UL << kLedPin); }

void busyWaitMs(uint32_t ms) {
  const uint32_t start = micros();
  const uint32_t durationUs = ms * 1000UL;
  while ((uint32_t)(micros() - start) < durationUs) {
    __asm volatile("nop");
  }
}

bool wokeFromTimedSystemOff() {
  const uint32_t resetReason = NRF_RESET->RESETREAS;
  const bool wokeFromOff = (resetReason & RESET_RESETREAS_OFF_Msk) != 0U;
  const bool wokeFromGrtc = (resetReason & RESET_RESETREAS_GRTC_Msk) != 0U;
  NRF_RESET->RESETREAS = resetReason;
  return wokeFromOff || wokeFromGrtc;
}

}  // namespace

void setup() {
  ledInit();

  // Keep the board in its default routed state briefly after flash/reset so
  // the sketch gives an obvious sign of life and the CMSIS-DAP path is not
  // dropped immediately. Repeated timed SYSTEM OFF wakes still go straight
  // back to the meter-oriented low-power path.
  if (!wokeFromTimedSystemOff()) {
    ledOn();
    delayMicroseconds(kPostUploadVisibleBlinkUs);
    ledOff();
    busyWaitMs(kPostUploadGraceMs);
  }
}

void loop() {
  ledOn();
  delayMicroseconds(kBlinkOnUs);
  ledOff();

  xiaoNrf54l15EnterLowestPowerBoardState();
  gPower.systemOffTimedWakeUsNoRetention(kSystemOffUs);
}
