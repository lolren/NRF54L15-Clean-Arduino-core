/*
  GATT descriptor helper diagnostic

  This sketch exposes Bluefruit characteristic descriptor helpers through a
  small custom service:
  - Characteristic User Description: 0x2901
  - Characteristic Presentation Format: 0x2904
  - Report Reference: 0x2908

  Use a BLE scanner and inspect service 0xFFF0. The Report Reference descriptor
  is normally used by HID Report characteristics; it is included here only to
  verify that the descriptor API is visible through ATT discovery and reads.
*/

#include <bluefruit.h>

BLEService descriptorService(0xFFF0);
BLECharacteristic temperatureChar(0xFFF1);
BLECharacteristic reportLikeChar(0xFFF2);

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 1500) {
    delay(10);
  }

  Bluefruit.begin();
  Bluefruit.setName("GATT Descriptor Demo");

  descriptorService.begin();

  temperatureChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  temperatureChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  temperatureChar.setFixedLen(2);
  temperatureChar.setUserDescriptor("Temperature x0.01 C");
  temperatureChar.setPresentationFormatDescriptor(0x0E, -2, 0x272F);
  temperatureChar.begin();

  reportLikeChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  reportLikeChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  reportLikeChar.setFixedLen(1);
  reportLikeChar.setUserDescriptor("Report reference demo");
  reportLikeChar.setReportRefDescriptor(1, 1);
  reportLikeChar.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(descriptorService);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 160);
  Bluefruit.Advertising.start(0);

  Serial.println("Advertising GATT Descriptor Demo");
}

void loop() {
  static int16_t temperatureCentiC = 2350;
  static uint8_t reportValue = 0;

  temperatureChar.write(&temperatureCentiC, sizeof(temperatureCentiC));
  reportLikeChar.write8(reportValue++);

  if (Bluefruit.connected()) {
    temperatureChar.notify(&temperatureCentiC, sizeof(temperatureCentiC));
    reportLikeChar.notify8(reportValue);
  }

  delay(1000);
}
