#include <Arduino.h>
#include <nrf54l15.h>

static constexpr uint8_t kLedPin = 0U;  // XIAO LED = P2.0, active-low.
static constexpr uint8_t kWakeMagic = 0xA5U;
static constexpr unsigned long kHeartbeatOnMs = 80UL;
static constexpr unsigned long kHeartbeatPeriodMs = 800UL;
static constexpr unsigned long kAnnounceWindowMs = 4000UL;
static constexpr unsigned long kSleepMs = 1000UL;

static unsigned long g_bootMs = 0UL;
static unsigned long g_lastHeartbeatMs = 0UL;

static void ledInit() {
  NRF_P2->DIRSET = (1UL << kLedPin);
  NRF_P2->OUTSET = (1UL << kLedPin);
}

static void ledOn() { NRF_P2->OUTCLR = (1UL << kLedPin); }

static void ledOff() { NRF_P2->OUTSET = (1UL << kLedPin); }

static uint8_t readGpregret(uint8_t index) {
  if (index >= 2U) {
    return 0U;
  }
  return static_cast<uint8_t>(NRF_POWER->GPREGRET[index] &
                              POWER_GPREGRET_GPREGRET_Msk);
}

static void writeGpregret(uint8_t index, uint8_t value) {
  if (index >= 2U) {
    return;
  }
  NRF_POWER->GPREGRET[index] = static_cast<uint32_t>(value);
}

static void blinkCode(uint8_t count, unsigned long onMs = 120UL,
                      unsigned long gapMs = 180UL) {
  for (uint8_t i = 0; i < count; ++i) {
    ledOn();
    delay(onMs);
    ledOff();
    delay(gapMs);
  }
}

static void printResetSummary(uint32_t resetReason, uint8_t wakeMarker,
                              uint8_t bootCount) {
  const bool wokeFromOff = (resetReason & RESET_RESETREAS_OFF_Msk) != 0U;
  const bool wokeFromGrtc = (resetReason & RESET_RESETREAS_GRTC_Msk) != 0U;

  Serial.println();
  Serial.println("SystemOffWakeDiag");
  Serial.print("boot_count=");
  Serial.println(bootCount);
  Serial.print("resetreas=0x");
  Serial.println(resetReason, HEX);
  Serial.print("gpregret0=0x");
  Serial.println(wakeMarker, HEX);
  Serial.print("woke_from_off=");
  Serial.println(wokeFromOff ? "yes" : "no");
  Serial.print("woke_from_grtc=");
  Serial.println(wokeFromGrtc ? "yes" : "no");

  if (wokeFromOff && wokeFromGrtc && wakeMarker == kWakeMagic) {
    Serial.println("status=timed_system_off_wake_ok");
    blinkCode(2);
    return;
  }

  if (wokeFromOff && wakeMarker == kWakeMagic) {
    Serial.println("status=system_off_without_grtc_flag");
    blinkCode(3);
    return;
  }

  if (wokeFromGrtc && wakeMarker == kWakeMagic) {
    Serial.println("status=grtc_without_off_flag");
    blinkCode(4);
    return;
  }

  if (wakeMarker == kWakeMagic) {
    Serial.println("status=marker_survived_but_no_expected_reset_flags");
    blinkCode(5);
    return;
  }

  Serial.println("status=fresh_boot_or_external_reset");
  blinkCode(1, 240UL, 240UL);
}

void setup() {
  ledInit();
  ledOff();

  Serial.begin(115200);
  delay(300);

  const uint32_t resetReason = NRF_RESET->RESETREAS;
  const uint8_t wakeMarker = readGpregret(0U);
  uint8_t bootCount = readGpregret(1U);
  bootCount = static_cast<uint8_t>(bootCount + 1U);

  // Preserve a simple boot counter across timed SYSTEM OFF cycles.
  writeGpregret(1U, bootCount);
  // Clear consumed diagnostics so the next boot shows the new wake state.
  NRF_RESET->RESETREAS = resetReason;
  writeGpregret(0U, 0U);

  printResetSummary(resetReason, wakeMarker, bootCount);
  Serial.print("sleep_in_ms=");
  Serial.println(kAnnounceWindowMs);
  Serial.flush();

  g_bootMs = millis();
  g_lastHeartbeatMs = g_bootMs;
}

void loop() {
  const unsigned long now = millis();

  if ((now - g_lastHeartbeatMs) >= kHeartbeatPeriodMs) {
    g_lastHeartbeatMs = now;
    ledOn();
    delay(kHeartbeatOnMs);
    ledOff();
  }

  if ((now - g_bootMs) >= kAnnounceWindowMs) {
    Serial.print("entering_delaySystemOffNoRetention_ms=");
    Serial.println(kSleepMs);
    Serial.flush();
    writeGpregret(0U, kWakeMagic);
    delay(50);
    delaySystemOffNoRetention(kSleepMs);
  }
}
