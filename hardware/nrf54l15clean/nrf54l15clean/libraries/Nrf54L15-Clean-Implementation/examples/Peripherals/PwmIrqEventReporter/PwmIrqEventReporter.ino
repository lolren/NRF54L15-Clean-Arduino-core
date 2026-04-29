#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr Pwm::ChannelConfig kChannels[] = {
    {kPinD0, 250U, true},
    {kPinD1, 750U, true},
};
static constexpr uint32_t kPwmHz = 1000UL;
static constexpr uint32_t kPrintPeriodMs = 1000UL;
static constexpr uint32_t kRestartPeriodMs = 4000UL;
static constexpr uint32_t kDutyStepPeriodMs = 1500UL;
static constexpr uint16_t kDutyPattern[] = {125U, 375U, 625U, 875U};

static Pwm g_pwm(nrf54l15::PWM20_BASE);
static volatile uint32_t g_irqPeriodEndCount = 0U;
static volatile uint32_t g_irqSequenceStarted0Count = 0U;
static volatile uint32_t g_irqStoppedCount = 0U;
static volatile uint32_t g_irqLastMask = 0U;
static uint32_t g_lastPrintMs = 0U;
static uint32_t g_lastRestartMs = 0U;
static uint32_t g_lastDutyStepMs = 0U;
static bool g_running = false;
static uint8_t g_dutyIndex = 0U;

static void pwmIrqCallback(uint32_t irqMask, void* context) {
  (void)context;
  g_irqLastMask = irqMask;
  if ((irqMask & Pwm::kIrqPeriodEnd) != 0U) {
    ++g_irqPeriodEndCount;
  }
  if ((irqMask & Pwm::irqSequenceStartedMask(0U)) != 0U) {
    ++g_irqSequenceStarted0Count;
  }
  if ((irqMask & Pwm::kIrqStopped) != 0U) {
    ++g_irqStoppedCount;
  }
}

static void printSnapshot() {
  noInterrupts();
  const uint32_t periodEndCount = g_irqPeriodEndCount;
  const uint32_t sequenceStarted0Count = g_irqSequenceStarted0Count;
  const uint32_t stoppedCount = g_irqStoppedCount;
  const uint32_t lastMask = g_irqLastMask;
  interrupts();

  Serial.print("running=");
  Serial.print(g_running ? 1 : 0);
  Serial.print(" period_end=");
  Serial.print(periodEndCount);
  Serial.print(" seqstarted0=");
  Serial.print(sequenceStarted0Count);
  Serial.print(" stopped=");
  Serial.print(stoppedCount);
  Serial.print(" last_mask=0x");
  Serial.println(lastMask, HEX);
}

static void stepDutyIfNeeded(uint32_t nowMs) {
  if (!g_running || (nowMs - g_lastDutyStepMs) < kDutyStepPeriodMs) {
    return;
  }

  g_lastDutyStepMs = nowMs;
  g_dutyIndex = static_cast<uint8_t>((g_dutyIndex + 1U) %
                                     (sizeof(kDutyPattern) / sizeof(kDutyPattern[0])));
  const uint16_t duty0 = kDutyPattern[g_dutyIndex];
  const uint16_t duty1 = static_cast<uint16_t>(1000U - duty0);
  (void)g_pwm.setDutyPermille(0U, duty0);
  (void)g_pwm.setDutyPermille(1U, duty1);

  Serial.print("duty_step d0_permille=");
  Serial.print(duty0);
  Serial.print(" d1_permille=");
  Serial.println(duty1);
}

static void restartIfNeeded(uint32_t nowMs) {
  if ((nowMs - g_lastRestartMs) < kRestartPeriodMs) {
    return;
  }

  g_lastRestartMs = nowMs;
  if (g_running) {
    (void)g_pwm.stop();
    g_running = false;
    Serial.println("action=stop");
    return;
  }

  g_running = g_pwm.start(0U);
  Serial.print("action=start ok=");
  Serial.println(g_running ? 1 : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println("PwmIrqEventReporter");
  Serial.println("PWM20 drives D0/D1 and reports IRQ-backed STOPPED, "
                 "SEQSTARTED0, and PWMPERIODEND events.");

  if (!g_pwm.beginChannels(kChannels,
                           static_cast<uint8_t>(sizeof(kChannels) /
                                                sizeof(kChannels[0])),
                           kPwmHz)) {
    Serial.println("pwm begin failed");
    while (true) {
      delay(1000);
    }
  }

  g_pwm.setIrqCallback(pwmIrqCallback);
  if (!g_pwm.makeActive()) {
    Serial.println("pwm makeActive failed");
    while (true) {
      delay(1000);
    }
  }

  g_pwm.enableInterruptMask(Pwm::kIrqStopped |
                            Pwm::irqSequenceStartedMask(0U) |
                            Pwm::kIrqPeriodEnd);

  g_running = g_pwm.start(0U);
  if (!g_running) {
    Serial.println("pwm start failed");
    while (true) {
      delay(1000);
    }
  }

  g_lastPrintMs = millis();
  g_lastRestartMs = g_lastPrintMs;
  g_lastDutyStepMs = g_lastPrintMs;

  Serial.print("pin0=D0 pin1=D1 hz=");
  Serial.print(kPwmHz);
  Serial.print(" countertop=");
  Serial.print(g_pwm.countertop());
  Serial.print(" prescaler=");
  Serial.println(g_pwm.prescaler());
}

void loop() {
  const uint32_t nowMs = millis();
  stepDutyIfNeeded(nowMs);
  restartIfNeeded(nowMs);
  if ((nowMs - g_lastPrintMs) >= kPrintPeriodMs) {
    g_lastPrintMs = nowMs;
    printSnapshot();
  }
}
