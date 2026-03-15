#include <Arduino.h>

#include <stdio.h>
#include <string.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static Spis g_spis;
static uint8_t g_txBuffer[48];
static uint8_t g_rxBuffer[48];
static uint32_t g_transactionCount = 0U;

static void pulseLed() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(25);
  digitalWrite(LED_BUILTIN, HIGH);
}

static void armNextTransaction() {
  memset(g_rxBuffer, 0, sizeof(g_rxBuffer));
  snprintf(reinterpret_cast<char*>(g_txBuffer), sizeof(g_txBuffer),
           "XIAO-SPIS #%lu", static_cast<unsigned long>(g_transactionCount));

  const size_t txLen =
      strnlen(reinterpret_cast<const char*>(g_txBuffer), sizeof(g_txBuffer) - 1U) + 1U;
  if (!g_spis.setBuffers(g_rxBuffer, sizeof(g_rxBuffer), g_txBuffer, txLen)) {
    Serial.println("setBuffers failed");
    return;
  }
  if (!g_spis.releaseTransaction()) {
    Serial.println("releaseTransaction failed");
  }
}

static void printBufferAscii(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const char c = static_cast<char>(data[i]);
    Serial.print((c >= 32 && c <= 126) ? c : '.');
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("SpisTargetEcho");
  Serial.println("Target pins: CS=D2 SCK=D8 MISO=D9 MOSI=D10");
  Serial.println("Connect an external SPI controller in MODE0 and read/write any payload.");

  if (!g_spis.begin(kDefaultSpiSck, kDefaultSpiMosi, kDefaultSpiMiso, kPinD2,
                    SpiMode::kMode0, false, 0xEEU, 0xCCU, true)) {
    Serial.println("SPIS begin failed");
    while (true) {
      delay(1000);
    }
  }

  armNextTransaction();
}

void loop() {
  if (!g_spis.pollEnd(true)) {
    return;
  }

  const size_t rxLen = g_spis.receivedBytes();
  const size_t txLen = g_spis.transmittedBytes();
  const bool overflow = g_spis.overflowed();
  const bool overread = g_spis.overread();

  Serial.print("transaction #");
  Serial.print(static_cast<unsigned long>(g_transactionCount));
  Serial.print(" rx=");
  Serial.print(static_cast<unsigned long>(rxLen));
  Serial.print(" tx=");
  Serial.print(static_cast<unsigned long>(txLen));
  Serial.print(" overflow=");
  Serial.print(overflow ? "yes" : "no");
  Serial.print(" overread=");
  Serial.println(overread ? "yes" : "no");

  if (rxLen > 0U) {
    Serial.print("rx ascii: ");
    printBufferAscii(g_rxBuffer, rxLen);
    Serial.println();
  }

  ++g_transactionCount;
  pulseLed();
  armNextTransaction();
}
