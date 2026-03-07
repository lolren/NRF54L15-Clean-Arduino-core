#include <nrf54l15_hal.h>
#include <variant.h>

using namespace xiao_nrf54l15;

// Best demonstration path:
// - Tools -> CPU Frequency -> 128 MHz (Turbo)
// - Tools -> Power Profile -> Low Power (WFI Idle)
//
// The sketch runs active work at 128 MHz, but delay()/yield() temporarily
// down-clock the CPU to 64 MHz before WFI and restore the previous speed after
// wake.

static constexpr uint8_t kLedPin = 0U;  // XIAO LED = P2.0, active low.
static constexpr unsigned long kBlinkOnUs = 80000UL;
static constexpr unsigned long kIdleMs = 1000UL;

static void ledInit() {
  NRF_P2->DIRSET = (1UL << kLedPin);
  NRF_P2->OUTSET = (1UL << kLedPin);
}

static void ledOn() { NRF_P2->OUTCLR = (1UL << kLedPin); }

static void ledOff() { NRF_P2->OUTSET = (1UL << kLedPin); }

void setup() {
  xiaoNrf54l15EnterLowestPowerBoardState();
  ledInit();

  (void)ClockControl::setCpuFrequency(CpuFrequency::k128MHz);
  (void)ClockControl::enableIdleCpuScaling(CpuFrequency::k64MHz);
}

void loop() {
  ledOn();
  delayMicroseconds(kBlinkOnUs);
  ledOff();

  delay(kIdleMs);
}
