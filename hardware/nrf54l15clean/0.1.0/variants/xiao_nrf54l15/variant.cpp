#include "Arduino.h"
#include "variant.h"

#include <nrf54l15.h>

namespace {

static xiao_nrf54l15_antenna_t g_antenna = XIAO_NRF54L15_ANTENNA_CERAMIC;

static inline NRF_GPIO_Type* gpioForPort(uint8_t port) {
    switch (port) {
        case 0: return NRF_P0;
        case 1: return NRF_P1;
        case 2: return NRF_P2;
        default: return nullptr;
    }
}

static inline void gpioSetOutput(uint8_t port, uint8_t pin, bool high) {
    NRF_GPIO_Type* gpio = gpioForPort(port);
    if (gpio == nullptr || pin > 31U) {
        return;
    }
    const uint32_t bit = (1UL << pin);
    gpio->DIRSET = bit;
    if (high) {
        gpio->OUTSET = bit;
    } else {
        gpio->OUTCLR = bit;
    }
}

static inline void gpioSetInputHighZ(uint8_t port, uint8_t pin) {
    NRF_GPIO_Type* gpio = gpioForPort(port);
    if (gpio == nullptr || pin > 31U) {
        return;
    }

    const uint32_t bit = (1UL << pin);
    uint32_t cnf = gpio->PIN_CNF[pin];
    cnf &= ~(GPIO_PIN_CNF_DIR_Msk |
             GPIO_PIN_CNF_INPUT_Msk |
             GPIO_PIN_CNF_PULL_Msk |
             GPIO_PIN_CNF_SENSE_Msk);
    cnf |= (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos);
    cnf |= GPIO_PIN_CNF_INPUT_Disconnect;
    cnf |= GPIO_PIN_CNF_PULL_Disabled;
    cnf |= GPIO_PIN_CNF_SENSE_Disabled;
    gpio->DIRCLR = bit;
    gpio->PIN_CNF[pin] = cnf;
}

}  // namespace

extern "C" void xiaoNrf54l15SetAntenna(xiao_nrf54l15_antenna_t antenna) {
    switch (antenna) {
        case XIAO_NRF54L15_ANTENNA_EXTERNAL:
            g_antenna = XIAO_NRF54L15_ANTENNA_EXTERNAL;
            gpioSetOutput(2U, 5U, true);
            break;
        case XIAO_NRF54L15_ANTENNA_CONTROL_HIZ:
            g_antenna = XIAO_NRF54L15_ANTENNA_CONTROL_HIZ;
            gpioSetInputHighZ(2U, 5U);
            break;
        case XIAO_NRF54L15_ANTENNA_CERAMIC:
        default:
            g_antenna = XIAO_NRF54L15_ANTENNA_CERAMIC;
            gpioSetOutput(2U, 5U, false);
            break;
    }
}

extern "C" xiao_nrf54l15_antenna_t xiaoNrf54l15GetAntenna(void) {
    return g_antenna;
}

extern "C" void initVariant(void) {
#if defined(NRF54L15_CLEAN_ANTENNA_EXTERNAL)
    xiaoNrf54l15SetAntenna(XIAO_NRF54L15_ANTENNA_EXTERNAL);
#else
    xiaoNrf54l15SetAntenna(XIAO_NRF54L15_ANTENNA_CERAMIC);
#endif
}
