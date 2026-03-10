// For lowest current, select Tools -> Power Profile -> Low Power (WFI Idle).
//
// This example uses only standard Arduino timing APIs on purpose. In
// `Low Power (WFI Idle)`, longer `delay()` calls on XIAO nRF54L15 now reuse the
// same board-collapse path as `delayLowPowerIdle(ms)` when the SAMD11 bridge
// UART is idle, so ordinary sketches get the lower-current System ON behavior
// automatically.
//
// `delayLowPowerIdle(ms)` is still the explicit helper when you want to force
// that board-collapse behavior from sketch code.
//
// Note: the XIAO user LED is active-low.

unsigned long lastToggleMs = 0;
bool ledState = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  const unsigned long now = millis();
  if ((now - lastToggleMs) >= 1000UL) {
    lastToggleMs = now;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
  }

  // Idle behavior is controlled by the Tools -> Power Profile menu.
  delay(5);
}
