/*
 * Generic 36-pad nRF54L15 module pin definitions.
 *
 * D-numbering follows the exposed module pad order. Code-facing aliases like
 * P2_08 keep the actual GPIO names intact.
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <nrf54l15.h>

#define NUM_DIGITAL_PINS 29
#define NUM_ANALOG_INPUTS 8

#define PIN_D0  (0)   /* P1.09  pad 2  */
#define PIN_D1  (1)   /* P1.10  pad 3  */
#define PIN_D2  (2)   /* P1.11  pad 5  */
#define PIN_D3  (3)   /* P1.12  pad 6  */
#define PIN_D4  (4)   /* P1.13  pad 7  */
#define PIN_D5  (5)   /* P1.14  pad 8  */
#define PIN_D6  (6)   /* P1.02  pad 10 */
#define PIN_D7  (7)   /* P1.03  pad 11 */
#define PIN_D8  (8)   /* P1.04  pad 12 */
#define PIN_D9  (9)   /* P1.05  pad 13 */
#define PIN_D10 (10)  /* P1.06  pad 14 */
#define PIN_D11 (11)  /* P1.07  pad 15 */
#define PIN_D12 (12)  /* P1.08  pad 16 */
#define PIN_D13 (13)  /* P2.00  pad 17 */
#define PIN_D14 (14)  /* P2.01  pad 18 */
#define PIN_D15 (15)  /* P2.02  pad 19 */
#define PIN_D16 (16)  /* P2.03  pad 20 */
#define PIN_D17 (17)  /* P2.04  pad 21 */
#define PIN_D18 (18)  /* P2.05  pad 22 */
#define PIN_D19 (19)  /* P2.06  pad 23 */
#define PIN_D20 (20)  /* P2.07  pad 24 */
#define PIN_D21 (21)  /* P2.08  pad 25 */
#define PIN_D22 (22)  /* P2.09  pad 26 */
#define PIN_D23 (23)  /* P2.10  pad 27 */
#define PIN_D24 (24)  /* P0.00  pad 28 */
#define PIN_D25 (25)  /* P0.01  pad 29 */
#define PIN_D26 (26)  /* P0.02  pad 32 */
#define PIN_D27 (27)  /* P0.03  pad 33 */
#define PIN_D28 (28)  /* P0.04  pad 34 */

#define PIN_P1_09 PIN_D0
#define PIN_P1_10 PIN_D1
#define PIN_P1_11 PIN_D2
#define PIN_P1_12 PIN_D3
#define PIN_P1_13 PIN_D4
#define PIN_P1_14 PIN_D5
#define PIN_P1_02 PIN_D6
#define PIN_P1_03 PIN_D7
#define PIN_P1_04 PIN_D8
#define PIN_P1_05 PIN_D9
#define PIN_P1_06 PIN_D10
#define PIN_P1_07 PIN_D11
#define PIN_P1_08 PIN_D12
#define PIN_P2_00 PIN_D13
#define PIN_P2_01 PIN_D14
#define PIN_P2_02 PIN_D15
#define PIN_P2_03 PIN_D16
#define PIN_P2_04 PIN_D17
#define PIN_P2_05 PIN_D18
#define PIN_P2_06 PIN_D19
#define PIN_P2_07 PIN_D20
#define PIN_P2_08 PIN_D21
#define PIN_P2_09 PIN_D22
#define PIN_P2_10 PIN_D23
#define PIN_P0_00 PIN_D24
#define PIN_P0_01 PIN_D25
#define PIN_P0_02 PIN_D26
#define PIN_P0_03 PIN_D27
#define PIN_P0_04 PIN_D28

// Bare module default Blink pin. There is no guaranteed onboard LED; this is
// the default external LED/demo pad used by Blink-like sketches.
#define PIN_LED_BUILTIN (PIN_P2_00)
#define PIN_LED         PIN_LED_BUILTIN
#define LED_BUILTIN     PIN_LED_BUILTIN
#define LED_RED         PIN_LED_BUILTIN
#define LED_GREEN       PIN_LED_BUILTIN
#define LED_BLUE        PIN_LED_BUILTIN
#define LED_STATE_ON    HIGH

#define PIN_BUTTON      (0xFFU)

// Secondary UART pair for alternate host/debug routing on bare modules.
#define PIN_SAMD11_RX   (PIN_P2_09)
#define PIN_SAMD11_TX   (PIN_P2_10)

#define PIN_A0 (PIN_P1_04)
#define PIN_A1 (PIN_P1_05)
#define PIN_A2 (PIN_P1_06)
#define PIN_A3 (PIN_P1_07)
#define PIN_A4 (PIN_P1_11)
#define PIN_A5 (PIN_P1_12)
#define PIN_A6 (PIN_P1_13)
#define PIN_A7 (PIN_P1_14)

enum {
    A0 = PIN_A0,
    A1 = PIN_A1,
    A2 = PIN_A2,
    A3 = PIN_A3,
    A4 = PIN_A4,
    A5 = PIN_A5,
    A6 = PIN_A6,
    A7 = PIN_A7,
};

#define PIN_WIRE_SDA  (PIN_P0_03)
#define PIN_WIRE_SCL  (PIN_P0_04)
#define PIN_WIRE1_SDA (PIN_P1_10)
#define PIN_WIRE1_SCL (PIN_P1_11)

#define PIN_SERIAL_TX  (PIN_P2_08)
#define PIN_SERIAL_RX  (PIN_P2_07)
#define PIN_SERIAL1_TX (PIN_SAMD11_TX)
#define PIN_SERIAL1_RX (PIN_SAMD11_RX)

#define PIN_SPI_MOSI (PIN_P2_05)
#define PIN_SPI_MISO (PIN_P2_04)
#define PIN_SPI_SCK  (PIN_P2_03)
#define PIN_SPI_SS   (PIN_P2_02)

// Compatibility aliases used by XIAO-focused sketches. Bare module boards do
// not expose those rails, so leave them disconnected.
#define PIN_IMU_MIC_PWR (0xFFU)
#define PIN_RF_SW       (0xFFU)
#define PIN_RF_SW_CTL   (0xFFU)
#define PIN_VBAT_EN     (0xFFU)
#define PIN_IMU_INT     (0xFFU)
#define PIN_PDM_CLK     (PIN_P1_12)
#define PIN_PDM_DATA    (PIN_P1_13)
#define PIN_VBAT_READ   (PIN_A7)

#define SAMD11_TX PIN_SAMD11_TX
#define SAMD11_RX PIN_SAMD11_RX
#define IMU_MIC PIN_IMU_MIC_PWR
#define IMU_MIC_EN PIN_IMU_MIC_PWR
#define RF_SW PIN_RF_SW
#define RF_SW_CTL PIN_RF_SW_CTL
#define VBAT_EN PIN_VBAT_EN
#define VBAT_READ PIN_VBAT_READ
#define IMU_INT PIN_IMU_INT
#define PDM_CLK PIN_PDM_CLK
#define PDM_DATA PIN_PDM_DATA
#define MIC_CLK PIN_PDM_CLK
#define MIC_DATA PIN_PDM_DATA

typedef volatile uint32_t PortReg;
typedef uint32_t PortMask;

static inline bool pinToPortPin(uint8_t pin, uint8_t* port, uint8_t* pinInPort)
{
    if (port == 0 || pinInPort == 0) {
        return false;
    }

    switch (pin) {
        case PIN_D0: *port = 1; *pinInPort = 9; return true;
        case PIN_D1: *port = 1; *pinInPort = 10; return true;
        case PIN_D2: *port = 1; *pinInPort = 11; return true;
        case PIN_D3: *port = 1; *pinInPort = 12; return true;
        case PIN_D4: *port = 1; *pinInPort = 13; return true;
        case PIN_D5: *port = 1; *pinInPort = 14; return true;
        case PIN_D6: *port = 1; *pinInPort = 2; return true;
        case PIN_D7: *port = 1; *pinInPort = 3; return true;
        case PIN_D8: *port = 1; *pinInPort = 4; return true;
        case PIN_D9: *port = 1; *pinInPort = 5; return true;
        case PIN_D10: *port = 1; *pinInPort = 6; return true;
        case PIN_D11: *port = 1; *pinInPort = 7; return true;
        case PIN_D12: *port = 1; *pinInPort = 8; return true;
        case PIN_D13: *port = 2; *pinInPort = 0; return true;
        case PIN_D14: *port = 2; *pinInPort = 1; return true;
        case PIN_D15: *port = 2; *pinInPort = 2; return true;
        case PIN_D16: *port = 2; *pinInPort = 3; return true;
        case PIN_D17: *port = 2; *pinInPort = 4; return true;
        case PIN_D18: *port = 2; *pinInPort = 5; return true;
        case PIN_D19: *port = 2; *pinInPort = 6; return true;
        case PIN_D20: *port = 2; *pinInPort = 7; return true;
        case PIN_D21: *port = 2; *pinInPort = 8; return true;
        case PIN_D22: *port = 2; *pinInPort = 9; return true;
        case PIN_D23: *port = 2; *pinInPort = 10; return true;
        case PIN_D24: *port = 0; *pinInPort = 0; return true;
        case PIN_D25: *port = 0; *pinInPort = 1; return true;
        case PIN_D26: *port = 0; *pinInPort = 2; return true;
        case PIN_D27: *port = 0; *pinInPort = 3; return true;
        case PIN_D28: *port = 0; *pinInPort = 4; return true;
        default: return false;
    }
}

static inline int8_t pinToSaadcChannel(uint8_t pin)
{
    switch (pin) {
        case PIN_P1_04: return 0;
        case PIN_P1_05: return 1;
        case PIN_P1_06: return 2;
        case PIN_P1_07: return 3;
        case PIN_P1_11: return 4;
        case PIN_P1_12: return 5;
        case PIN_P1_13: return 6;
        case PIN_P1_14: return 7;
        default: return -1;
    }
}

static inline uint8_t digitalPinToPort(uint8_t pin)
{
    uint8_t port = 0;
    uint8_t pinInPort = 0;
    return pinToPortPin(pin, &port, &pinInPort) ? port : 0xFF;
}

static inline uint32_t digitalPinToBitMask(uint8_t pin)
{
    uint8_t port = 0;
    uint8_t pinInPort = 0;
    (void)port;
    return pinToPortPin(pin, &port, &pinInPort) ? (1UL << pinInPort) : 0UL;
}

static inline volatile uint32_t* portOutputRegister(uint8_t port)
{
    switch (port) {
        case 0: return &NRF_P0->OUT;
        case 1: return &NRF_P1->OUT;
        case 2: return &NRF_P2->OUT;
        default: return (volatile uint32_t*)0;
    }
}

static inline volatile uint32_t* portInputRegister(uint8_t port)
{
    switch (port) {
        case 0: return (volatile uint32_t*)&NRF_P0->IN;
        case 1: return (volatile uint32_t*)&NRF_P1->IN;
        case 2: return (volatile uint32_t*)&NRF_P2->IN;
        default: return (volatile uint32_t*)0;
    }
}

static inline volatile uint32_t* portModeRegister(uint8_t port)
{
    switch (port) {
        case 0: return &NRF_P0->DIR;
        case 1: return &NRF_P1->DIR;
        case 2: return &NRF_P2->DIR;
        default: return (volatile uint32_t*)0;
    }
}

#define digitalPinHasPWM(p) ((p) <= PIN_D15)

#ifndef NOT_AN_INTERRUPT
#define NOT_AN_INTERRUPT 0xFF
#endif

static inline int digitalPinToInterrupt(uint8_t pin)
{
    uint8_t port = 0;
    uint8_t pinInPort = 0;
    if (!pinToPortPin(pin, &port, &pinInPort) || port == 2U) {
        return NOT_AN_INTERRUPT;
    }
    return pin;
}

static inline uint8_t analogInputToDigitalPin(uint8_t p)
{
    switch (p) {
        case 0: return PIN_A0;
        case 1: return PIN_A1;
        case 2: return PIN_A2;
        case 3: return PIN_A3;
        case 4: return PIN_A4;
        case 5: return PIN_A5;
        case 6: return PIN_A6;
        case 7: return PIN_A7;
        default: return 0xFF;
    }
}

static const uint8_t P1_09 = PIN_P1_09;
static const uint8_t P1_10 = PIN_P1_10;
static const uint8_t P1_11 = PIN_P1_11;
static const uint8_t P1_12 = PIN_P1_12;
static const uint8_t P1_13 = PIN_P1_13;
static const uint8_t P1_14 = PIN_P1_14;
static const uint8_t P1_02 = PIN_P1_02;
static const uint8_t P1_03 = PIN_P1_03;
static const uint8_t P1_04 = PIN_P1_04;
static const uint8_t P1_05 = PIN_P1_05;
static const uint8_t P1_06 = PIN_P1_06;
static const uint8_t P1_07 = PIN_P1_07;
static const uint8_t P1_08 = PIN_P1_08;
static const uint8_t P2_00 = PIN_P2_00;
static const uint8_t P2_01 = PIN_P2_01;
static const uint8_t P2_02 = PIN_P2_02;
static const uint8_t P2_03 = PIN_P2_03;
static const uint8_t P2_04 = PIN_P2_04;
static const uint8_t P2_05 = PIN_P2_05;
static const uint8_t P2_06 = PIN_P2_06;
static const uint8_t P2_07 = PIN_P2_07;
static const uint8_t P2_08 = PIN_P2_08;
static const uint8_t P2_09 = PIN_P2_09;
static const uint8_t P2_10 = PIN_P2_10;
static const uint8_t P0_00 = PIN_P0_00;
static const uint8_t P0_01 = PIN_P0_01;
static const uint8_t P0_02 = PIN_P0_02;
static const uint8_t P0_03 = PIN_P0_03;
static const uint8_t P0_04 = PIN_P0_04;
static const uint8_t SDA  = PIN_WIRE_SDA;
static const uint8_t SCL  = PIN_WIRE_SCL;
static const uint8_t SDA1 = PIN_WIRE1_SDA;
static const uint8_t SCL1 = PIN_WIRE1_SCL;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t SS   = PIN_SPI_SS;

#define SERIAL_PORT_MONITOR Serial
#define SERIAL_PORT_USBVIRTUAL Serial
#define SERIAL_PORT_HARDWARE Serial1
#define SERIAL_PORT_HARDWARE1 Serial1
#define SERIAL_PORT_HARDWARE2 Serial2

#define HAVE_HWSERIAL1
#define HAVE_HWSERIAL2

#endif
