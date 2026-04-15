#include "Arduino.h"

Nrf52CompatUicr g_nrf52_compat_uicr = {0U};
Nrf52CompatNvmc g_nrf52_compat_nvmc = {0U, NVMC_READY_READY_Ready};

SchedulerClass Scheduler;
HwPWMCompat HwPWM0;
HwPWMCompat HwPWM1;

extern "C" void sd_power_system_off(void) {
  delaySystemOff(0UL);
}

extern "C" void NVIC_SystemReset(void) {
  softReset();
}

extern "C" void enterOTADfu(void) {
  softReset();
}

extern "C" void enterSerialDfu(void) {
  softReset();
}

extern "C" void dbgPrintVersion(void) {
  Serial.print("nRF54L15 clean core ");
  Serial.println(NRF54L15_CLEAN_CORE_VERSION_STRING);
}

extern "C" void dbgMemInfo(void) {
  Serial.print("Heap total: ");
  Serial.println(dbgHeapTotal());
  Serial.print("Heap used: ");
  Serial.println(dbgHeapUsed());
  Serial.print("Free heap: ");
  Serial.println(getFreeHeapSize());
}

void SchedulerClass::startLoop(void (*fn)(void)) {
  loop_fn_ = fn;
}

void SchedulerClass::run(void) {
  if (loop_fn_ != nullptr) {
    loop_fn_();
  }
}

void HwPWMCompat::addPin(uint8_t pin) {
  pinMode(pin, OUTPUT);
}

void HwPWMCompat::setResolution(uint8_t bits) {
  resolution_bits_ = bits;
  analogWriteResolution(bits);
}

void HwPWMCompat::writePin(uint8_t pin, uint32_t value, bool invert) {
  if (resolution_bits_ == 0U) {
    analogWrite(pin, static_cast<int>(value));
    return;
  }

  const uint32_t max_value =
      (resolution_bits_ >= 31U) ? 0x7FFFFFFFUL : ((1UL << resolution_bits_) - 1UL);
  if (value > max_value) {
    value = max_value;
  }
  if (invert) {
    value = max_value - value;
  }
  analogWrite(pin, static_cast<int>(value));
}
