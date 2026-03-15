#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

/*
 * This sketch reads 32 bytes from the CRACEN hardware RNG and prints them.
 *
 * Use this when you want real entropy from the silicon instead of Arduino's
 * legacy pseudo-random generator state.
 */

static CracenRng g_rng;

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

  uint8_t bytes[32] = {};
  const bool ok = g_rng.begin(600000UL) &&
                  g_rng.fill(bytes, sizeof(bytes), 600000UL);
  const uint32_t status = g_rng.status();
  const uint32_t fifoWords = g_rng.availableWords();
  g_rng.end();

  Serial.println();
  Serial.println("CracenRandomBytes");
  Serial.println("Reads one 32-byte burst from the hardware entropy FIFO.");
  Serial.print("bytes=");
  printHex(bytes, sizeof(bytes));
  Serial.println();
  Serial.print("status=0x");
  Serial.println(status, HEX);
  Serial.print("fifoWords=");
  Serial.println(static_cast<unsigned long>(fifoWords));
  Serial.println(ok ? "PASS" : "FAIL");

  // The XIAO LED is active low. Leave it on for PASS, off for FAIL.
  digitalWrite(LED_BUILTIN, ok ? LOW : HIGH);
}

void loop() {
  delay(1000);
}
