#include <Arduino.h>

#include <string.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

/*
 * This sketch runs one Bluetooth LE CCM example straight from the Core spec.
 *
 * BLE CCM has one detail that trips people up:
 * - the first header byte is authenticated AAD, so it stays in the clear
 * - only the payload is encrypted
 * - the hardware appends a 4-byte MIC to the encrypted payload
 *
 * The wrapper takes the BLE ingredients in the same practical form the rest of
 * the Arduino core already uses: session key, IV, packet counter, direction,
 * header byte, and payload bytes.
 */

static Ccm g_ccm;

static const uint8_t kKey[16] = {
    0x99, 0xAD, 0x1B, 0x52, 0x26, 0xA3, 0x7E, 0x3E,
    0x05, 0x8E, 0x3B, 0x8E, 0x27, 0xC2, 0xC6, 0x66,
};

static const uint8_t kIv[8] = {
    0x24, 0xAB, 0xDC, 0xBA, 0xBE, 0xBA, 0xAF, 0xDE,
};

static const uint64_t kCounter = 0U;
static const uint8_t kDirection = 1U;
static const uint8_t kHeader = 0x0FU;
static const uint8_t kPlaintext[1] = {0x06};
static const uint8_t kExpectedCipherWithMic[5] = {
    0x9F, 0xCD, 0xA7, 0xF4, 0x48,
};

static void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(300);

  uint8_t cipherWithMic[8] = {};
  uint8_t cipherWithMicLen = 0U;
  const bool encOk =
      g_ccm.encryptBlePacket(kKey, kIv, kCounter, kDirection, kHeader,
                             kPlaintext, sizeof(kPlaintext),
                             cipherWithMic, &cipherWithMicLen,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);
  const bool encMatch =
      encOk &&
      (cipherWithMicLen == sizeof(kExpectedCipherWithMic)) &&
      (memcmp(cipherWithMic, kExpectedCipherWithMic,
              sizeof(kExpectedCipherWithMic)) == 0);

  uint8_t plaintext[4] = {};
  uint8_t plaintextLen = 0U;
  bool macValid = false;
  const bool decOk =
      g_ccm.decryptBlePacket(kKey, kIv, kCounter, kDirection, kHeader,
                             kExpectedCipherWithMic, sizeof(kExpectedCipherWithMic),
                             plaintext, &plaintextLen, &macValid,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);
  const bool decMatch =
      decOk && macValid &&
      (plaintextLen == sizeof(kPlaintext)) &&
      (memcmp(plaintext, kPlaintext, sizeof(kPlaintext)) == 0);

  Serial.println();
  Serial.println("CcmBleSpecVector");
  Serial.println("Encrypts and decrypts one Bluetooth LE CCM spec packet.");
  Serial.print("header=0x");
  Serial.println(kHeader, HEX);
  Serial.print("cipher+mic=");
  printHex(cipherWithMic, cipherWithMicLen);
  Serial.println();
  Serial.print("errorStatus=");
  Serial.println(static_cast<unsigned long>(g_ccm.errorStatus()));
  Serial.print("macValid=");
  Serial.println(macValid ? "yes" : "no");
  Serial.println((encMatch && decMatch) ? "PASS" : "FAIL");

  digitalWrite(LED_BUILTIN, (encMatch && decMatch) ? LOW : HIGH);
}

void loop() {
  delay(1000);
}
