#include "nrf54l15_hal_support_internal.h"

#include <Arduino.h>
#include <cmsis.h>
#include <string.h>
#include "variant.h"

extern uint32_t SystemCoreClock;
extern "C" void nrf54l15_core_prepare_system_off_wake_timebase(void)
    __attribute__((weak));
extern "C" void nrf54l15_core_prepare_system_off(void) __attribute__((weak));
extern "C" void nrf54l15_core_disable_system_off_retention(void)
    __attribute__((weak));

namespace xiao_nrf54l15::hal_internal {
namespace {

static constexpr uint16_t kSystemOffTimeoutLfclk = 5U;
static constexpr uint8_t kSystemOffWakeLeadLfclk = 4U;
static constexpr uint32_t kLfclkFrequencyHz = 32768UL;
static constexpr uint32_t kMaxCcLatchWaitUs = 77UL;
static constexpr uint32_t kSystemOffMinimumLatencyGuardUs = 1000UL;
#if defined(ARDUINO_XIAO_NRF54L15)
static constexpr uint32_t kZephyrAllowedCcMaskXiao = 0x67UL;
static constexpr uint8_t kZephyrMainCcChannelXiao = 1U;
#endif

uint32_t lfclkStatSource(NRF_CLOCK_Type* clock) {
  return (clock->LFCLK.STAT & CLOCK_LFCLK_STAT_SRC_Msk) >>
         CLOCK_LFCLK_STAT_SRC_Pos;
}

bool lfclkRunningFrom(NRF_CLOCK_Type* clock, uint32_t src) {
  if (clock == nullptr) {
    return false;
  }

  const uint32_t stat = clock->LFCLK.STAT;
  const bool running =
      ((stat & CLOCK_LFCLK_STAT_STATE_Msk) >> CLOCK_LFCLK_STAT_STATE_Pos) ==
      CLOCK_LFCLK_STAT_STATE_Running;
  return running && (lfclkStatSource(clock) == src);
}

bool waitForLfclkStartedInternal(NRF_CLOCK_Type* clock, uint32_t expectedSrc,
                                 uint32_t spinLimit) {
  if (clock == nullptr) {
    return false;
  }

  while (spinLimit-- > 0U) {
    if (clock->EVENTS_LFCLKSTARTED != 0U &&
        lfclkRunningFrom(clock, expectedSrc)) {
      return true;
    }
  }
  return false;
}

void startLfclkSource(NRF_CLOCK_Type* clock, uint32_t src) {
  if (clock == nullptr) {
    return;
  }

  clock->EVENTS_LFCLKSTARTED = 0U;
  clock->LFCLK.SRC =
      ((src << CLOCK_LFCLK_SRC_SRC_Pos) & CLOCK_LFCLK_SRC_SRC_Msk);
  clock->TASKS_LFCLKSTART = CLOCK_TASKS_LFCLKSTART_TASKS_LFCLKSTART_Trigger;
}

bool lfclkStartAlreadyRequested(NRF_CLOCK_Type* clock, uint32_t expectedSrcCopy) {
  if (clock == nullptr) {
    return false;
  }

  const uint32_t srcCopy = clock->LFCLK.SRCCOPY;
  const uint32_t currentSrc =
      (srcCopy & CLOCK_LFCLK_SRCCOPY_SRC_Msk) >> CLOCK_LFCLK_SRCCOPY_SRC_Pos;
  return currentSrc == expectedSrcCopy;
}

void waitForSystemOffWakeLatch() {
  const uint32_t waitUs =
      (static_cast<uint32_t>(kSystemOffTimeoutLfclk) * 1000000UL) /
          kLfclkFrequencyHz +
      kMaxCcLatchWaitUs;
  busyWaitApproxUs(waitUs);
}

uint8_t systemOffWakeChannel() {
#if defined(ARDUINO_XIAO_NRF54L15)
  const uint32_t available =
      kZephyrAllowedCcMaskXiao & ~(1UL << kZephyrMainCcChannelXiao);
  uint8_t channel = kZephyrMainCcChannelXiao;
  if (tryAllocateHighestSetBit(available, &channel)) {
    return channel;
  }
  return kZephyrMainCcChannelXiao;
#else
  return 1U;
#endif
}

uint32_t systemOffMinimumLatencyUs() {
  return ((((uint32_t)kSystemOffTimeoutLfclk +
            (uint32_t)kSystemOffWakeLeadLfclk) *
           1000000UL) /
          kLfclkFrequencyHz) +
         kSystemOffMinimumLatencyGuardUs;
}

uint32_t clampSystemOffDelayUs(uint32_t delayUs) {
  const uint32_t minimumLatencyUs = systemOffMinimumLatencyUs();
  if (delayUs < minimumLatencyUs) {
    return minimumLatencyUs;
  }
  return delayUs;
}

void configureSystemOffWakeSleep(NRF_GRTC_Type* grtc) {
  uint32_t mode = grtc->MODE;
  mode &= ~GRTC_MODE_AUTOEN_Msk;
  mode &= ~GRTC_MODE_SYSCOUNTEREN_Msk;
  mode |= (GRTC_MODE_AUTOEN_Default << GRTC_MODE_AUTOEN_Pos);
  mode |= (GRTC_MODE_SYSCOUNTEREN_Disabled << GRTC_MODE_SYSCOUNTEREN_Pos);
  grtc->MODE = mode;
  __asm volatile("dsb 0xF" ::: "memory");

  uint32_t clkcfg = grtc->CLKCFG;
  clkcfg &= ~GRTC_CLKCFG_CLKSEL_Msk;
  clkcfg |= (GRTC_CLKCFG_CLKSEL_LFXO << GRTC_CLKCFG_CLKSEL_Pos);
  grtc->CLKCFG = clkcfg;

  grtc->TIMEOUT = ((static_cast<uint32_t>(kSystemOffTimeoutLfclk)
                    << GRTC_TIMEOUT_VALUE_Pos) &
                   GRTC_TIMEOUT_VALUE_Msk);
  grtc->WAKETIME = ((static_cast<uint32_t>(kSystemOffWakeLeadLfclk)
                     << GRTC_WAKETIME_VALUE_Pos) &
                    GRTC_WAKETIME_VALUE_Msk);

  mode &= ~GRTC_MODE_SYSCOUNTEREN_Msk;
  mode |= (GRTC_MODE_SYSCOUNTEREN_Enabled << GRTC_MODE_SYSCOUNTEREN_Pos);
  grtc->MODE = mode;
  __asm volatile("dsb 0xF" ::: "memory");
}

void armSystemOffWakeCompare(NRF_GRTC_Type* grtc, uint8_t wakeChannel,
                             uint64_t wakeTimestamp) {
  grtc->EVENTS_COMPARE[wakeChannel] = 0U;
  grtc->CC[wakeChannel].CCEN =
      (GRTC_CC_CCEN_ACTIVE_Disable << GRTC_CC_CCEN_ACTIVE_Pos);
  grtc->CC[wakeChannel].CCL =
      static_cast<uint32_t>(wakeTimestamp & 0xFFFFFFFFULL);
  grtc->CC[wakeChannel].CCH =
      (static_cast<uint32_t>((wakeTimestamp >> 32U) & 0xFFFFFUL)
       << GRTC_CC_CCH_CCH_Pos) &
      GRTC_CC_CCH_CCH_Msk;
  NRF54L15_GRTC_INTENSET_REG(grtc) = (1UL << static_cast<uint32_t>(wakeChannel));
  grtc->CC[wakeChannel].CCEN =
      (GRTC_CC_CCEN_ACTIVE_Enable << GRTC_CC_CCEN_ACTIVE_Pos);
}

double adcGainValue(AdcGain gain) {
  switch (gain) {
    case AdcGain::k2:
      return 2.0;
    case AdcGain::k1:
      return 1.0;
    case AdcGain::k2over3:
      return 2.0 / 3.0;
    case AdcGain::k2over4:
      return 2.0 / 4.0;
    case AdcGain::k2over5:
      return 2.0 / 5.0;
    case AdcGain::k2over6:
      return 2.0 / 6.0;
    case AdcGain::k2over7:
      return 2.0 / 7.0;
    case AdcGain::k2over8:
    default:
      return 2.0 / 8.0;
  }
}

uint8_t adcResolutionBits(AdcResolution resolution) {
  return static_cast<uint8_t>(8U + (static_cast<uint8_t>(resolution) * 2U));
}

}  // namespace

uint32_t gpioBaseForPort(uint8_t port) {
  switch (port) {
    case 0:
      return GPIO_P0_BASE;
    case 1:
      return GPIO_P1_BASE;
    case 2:
      return GPIO_P2_BASE;
    default:
      return 0;
  }
}

bool waitForEvent(uint32_t base, uint32_t eventOffset, uint32_t spinLimit) {
  while (spinLimit-- > 0U) {
    if (reg32(base + eventOffset) != 0U) {
      return true;
    }
  }
  return false;
}

bool waitForEventOrError(uint32_t base, uint32_t eventOffset,
                         uint32_t errorOffset, uint32_t spinLimit) {
  while (spinLimit-- > 0U) {
    if (reg32(base + eventOffset) != 0U) {
      return true;
    }
    if (reg32(base + errorOffset) != 0U) {
      return false;
    }
  }
  return false;
}

void clearEvent(uint32_t base, uint32_t eventOffset) {
  reg32(base + eventOffset) = 0U;
}

void clearSystemOffVprRetention() {
  if (MEMCONF_POWER_MaxCount > 1U) {
    NRF_MEMCONF->POWER[1U].RET &= ~MEMCONF_POWER_RET_MEM0_Msk;
  }
}

[[noreturn]] void enterSystemOff(NRF_RESET_Type* reset,
                                 NRF_REGULATORS_Type* regulators,
                                 bool disableRamRetention) {
  if (nrf54l15_core_prepare_system_off != nullptr) {
    nrf54l15_core_prepare_system_off();
  }
  clearSystemOffVprRetention();
  if (disableRamRetention &&
      nrf54l15_core_disable_system_off_retention != nullptr) {
    nrf54l15_core_disable_system_off_retention();
  }

  __asm volatile("cpsid i" ::: "memory");
  __asm volatile("dsb 0xF" ::: "memory");
  __asm volatile("isb 0xF" ::: "memory");
  reset->RESETREAS = 0xFFFFFFFFUL;
  __asm volatile("dsb 0xF" ::: "memory");

  regulators->SYSTEMOFF = REGULATORS_SYSTEMOFF_SYSTEMOFF_Enter;
  __asm volatile("dsb 0xF" ::: "memory");
  while (true) {
    __asm volatile("wfe");
  }
}

bool waitForNonZero(volatile uint32_t* reg, uint32_t spinLimit) {
  if (reg == nullptr) {
    return false;
  }
  while (spinLimit-- > 0U) {
    if (*reg != 0U) {
      return true;
    }
  }
  return false;
}

bool tryAllocateHighestSetBit(uint32_t mask, uint8_t* outBit) {
  if (outBit == nullptr || mask == 0U) {
    return false;
  }
  *outBit =
      static_cast<uint8_t>(31U - static_cast<uint32_t>(__builtin_clz(mask)));
  return true;
}

void ensureLfxoRunning() {
  NRF_CLOCK_Type* const clock = NRF_CLOCK;
  if (clock == nullptr) {
    return;
  }

  if (lfclkRunningFrom(clock, CLOCK_LFCLK_STAT_SRC_LFXO)) {
    return;
  }

  static constexpr uint32_t kLfclkStartSpinLimit = 2000000UL;
  if (!lfclkStartAlreadyRequested(clock, CLOCK_LFCLK_SRCCOPY_SRC_LFXO)) {
    startLfclkSource(clock, CLOCK_LFCLK_SRC_SRC_LFXO);
  }
  (void)waitForLfclkStartedInternal(clock, CLOCK_LFCLK_STAT_SRC_LFXO,
                                    kLfclkStartSpinLimit);
}

void busyWaitApproxUs(uint32_t us) {
  uint32_t cyclesPerUs = SystemCoreClock / 1000000UL;
  if (cyclesPerUs == 0UL) {
    cyclesPerUs = 64UL;
  }

  uint32_t iterations = cyclesPerUs * us;
  if (iterations == 0UL) {
    iterations = 1UL;
  }

  while (iterations-- > 0UL) {
    __NOP();
  }
}

void programSystemOffWake(uint32_t delayUs) {
  delayUs = clampSystemOffDelayUs(delayUs);

  NRF_GRTC_Type* const grtc = NRF_GRTC;
  if (nrf54l15_core_prepare_system_off_wake_timebase != nullptr) {
    nrf54l15_core_prepare_system_off_wake_timebase();
  }
  ensureLfxoRunning();
  configureSystemOffWakeSleep(grtc);

  const uint8_t wakeChannel = systemOffWakeChannel();

  for (uint8_t channel = 0; channel < GRTC_CC_MaxCount; ++channel) {
    NRF54L15_GRTC_INTENCLR_REG(grtc) = (1UL << channel);
    if (channel != wakeChannel) {
      grtc->CC[channel].CCEN =
          (GRTC_CC_CCEN_ACTIVE_Disable << GRTC_CC_CCEN_ACTIVE_Pos);
    }
  }

  ensureGrtcReady(grtc);
  const uint32_t minimumLatencyUs = systemOffMinimumLatencyUs();
  uint32_t wakeDelayUs = delayUs;

  for (uint8_t attempt = 0U; attempt < 2U; ++attempt) {
    const uint64_t wakeTimestamp = readGrtcCounter(grtc) + wakeDelayUs;
    armSystemOffWakeCompare(grtc, wakeChannel, wakeTimestamp);
    waitForSystemOffWakeLatch();

    if (grtc->EVENTS_COMPARE[wakeChannel] == 0U) {
      return;
    }

    const uint64_t now = readGrtcCounter(grtc);
    if (wakeTimestamp > now) {
      grtc->EVENTS_COMPARE[wakeChannel] = 0U;
      return;
    }

    wakeDelayUs += minimumLatencyUs;
  }
}

int32_t adcRawToMilliVolts(int16_t raw, AdcResolution resolution,
                           AdcGain gain, bool differential) {
  const uint32_t bits = adcResolutionBits(resolution);
  uint32_t exponent = bits;
  if (differential && exponent > 0U) {
    --exponent;
  }

  const double scale = static_cast<double>(1UL << exponent);
  const double mv =
      (static_cast<double>(raw) * 900.0) / (adcGainValue(gain) * scale);
  return static_cast<int32_t>(mv >= 0.0 ? (mv + 0.5) : (mv - 0.5));
}

uint32_t saadcPselValue(const Pin& pin) {
  uint32_t psel = 0U;
  psel |= (static_cast<uint32_t>(pin.pin) << saadc::CH_PSEL_PIN_Pos);
  psel |= (static_cast<uint32_t>(pin.port) << saadc::CH_PSEL_PORT_Pos);
  psel |= (saadc::CH_PSEL_CONNECT_ANALOG << saadc::CH_PSEL_CONNECT_Pos);
  return psel;
}

uint32_t spimPrescaler(uint32_t coreHz, uint32_t targetHz, uint32_t minDivisor) {
  if (targetHz == 0U) {
    targetHz = 1000000U;
  }

  uint32_t divisor = coreHz / targetHz;
  if ((coreHz % targetHz) != 0U) {
    ++divisor;
  }

  if (divisor < minDivisor) {
    divisor = minDivisor;
  }
  if ((divisor & 1U) != 0U) {
    ++divisor;
  }
  if (divisor > 126U) {
    divisor = 126U;
  }

  return divisor;
}

void clearTwimState(uint32_t base) {
  clearEvent(base, twim::EVENTS_STOPPED);
  clearEvent(base, twim::EVENTS_ERROR);
  clearEvent(base, twim::EVENTS_LASTRX);
  clearEvent(base, twim::EVENTS_LASTTX);
  clearEvent(base, twim::EVENTS_DMA_RX_END);
  clearEvent(base, twim::EVENTS_DMA_TX_END);
  reg32(base + twim::ERRORSRC) = twim::ERRORSRC_ALL;
}

uint32_t absDiffU32(uint32_t a, uint32_t b) {
  return (a >= b) ? (a - b) : (b - a);
}

uint32_t timerCompareEventOffset(uint8_t channel) {
  return timer::EVENTS_COMPARE +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerCaptureTaskOffset(uint8_t channel) {
  return timer::TASKS_CAPTURE +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerCcOffset(uint8_t channel) {
  return timer::CC + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerOneShotOffset(uint8_t channel) {
  return timer::ONESHOTEN + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerPublishCompareOffset(uint8_t channel) {
  return timer::PUBLISH_COMPARE +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerSubscribeCaptureOffset(uint8_t channel) {
  return timer::SUBSCRIBE_CAPTURE +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t timerCompareIntMask(uint8_t channel) {
  return (1UL << (16U + static_cast<uint32_t>(channel)));
}

bool computePwmTiming(uint32_t targetHz, PwmTiming* timing) {
  if (timing == nullptr || targetHz == 0U) {
    return false;
  }

  uint32_t bestError = 0xFFFFFFFFUL;
  PwmTiming best{0, 0, 0};
  bool found = false;

  for (uint8_t prescaler = 0; prescaler <= 7U; ++prescaler) {
    const uint32_t pwmClk = 16000000UL >> prescaler;
    if (pwmClk == 0U) {
      continue;
    }

    uint32_t top = (pwmClk + (targetHz / 2U)) / targetHz;
    if (top < 3U) {
      top = 3U;
    }
    if (top > 32767U) {
      continue;
    }

    const uint32_t actualHz = pwmClk / top;
    const uint32_t error = absDiffU32(actualHz, targetHz);

    if (!found || error < bestError) {
      found = true;
      bestError = error;
      best.prescaler = prescaler;
      best.countertop = static_cast<uint16_t>(top);
      best.actualHz = actualHz;
      if (error == 0U) {
        break;
      }
    }
  }

  if (!found) {
    return false;
  }

  *timing = best;
  return true;
}

uint32_t pwmTaskSeqStartOffset(uint8_t sequence) {
  return pwm::TASKS_DMA_SEQ_START + (static_cast<uint32_t>(sequence) * 8U);
}

uint32_t pwmEventSeqStartedOffset(uint8_t sequence) {
  return pwm::EVENTS_SEQSTARTED +
         (static_cast<uint32_t>(sequence) * sizeof(uint32_t));
}

uint32_t pwmEventSeqEndOffset(uint8_t sequence) {
  return pwm::EVENTS_SEQEND + (static_cast<uint32_t>(sequence) * sizeof(uint32_t));
}

uint32_t pwmEventDmaSeqEndOffset(uint8_t sequence) {
  return pwm::EVENTS_DMA_SEQ_END + (static_cast<uint32_t>(sequence) * 0x0CU);
}

uint32_t pwmDmaSeqPtrOffset(uint8_t sequence) {
  return pwm::DMA_SEQ_PTR + (static_cast<uint32_t>(sequence) * 8U);
}

uint32_t pwmDmaSeqMaxCntOffset(uint8_t sequence) {
  return pwm::DMA_SEQ_MAXCNT + (static_cast<uint32_t>(sequence) * 8U);
}

uint32_t gpioteInEventOffset(uint8_t channel) {
  return gpiote::EVENTS_IN + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteTaskOutOffset(uint8_t channel) {
  return gpiote::TASKS_OUT + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteTaskSetOffset(uint8_t channel) {
  return gpiote::TASKS_SET + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteTaskClrOffset(uint8_t channel) {
  return gpiote::TASKS_CLR + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteConfigOffset(uint8_t channel) {
  return gpiote::CONFIG + (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteSubscribeOutOffset(uint8_t channel) {
  return gpiote::SUBSCRIBE_OUT +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteSubscribeSetOffset(uint8_t channel) {
  return gpiote::SUBSCRIBE_SET +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

uint32_t gpioteSubscribeClrOffset(uint8_t channel) {
  return gpiote::SUBSCRIBE_CLR +
         (static_cast<uint32_t>(channel) * sizeof(uint32_t));
}

}  // namespace xiao_nrf54l15::hal_internal
