# Zephyr Low-Power Parity on XIAO nRF54L15

This repo now carries the same startup and `SYSTEM OFF` mechanism that was
required to match the Zephyr microamp result on XIAO nRF54L15.

## Required board options

Use these Arduino Tools settings when you want the Zephyr-parity path:

- `Security Domain = Secure`
- `CPU Frequency = 128 MHz`
- `Boot Profile = Minimal`
- `BLE = Off` for pure current-floor measurement
- `Zigbee = Off` for pure current-floor measurement

The secure build is important. The working Zephyr image used the secure
peripheral aliases:

- `NRF_GRTC = 0x500E2000`
- `NRF_MEMCONF = 0x500CF000`
- `NRF_RESET = 0x5010E000`
- `NRF_REGULATORS = 0x50120000`

## What moved into the core

The Zephyr startup parity is no longer hidden in a one-off example.

It now runs from
[`system_nrf54l15.c`](../hardware/nrf54l15clean/nrf54l15clean/cores/nrf54l15/system_nrf54l15.c)
inside `SystemInit()` for secure builds:

- PLL frequency selection
- FICR trim copy loop
- errata writes used by the Zephyr startup path
- `RRAMC LOWPOWERCONFIG = 3`
- glitch detector disable
- LFXO/HFXO internal capacitor trim programming
- `VREGMAIN` DCDC enable
- instruction cache enable

The shared power-off path lives in
[`nrf54l15_hal.cpp`](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal.cpp):

- GRTC secure-domain wake programming
- LFCLK -> LFXO path
- Zephyr-style wake compare channel selection for XIAO
- board low-power hook before `SYSTEM OFF`
- optional RAM retention clear through `MEMCONF` for the explicit `NoRetention`
  system-off helpers used by the parity sketch
- reset-reason clear
- final `REGULATORS->SYSTEMOFF`

The internal parity-only minimal boot path also skips Arduino `SysTick` startup in
[`main.cpp`](../hardware/nrf54l15clean/nrf54l15clean/cores/nrf54l15/main.cpp),
because the parity measurement path should not start the normal Arduino tick.

## Example sketch

Use:

- [`LowPowerZephyrParityBlink`](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/LowPowerZephyrParityBlink/LowPowerZephyrParityBlink.ino)

Behavior:

- pulse LED on `P2.0` for `5 ms`
- arm GRTC wake for `1 s`
- enter `SYSTEM OFF`
- cold boot and repeat

## Why this matters

The current gap was not just the final `SYSTEMOFF` write.

The Zephyr result depended on the full chain:

1. secure image build
2. Zephyr-like secure startup writes
3. Zephyr-like oscillator/regulator trim programming
4. Zephyr-like GRTC wake domain and compare setup
5. board rail and retention shutdown before `SYSTEM OFF`

Leaving out any of those was enough to sit in the sub-mA range instead of the
Zephyr result.

## BLE note

Yes, this core can advertise BLE. The repo already includes BLE advertiser and
low-duty-cycle beacon examples.

But BLE advertising is not the same power state as this blink test:

- this blink test spends almost all of its time in `SYSTEM OFF`
- advertising must keep the radio timing path alive between events
- so the low-power target becomes "radio sleep between advertising events",
  not "full `SYSTEM OFF` floor"

That means BLE low-power parity is possible, but it is a separate audit from
the pure `SYSTEM OFF` parity path documented here.
