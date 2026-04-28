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

static constexpr uint8_t kDuty8 = 128U;
static constexpr uint32_t kSleepMs = 5000UL;

static GrtcPwm g_pwm;
static PowerManager g_power;
__attribute__((section(".noinit"))) static uint32_t g_bootCount;

bool wokeFromGrtcOrSystemOff() {
  const uint32_t resetReason = NRF_RESET->RESETREAS;
  return ((resetReason & RESET_RESETREAS_OFF_Msk) != 0U) ||
         ((resetReason & RESET_RESETREAS_GRTC_Msk) != 0U);
}

}  // namespace

void setup() {
  ++g_bootCount;

  Serial.begin(115200);
  delay(250);

  Serial.println("LowPowerGrtcPwmSystemOff");
  Serial.print("boot=");
  Serial.println(g_bootCount);
  Serial.print("wake_from_grtc_or_off=");
  Serial.println(wokeFromGrtcOrSystemOff() ? 1 : 0);
  Serial.print("pin=");
  Serial.println(kGrtcPwmPinLabel);
  Serial.print("frequency_hz=");
  Serial.println(GrtcPwm::frequencyHz());
  Serial.println("Observe the fixed P0.03 waveform while the MCU drops into SYSTEM OFF.");
  Serial.println("On XIAO Sense this pin is shared with the Wire1/IMU route, so the IMU/mic rail is turned off first.");

  (void)BoardControl::setBatterySenseEnabled(false);
  (void)BoardControl::setImuMicEnabled(false);
  delay(5);

  if (!g_pwm.beginArduinoPin(kGrtcPwmPin, kDuty8, GrtcClockSource::kLfxo, true)) {
    Serial.println("GRTC PWM begin failed");
    while (true) {
      delay(1000);
    }
  }

  delay(100);
  Serial.print("Entering SYSTEM OFF for ");
  Serial.print(kSleepMs);
  Serial.println(" ms");
  Serial.flush();
  g_power.systemOffTimedWakeMs(kSleepMs);
}

void loop() {}
