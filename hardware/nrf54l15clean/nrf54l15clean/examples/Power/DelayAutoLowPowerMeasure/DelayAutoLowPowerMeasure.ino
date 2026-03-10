// Minimal current-measurement sketch for the automatic low-power delay path.
//
// On XIAO nRF54L15, when Tools -> Power Profile is set to
// `Low Power (WFI Idle)`, long delay() calls automatically reuse the same
// board-collapse path as delayLowPowerIdle(ms) as long as the SAMD11 bridge
// UART is not active.
//
// This sketch intentionally does nothing except sleep with delay(). It is the
// cleanest way to compare the core's ordinary-Arduino idle behavior against a
// Zephyr `k_sleep()` test.
//
// For a fair measurement:
// - do not call Serial.begin()
// - close any active Serial monitor
// - disconnect external LEDs and other GPIO loads
// - select Tools -> Power Profile -> Low Power (WFI Idle)

void setup() {}

void loop() {
  delay(1000);
}
