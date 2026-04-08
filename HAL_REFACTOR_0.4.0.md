# HAL Refactor 0.4.0

This worktree is the local refactor track for the nRF54L15 HAL. The goal is
to reduce correctness risk first, then split the oversized HAL into smaller
units without losing the currently working BLE and Zigbee behavior from main.

## Current baseline

- Source branch: `main` at `b73658a`
- Local refactor branch: `v0.4.0-local`
- Working tree: `NRF54L15-Clean-Arduino-core.v040`

## First-wave hardening

These changes are intended to be behavior-preserving:

1. Remove undefined behavior from highest-set-bit allocation.
2. Add generic IRQ critical helpers for non-BLE ownership transitions.
3. Make comparator ownership changes IRQ-safe.
4. Make BLE GRTC one-time init safer against concurrent first use.

Completed:

- generic support helpers have been split into
  `src/nrf54l15_hal_support.cpp` with declarations in
  `src/nrf54l15_hal_support_internal.h`
- the old duplicated support block has been removed from `nrf54l15_hal.cpp`
- `spimPrescaler()` now rejects invalid zero-input requests instead of silently
  defaulting them to `1 MHz`
- native NUS host regression still passes after the split and helper cleanup:
  `.build/v040_hal_refactor_ble_nus_runtime/summary.json`
- Zigbee sleepy button still joins and interviews against Zigbee2MQTT on the
  Home Assistant box at `192.168.1.100`:
  `.build/v040_zigbee_sleepy_button_ha_validation/summary.txt`
- local coordinator + sleepy button runtime still shows join/action traffic on
  the two connected nRF54 boards:
  `.build/v040_zigbee_sleepy_button_local_validation/button.log`
  `.build/v040_zigbee_sleepy_button_local_validation/coord.log`

## Planned follow-up

1. Tighten invalid-argument handling in peripheral helpers such as
   timer/PWM helpers and other silent fallback paths.
2. Audit one-time init and ownership transitions outside the BLE-specific
   critical helpers.
3. Start carving the HAL into focused translation units:
   - `hal_clock_grtc.cpp`
   - `hal_system_off.cpp`
   - `hal_analog.cpp`
   - `hal_peripherals.cpp`
   - `hal_ble_radio.cpp`
   - `hal_ble_gatt.cpp`
   - `hal_ble_bond.cpp`
   - `hal_board_policy.cpp`
4. Re-run BLE and Zigbee regressions after each structural step.
