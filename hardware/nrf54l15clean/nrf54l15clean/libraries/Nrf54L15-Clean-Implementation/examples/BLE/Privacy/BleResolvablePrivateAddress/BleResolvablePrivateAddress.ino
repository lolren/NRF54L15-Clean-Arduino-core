#include <Arduino.h>
#include <bluefruit.h>

// BLE privacy helper demo.
//
// This sketch shows the currently supported privacy primitives:
// - read the factory identity root as a demo IRK
// - generate a Resolvable Private Address (RPA)
// - resolve the RPA through the hardware AAR block
// - enable opt-in automatic local RPA rotation for advertising
//
// This is still not a full controller privacy policy: resolving-list
// integration and bonded reconnect identity handling remain application-level.

BLEUart bleuart;

namespace {

constexpr uint32_t kSerialWaitMs = 1500UL;
constexpr uint32_t kPingPeriodMs = 3000UL;
constexpr uint32_t kDemoRpaRotationMs = 60000UL;
uint32_t lastPingMs = 0;
uint32_t pingSeq = 0;

void printHex16(const uint8_t value[16]) {
  static const char kHex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < 16U; ++i) {
    Serial.write(kHex[value[i] >> 4]);
    Serial.write(kHex[value[i] & 0x0FU]);
  }
}

void printBleAddress(const uint8_t address[6]) {
  static const char kHex[] = "0123456789ABCDEF";
  for (int i = 5; i >= 0; --i) {
    const uint8_t value = address[i];
    Serial.write(kHex[(value >> 4U) & 0x0FU]);
    Serial.write(kHex[value & 0x0FU]);
    if (i > 0) Serial.write(':');
  }
}

void startAdvertising() {
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 400);
  Bluefruit.Advertising.start(0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t startMs = millis();
  while (!Serial && millis() - startMs < kSerialWaitMs) {
    delay(10);
  }

  Bluefruit.autoConnLed(false);
  Bluefruit.begin(1, 0);
  Bluefruit.setName("X54-RPA");
  Bluefruit.setTxPower(0);
  bleuart.begin();

  uint8_t irk[16] = {};
  uint8_t previewRpa[6] = {};
  uint8_t activeRpa[6] = {};
  bool resolved = false;
  uint16_t resolvedIndex = 0xFFFFU;

  const bool irkOk = Bluefruit.Security.getLocalIdentityRoot(irk);
  const bool generateOk =
      Bluefruit.Security.generateResolvablePrivateAddress(irk, previewRpa);
  const bool resolveOk =
      Bluefruit.Security.resolveResolvablePrivateAddress(
          previewRpa, irk, 1U, &resolved, &resolvedIndex);
  const bool rotationOk =
      Bluefruit.Security.enableResolvablePrivateAddressRotation(
          irk, kDemoRpaRotationMs, true, activeRpa);

  Serial.println();
  Serial.println("BleResolvablePrivateAddress");
  Serial.print("local_irk=");
  printHex16(irk);
  Serial.println();
  Serial.print("preview_rpa=");
  printBleAddress(previewRpa);
  Serial.println();
  Serial.print("preview_resolved=");
  Serial.println((resolveOk && resolved && resolvedIndex == 0U) ? "yes" : "no");
  Serial.print("active_rpa=");
  printBleAddress(activeRpa);
  Serial.println();
  Serial.print("rotation_ms=");
  Serial.println(kDemoRpaRotationMs);
  Serial.print("result=");
  Serial.println((irkOk && generateOk && resolveOk && resolved &&
                  resolvedIndex == 0U && rotationOk) ? "PASS" : "FAIL");
  Serial.println("Advertising as X54-RPA with opt-in RPA rotation enabled");

  startAdvertising();
}

void loop() {
  while (bleuart.available() > 0) {
    const int ch = bleuart.read();
    if (ch >= 0) Serial.write(static_cast<uint8_t>(ch));
  }

  if (Bluefruit.connected() && (millis() - lastPingMs >= kPingPeriodMs)) {
    lastPingMs = millis();
    bleuart.print("rpa ");
    bleuart.println(pingSeq++);
  }
}
