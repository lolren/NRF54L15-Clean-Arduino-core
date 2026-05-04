// Temperature Sensor Probe
// =========================
// Reads the nRF54L15 internal temperature sensor every second.
// Prints die temperature in Celsius to Serial Monitor at 115200 baud.
//
// The internal TEMP sensor measures the silicon die temperature,
// which is typically 2-5°C above ambient due to self-heating.
//
// Hardware: Single XIAO nRF54L15 board
// Output:   "TEMP: 32.5 C" every 1 second

#include <nrf54_all.h>

using xiao_nrf54l15::TempSensor;

TempSensor g_temp;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  Serial.println("=== Temperature Sensor Probe ===");
  Serial.println("Reading internal die temperature every 1s...");
}

void loop() {
  int32_t quarterDeg = 0;
  if (g_temp.sampleQuarterDegreesC(&quarterDeg)) {
    float celsius = quarterDeg / 4.0f;
    Serial.print("TEMP: ");
    Serial.print(celsius, 1);
    Serial.println(" C");
  } else {
    Serial.println("TEMP: read failed");
  }
  delay(1000);
}
