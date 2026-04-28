#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

struct FrameConfig {
  uint16_t duty0Permille;
  uint16_t duty1Permille;
  uint16_t duty2Permille;
  uint16_t countertop;
};

static constexpr Pin kPins[] = {kPinD0, kPinD1, kPinD2};
static constexpr uint32_t kBaseFrequencyHz = 1000UL;
static constexpr uint32_t kStepHoldMs = 2000UL;
static constexpr FrameConfig kFrames[] = {
    {150U, 500U, 850U, 16000U},
    {300U, 600U, 900U, 8000U},
    {500U, 200U, 700U, 4000U},
    {800U, 400U, 100U, 12000U},
};

static Pwm g_pwm(nrf54l15::PWM20_BASE);
static uint16_t g_sequenceWords[(sizeof(kFrames) / sizeof(kFrames[0])) * 4U];
static uint8_t g_frameIndex = 0U;

static uint32_t currentPwmClockHz() {
  return 16000000UL >> g_pwm.prescaler();
}

static uint32_t waveformFrequencyHz(uint16_t countertop) {
  countertop = Pwm::clampCountertop(countertop);
  return currentPwmClockHz() / static_cast<uint32_t>(countertop);
}

static void buildSequenceWords() {
  for (uint8_t frame = 0U; frame < (sizeof(kFrames) / sizeof(kFrames[0])); ++frame) {
    const FrameConfig& cfg = kFrames[frame];
    const uint16_t top = Pwm::clampCountertop(cfg.countertop);
    g_sequenceWords[frame * 4U + 0U] =
        Pwm::encodeSequenceWordPermille(cfg.duty0Permille, top, true);
    g_sequenceWords[frame * 4U + 1U] =
        Pwm::encodeSequenceWordPermille(cfg.duty1Permille, top, true);
    g_sequenceWords[frame * 4U + 2U] =
        Pwm::encodeSequenceWordPermille(cfg.duty2Permille, top, true);
    g_sequenceWords[frame * 4U + 3U] = top;
  }
}

static void printFrame(uint8_t frameIndex) {
  const FrameConfig& cfg = kFrames[frameIndex];
  Serial.print("frame=");
  Serial.print(frameIndex);
  Serial.print(" top=");
  Serial.print(Pwm::clampCountertop(cfg.countertop));
  Serial.print(" hz~");
  Serial.print(waveformFrequencyHz(cfg.countertop));
  Serial.print(" d0=");
  Serial.print(cfg.duty0Permille);
  Serial.print(" d1=");
  Serial.print(cfg.duty1Permille);
  Serial.print(" d2=");
  Serial.println(cfg.duty2Permille);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println("PwmWaveFormSequenceStepper");
  Serial.println("PWM20 waveform-load sequence stepper on D0-D2.");
  Serial.println("Word 3 of each frame is the live COUNTERTOP, so D3 is not part of this mode.");

  if (!g_pwm.beginRaw(kPins, 3U, kBaseFrequencyHz, Pwm::DecoderLoad::kWaveForm,
                      Pwm::DecoderMode::kNextStep)) {
    Serial.println("PWM beginRaw failed");
    while (true) {
      delay(1000);
    }
  }

  buildSequenceWords();
  if (!g_pwm.setSequence(
          0U, g_sequenceWords,
          static_cast<uint16_t>(sizeof(g_sequenceWords) / sizeof(g_sequenceWords[0])))) {
    Serial.println("PWM setSequence failed");
    while (true) {
      delay(1000);
    }
  }

  if (!g_pwm.start(0U)) {
    Serial.println("PWM start failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.print("base_countertop=");
  Serial.println(g_pwm.countertop());
  Serial.print("pwm_clock_hz=");
  Serial.println(currentPwmClockHz());
  printFrame(g_frameIndex);
}

void loop() {
  delay(kStepHoldMs);

  if (!g_pwm.triggerNextStep()) {
    Serial.println("nextstep_failed");
    return;
  }

  g_frameIndex = static_cast<uint8_t>(
      (g_frameIndex + 1U) % (sizeof(kFrames) / sizeof(kFrames[0])));
  printFrame(g_frameIndex);

  if (g_pwm.pollRamUnderflow(true)) {
    Serial.println("ram_underflow");
  }
}
