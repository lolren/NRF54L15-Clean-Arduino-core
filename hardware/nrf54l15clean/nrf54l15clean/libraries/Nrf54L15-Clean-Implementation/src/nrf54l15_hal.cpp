#include "nrf54l15_hal.h"
#include "nrf54l15_hal_board_policy_internal.h"
#include "nrf54l15_hal_support_internal.h"
#include "nrf54l15_hal_timebase_internal.h"

#include <Arduino.h>
#include <string.h>
#include <cmsis.h>
#include "variant.h"

extern "C" uint8_t nrf54l15_constlat_users_active(void) __attribute__((weak));
namespace xiao_nrf54l15 {
class I2sTx;
class I2sRx;
class I2sDuplex;
class Pwm;
}

namespace {
xiao_nrf54l15::Pwm* g_activePwm20 = nullptr;
xiao_nrf54l15::Pwm* g_activePwm21 = nullptr;
xiao_nrf54l15::Pwm* g_activePwm22 = nullptr;

xiao_nrf54l15::Pwm** pwmActiveSlotForBase(uint32_t base) {
  switch (base) {
    case nrf54l15::PWM20_BASE:
      return &g_activePwm20;
    case nrf54l15::PWM21_BASE:
      return &g_activePwm21;
    case nrf54l15::PWM22_BASE:
      return &g_activePwm22;
    default:
      return nullptr;
  }
}

int32_t pwmIrqNumberForBase(uint32_t base) {
  switch (base) {
    case nrf54l15::PWM20_BASE:
      return static_cast<int32_t>(PWM20_IRQn);
    case nrf54l15::PWM21_BASE:
      return static_cast<int32_t>(PWM21_IRQn);
    case nrf54l15::PWM22_BASE:
      return static_cast<int32_t>(PWM22_IRQn);
    default:
      return -1;
  }
}
}


// This file is intentionally an ordered amalgamation of smaller HAL fragments.
// Keep fragments in this order unless the cross-fragment helper dependencies are also refactored.
#include "nrf54l15_hal_parts/nrf54l15_hal_internal_ble_timing.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_internal_gatt_bond.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_internal_crypto_service.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_hooks.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_peripherals.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_crypto_analog.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_i2s.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_802154_rawradio.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_core_setup.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_custom_gatt.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_advertising.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_connection_api.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_central_event.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_rx.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tx.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tail.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_scanning_connections.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_att_l2cap.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_ll_security.inc"
#include "nrf54l15_hal_parts/nrf54l15_hal_ble_radio_tail.inc"

extern "C" void nrf54l15_pwm20_irq_service(void) {
  if (g_activePwm20 != nullptr) {
    g_activePwm20->onIrq();
  }
}

extern "C" void nrf54l15_pwm21_irq_service(void) {
  if (g_activePwm21 != nullptr) {
    g_activePwm21->onIrq();
  }
}

extern "C" void nrf54l15_pwm22_irq_service(void) {
  if (g_activePwm22 != nullptr) {
    g_activePwm22->onIrq();
  }
}
