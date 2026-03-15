#include <Arduino.h>

#include <string.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

/*
 * This sketch shows the practical BLE CCM flow for an application packet:
 * 1. encrypt payload bytes and append a MIC
 * 2. decrypt the untouched packet and confirm the MAC passes
 * 3. flip one byte and confirm the MAC check fails
 *
 * That last step is the useful part for real projects: the hardware is not just
 * encrypting, it is also authenticating the packet content.
 */

static Ccm g_ccm;

static const uint8_t kKey[16] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
};

static const uint8_t kIv[8] = {
    0x55, 0x44, 0x33, 0x22, 0x11, 0x90, 0xA0, 0xB0,
};

static uint32_t gPacketCounter = 1U;

static void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
}

static void runOneRound() {
  static const uint8_t kHeader = 0x0EU;
  static const uint8_t kPayload[] = {
      'H', 'E', 'L', 'L', 'O', '-', 'C', 'C', 'M'
  };

  uint8_t cipherWithMic[sizeof(kPayload) + 4U] = {};
  uint8_t cipherWithMicLen = 0U;
  const bool encOk =
      g_ccm.encryptBlePacket(kKey, kIv, gPacketCounter, 1U, kHeader,
                             kPayload, sizeof(kPayload),
                             cipherWithMic, &cipherWithMicLen,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);

  uint8_t plaintext[sizeof(kPayload)] = {};
  uint8_t plaintextLen = 0U;
  bool macValid = false;
  const bool decOk =
      g_ccm.decryptBlePacket(kKey, kIv, gPacketCounter, 1U, kHeader,
                             cipherWithMic, cipherWithMicLen,
                             plaintext, &plaintextLen, &macValid,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);

  uint8_t tampered[sizeof(cipherWithMic)] = {};
  memcpy(tampered, cipherWithMic, sizeof(tampered));
  if (cipherWithMicLen > 0U) {
    tampered[cipherWithMicLen - 1U] ^= 0x01U;
  }

  bool tamperedMacValid = false;
  uint8_t tamperedPlaintext[sizeof(kPayload)] = {};
  uint8_t tamperedPlaintextLen = 0U;
  const bool tamperedOk =
      g_ccm.decryptBlePacket(kKey, kIv, gPacketCounter, 1U, kHeader,
                             tampered, cipherWithMicLen,
                             tamperedPlaintext, &tamperedPlaintextLen,
                             &tamperedMacValid,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);

  const bool pass =
      encOk &&
      decOk &&
      macValid &&
      (plaintextLen == sizeof(kPayload)) &&
      (memcmp(plaintext, kPayload, sizeof(kPayload)) == 0) &&
      !tamperedOk &&
      !tamperedMacValid;

  Serial.print("ctr=");
  Serial.println(static_cast<unsigned long>(gPacketCounter));
  Serial.print("cipher+mic=");
  printHex(cipherWithMic, cipherWithMicLen);
  Serial.println();
  Serial.print("roundtrip=");
  Serial.println((decOk && macValid) ? "ok" : "fail");
  Serial.print("tampered=");
  Serial.println((!tamperedOk && !tamperedMacValid) ? "rejected" : "unexpected");
  Serial.println(pass ? "PASS" : "FAIL");
  Serial.println();

  digitalWrite(LED_BUILTIN, pass ? LOW : HIGH);
  ++gPacketCounter;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("CcmBlePacketTamperDetect");
  Serial.println("Shows hardware BLE CCM round-trip and tamper rejection.");
}

void loop() {
  runOneRound();
  delay(2000);
}
