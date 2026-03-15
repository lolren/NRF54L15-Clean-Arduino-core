#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

/*
 * This sketch uses the CRACEN hardware RNG to seed Arduino's random().
 *
 * That is the practical bridge between "real entropy from the chip" and the
 * normal Arduino helpers that many sketches already use.
 */

static CracenRng g_rng;
static bool g_seeded = false;

static void seedPseudoRandomFromHardware() {
  uint32_t seed = 0U;

  // fill() can one-shot enable the block, read from the FIFO, and return.
  if (!g_rng.fill(&seed, sizeof(seed), 600000UL)) {
    Serial.println("Hardware RNG seed failed");
    return;
  }

  randomSeed(seed);
  g_seeded = true;

  Serial.print("seed=0x");
  Serial.println(seed, HEX);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("CracenSeedArduinoRandom");
  Serial.println("Seeds Arduino random() from the hardware entropy source.");

  seedPseudoRandomFromHardware();
  digitalWrite(LED_BUILTIN, g_seeded ? LOW : HIGH);
}

void loop() {
  if (!g_seeded) {
    delay(1000);
    return;
  }

  Serial.print("dice=");
  Serial.print(random(1, 7));
  Serial.print(',');
  Serial.print(random(1, 7));
  Serial.print(',');
  Serial.println(random(1, 7));

  delay(1000);
}
