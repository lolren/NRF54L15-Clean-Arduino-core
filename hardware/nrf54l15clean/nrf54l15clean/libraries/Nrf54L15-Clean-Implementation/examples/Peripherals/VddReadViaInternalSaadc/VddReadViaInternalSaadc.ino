/*
  VddReadViaInternalSaadc

  Reads the nRF54L15 chip supply rail through the SAADC internal VDD path.

  Notes:
  - this does not consume any external analog pin
  - this reports the MCU VDD rail seen by the chip
  - on regulated boards, VDD is not the same thing as raw battery voltage
  - for raw battery on XIAO-style boards, use BoardControl::sampleBatteryMilliVolts(...)
  - for threshold warning instead of ADC sampling, see PofWarningMonitor
  - advanced code can also use Saadc + AdcInternalInput::kVdd directly
*/

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr uint32_t kSamplePeriodMs = 1000UL;
static constexpr uint32_t kSpinLimit = 400000UL;

bool sampleVddMilliVolts(int32_t* outMilliVolts) {
  return BoardControl::sampleVddMilliVolts(outMilliVolts, kSpinLimit);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("VddReadViaInternalSaadc");
  Serial.println("Sampling the chip VDD rail through the core VDD helper.");
  Serial.println("No external analog pin is used.");
  Serial.println("On regulated boards this is VDD, not raw battery voltage.");
}

void loop() {
  int32_t vddMilliVolts = 0;
  if (sampleVddMilliVolts(&vddMilliVolts)) {
    Serial.print("vdd_mv=");
    Serial.println(vddMilliVolts);
  } else {
    Serial.println("vdd_sample_failed");
  }

  delay(kSamplePeriodMs);
}
