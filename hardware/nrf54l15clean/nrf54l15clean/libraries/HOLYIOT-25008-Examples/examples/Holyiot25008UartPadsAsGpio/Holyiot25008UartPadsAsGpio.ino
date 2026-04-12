#if !defined(ARDUINO_HOLYIOT_25008_NRF54L15)
#error "Select Tools > Board > HOLYIOT-25008 nRF54L15 Module for this example."
#endif

#if !defined(NRF54L15_CLEAN_SERIAL_DISABLED)
#warning "For guaranteed D0/D1 GPIO ownership, select Tools > Serial Routing > GPIO on D0/D1 (Serial disabled)."
#endif

namespace {

void setChannel(uint8_t pin, bool on) {
  digitalWrite(pin, on ? LED_STATE_ON : !LED_STATE_ON);
}

void setGreen(bool on) {
  setChannel(LED_GREEN, on);
}

}  // namespace

void setup() {
  pinMode(PIN_D0, OUTPUT);
  pinMode(PIN_D1, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(PIN_D0, LOW);
  digitalWrite(PIN_D1, LOW);
  setGreen(false);
}

void loop() {
  digitalWrite(PIN_D0, HIGH);
  digitalWrite(PIN_D1, LOW);
  setGreen(true);
  delay(150);

  digitalWrite(PIN_D0, LOW);
  digitalWrite(PIN_D1, HIGH);
  setGreen(false);
  delay(150);
}
