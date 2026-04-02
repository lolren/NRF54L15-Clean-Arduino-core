#ifndef NRF52_COMPAT_H_
#define NRF52_COMPAT_H_

#include <stdint.h>
#include "nrf54l15_types.h"

#ifndef NRF_FICR
#define NRF_FICR (reinterpret_cast<NRF_FICR_Type*>(0x00FFC000UL))
#endif

static inline uint32_t SysTick_Config(uint32_t ticks) {
  (void)ticks;
  return 0UL;
}

void sd_power_system_off(void);
void NVIC_SystemReset(void);
void enterOTADfu(void);
void enterSerialDfu(void);
void dbgPrintVersion(void);
void dbgMemInfo(void);

class SchedulerClass {
 public:
  void startLoop(void (*fn)(void));
  void run(void);

 private:
  void (*loop_fn_)(void) = nullptr;
};

extern SchedulerClass Scheduler;

class HwPWMCompat {
 public:
  void addPin(uint8_t pin);
  void begin(void) {}
  void setResolution(uint8_t bits);
  void setClockDiv(uint32_t div) { (void)div; }
  void writePin(uint8_t pin, uint32_t value, bool invert = false);

 private:
  uint8_t resolution_bits_ = 8U;
};

extern HwPWMCompat HwPWM0;
extern HwPWMCompat HwPWM1;

#endif  // NRF52_COMPAT_H_
