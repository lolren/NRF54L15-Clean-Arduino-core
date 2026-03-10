// Pulse the built-in LED briefly, then spend the rest of the second in a
// normal delay() call.
//
// On XIAO nRF54L15, when Tools -> Power Profile is set to
// `Low Power (WFI Idle)`, long delay() calls automatically reuse the same
// board-collapse path as delayLowPowerIdle(ms) as long as the SAMD11 bridge
// UART is not active.
//
// This is the closest ordinary-Arduino sketch to the Zephyr `k_sleep()`
// measurement path: no explicit low-power helper in user code, just delay().
//
// For the lowest current:
// - avoid Serial.begin() in the sketch
// - close any active Serial monitor
// - disconnect external LEDs or other loads from GPIOs while measuring
//
// Note: the XIAO user LED is active-low.

static constexpr unsigned long kBlinkOnMs = 5UL;
static constexpr unsigned long kSleepMs = 995UL;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(kBlinkOnMs);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(kSleepMs);
}
