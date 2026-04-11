# Pico Debugprobe Blink Test

This folder is a minimal smoke test for using a Raspberry Pi Pico running the
official Debugprobe firmware as the upload probe for an nRF54L15 target.

## Contents

- `BlinkViaPico/`
  - minimal blink sketch using `LED_BUILTIN`
- `SerialMonitorViaPico/`
  - periodic UART log and input-echo sketch for serial monitor testing
- `run_picoprobe_blink.sh`
  - compiles the sketch with the repo-local core
  - attempts an `arduino-cli upload` with `Tools > Upload Method = pyOCD`
  - if that fails, retries the built HEX with direct `pyOCD` for clearer logs
- `run_picoprobe_serial_monitor.sh`
  - compiles with `Tools > Serial Routing = Header UART`
  - uploads through `pyOCD`
  - captures initial UART output from the Pico Debugprobe CDC port

## Expected Wiring

For `debugprobe_on_pico`, the upstream Raspberry Pi source uses these Pico pins:

- `GP2` = SWCLK
- `GP3` = SWDIO
- `GP1` = target `nRESET`
- `GP4` = UART TX from the Pico Debugprobe
- `GP5` = UART RX into the Pico Debugprobe

So the basic SWD wiring is:

- Pico `GP2` -> target `SWCLK`
- Pico `GP3` -> target `SWDIO`
- Pico `GND` -> target `GND`
- Pico `VTREF` sense -> target `VDD`

Optional:

- Pico `GP1` -> target `nRESET`

For the serial monitor test, add UART wiring as well. Raspberry Pi documents the
Debug Probe firmware for Pico uses `GP4`/`GP5` for its UART bridge in
`board_pico_config.h`, and the UART bridge is exposed on the probe USB CDC
port. Sources:

- https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html
- https://github.com/raspberrypi/debugprobe

Cross-connect UART:

- target `P2.08` / `Serial TX` -> Pico `GP5` / Debugprobe UART RX
- target `P2.07` / `Serial RX` -> Pico `GP4` / Debugprobe UART TX
- common `GND`

## Run

```bash
cd tools/picoprobe_blink_test
./run_picoprobe_blink.sh
./run_picoprobe_serial_monitor.sh
```

Optional environment overrides:

```bash
PICO_PORT=/dev/ttyACM0 ./run_picoprobe_blink.sh
PICO_UID=E66118C4E3249A25 ./run_picoprobe_blink.sh
FQBN=nrf54l15clean:nrf54l15clean:xiao_nrf54l15 ./run_picoprobe_blink.sh
FQBN=nrf54l15clean:nrf54l15clean:holyiot_25007_nrf54l15 ./run_picoprobe_blink.sh
FQBN=nrf54l15clean:nrf54l15clean:generic_nrf54l15_module_36pin ./run_picoprobe_blink.sh
```

Board note:

- the helper scripts now default to the named 36-pad module board
  `HOLYIOT-25007 nRF54L15 Module`
- for the bare module boards, `BlinkViaPico` drives `LED_BUILTIN = D13/P2.00`,
  which is a default external LED/demo pad rather than a guaranteed onboard LED

## Logs

The script writes logs into `tools/picoprobe_blink_test/logs/`.
