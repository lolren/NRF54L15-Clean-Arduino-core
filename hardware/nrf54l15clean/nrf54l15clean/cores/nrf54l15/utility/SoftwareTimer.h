#ifndef SOFTWARETIMER_H_
#define SOFTWARETIMER_H_

#include "Arduino.h"

class SoftwareTimer;

typedef SoftwareTimer* TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t timer);

class SoftwareTimer {
 public:
  SoftwareTimer();
  virtual ~SoftwareTimer();

  void begin(uint32_t ms, TimerCallbackFunction_t callback, void* timerID = nullptr,
             bool repeating = true);
  TimerHandle_t getHandle(void) { return this; }

  void setID(void* id);
  void* getID(void);

  bool start(void);
  bool stop(void);
  bool reset(void);
  bool setPeriod(uint32_t ms);
  static void serviceAll();

 private:
  static SoftwareTimer* head_;

  SoftwareTimer* next_;
  uint32_t period_ms_;
  uint32_t next_fire_ms_;
  TimerCallbackFunction_t callback_;
  void* timer_id_;
  bool repeating_;
  bool active_;

  void serviceOne(uint32_t now_ms);
};

extern "C" void nrf54l15_software_timer_service(void);

#endif  // SOFTWARETIMER_H_
