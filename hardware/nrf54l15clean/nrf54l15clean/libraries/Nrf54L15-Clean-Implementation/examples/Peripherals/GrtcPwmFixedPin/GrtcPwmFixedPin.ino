#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

#if defined(PIN_WIRE1_SCL)
static constexpr uint8_t kGrtcPwmPin = PIN_WIRE1_SCL;
static constexpr char kGrtcPwmPinLabel[] = "PIN_WIRE1_SCL / P0.03";
#elif defined(PIN_D11)
static constexpr uint8_t kGrtcPwmPin = PIN_D11;
static constexpr char kGrtcPwmPinLabel[] = "PIN_D11 / P0.03";
#else
#error "This board variant does not expose a known Arduino alias for the fixed GRTC PWM pin."
#endif

static constexpr uint8_t kDutyCodes[] = {32U, 64U, 128U, 192U, 224U};
static constexpr uint32_t kDutyHoldMs = 2000UL;
static constexpr uint32_t kConfigLatchTimeoutUs = 20000UL;

static GrtcPwm g_pwm;
static uint8_t g_index = 0U;

static uint32_t dutyPercent(uint8_t duty8) {
  return (static_cast<uint32_t>(duty8) * 100UL + 127UL) / 255UL;
}

static void applyDuty(uint8_t duty8) {
  const bool dutyOk = g_pwm.setDuty8(duty8);
  bool latched = false;
  const uint32_t startUs = micros();
  while ((micros() - startUs) < kConfigLatchTimeoutUs) {
    if (g_pwm.pollPeriodEnd(true)) {
      latched = true;
      break;
    }
  }

  Serial.print("duty8=");
  Serial.print(duty8);
  Serial.print(" duty~");
  Serial.print(dutyPercent(duty8));
  Serial.print("% latch=");
  Serial.println((dutyOk && latched) ? "ok" : "timeout");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println("GrtcPwmFixedPin");
  Serial.println("Single-pin 8-bit GRTC PWM. Fixed silicon pin only.");
  Serial.print("Pin: ");
  Serial.println(kGrtcPwmPinLabel);
  Serial.print("Frequency: ");
  Serial.print(GrtcPwm::frequencyHz());
  Serial.println(" Hz");
  Serial.println("This sketch checks duty-write latch timing on the fixed GRTC pin.");
  Serial.println("Use an exposed P0.03 board and a scope/logic analyzer to verify the public waveform.");
  Serial.println("On XIAO Sense this pin is shared with the Wire1/IMU route.");

  (void)BoardControl::setBatterySenseEnabled(false);
  (void)BoardControl::setImuMicEnabled(false);
  delay(5);

  if (!GrtcPwm::supportsArduinoPin(kGrtcPwmPin)) {
    Serial.println("This board alias does not map to the GRTC PWM pin.");
    while (true) {
      delay(1000);
    }
  }

  if (!g_pwm.beginArduinoPin(kGrtcPwmPin, kDutyCodes[0], GrtcClockSource::kLfxo,
                             true)) {
    Serial.println("GRTC PWM begin failed");
    while (true) {
      delay(1000);
    }
  }

  g_pwm.enablePeriodEndEvent(true);
  applyDuty(kDutyCodes[0]);
}

void loop() {
  delay(kDutyHoldMs);

  g_index = static_cast<uint8_t>((g_index + 1U) %
                                 (sizeof(kDutyCodes) / sizeof(kDutyCodes[0])));
  applyDuty(kDutyCodes[g_index]);
}
