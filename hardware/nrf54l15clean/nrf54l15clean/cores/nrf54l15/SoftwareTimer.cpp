#include "utility/SoftwareTimer.h"

namespace {

bool timeReached(uint32_t now_ms, uint32_t target_ms) {
  return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

}  // namespace

SoftwareTimer* SoftwareTimer::head_ = nullptr;

SoftwareTimer::SoftwareTimer()
    : next_(head_),
      period_ms_(0U),
      next_fire_ms_(0U),
      callback_(nullptr),
      timer_id_(nullptr),
      repeating_(true),
      active_(false) {
  head_ = this;
}

SoftwareTimer::~SoftwareTimer() {
  SoftwareTimer** current = &head_;
  while (*current != nullptr) {
    if (*current == this) {
      *current = next_;
      break;
    }
    current = &((*current)->next_);
  }
}

void SoftwareTimer::begin(uint32_t ms, TimerCallbackFunction_t callback, void* timerID,
                          bool repeating) {
  period_ms_ = ms;
  callback_ = callback;
  timer_id_ = timerID;
  repeating_ = repeating;
  active_ = false;
  next_fire_ms_ = 0U;
}

void SoftwareTimer::setID(void* id) { timer_id_ = id; }

void* SoftwareTimer::getID(void) { return timer_id_; }

bool SoftwareTimer::start(void) {
  if (callback_ == nullptr || period_ms_ == 0U) {
    return false;
  }
  active_ = true;
  next_fire_ms_ = millis() + period_ms_;
  return true;
}

bool SoftwareTimer::stop(void) {
  active_ = false;
  return true;
}

bool SoftwareTimer::reset(void) {
  if (callback_ == nullptr || period_ms_ == 0U) {
    return false;
  }
  active_ = true;
  next_fire_ms_ = millis() + period_ms_;
  return true;
}

bool SoftwareTimer::setPeriod(uint32_t ms) {
  if (ms == 0U) {
    return false;
  }
  period_ms_ = ms;
  if (active_) {
    next_fire_ms_ = millis() + period_ms_;
  }
  return true;
}

void SoftwareTimer::serviceAll() {
  const uint32_t now_ms = millis();
  for (SoftwareTimer* timer = head_; timer != nullptr; timer = timer->next_) {
    timer->serviceOne(now_ms);
  }
}

void SoftwareTimer::serviceOne(uint32_t now_ms) {
  if (!active_ || callback_ == nullptr || period_ms_ == 0U ||
      !timeReached(now_ms, next_fire_ms_)) {
    return;
  }

  if (repeating_) {
    next_fire_ms_ = now_ms + period_ms_;
  } else {
    active_ = false;
  }

  callback_(this);
}

extern "C" void nrf54l15_software_timer_service(void) {
  SoftwareTimer::serviceAll();
}
