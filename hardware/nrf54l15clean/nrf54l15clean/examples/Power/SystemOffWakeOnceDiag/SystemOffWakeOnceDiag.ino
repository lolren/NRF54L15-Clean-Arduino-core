#include <Arduino.h>
#include <nrf54l15.h>

static constexpr uint8_t kLedPin = 0U;  // XIAO LED = P2.0, active-low.
static constexpr uint8_t kWakeMagic = 0xA5U;
static constexpr unsigned long kWakeDelayMs = 1000UL;

static void ledInit() {
  NRF_P2->DIRSET = (1UL << kLedPin);
  NRF_P2->OUTSET = (1UL << kLedPin);
}

static void ledOn() { NRF_P2->OUTCLR = (1UL << kLedPin); }

static void ledOff() { NRF_P2->OUTSET = (1UL << kLedPin); }

static uint8_t readGpregret0() {
  return static_cast<uint8_t>(NRF_POWER->GPREGRET[0] &
                              POWER_GPREGRET_GPREGRET_Msk);
}

static void writeGpregret0(uint8_t value) {
  NRF_POWER->GPREGRET[0] = static_cast<uint32_t>(value);
}

static void waitMs(unsigned long ms) {
  while (ms-- > 0UL) {
    delayMicroseconds(1000);
  }
}

static void blinkCode(uint8_t count, unsigned long onMs, unsigned long offMs) {
  for (uint8_t i = 0; i < count; ++i) {
    ledOn();
    waitMs(onMs);
    ledOff();
    waitMs(offMs);
  }
}

void setup() {
  ledInit();
  ledOff();

  const uint32_t resetReason = NRF_RESET->RESETREAS;
  const uint8_t marker = readGpregret0();

  NRF_RESET->RESETREAS = resetReason;

  if (marker == kWakeMagic) {
    writeGpregret0(0U);

    const bool wokeFromOff = (resetReason & RESET_RESETREAS_OFF_Msk) != 0U;
    const bool wokeFromGrtc = (resetReason & RESET_RESETREAS_GRTC_Msk) != 0U;

    uint8_t code = 5U;
    if (wokeFromOff && wokeFromGrtc) {
      code = 2U;
    } else if (wokeFromOff) {
      code = 3U;
    } else if (wokeFromGrtc) {
      code = 4U;
    }

    while (true) {
      blinkCode(code, 140UL, 200UL);
      waitMs(1200UL);
    }
  }

  // Fresh boot / external reset path.
  blinkCode(1U, 500UL, 300UL);
  waitMs(1000UL);
  writeGpregret0(kWakeMagic);
  delaySystemOffNoRetention(kWakeDelayMs);
}

void loop() {}
