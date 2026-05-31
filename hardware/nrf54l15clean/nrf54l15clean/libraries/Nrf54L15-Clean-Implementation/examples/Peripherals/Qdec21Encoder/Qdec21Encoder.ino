/*
 * QDEC21 Encoder - Second Quadrature Decoder Instance
 *
 * Demonstrates the public HAL wrapper on the second QDEC block:
 *
 *   Qdec encoder(nrf54l15::QDEC21_BASE);
 *
 * Pins below are raw nRF54L15 port pins for an external quadrature encoder.
 * Change them to match your board wiring. The wrapper configures input pulls,
 * debounce, start/stop, accumulator reads, and pin disconnection on end().
 */

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static constexpr Pin kQdec21A{0, 10};
static constexpr Pin kQdec21B{0, 11};
static constexpr uint32_t kReportEveryMs = 500UL;

static Qdec g_qdec(nrf54l15::QDEC21_BASE);
static int32_t g_totalSteps = 0;

static void printPin(const __FlashStringHelper* label, const Pin& pin) {
  Serial.print(label);
  Serial.print(F("P"));
  Serial.print(pin.port);
  Serial.print(F("."));
  Serial.println(pin.pin);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println();
  Serial.println(F("QDEC21 Encoder wrapper example"));
  Serial.print(F("QDEC21 base: 0x"));
  Serial.println(nrf54l15::QDEC21_BASE, HEX);
  printPin(F("A: "), kQdec21A);
  printPin(F("B: "), kQdec21B);

  if (!g_qdec.begin(kQdec21A, kQdec21B, QdecSamplePeriod::k1024us,
                    QdecReportPeriod::k1Sample, true, QdecInputPull::kPullUp)) {
    Serial.println(F("ERROR: QDEC21 begin failed. Check pins and board support."));
    return;
  }

  g_qdec.start();
  Serial.println(F("Rotate the encoder to see accumulator changes."));
}

void loop() {
  static uint32_t lastReportMs = 0;
  const uint32_t nowMs = millis();
  if ((nowMs - lastReportMs) < kReportEveryMs) {
    return;
  }
  lastReportMs = nowMs;

  const int32_t delta = g_qdec.readAndClearAccumulator();
  const uint32_t doubleTransitions = g_qdec.readAndClearDoubleTransitions();
  g_totalSteps += delta;

  Serial.print(F("delta="));
  Serial.print(delta);
  Serial.print(F(" total="));
  Serial.print(g_totalSteps);
  Serial.print(F(" sample="));
  Serial.print(g_qdec.sampleValue());
  Serial.print(F(" double_transitions="));
  Serial.println(doubleTransitions);

  if (g_qdec.pollOverflow()) {
    Serial.println(F("warning: QDEC21 accumulator overflow"));
  }
}
