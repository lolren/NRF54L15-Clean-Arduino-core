# HOLYIOT-25008 Bring-Up Notes

This note captures the upstream Zephyr pin information that was used to bring
the dedicated `HOLYIOT-25008 nRF54L15 Module` board target into the core.

Primary source:

- Zephyr board docs:
  `https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/holyiot/holyiot_25008`

Relevant upstream statements:

- UART: `P1.04` (`TX`), `P1.05` (`RX`)
- Button: `P1.13`, active low with pull-up
- RGB LED:
  - red: `P2.09`, active low
  - green: `P1.10`, active low
  - blue: `P2.07`, active low
- LIS2DH12 accelerometer (SPI):
  - `P2.01` = `SCK`
  - `P2.02` = `MOSI`
  - `P2.04` = `MISO`
  - `P2.05` = `CS`
  - `P2.00` = `INT1`
  - `P2.03` = `INT2`

What this likely means on the physical module:

- the two extra pads are very likely the console UART pads:
  - `P1.04` = `TX`
  - `P1.05` = `RX`
- the currently published upstream board support describes an `RGB` LED, not an
  `RGBW` LED

Current status:

- the board now has a dedicated core target:
  [HOLYIOT-25008 Module Reference](holyiot-25008-module-reference.md)
- the original raw probe is still useful for board-level sanity checks:
  [tools/holyiot_25008_probe](/home/lolren/Desktop/Nrf54L15/NRF54L15-Clean-Arduino-core/tools/holyiot_25008_probe)
