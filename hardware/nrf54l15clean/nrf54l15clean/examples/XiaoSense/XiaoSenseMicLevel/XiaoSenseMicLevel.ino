/*
  XiaoSenseMicLevel

  Sense-only microphone level example for the onboard MSM261DGT006.

  What it does:
  - enables the shared IMU/MIC rail with IMU_MIC_EN
  - captures mono PDM audio from the onboard microphone
  - prints min/max, peak-to-peak, mean absolute level, and peak absolute level

  Usage:
  - open Serial Monitor at 115200
  - talk, tap the desk, or blow near the mic and watch the level change
*/

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static Pdm g_pdm;
static int16_t g_samples[512];

static int16_t absoluteI16(int16_t value) {
  if (value == INT16_MIN) {
    return INT16_MAX;
  }
  return (value < 0) ? static_cast<int16_t>(-value) : value;
}

void setup() {
  Serial.begin(115200);
  delay(250);

  BoardControl::setImuMicEnabled(true);
  delay(10);

  const bool ok =
      g_pdm.begin(kPinMicClk, kPinMicData, true, 40U, PDM_RATIO_RATIO_Ratio64,
                  PdmEdge::kLeftRising);

  Serial.println("XiaoSenseMicLevel");
  Serial.print("pdm_begin=");
  Serial.println(ok ? "ok" : "fail");
}

void loop() {
  if (!g_pdm.capture(g_samples, sizeof(g_samples) / sizeof(g_samples[0]),
                     1200000UL)) {
    Serial.println("mic=capture-failed");
    delay(250);
    return;
  }

  int16_t minimum = INT16_MAX;
  int16_t maximum = INT16_MIN;
  uint32_t meanAbs = 0UL;
  int16_t peakAbs = 0;

  for (size_t i = 0U; i < (sizeof(g_samples) / sizeof(g_samples[0])); ++i) {
    const int16_t sample = g_samples[i];
    if (sample < minimum) {
      minimum = sample;
    }
    if (sample > maximum) {
      maximum = sample;
    }

    const int16_t absValue = absoluteI16(sample);
    meanAbs += static_cast<uint32_t>(absValue);
    if (absValue > peakAbs) {
      peakAbs = absValue;
    }
  }

  meanAbs /= (sizeof(g_samples) / sizeof(g_samples[0]));

  Serial.print("mic min=");
  Serial.print(minimum);
  Serial.print(" max=");
  Serial.print(maximum);
  Serial.print(" p2p=");
  Serial.print(static_cast<long>(maximum) - static_cast<long>(minimum));
  Serial.print(" meanAbs=");
  Serial.print(meanAbs);
  Serial.print(" peakAbs=");
  Serial.println(peakAbs);

  delay(100);
}
