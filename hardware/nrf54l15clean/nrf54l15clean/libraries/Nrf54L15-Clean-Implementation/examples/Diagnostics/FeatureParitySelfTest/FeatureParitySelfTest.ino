#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static PowerManager g_power;
static Grtc g_grtc;
static TempSensor g_temp;
static Watchdog g_wdt;
static Pdm g_pdm;
static CracenRng g_rng;
static Aar g_aar;
static Ecb g_ecb;
static Ccm g_ccm;
static BleRadio g_ble;

static uint32_t g_passCount = 0;
static uint32_t g_totalCount = 0;
static constexpr int8_t kBleTxPowerDbm = -8;

static void reportResult(const char* name, bool pass, const char* detail) {
  Serial.print(pass ? "[PASS] " : "[FAIL] ");
  Serial.print(name);
  if (detail != nullptr && detail[0] != '\0') {
    Serial.print(" : ");
    Serial.print(detail);
  }
  Serial.print("\r\n");

  ++g_totalCount;
  if (pass) {
    ++g_passCount;
  }
}

static bool testPowerReset() {
  uint8_t previous = 0;
  bool ok = g_power.getRetention(0, &previous);
  if (ok) {
    ok = g_power.setRetention(0, 0xA5U);
  }
  uint8_t now = 0;
  if (ok) {
    ok = g_power.getRetention(0, &now) && (now == 0xA5U);
  }

  const uint32_t resetReason = g_power.resetReason();
  g_power.clearResetReason(resetReason);

  g_power.setLatencyMode(PowerLatencyMode::kConstantLatency);
  delayMicroseconds(50);
  const bool constLat = g_power.isConstantLatency();
  g_power.setLatencyMode(PowerLatencyMode::kLowPower);

  const bool dcdcOk = g_power.enableMainDcdc(true);
  const bool pofOk =
      g_power.configurePowerFailComparator(PowerFailThreshold::k2V8, true) &&
      g_power.powerFailComparatorEnabled() &&
      g_power.powerFailWarningEventEnabled();
  const bool pofBelow = g_power.powerBelowPowerFailThreshold();
  g_power.disablePowerFailComparator();
  g_power.setRetention(0, previous);

  char detail[128];
  snprintf(detail, sizeof(detail),
           "ret=0x%02X reset=0x%08lX constlat=%s dcdc=%s pof=%s below=%s",
           now, static_cast<unsigned long>(resetReason),
           constLat ? "yes" : "no",
           dcdcOk ? "ok" : "fail",
           pofOk ? "ok" : "fail",
           pofBelow ? "yes" : "no");
  reportResult("POWER+RESET", ok && dcdcOk && pofOk, detail);
  return ok && dcdcOk && pofOk;
}

static bool testGrtc() {
  bool ok = g_grtc.begin(GrtcClockSource::kSystemLfclk);
  if (ok) {
    g_grtc.clear();
    ok = g_grtc.setWakeLeadLfclk(4U) &&
         g_grtc.setCompareOffsetUs(0U, 20000U, true);
  }

  bool fired = false;
  const uint32_t startMs = millis();
  while ((millis() - startMs) < 100U) {
    if (g_grtc.pollCompare(0U, true)) {
      fired = true;
      break;
    }
    __asm volatile("wfi");
  }

  const uint64_t nowUs = g_grtc.counter();
  g_grtc.end();

  char detail[96];
  snprintf(detail, sizeof(detail), "fired=%s counter=%llu",
           fired ? "yes" : "no",
           static_cast<unsigned long long>(nowUs));
  reportResult("GRTC", ok && fired, detail);
  return ok && fired;
}

static bool testTemp() {
  int32_t tempMilliC = 0;
  const bool ok = g_temp.sampleMilliDegreesC(&tempMilliC, 400000UL);
  const bool rangeOk = (tempMilliC > -50000) && (tempMilliC < 130000);

  char detail[64];
  snprintf(detail, sizeof(detail), "temp=%ldmC", static_cast<long>(tempMilliC));
  reportResult("TEMP", ok && rangeOk, detail);
  return ok && rangeOk;
}

static bool testWatchdogConfig() {
  const bool running = g_wdt.isRunning();
  bool ok = !running &&
            g_wdt.configure(3000U, 0U, true, false, true);
  const uint32_t reqStatus = g_wdt.requestStatus();
  const bool rr0Enabled = (reqStatus & WDT_REQSTATUS_RR0_Msk) != 0U;

  char detail[80];
  snprintf(detail, sizeof(detail), "running=%s req=0x%08lX",
           running ? "yes" : "no",
           static_cast<unsigned long>(reqStatus));
  reportResult("WDT", ok && rr0Enabled, detail);
  return ok && rr0Enabled;
}

static bool testBoardControl() {
  bool ok = BoardControl::setAntennaPath(BoardAntennaPath::kCeramic) &&
            BoardControl::setAntennaPath(BoardAntennaPath::kExternal) &&
            BoardControl::setAntennaPath(BoardAntennaPath::kControlHighImpedance) &&
            BoardControl::setAntennaPath(BoardAntennaPath::kCeramic);

  int32_t vbatMilliVolts = -1;
  uint8_t vbatPercent = 0;
  const bool batteryOk = BoardControl::sampleBatteryMilliVolts(&vbatMilliVolts) &&
                         BoardControl::sampleBatteryPercent(&vbatPercent);
  ok = ok && batteryOk;

  char detail[96];
  snprintf(detail, sizeof(detail), "vbat=%ldmV pct=%u ant=%u",
           static_cast<long>(vbatMilliVolts),
           static_cast<unsigned>(vbatPercent),
           static_cast<unsigned>(BoardControl::antennaPath()));
  reportResult("BOARDCTRL", ok, detail);
  return ok;
}

static bool testPdm() {
  alignas(4) static int16_t pcm[64] = {};

  bool ok = g_pdm.begin(kPinMicClk, kPinMicData, true, 40U,
                        PDM_RATIO_RATIO_Ratio64, PdmEdge::kLeftRising);
  bool captured = false;
  if (ok) {
    captured = g_pdm.capture(pcm, 64U, 8000000UL);
  }
  g_pdm.end();

  long checksum = 0;
  for (size_t i = 0; i < 64U; ++i) {
    checksum += pcm[i];
  }

  char detail[96];
  snprintf(detail, sizeof(detail), "capture=%s sum=%ld first=%d",
           captured ? "ok" : "fail",
           checksum,
           static_cast<int>(pcm[0]));
  reportResult("PDM", ok && captured, detail);
  return ok && captured;
}

static bool testCracenRng() {
  uint8_t first[16] = {};
  uint8_t second[16] = {};

  const bool ok = g_rng.begin(600000UL) &&
                  g_rng.fill(first, sizeof(first), 600000UL) &&
                  g_rng.fill(second, sizeof(second), 600000UL);
  const bool same = (memcmp(first, second, sizeof(first)) == 0);
  const uint32_t status = g_rng.status();
  const uint32_t fifoWords = g_rng.availableWords();
  g_rng.end();

  char firstHex[9];
  char secondHex[9];
  snprintf(firstHex, sizeof(firstHex), "%02X%02X%02X%02X",
           first[0], first[1], first[2], first[3]);
  snprintf(secondHex, sizeof(secondHex), "%02X%02X%02X%02X",
           second[0], second[1], second[2], second[3]);

  char detail[128];
  snprintf(detail, sizeof(detail),
           "ok=%s healthy=%s same=%s status=0x%08lX fifo=%lu a=%s b=%s",
           ok ? "yes" : "no",
           g_rng.healthy() ? "yes" : "no",
           same ? "yes" : "no",
           static_cast<unsigned long>(status),
           static_cast<unsigned long>(fifoWords),
           firstHex,
           secondHex);
  const bool pass = ok && !same && (status != 0xFFFFFFFFUL);
  reportResult("CRACEN-RNG", pass, detail);
  return pass;
}

static bool testEcb() {
  static const uint8_t kKey[16] = {
      0x4C, 0x68, 0x38, 0x41, 0x39, 0xF5, 0x74, 0xD8,
      0x36, 0xBC, 0xF3, 0x4E, 0x9D, 0xFB, 0x01, 0xBF,
  };
  static const uint8_t kPlaintext[16] = {
      0x02, 0x13, 0x24, 0x35, 0x46, 0x57, 0x68, 0x79,
      0xAC, 0xBD, 0xCE, 0xDF, 0xE0, 0xF1, 0x02, 0x13,
  };
  static const uint8_t kExpected[16] = {
      0x99, 0xAD, 0x1B, 0x52, 0x26, 0xA3, 0x7E, 0x3E,
      0x05, 0x8E, 0x3B, 0x8E, 0x27, 0xC2, 0xC6, 0x66,
  };

  uint8_t ciphertext[16] = {};
  const bool ok = g_ecb.encryptBlock(kKey, kPlaintext, ciphertext, 400000UL);
  const bool match = (memcmp(ciphertext, kExpected, sizeof(ciphertext)) == 0);

  char hex[sizeof(ciphertext) * 2U + 1U];
  for (size_t i = 0; i < sizeof(ciphertext); ++i) {
    snprintf(&hex[i * 2U], 3U, "%02X", ciphertext[i]);
  }

  char detail[128];
  snprintf(detail, sizeof(detail), "ok=%s err=%lu out=%s",
           ok ? "yes" : "no",
           static_cast<unsigned long>(g_ecb.errorStatus()),
           hex);
  reportResult("ECB", ok && match, detail);
  return ok && match;
}

static bool testCcm() {
  static const uint8_t kKey[16] = {
      0x99, 0xAD, 0x1B, 0x52, 0x26, 0xA3, 0x7E, 0x3E,
      0x05, 0x8E, 0x3B, 0x8E, 0x27, 0xC2, 0xC6, 0x66,
  };
  static const uint8_t kIv[8] = {
      0x24, 0xAB, 0xDC, 0xBA, 0xBE, 0xBA, 0xAF, 0xDE,
  };
  static const uint8_t kPlaintext[1] = {0x06};
  static const uint8_t kExpectedCipherWithMic[5] = {
      0x9F, 0xCD, 0xA7, 0xF4, 0x48,
  };
  static const uint8_t kHeader = 0x0FU;

  uint8_t cipherWithMic[8] = {};
  uint8_t cipherWithMicLen = 0U;
  const bool encOk =
      g_ccm.encryptBlePacket(kKey, kIv, 0U, 1U, kHeader,
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
      g_ccm.decryptBlePacket(kKey, kIv, 0U, 1U, kHeader,
                             kExpectedCipherWithMic, sizeof(kExpectedCipherWithMic),
                             plaintext, &plaintextLen, &macValid,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);
  const bool decMatch =
      decOk && macValid &&
      (plaintextLen == sizeof(kPlaintext)) &&
      (memcmp(plaintext, kPlaintext, sizeof(kPlaintext)) == 0);

  uint8_t tampered[sizeof(kExpectedCipherWithMic)] = {};
  memcpy(tampered, kExpectedCipherWithMic, sizeof(tampered));
  tampered[sizeof(tampered) - 1U] ^= 0x01U;

  uint8_t tamperedPlaintext[4] = {};
  uint8_t tamperedPlaintextLen = 0U;
  bool tamperedMacValid = false;
  const bool tamperedOk =
      g_ccm.decryptBlePacket(kKey, kIv, 0U, 1U, kHeader,
                             tampered, sizeof(tampered),
                             tamperedPlaintext, &tamperedPlaintextLen,
                             &tamperedMacValid,
                             CcmBleDataRate::k125Kbit, 0xE3U, 600000UL);
  const bool tamperRejected = !tamperedOk && !tamperedMacValid;

  char detail[128];
  snprintf(detail, sizeof(detail),
           "enc=%s dec=%s mac=%s tamper=%s err=%lu len=%u",
           encMatch ? "ok" : "fail",
           decMatch ? "ok" : "fail",
           macValid ? "yes" : "no",
           tamperRejected ? "rejected" : "fail",
           static_cast<unsigned long>(g_ccm.errorStatus()),
           static_cast<unsigned>(cipherWithMicLen));
  const bool pass = encMatch && decMatch && tamperRejected;
  reportResult("CCM", pass, detail);
  return pass;
}

static bool computeBleAh(Ecb& ecb, const uint8_t irk[16], const uint8_t prand[3],
                         uint8_t hash[3]) {
  uint8_t keyBe[16] = {};
  uint8_t plaintextBe[16] = {};
  uint8_t ciphertextBe[16] = {};
  uint8_t ciphertextLe[16] = {};

  for (uint8_t i = 0U; i < 16U; ++i) {
    keyBe[i] = irk[15U - i];
  }
  plaintextBe[13] = prand[2];
  plaintextBe[14] = prand[1];
  plaintextBe[15] = prand[0];

  if (!ecb.encryptBlock(keyBe, plaintextBe, ciphertextBe, 400000UL)) {
    return false;
  }
  for (uint8_t i = 0U; i < 16U; ++i) {
    ciphertextLe[i] = ciphertextBe[15U - i];
  }
  memcpy(hash, &ciphertextLe[0], 3U);
  return true;
}

static bool buildResolvablePrivateAddress(Ecb& ecb, const uint8_t irk[16],
                                         const uint8_t prandSeed[3],
                                         uint8_t address[6]) {
  uint8_t prand[3] = {prandSeed[0], prandSeed[1],
                      static_cast<uint8_t>((prandSeed[2] & 0x3FU) | 0x40U)};
  uint8_t hash[3] = {};
  if (!computeBleAh(ecb, irk, prand, hash)) {
    return false;
  }
  memcpy(&address[0], hash, 3U);
  memcpy(&address[3], prand, 3U);
  return true;
}

static bool testAar() {
  static const uint8_t kSpecIrk[16] = {
      0x9B, 0x7D, 0x39, 0x0A, 0xA6, 0x10, 0x10, 0x34,
      0x05, 0xAD, 0xC8, 0x57, 0xA3, 0x34, 0x02, 0xEC,
  };
  static const uint8_t kSpecMatchingResolvable[6] = {
      0xAA, 0xFB, 0x0D, 0x94, 0x81, 0x70,
  };
  static const uint8_t kIrks[3][16] = {
      {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,
       0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED, 0xFE, 0x0F},
      {0x6C, 0xD4, 0x18, 0x5A, 0xB1, 0x22, 0x39, 0x7E,
       0x08, 0x6F, 0xC3, 0x91, 0xAA, 0x54, 0x2D, 0x17},
      {0xE0, 0xD1, 0xC2, 0xB3, 0xA4, 0x95, 0x86, 0x77,
       0x68, 0x59, 0x4A, 0x3B, 0x2C, 0x1D, 0x0E, 0xFF},
  };
  static const uint8_t kPrandSeed[3] = {0xA5, 0x5A, 0x13};

  bool specResolved = false;
  bool specOk = g_aar.resolveSingle(kSpecMatchingResolvable, kSpecIrk,
                                    &specResolved, 800000UL);

  uint8_t address[6] = {};
  bool generatedOk =
      buildResolvablePrivateAddress(g_ecb, kIrks[1], kPrandSeed, address);

  bool resolved = false;
  uint16_t index = 0xFFFFU;
  if (generatedOk) {
    generatedOk = g_aar.resolveFirst(address, &kIrks[0][0], 3U, &resolved,
                                     &index, 800000UL);
  }

  char addressHex[13];
  snprintf(addressHex, sizeof(addressHex), "%02X%02X%02X%02X%02X%02X",
           address[0], address[1], address[2], address[3], address[4],
           address[5]);

  char detail[128];
  snprintf(detail, sizeof(detail),
           "spec=%s/%s gen=%s/%s idx=%u amt=%lu err=%lu addr=%s",
           specOk ? "yes" : "no",
           specResolved ? "yes" : "no",
           generatedOk ? "yes" : "no",
           resolved ? "yes" : "no",
           static_cast<unsigned>(index),
           static_cast<unsigned long>(g_aar.resolvedAmountBytes()),
           static_cast<unsigned long>(g_aar.errorStatus()),
           addressHex);
  const bool pass =
      specOk && specResolved && generatedOk && resolved && (index == 1U);
  reportResult("AAR", pass, detail);
  return pass;
}

static bool testBleRadio() {
  bool ok = g_ble.begin(kBleTxPowerDbm);
  if (ok) {
    ok = g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd) &&
         g_ble.setAdvertisingName("PARITY-BLE", true) &&
         g_ble.setScanResponseName("PARITY-BLE-SCAN") &&
         g_ble.setGattDeviceName("PARITY-GATT") &&
         g_ble.setGattBatteryLevel(88U);
  }

  BleAdvInteraction interaction{};
  bool advertised = false;
  if (ok) {
    advertised = g_ble.advertiseInteractEvent(&interaction, 350U, 300000UL, 700000UL);
  }
  g_ble.end();

  char detail[112];
  snprintf(detail, sizeof(detail), "init=%s adv=%s scan_req=%s conn_ind=%s",
           ok ? "ok" : "fail",
           advertised ? "ok" : "fail",
           interaction.receivedScanRequest ? "yes" : "no",
           interaction.receivedConnectInd ? "yes" : "no");
  reportResult("BLE", ok && advertised, detail);
  return ok && advertised;
}

void setup() {
  Serial.begin(115200);
  delay(350);
  Serial.print("\r\nNrf54L15-Clean-Implementation FeatureParitySelfTest\r\n");

  testPowerReset();
  testGrtc();
  testTemp();
  testWatchdogConfig();
  testBoardControl();
  testPdm();
  testCracenRng();
  testEcb();
  testCcm();
  testAar();
  testBleRadio();

  char summary[96];
  snprintf(summary, sizeof(summary), "SUMMARY: %lu/%lu PASS\r\n",
           static_cast<unsigned long>(g_passCount),
           static_cast<unsigned long>(g_totalCount));
  Serial.print(summary);
}

void loop() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if ((now - lastMs) < 2000UL) {
    return;
  }
  lastMs = now;
  Serial.print("alive\r\n");
}
