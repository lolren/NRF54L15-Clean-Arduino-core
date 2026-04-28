#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr Pin kPins[] = {kPinD0, kPinD1, kPinD2, kPinD3};
static constexpr uint32_t kFrequencyHz = 2000UL;
static constexpr uint32_t kStepHoldMs = 2000UL;
static constexpr uint16_t kDutyPermille[] = {100U, 300U, 500U, 800U};

static Pwm g_pwm(nrf54l15::PWM20_BASE);
static uint16_t g_sequenceWords[sizeof(kDutyPermille) / sizeof(kDutyPermille[0])];
static uint8_t g_stepIndex = 0U;

static void buildSequenceWords() {
  const uint16_t top = g_pwm.countertop();
  for (uint8_t i = 0U; i < (sizeof(kDutyPermille) / sizeof(kDutyPermille[0])); ++i) {
    g_sequenceWords[i] =
        Pwm::encodeSequenceWordPermille(kDutyPermille[i], top, true);
  }
}

static void printStep(uint8_t stepIndex) {
  Serial.print("step=");
  Serial.print(stepIndex);
  Serial.print(" duty_permille=");
  Serial.println(kDutyPermille[stepIndex]);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println("PwmCommonBroadcastStepper");
  Serial.println("PWM20 common-load sequence stepper on D0-D3.");
  Serial.println("All four outputs share one compare word per step.");

  if (!g_pwm.beginRaw(kPins, 4U, kFrequencyHz, Pwm::DecoderLoad::kCommon,
                      Pwm::DecoderMode::kNextStep, 0U,
                      Pwm::CounterMode::kUpDown)) {
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

  Serial.print("frequency_hz=");
  Serial.println(kFrequencyHz);
  Serial.print("countertop=");
  Serial.println(g_pwm.countertop());
  Serial.println("counter_mode=updown");
  printStep(g_stepIndex);
}

void loop() {
  delay(kStepHoldMs);

  if (!g_pwm.triggerNextStep()) {
    Serial.println("nextstep_failed");
    return;
  }

  g_stepIndex = static_cast<uint8_t>(
      (g_stepIndex + 1U) % (sizeof(kDutyPermille) / sizeof(kDutyPermille[0])));
  printStep(g_stepIndex);

  if (g_pwm.pollRamUnderflow(true)) {
    Serial.println("ram_underflow");
  }
}
