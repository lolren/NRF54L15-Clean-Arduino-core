#include <SPI.h>
#include <Wire.h>

// Minimal peripheral smoke test using the standard Arduino APIs only.
//
// This intentionally does not use the low-level HAL wrappers. It is a quick
// sanity check that Wire, SPI, ADC, and the built-in LED path all compile and
// run together.

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Wire.begin();
  Wire.setClock(400000);

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  (void)SPI.transfer(0x9F);
  SPI.endTransaction();

  int adc = analogRead(A0);
  digitalWrite(LED_BUILTIN, (adc > 512) ? HIGH : LOW);
}

void loop() {
  delay(250);
}
