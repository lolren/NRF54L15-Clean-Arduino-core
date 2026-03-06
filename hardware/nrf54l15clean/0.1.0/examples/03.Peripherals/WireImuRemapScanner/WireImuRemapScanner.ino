/*
  WireImuRemapScanner

  Keeps Serial logging alive while scanning the IMU/back-pad I2C bus on D12/D11.

  On this core, Wire1 shares a serial-fabric instance with Serial, so sketches
  that call Wire1.begin() and then print to Serial can look like they hang.
  The workaround is to remap Wire onto the IMU/back-pad pins instead.

  Hardware:
  - IMU/back-pad SDA: D12 / P0.04
  - IMU/back-pad SCL: D11 / P0.03
  - IMU power gate:   IMU_MIC_EN / P0.01
*/

#include <Wire.h>

static void scanBus(TwoWire& bus) {
  uint8_t found = 0;

  Serial.println("Scanning remapped Wire bus...");
  for (uint8_t address = 1; address < 127; ++address) {
    bus.beginTransmission(address);
    const uint8_t error = bus.endTransmission();
    if (error == 0U) {
      Serial.print("Found device at 0x");
      if (address < 16U) {
        Serial.print('0');
      }
      Serial.println(address, HEX);
      ++found;
    }
  }

  if (found == 0U) {
    Serial.println("No devices found on D12/D11");
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }

  pinMode(IMU_MIC_EN, OUTPUT);
  digitalWrite(IMU_MIC_EN, HIGH);
  delay(10);

  if (!Wire.setPins(PIN_WIRE1_SDA, PIN_WIRE1_SCL)) {
    Serial.println("Wire.setPins failed");
    while (true) {
      delay(1000);
    }
  }

  Wire.begin();
  Wire.setClock(400000);

  Serial.println("Wire remapped to D12/D11 for IMU/back-pad scan");
}

void loop() {
  scanBus(Wire);
  delay(1000);
}
