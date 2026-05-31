/*
 * PDM21 Microphone - Second PDM Instance
 *
 * Demonstrates the public HAL wrapper on the second PDM block:
 *
 *   Pdm mic(nrf54l15::PDM21_BASE);
 *
 * Pins below are raw nRF54L15 port pins for an external digital microphone.
 * Change them to match your board wiring. The XIAO Sense onboard microphone is
 * normally handled by the board audio path; this sketch is for validating and
 * using the separate PDM21 instance.
 */

#include <Arduino.h>
#include <algorithm>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static constexpr Pin kPdm21Clk{0, 4};
static constexpr Pin kPdm21Din{0, 5};
static constexpr size_t kSampleCount = 256;
static constexpr uint32_t kCaptureSpinLimit = 4000000UL;

static Pdm g_pdm(nrf54l15::PDM21_BASE);
alignas(4) static int16_t g_samples[kSampleCount];

static void printPin(const __FlashStringHelper* label, const Pin& pin) {
  Serial.print(label);
  Serial.print(F("P"));
  Serial.print(pin.port);
  Serial.print(F("."));
  Serial.println(pin.pin);
}

static void printSampleSummary() {
  int32_t sum = 0;
  for (size_t i = 0; i < kSampleCount; ++i) {
    sum += g_samples[i];
  }

  const int16_t* minSample = std::min_element(g_samples, g_samples + kSampleCount);
  const int16_t* maxSample = std::max_element(g_samples, g_samples + kSampleCount);

  Serial.println(F("--- First 20 samples ---"));
  const size_t sampleLines = std::min<size_t>(20, kSampleCount);
  for (size_t i = 0; i < sampleLines; ++i) {
    Serial.print(F("  ["));
    Serial.print(i);
    Serial.print(F("]: "));
    Serial.println(g_samples[i]);
  }

  Serial.println();
  Serial.print(F("Average sample: "));
  Serial.println(sum / static_cast<int32_t>(kSampleCount));
  Serial.print(F("Min sample: "));
  Serial.println(*minSample);
  Serial.print(F("Max sample: "));
  Serial.println(*maxSample);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println();
  Serial.println(F("PDM21 Microphone wrapper example"));
  Serial.print(F("PDM21 base: 0x"));
  Serial.println(nrf54l15::PDM21_BASE, HEX);
  printPin(F("CLK: "), kPdm21Clk);
  printPin(F("DIN: "), kPdm21Din);

  if (!g_pdm.begin(kPdm21Clk, kPdm21Din, true, 40, PDM_RATIO_RATIO_Ratio64,
                   PdmEdge::kLeftRising)) {
    Serial.println(F("ERROR: PDM21 begin failed. Check pins and board support."));
    return;
  }

  const bool captured = g_pdm.capture(g_samples, kSampleCount, kCaptureSpinLimit);
  g_pdm.end();

  if (!captured) {
    Serial.println(F("ERROR: PDM21 capture timed out or DMA reported an error."));
    return;
  }

  printSampleSummary();
  Serial.println(F("PDM21 demo complete."));
}

void loop() {
  delay(1000);
}
