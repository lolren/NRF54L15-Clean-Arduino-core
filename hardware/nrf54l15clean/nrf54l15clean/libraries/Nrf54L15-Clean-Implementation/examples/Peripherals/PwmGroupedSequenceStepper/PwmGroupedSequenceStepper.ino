#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr Pin kPins[] = {kPinD0, kPinD1, kPinD2, kPinD3};
static constexpr uint32_t kFrequencyHz = 2000UL;
static constexpr uint32_t kHoldMs = 2000UL;
static constexpr uint16_t kPairPermille[][2] = {
    {100U, 700U},
    {300U, 500U},
    {700U, 250U},
    {500U, 900U},
};

static Pwm g_pwm(nrf54l15::PWM20_BASE);
static uint16_t g_sequenceWords[(sizeof(kPairPermille) / sizeof(kPairPermille[0])) * 2U];
static uint8_t g_frameIndex = 0U;

static void buildSequenceWords() {
  const uint16_t top = g_pwm.countertop();
  for (uint8_t frame = 0U;
       frame < (sizeof(kPairPermille) / sizeof(kPairPermille[0])); ++frame) {
    g_sequenceWords[frame * 2U + 0U] =
        Pwm::encodeSequenceWordPermille(kPairPermille[frame][0], top, true);
    g_sequenceWords[frame * 2U + 1U] =
        Pwm::encodeSequenceWordPermille(kPairPermille[frame][1], top, true);
  }
}

static void printFrame(uint8_t frameIndex) {
  Serial.print("frame=");
  Serial.print(frameIndex);
  Serial.print(" d01_permille=");
  Serial.print(kPairPermille[frameIndex][0]);
  Serial.print(" d23_permille=");
  Serial.println(kPairPermille[frameIndex][1]);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println("PwmGroupedSequenceStepper");
  Serial.println("PWM20 grouped-sequence stepper on D0-D3.");
  Serial.println("D0/D1 share one grouped word. D2/D3 share the second grouped word.");

  if (!g_pwm.beginRaw(kPins, 4U, kFrequencyHz, Pwm::DecoderLoad::kGrouped,
                      Pwm::DecoderMode::kNextStep, 0U)) {
    Serial.println("PWM beginRaw failed");
    while (true) {
      delay(1000);
    }
  }

  buildSequenceWords();

  if (!g_pwm.setSequence(0U, g_sequenceWords,
                         static_cast<uint16_t>(sizeof(g_sequenceWords) /
                                               sizeof(g_sequenceWords[0])))) {
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
  Serial.print("configured_mask=0x");
  Serial.println(g_pwm.configuredChannelMask(), HEX);
  printFrame(g_frameIndex);
}

void loop() {
  delay(kHoldMs);

  if (!g_pwm.triggerNextStep()) {
    Serial.println("nextstep_failed");
    return;
  }

  g_frameIndex = static_cast<uint8_t>(
      (g_frameIndex + 1U) % (sizeof(kPairPermille) / sizeof(kPairPermille[0])));
  printFrame(g_frameIndex);

  if (g_pwm.pollRamUnderflow(true)) {
    Serial.println("ram_underflow");
  }
}
