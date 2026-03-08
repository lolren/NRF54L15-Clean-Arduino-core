#include <Arduino.h>
#include <cmsis.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

// Reusable full-duplex I2S wrapper example.
//
// XIAO route used by this example:
// - D11 = SDOUT
// - D15 = SDIN
// - D12 = LRCK
// - D13 = SCK
// - D14 = MCK
//
// Jumper D11 -> D15 for one-board loopback. Without that jumper, this sketch is
// still useful as an IRQ/service smoke test.

static constexpr uint32_t kFrameWordCount = 64U;
static constexpr uint32_t kHeartbeatMs = 1000U;
static constexpr uint32_t kStopCycleMs = 4000U;

alignas(4) uint32_t gTxFrames[2][kFrameWordCount];
alignas(4) uint32_t gRxFrames[2][kFrameWordCount];
I2sDuplex gI2s;
uint32_t gLastHeartbeatMs = 0U;
uint32_t gLastStopMs = 0U;

struct TxState {
  uint8_t phase = 0U;
};

struct RxState {
  volatile uint32_t bufferCount = 0U;
  volatile uint32_t firstWord = 0U;
  volatile uint32_t xorWord = 0U;
};

TxState gTxState{};
RxState gRxState{};

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

void refillTxBuffer(uint32_t* buffer, uint32_t wordCount, void* context) {
  if (buffer == nullptr || context == nullptr || wordCount != kFrameWordCount) {
    return;
  }

  auto* state = static_cast<TxState*>(context);
  fillStereoBuffer(buffer, state->phase);
  ++state->phase;
}

void captureRxBuffer(uint32_t* buffer, uint32_t wordCount, void* context) {
  if (buffer == nullptr || context == nullptr || wordCount != kFrameWordCount) {
    return;
  }

  auto* state = static_cast<RxState*>(context);
  uint32_t folded = 0U;
  for (uint32_t i = 0; i < wordCount; ++i) {
    folded ^= buffer[i];
  }

  state->firstWord = buffer[0];
  state->xorWord = folded;
  ++state->bufferCount;
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

I2sDuplexConfig makeConfig() {
  I2sDuplexConfig config;
  // Duplex config is explicit so the whole route is visible in one place.
  config.sdout = kPinD11;
  config.lrck = kPinD12;
  config.sck = kPinD13;
  config.mck = kPinD14;
  config.sdin = kPinD15;
  config.autoRestart = true;
  return config;
}

}  // namespace

extern "C" void I2S20_IRQHandler(void) { I2sDuplex::irqHandler(); }

void setup() {
  configureBoard();
  fillStereoBuffer(gTxFrames[0], 0U);
  fillStereoBuffer(gTxFrames[1], 1U);

  if (!gI2s.begin(makeConfig(), gTxFrames[0], gTxFrames[1], gRxFrames[0],
                  gRxFrames[1], kFrameWordCount)) {
    Serial.println(F("I2S duplex begin failed"));
    while (true) {
      pulse(80U, 200U);
      delay(600U);
    }
  }

  gI2s.setTxRefillCallback(refillTxBuffer, &gTxState);
  gI2s.setRxReceiveCallback(captureRxBuffer, &gRxState);

  // makeActive() claims the shared I2S20 IRQ path for this wrapper instance.
  if (!gI2s.makeActive() || !gI2s.start()) {
    Serial.println(F("I2S duplex start failed"));
    while (true) {
      pulse(80U, 200U);
      delay(600U);
    }
  }

  Serial.println(F("I2S duplex wrapper interrupt example"));
  Serial.println(F("Pins: SDOUT=D11 SDIN=D15 LRCK=D12 SCK=D13 MCK=D14"));
  Serial.println(F("Jumper D11 -> D15 for one-board loopback, or leave SDIN floating for an IRQ-path smoke test."));
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
    Serial.print(F(" RXPTRUPD="));
    Serial.print(gI2s.rxPtrUpdCount());
    Serial.print(F(" STOPPED="));
    Serial.print(gI2s.stoppedCount());
    Serial.print(F(" restarts="));
    Serial.print(gI2s.restartCount());
    Serial.print(F(" stop_cycles="));
    Serial.print(gI2s.manualStopCount());
    Serial.print(F(" rx_callbacks="));
    Serial.print(gRxState.bufferCount);
    Serial.print(F(" first=0x"));
    Serial.print(gRxState.firstWord, HEX);
    Serial.print(F(" fold=0x"));
    Serial.print(gRxState.xorWord, HEX);
    Serial.print(F(" running="));
    Serial.println(gI2s.running() ? F("yes") : F("no"));
    pulse(10U, 0U);
  }

  delay(10U);
}
