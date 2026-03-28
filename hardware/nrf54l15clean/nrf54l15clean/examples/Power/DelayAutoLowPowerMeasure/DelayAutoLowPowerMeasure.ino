// Minimal current-measurement sketch for plain delay() in low-power mode.
//
// On XIAO nRF54L15, when Tools -> Power Profile is set to
// `Low Power (WFI Idle)`, delay() uses the tickless GRTC + WFI idle path.
// For sketches that need the explicit XIAO board-collapse/restore sequence,
// use delayLowPowerIdle(ms) instead.
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
