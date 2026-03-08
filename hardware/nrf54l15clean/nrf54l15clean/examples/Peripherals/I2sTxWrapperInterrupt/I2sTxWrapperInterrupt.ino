#include <Arduino.h>
#include <cmsis.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

// Reusable I2S TX wrapper example.
//
// This sketch demonstrates the higher-level I2sTx helper:
// - wrapper-owned peripheral setup
// - interrupt-driven TX buffer rotation
// - callback-based buffer refill
//
// XIAO route used by this example:
// - D11 = SDOUT
// - D12 = LRCK
// - D13 = SCK
// - D14 = MCK
//
// These are board-specific XIAO route choices, not generic Arduino pin names.

static constexpr uint32_t kFrameWordCount = 64U;
static constexpr uint32_t kHeartbeatMs = 1000U;
static constexpr uint32_t kStopCycleMs = 4000U;

alignas(4) uint32_t gFrames[2][kFrameWordCount];
I2sTx gI2s;
uint32_t gLastHeartbeatMs = 0U;
uint32_t gLastStopMs = 0U;

struct WaveState {
  uint8_t phase = 0U;
};

WaveState gWaveState{};

void ledOn() { (void)Gpio::write(kPinUserLed, false); }
void ledOff() { (void)Gpio::write(kPinUserLed, true); }

void pulse(uint16_t onMs = 30U, uint16_t offMs = 0U) {
  ledOn();
  delay(onMs);
  ledOff();
  if (offMs > 0U) {
    delay(offMs);
  }
}

void fillStereoBuffer(uint32_t* frames, uint8_t phaseOffset) {
  for (uint32_t i = 0; i < kFrameWordCount; ++i) {
    const bool high = (((i / 8U) + phaseOffset) & 1U) != 0U;
    const int16_t sample = high ? 12000 : -12000;
    const uint16_t word = static_cast<uint16_t>(sample);
    frames[i] = (static_cast<uint32_t>(word) << 16U) | word;
  }
}

void refillBuffer(uint32_t* buffer, uint32_t wordCount, void* context) {
  if (buffer == nullptr || context == nullptr || wordCount != kFrameWordCount) {
    return;
  }

  auto* state = static_cast<WaveState*>(context);
  fillStereoBuffer(buffer, state->phase);
  ++state->phase;
}

void configureBoard() {
  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  ledOff();

  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && ((millis() - start) < 1500U)) {
  }

  BoardControl::setBatterySenseEnabled(false);
  BoardControl::setImuMicEnabled(false);
  BoardControl::collapseRfPathIdle();
}

I2sTxConfig makeConfig() {
  I2sTxConfig config;
  // Wrapper config is explicit so you can remap the board route in one place.
  config.sdout = kPinD11;
  config.lrck = kPinD12;
  config.sck = kPinD13;
  config.mck = kPinD14;
  config.sdin = kPinDisconnected;
  config.autoRestart = true;
  return config;
}

}  // namespace

extern "C" void I2S20_IRQHandler(void) { I2sTx::irqHandler(); }

void setup() {
  configureBoard();

  if (!gI2s.begin(makeConfig(), gFrames[0], gFrames[1], kFrameWordCount)) {
    Serial.println(F("I2S wrapper begin failed"));
    while (true) {
      pulse(80U, 200U);
      delay(600U);
    }
  }

  gI2s.setRefillCallback(refillBuffer, &gWaveState);

  // makeActive() claims the shared I2S20 IRQ path for this wrapper instance.
  if (!gI2s.makeActive() || !gI2s.start()) {
    Serial.println(F("I2S wrapper start failed"));
    while (true) {
      pulse(80U, 200U);
      delay(600U);
    }
  }

  Serial.println(F("I2S TX wrapper interrupt example"));
  Serial.println(F("Pins: SDOUT=D11 LRCK=D12 SCK=D13 MCK=D14"));
  Serial.println(F("Wrapper owns setup/start/IRQ service and refills buffers through a callback."));
  pulse(40U, 80U);
}

void loop() {
  // service() handles deferred restart work outside IRQ context.
  gI2s.service();

  const uint32_t now = millis();
  if (gI2s.running() && ((now - gLastStopMs) >= kStopCycleMs)) {
    gLastStopMs = now;
    (void)gI2s.stop();
  }

  if ((now - gLastHeartbeatMs) >= kHeartbeatMs) {
    gLastHeartbeatMs = now;
    Serial.print(F("I2S TXPTRUPD="));
    Serial.print(gI2s.txPtrUpdCount());
    Serial.print(F(" STOPPED="));
    Serial.print(gI2s.stoppedCount());
    Serial.print(F(" restarts="));
    Serial.print(gI2s.restartCount());
    Serial.print(F(" stop_cycles="));
    Serial.print(gI2s.manualStopCount());
    Serial.print(F(" running="));
    Serial.println(gI2s.running() ? F("yes") : F("no"));
    pulse(10U, 0U);
  }

  delay(10U);
}
