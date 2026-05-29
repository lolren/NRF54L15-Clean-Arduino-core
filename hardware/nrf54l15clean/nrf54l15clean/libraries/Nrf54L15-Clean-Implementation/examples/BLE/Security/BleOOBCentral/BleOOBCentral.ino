/**
 * OOB (Out-of-Band) Pairing - Central
 * 
 * Generates local OOB data and reads remote OOB data from Serial.
 * Connects to the peripheral and initiates OOB pairing.
 * 
 * Serial input format:
 * OOB: r=00112233445566778899AABBCCDDEEFF c=00112233445566778899AABBCCDDEEFF
 * 
 * Flash on both XIAO nRF54L15 boards, swap the ROLE between PERIPHERAL/CENTRAL.
 */

#include <Arduino.h>
#include <nrf54l15_hal.h>

using namespace xiao_nrf54l15;

#ifndef ROLE
#define ROLE CENTRAL
#endif

BleRadio g_ble;

static uint8_t local_oob_r[16];
static uint8_t local_oob_c[16];
static uint8_t remote_oob_r[16];
static uint8_t remote_oob_c[16];
static bool oob_data_ready = false;
static bool pairing_done = false;

void printHex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  Serial.print(": ");
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

void onConnected(uint16_t conn) {
  Serial.print("CONNECTED conn=");
  Serial.println(conn);
}

void onSecured(uint16_t conn) {
  Serial.println("SECURED: OOB pairing complete!");
  pairing_done = true;
}

void onDisconnected(uint16_t conn, uint8_t reason) {
  Serial.print("DISCONNECTED conn=");
  Serial.print(conn);
  Serial.print(" reason=");
  Serial.println(reason);
  pairing_done = false;
}

bool parseOOBData(const char* input) {
  const char* r_ptr = strstr(input, "r=");
  const char* c_ptr = strstr(input, "c=");
  if (!r_ptr || !c_ptr) {
    Serial.println("ERROR: Invalid format. Use: OOB: r=... c=...");
    return false;
  }
  r_ptr += 2;
  c_ptr += 2;
  
  for (int i = 0; i < 16; i++) {
    char hex[3] = {r_ptr[i * 2], r_ptr[i * 2 + 1], 0};
    remote_oob_r[i] = (uint8_t)strtol(hex, nullptr, 16);
  }
  for (int i = 0; i < 16; i++) {
    char hex[3] = {c_ptr[i * 2], c_ptr[i * 2 + 1], 0};
    remote_oob_c[i] = (uint8_t)strtol(hex, nullptr, 16);
  }
  
  oob_data_ready = true;
  Serial.println("OOB data loaded:");
  printHex("  r", remote_oob_r, 16);
  printHex("  c", remote_oob_c, 16);
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("=== OOB Central ===");
  
  g_ble.begin();
  g_ble.setDeviceName("OOB-C");
  g_ble.setSecureConnections(true);
  g_ble.setConnectedCallback(onConnected);
  g_ble.setSecuredCallback(onSecured);
  g_ble.setDisconnectedCallback(onDisconnected);
  
  // Generate local OOB data
  if (!g_ble.generateSecurityOobData(local_oob_r, local_oob_c)) {
    Serial.println("ERROR: Failed to generate local OOB data!");
    while (true) delay(1000);
  }
  
  Serial.println("Local OOB data generated:");
  printHex("  r", local_oob_r, 16);
  printHex("  c", local_oob_c, 16);
  Serial.println();
  Serial.println("Paste the OOB line from the peripheral:");
  Serial.println("OOB: r=00112233445566778899AABBCCDDEEFF c=00112233445566778899AABBCCDDEEFF");
  Serial.println();
  
  // Wait for user input
  while (!oob_data_ready) {
    if (Serial.available() > 0) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("OOB:")) {
        parseOOBData(line.c_str());
      }
    }
    delay(10);
  }
  
  // Set OOB data
  g_ble.setSecurityOobEnabled(true);
  g_ble.setSecurityOobLocalData(local_oob_r, local_oob_c);
  g_ble.setSecurityOobRemoteData(remote_oob_r, remote_oob_c);
  
  // Start scanning
  g_ble.startScanning();
  Serial.println("Scanning for peripheral...");
}

void loop() {
  g_ble.poll();
  
  // When connected, initiate pairing
  if (g_ble.isConnected() && oob_data_ready && !pairing_done) {
    static unsigned long last_try = 0;
    if (millis() - last_try > 2000) {
      last_try = millis();
      g_ble.setSecurityOobEnabled(true);
      g_ble.setSecurityOobLocalData(local_oob_r, local_oob_c);
      g_ble.setSecurityOobRemoteData(remote_oob_r, remote_oob_c);
      Serial.println("Requesting OOB pairing...");
      if (!g_ble.requestPairing()) {
        Serial.println("Pairing request failed, retrying...");
      }
    }
  }
  
  // If not connected, keep scanning
  if (!g_ble.isConnected() && !g_ble.isScanning()) {
    g_ble.startScanning();
  }
}
