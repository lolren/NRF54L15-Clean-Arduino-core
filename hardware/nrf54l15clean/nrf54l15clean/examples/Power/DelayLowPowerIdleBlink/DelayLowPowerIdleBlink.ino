// Pulse the LED, then enter an explicit low-power idle window.
//
// delayLowPowerIdle(ms) keeps the MCU in System ON, temporarily collapses the
// XIAO board-control rails/nets, uses the core's low-power idle path, and
// restores the previous GPIO state after wake.
//
// This is opt-in because it is closer to the Zephyr low-current measurement
// setup than a normal Arduino delay() call.
//
// Select Tools -> Power Profile -> Low Power (WFI Idle) for the lowest current.
//
// Avoid active Serial bridge traffic during the low-power window: the helper
// temporarily releases the SAMD11 bridge pins at the GPIO level as part of the
// board collapse, but it does not suspend an already-running UARTE session.

static constexpr unsigned long kBlinkOnMs = 40UL;
static constexpr unsigned long kSleepMs = 960UL;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(kBlinkOnMs);
  digitalWrite(LED_BUILTIN, LOW);
  delayLowPowerIdle(kSleepMs);
}
