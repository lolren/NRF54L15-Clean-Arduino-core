/*
 * XIAO nRF54L15 pin definitions for clean bare-metal core.
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define NUM_DIGITAL_PINS 20
#define NUM_ANALOG_INPUTS 8

#define PIN_D0  (0)
#define PIN_D1  (1)
#define PIN_D2  (2)
#define PIN_D3  (3)
#define PIN_D4  (4)
#define PIN_D5  (5)
#define PIN_D6  (6)
#define PIN_D7  (7)
#define PIN_D8  (8)
#define PIN_D9  (9)
#define PIN_D10 (10)
#define PIN_D11 (11)
#define PIN_D12 (12)
#define PIN_D13 (13)
#define PIN_D14 (14)
#define PIN_D15 (15)

#define PIN_LED_BUILTIN (16)
#define PIN_BUTTON      (17)

// Routed to SAMD11 USB bridge on XIAO nRF54L15.
#define PIN_SAMD11_TX   (18)
#define PIN_SAMD11_RX   (19)

#define LED_BUILTIN PIN_LED_BUILTIN

#define PIN_A0 (0)
#define PIN_A1 (1)
#define PIN_A2 (2)
#define PIN_A3 (3)
#define PIN_A4 (4)
#define PIN_A5 (5)
#define PIN_A6 (6)
#define PIN_A7 (7)

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

#define PIN_WIRE_SDA (PIN_D4)
#define PIN_WIRE_SCL (PIN_D5)

#define PIN_SERIAL_TX (PIN_D6)
#define PIN_SERIAL_RX (PIN_D7)
#define PIN_SERIAL1_TX PIN_SERIAL_TX
#define PIN_SERIAL1_RX PIN_SERIAL_RX

#define PIN_SPI_MOSI (PIN_D10)
#define PIN_SPI_MISO (PIN_D9)
#define PIN_SPI_SCK  (PIN_D8)
#define PIN_SPI_SS   (PIN_D2)

#define digitalPinHasPWM(p) ((p) == PIN_D6 || (p) == PIN_D7 || (p) == PIN_D8 || (p) == PIN_D9)
#define digitalPinToInterrupt(p) (p)

static inline uint8_t analogInputToDigitalPin(uint8_t p)
{
    return (p < NUM_ANALOG_INPUTS) ? p : 0xFF;
}

#define SERIAL_PORT_MONITOR Serial
#define SERIAL_PORT_USBVIRTUAL Serial
#define SERIAL_PORT_HARDWARE Serial1
#define SERIAL_PORT_HARDWARE1 Serial1

#endif
