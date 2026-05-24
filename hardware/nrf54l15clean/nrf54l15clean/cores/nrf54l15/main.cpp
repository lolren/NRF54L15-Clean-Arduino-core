#include "Arduino.h"
#include "cmsis.h"

extern "C" void nrf54l15_clean_idle_service(void);
extern "C" void nrf54l15_clean_yield_service(void);
extern "C" void nrf54l15_software_timer_service(void);
extern "C" uint32_t nrf54l15_clean_ble_idle_sleep_cap_us(void)
    __attribute__((weak));
extern "C" uint8_t nrf54l15_clean_ble_idle_yield_wfi_allowed(void)
    __attribute__((weak));
extern "C" size_t nrf54l15_heap_total_bytes(void);
extern "C" size_t nrf54l15_heap_used_bytes(void);
extern "C" size_t nrf54l15_heap_free_bytes(void);
#if defined(NRF54L15_CLEAN_POWER_LOW)
extern "C" void nrf54l15_core_bootstrap_low_power_timebase(void);
#endif

namespace {
volatile bool g_loop_suspended = false;
static volatile uint32_t* const kScbScr =
    reinterpret_cast<volatile uint32_t*>(0xE000ED10UL);
static constexpr uint32_t kScbScrSleepDeep_Msk = (1UL << 2);
static constexpr uint32_t kScbScrSleepOnExit_Msk = (1UL << 1);

#if defined(NRF54L15_CLEAN_POWER_LOW)
void disableSysTickForLowPowerProfile() {
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    static volatile uint32_t* const kScbIcsr =
        reinterpret_cast<volatile uint32_t*>(0xE000ED04UL);
    static constexpr uint32_t kScbIcsrPendstclr = (1UL << 25);
    *kScbIcsr = kScbIcsrPendstclr;
    __DSB();
    __ISB();
}

void yieldWfiOnceIfAllowed() {
    if ((__get_PRIMASK() & 1U) != 0U ||
        nrf54l15_clean_ble_idle_yield_wfi_allowed == nullptr ||
        nrf54l15_clean_ble_idle_yield_wfi_allowed() == 0U) {
        __asm volatile("nop");
        return;
    }

    const uint32_t restoreRaw = nrf54l15_core_enter_idle_cpu_scaling();
    *kScbScr &= ~(kScbScrSleepDeep_Msk | kScbScrSleepOnExit_Msk);
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
    __asm volatile("wfi");
    nrf54l15_core_exit_idle_cpu_scaling(restoreRaw);
}
#endif
}  // namespace

extern "C" void __attribute__((weak)) init(void) {
#if !defined(NRF54L15_CLEAN_LOWPOWER_BOOT_MINIMAL)
#if defined(NRF54L15_CLEAN_POWER_LOW)
    // Low-power builds use the GRTC-backed monotonic clock for millis()/micros().
    // Leaving the 1 kHz SysTick running destroys tickless idle current.
    disableSysTickForLowPowerProfile();
#else
    initSysTick();
#endif
#endif
}
extern "C" void __attribute__((weak)) initVariant(void) {}
extern "C" void __attribute__((weak)) yield(void) {
    nrf54l15_software_timer_service();
    nrf54l15_clean_yield_service();
#if defined(NRF54L15_CLEAN_POWER_LOW)
    yieldWfiOnceIfAllowed();
    return;
#else
    // Balanced profile keeps the foreground loop pump-driven. SysTick is
    // CPU-clocked on this target and is not a safe standalone wake source once
    // WFI gates the CPU clock. Low Power builds use the GRTC-backed delay path
    // when real tickless sleep is needed.
    if ((__get_PRIMASK() & 1U) != 0U) {
        __asm volatile("nop");
        return;
    }
    if (nrf54l15_clean_ble_idle_sleep_cap_us != nullptr) {
        const uint32_t sleepCapUs = nrf54l15_clean_ble_idle_sleep_cap_us();
        // Bluefruit / BLE CPUAPP flows are still pump-driven whenever the BLE
        // layer reports an active timing budget. A 1 ms SysTick wake cadence
        // is still too coarse for foreground advertising / connect windows on
        // some stacks, so Balanced mode must stay out of WFI for any active
        // BLE slice and leave low-power WFI behavior to clean_power=low.
        if (sleepCapUs != 0U) {
            __asm volatile("nop");
            return;
        }
    }
    __asm volatile("nop");
    return;
#endif
}

extern "C" void __attribute__((weak)) softReset(void) {
    static constexpr uintptr_t kScbAircr = 0xE000ED0CUL;
    static constexpr uint32_t kAircrVectkey = (0x5FAUL << 16);
    static constexpr uint32_t kAircrSysResetReq = (1UL << 2);

    __DSB();
    *reinterpret_cast<volatile uint32_t*>(kScbAircr) =
        kAircrVectkey | kAircrSysResetReq;
    __DSB();
    __ISB();
    while (true) {
        __NOP();
    }
}

extern "C" void __attribute__((weak)) SoftReset(void) {
    softReset();
}

extern "C" void __attribute__((weak)) suspendLoop(void) {
    g_loop_suspended = true;
}

extern "C" void __attribute__((weak)) resumeLoop(void) {
    g_loop_suspended = false;
}

extern "C" uint32_t __attribute__((weak)) getFreeHeap(void) {
    return static_cast<uint32_t>(nrf54l15_heap_free_bytes());
}

extern "C" uint32_t __attribute__((weak)) getFreeHeapSize(void) {
    return getFreeHeap();
}

extern "C" int __attribute__((weak)) dbgHeapTotal(void) {
    return static_cast<int>(nrf54l15_heap_total_bytes());
}

extern "C" int __attribute__((weak)) dbgHeapUsed(void) {
    return static_cast<int>(nrf54l15_heap_used_bytes());
}

extern "C" void setup(void) __attribute__((weak));
extern "C" void loop(void) __attribute__((weak));

int __attribute__((weak)) main(void) {
    init();
    initVariant();

    if (setup != nullptr) {
        setup();
    }

    while (true) {
        if (!g_loop_suspended && loop != nullptr) {
            loop();
        }
        Scheduler.run();
        nrf54l15_clean_idle_service();
        yield();
    }

    return 0;
}
