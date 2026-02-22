#include "Arduino.h"

extern "C" void nrf54l15_clean_idle_service(void);

extern "C" void __attribute__((weak)) init(void) {
    initSysTick();
}
extern "C" void __attribute__((weak)) initVariant(void) {}
extern "C" void __attribute__((weak)) yield(void) {
    nrf54l15_clean_idle_service();
#if defined(NRF54L15_CLEAN_POWER_LOW)
    __asm volatile("wfi");
#else
    __asm volatile("nop");
#endif
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
        if (loop != nullptr) {
            loop();
        }
        yield();
    }

    return 0;
}
