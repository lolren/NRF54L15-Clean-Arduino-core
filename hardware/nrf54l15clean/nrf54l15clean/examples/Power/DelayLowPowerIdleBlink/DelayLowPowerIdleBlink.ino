// Pulse the LED, then enter an explicit low-power idle window.
//
// delayLowPowerIdle(ms) keeps the MCU in System ON, temporarily collapses the
// XIAO board-control rails/nets, uses the core's low-power idle path, and
// restores the previous GPIO state after wake.
//
// This remains the explicit helper for forcing that board-collapse path from a
// sketch. In `Low Power (WFI Idle)`, normal long `delay()` calls on XIAO now
// use the same path automatically when the SAMD11 bridge UART is idle.
//
// Select Tools -> Power Profile -> Low Power (WFI Idle) for the lowest current.
//
// Avoid active Serial bridge traffic during the low-power window: the helper
// temporarily releases the SAMD11 bridge pins at the GPIO level as part of the
// board collapse, but it does not suspend an already-running UARTE session.
//
// Note: the XIAO user LED is active-low.

static constexpr unsigned long kBlinkOnMs = 40UL;
static constexpr unsigned long kSleepMs = 960UL;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(kBlinkOnMs);
  digitalWrite(LED_BUILTIN, HIGH);
  delayLowPowerIdle(kSleepMs);
}
