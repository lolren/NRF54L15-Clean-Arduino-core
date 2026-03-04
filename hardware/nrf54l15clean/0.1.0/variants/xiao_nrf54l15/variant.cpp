#include "Arduino.h"
#include "variant.h"

#include <nrf54l15.h>

namespace {

static constexpr uint8_t kRfSwitchPowerPort = 2U;
static constexpr uint8_t kRfSwitchPowerPin = 3U;
static constexpr uint8_t kRfSwitchCtlPort = 2U;
static constexpr uint8_t kRfSwitchCtlPin = 5U;
static constexpr uint8_t kBatteryEnablePort = 1U;
static constexpr uint8_t kBatteryEnablePin = 15U;
static constexpr uint8_t kImuMicEnablePort = 0U;
static constexpr uint8_t kImuMicEnablePin = 1U;
static constexpr uint8_t kRfSwitchCeramic = 0U;
static constexpr uint8_t kRfSwitchExternal = 1U;

static xiao_nrf54l15_antenna_t g_antenna = XIAO_NRF54L15_ANTENNA_CERAMIC;
static uint8_t g_rfSwitchPower = 0U;
static uint8_t g_batteryEnable = 0U;
static uint8_t g_imuMicEnable = 0U;

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

static inline void applyRfSwitchPower(bool enable) {
    gpioSetOutput(kRfSwitchPowerPort, kRfSwitchPowerPin, enable);
    g_rfSwitchPower = enable ? 1U : 0U;
}

static inline void applyBatteryEnable(bool enable) {
    gpioSetOutput(kBatteryEnablePort, kBatteryEnablePin, enable);
    g_batteryEnable = enable ? 1U : 0U;
}

static inline void applyImuMicEnable(bool enable) {
    gpioSetOutput(kImuMicEnablePort, kImuMicEnablePin, enable);
    g_imuMicEnable = enable ? 1U : 0U;
}

static inline void applyRfSwitchSelection(uint8_t selection) {
    const uint8_t normalized = (selection == kRfSwitchExternal) ? kRfSwitchExternal : kRfSwitchCeramic;
    applyRfSwitchPower(true);
    gpioSetOutput(kRfSwitchCtlPort, kRfSwitchCtlPin, normalized == kRfSwitchExternal);
    g_antenna = (normalized == kRfSwitchExternal) ? XIAO_NRF54L15_ANTENNA_EXTERNAL
                                                  : XIAO_NRF54L15_ANTENNA_CERAMIC;
}

}  // namespace

extern "C" uint8_t arduinoXiaoNrf54l15SetAntenna(uint8_t selection) {
    applyRfSwitchSelection(selection);
    return 1U;
}

extern "C" uint8_t arduinoXiaoNrf54l15GetAntenna(void) {
    return (g_antenna == XIAO_NRF54L15_ANTENNA_EXTERNAL) ? kRfSwitchExternal : kRfSwitchCeramic;
}

extern "C" uint8_t arduinoXiaoNrf54l15SetRfSwitchPower(uint8_t enabled) {
    applyRfSwitchPower(enabled != 0U);
    return 1U;
}

extern "C" uint8_t arduinoXiaoNrf54l15GetRfSwitchPower(void) {
    return g_rfSwitchPower;
}

extern "C" uint8_t arduinoXiaoNrf54l15SetBatteryEnable(uint8_t enabled) {
    applyBatteryEnable(enabled != 0U);
    return 1U;
}

extern "C" uint8_t arduinoXiaoNrf54l15GetBatteryEnable(void) {
    return g_batteryEnable;
}

extern "C" uint8_t arduinoXiaoNrf54l15SetImuMicEnable(uint8_t enabled) {
    applyImuMicEnable(enabled != 0U);
    return 1U;
}

extern "C" uint8_t arduinoXiaoNrf54l15GetImuMicEnable(void) {
    return g_imuMicEnable;
}

extern "C" void xiaoNrf54l15SetAntenna(xiao_nrf54l15_antenna_t antenna) {
    switch (antenna) {
        case XIAO_NRF54L15_ANTENNA_EXTERNAL:
            applyRfSwitchSelection(kRfSwitchExternal);
            break;
        case XIAO_NRF54L15_ANTENNA_CONTROL_HIZ:
            g_antenna = XIAO_NRF54L15_ANTENNA_CONTROL_HIZ;
            gpioSetInputHighZ(kRfSwitchCtlPort, kRfSwitchCtlPin);
            break;
        case XIAO_NRF54L15_ANTENNA_CERAMIC:
        default:
            applyRfSwitchSelection(kRfSwitchCeramic);
            break;
    }
}

extern "C" xiao_nrf54l15_antenna_t xiaoNrf54l15GetAntenna(void) {
    return g_antenna;
}

extern "C" void initVariant(void) {
    applyImuMicEnable(false);
    applyBatteryEnable(false);

#if defined(NRF54L15_CLEAN_ANTENNA_EXTERNAL)
    xiaoNrf54l15SetAntenna(XIAO_NRF54L15_ANTENNA_EXTERNAL);
#else
    xiaoNrf54l15SetAntenna(XIAO_NRF54L15_ANTENNA_CERAMIC);
#endif
}
