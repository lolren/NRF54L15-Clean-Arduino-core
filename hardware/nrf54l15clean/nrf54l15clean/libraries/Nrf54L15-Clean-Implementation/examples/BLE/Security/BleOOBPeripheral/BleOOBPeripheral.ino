/**
 * OOB (Out-of-Band) Pairing - Peripheral
 * 
 * Generates OOB data (r, c values) and prints them as hex on Serial.
 * The central device should read these values and provide them via the OOB channel
 * (e.g., Serial monitor, manual entry).
 * 
 * Test with two XIAO nRF54L15 boards:
 * 1. Flash this sketch on the peripheral
 * 2. Flash BleOOBCentral on the central  
 * 3. Copy the OOB data from the peripheral Serial monitor to the central
 * 4. The central will connect and initiate OOB pairing
 * 5. After pairing, the peripheral sends encrypted notifications
 * 
 * Serial output format (copy this to the central):
 * OOB: r=00112233445566778899AABBCCDDEEFF c=00112233445566778899AABBCCDDEEFF
 */

#include <BleRadio.h>

BleRadio ble;

// OOB data buffers
uint8_t oob_r[16];
uint8_t oob_c[16];

// Service/char for encrypted notifications
constexpr uint16_t kService = 0x180F;
constexpr uint16_t kChar = 0x2A19;

void printHex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  Serial.print(": ");
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

void onSecured(uint16_t conn) {
  Serial.print("SECURED conn=");
  Serial.println(conn);
}

void onDisconnected(uint16_t conn, uint8_t reason) {
  Serial.print("DISCONNECTED conn=");
  Serial.print(conn);
  Serial.print(" reason=");
  Serial.println(reason);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("=== OOB Peripheral ===");
  
  ble.begin();
  ble.setDeviceName("OOB-P");
  ble.setSecureConnections(true);
  ble.setSecuredCallback(onSecured);
  ble.setDisconnectedCallback(onDisconnected);
  
  // Generate OOB data
  if (ble.generateSecurityOobData(oob_r, oob_c)) {
    Serial.println();
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
    Serial.println("Peripheral will start advertising when you press any key.");
  } else {
    Serial.println("ERROR: Failed to generate OOB data!");
    while (true) delay(1000);
  }
  
  // Wait for user to copy OOB data
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();
  
  // Set OOB flag and start advertising
  ble.setSecurityOobEnabled(true);
  ble.startAdvertising();
  Serial.println("Advertising with OOB pairing enabled...");
}

void loop() {
  ble.poll();
  
  // Send encrypted notifications when connected
  if (ble.isConnected()) {
    static unsigned long last_notify = 0;
    if (millis() - last_notify > 2000) {
      last_notify = millis();
      
      uint8_t data[4];
      data[0] = (millis() / 1000) & 0xFF;
      data[1] = (millis() / 256) & 0xFF;
      data[2] = 0xAA;
      data[3] = 0xBB;
      
      if (ble.queueNotifyValue(kChar, data, 4)) {
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
