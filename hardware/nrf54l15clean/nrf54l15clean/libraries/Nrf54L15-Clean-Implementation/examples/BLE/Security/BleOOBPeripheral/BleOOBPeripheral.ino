/**
 * OOB (Out-of-Band) Pairing - Peripheral
 * 
 * Generates OOB data (r, c values) and prints them as hex on Serial.
 * The central device should read these values and provide them via the OOB channel.
 * 
 * Serial output format (copy this to the central):
 * OOB: r=00112233445566778899AABBCCDDEEFF c=00112233445566778899AABBCCDDEEFF
 * 
 * Flash on both XIAO nRF54L15 boards, swap the ROLE between PERIPHERAL/CENTRAL.
 */

#include <Arduino.h>
#include <nrf54l15_hal.h>

using namespace xiao_nrf54l15;

#ifndef ROLE
#define ROLE PERIPHERAL
#endif

BleRadio g_ble;

static uint8_t oob_r[16];
static uint8_t oob_c[16];

void printHex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  Serial.print(": ");
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("=== OOB Peripheral ===");
  
  g_ble.begin();
  g_ble.setDeviceName("OOB-P");
  g_ble.setSecureConnections(true);
  
  // Generate OOB data
  if (!g_ble.generateSecurityOobData(oob_r, oob_c)) {
    Serial.println("ERROR: Failed to generate OOB data!");
    while (true) delay(1000);
  }
  
  Serial.println("OOB data generated:");
  printHex("  r", oob_r, 16);
  printHex("  c", oob_c, 16);
  Serial.println();
  
  // Print in copyable format
  Serial.print("OOB: r=");
  for (int i = 0; i < 16; i++) {
    if (oob_r[i] < 0x10) Serial.print('0');
    Serial.print(oob_r[i], HEX);
  }
  Serial.print(" c=");
  for (int i = 0; i < 16; i++) {
    if (oob_c[i] < 0x10) Serial.print('0');
    Serial.print(oob_c[i], HEX);
  }
  Serial.println();
  
  Serial.println();
  Serial.println("Copy the OOB line above to the central device.");
  Serial.println("Press any key to start advertising...");
  
  // Wait for user to copy OOB data
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();
  
  // Set OOB flag and start advertising
  g_ble.setSecurityOobEnabled(true);
  g_ble.startAdvertising();
  Serial.println("Advertising with OOB pairing enabled...");
}

void loop() {
  g_ble.poll();
  
  // Send encrypted notifications when connected
  if (g_ble.isConnected()) {
    static unsigned long last_notify = 0;
    if (millis() - last_notify > 2000) {
      last_notify = millis();
      uint8_t data[4];
      data[0] = (millis() / 1000) & 0xFF;
      data[1] = (millis() / 256) & 0xFF;
      data[2] = 0xAA;
      data[3] = 0xBB;
      
      if (g_ble.queueNotifyValue(0x2A19, data, 4)) {
        Serial.print("Notified: ");
        for (int i = 0; i < 4; i++) {
          if (data[i] < 0x10) Serial.print('0');
          Serial.print(data[i], HEX);
          Serial.print(' ');
        }
        Serial.println();
      }
    }
  }
}
