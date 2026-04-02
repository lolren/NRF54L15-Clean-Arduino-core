#ifndef SOFTWARE_SERIAL_H_
#define SOFTWARE_SERIAL_H_

#include "HardwareSerial.h"

class SoftwareSerial : public Stream {
 public:
  SoftwareSerial(int8_t rxPin, int8_t txPin) : rx_pin_(rxPin), tx_pin_(txPin) {}

  void begin(unsigned long baud) {
    (void)Serial1.setPins(rx_pin_, tx_pin_);
    Serial1.begin(baud);
  }

  void end() { Serial1.end(); }

  int available() override { return Serial1.available(); }
  int read() override { return Serial1.read(); }
  int peek() override { return Serial1.peek(); }
  void flush() override { Serial1.flush(); }
  size_t write(uint8_t value) override { return Serial1.write(value); }
  using Print::write;

  operator bool() const { return static_cast<bool>(Serial1); }

 private:
  int8_t rx_pin_;
  int8_t tx_pin_;
};

#endif  // SOFTWARE_SERIAL_H_
