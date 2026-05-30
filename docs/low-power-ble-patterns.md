# Low-Power BLE Patterns

This note captures the BLE operating modes now implemented in the Arduino core
for the XIAO nRF54L15, along with the helper APIs that make the board-specific
current savings reproducible.

## The Board-Level Lever That Mattered

On XIAO nRF54L15, the external RF switch path itself costs current if left powered.
For BLE sketches, the important board-specific pattern is:

1. power/select the RF path only for the active TX/RX window
2. release `RF_SW_CTL` to high-impedance when idle
3. turn off RF switch power on `P2.03` while idle

Those steps are now exposed as library APIs instead of sketch-local glue:

```cpp
BoardControl::enableRfPath(BoardAntennaPath::kCeramic);
BoardControl::collapseRfPathIdle();
BoardControl::setImuMicEnabled(false);
BoardControl::enterLowestPowerState();
PowerManager power;
power.systemOffTimedWakeMsNoRetention(1000);
```

The explicit `NoRetention` helpers are for the lowest-current sketches. The
default `systemOff*()` helpers preserve `.noinit` RAM so retained state and BLE
bond data are not discarded implicitly.

The low-power BLE examples are now sketch-owned. Advertising cadence, burst
count, burst gap, and system-off interval live as plain top-of-sketch
constants inside each example instead of hidden board-menu overrides.

## Mode 1: Lowest Continuous Background Beacon

Example:

- `examples/BLE/AdvertisingLowPower/BleAdvertiserRotatingBackground/BleAdvertiserRotatingBackground.ino`

Intent:

- stay continuously discoverable over time
- remain in `System ON`
- let the BLE HAL schedule HFXO/RADIO from GRTC/DPPI-assisted background timing
- transmit one primary-channel packet per interval and rotate channels over successive intervals
- collapse the board RF switch path between events

Why it exists:

- a full three-channel advertising event costs extra radio time and CPU service work every interval
- rotating `37 -> 38 -> 39` over intervals keeps scanner coverage while reducing average current for beacon-style payloads
- this is the first practical step toward Zephyr-like low-power advertising behavior in this raw Arduino BLE path

Default sketch profile:

- `ADV_NONCONN_IND`
- `1000 ms` interval
- `-10 dBm`
- low-power board build with VPR, Thread, Matter, and Zigbee disabled

## Mode 2: Continuous Advertising With RF-Switch Duty-Cycling

Example:

- `examples/BLE/AdvertisingLowPower/BleAdvertiserRfSwitchDutyCycle/BleAdvertiserRfSwitchDutyCycle.ino`

Intent:

- stay continuously discoverable
- remain in `System ON`
- collapse only the board RF switch path between advertising events

Tradeoff:

- best mode when the device should remain easy to discover
- higher average current than a burst beacon because the system stays awake enough to keep advertising cadence alive

Observed result in local testing:

- user-measured around `0.1 mA` with the validated sketch configuration
- change `kAdvertisingIntervalMs` in the sketch to `1000UL` if you want easier scanner visibility

## Mode 3: Hybrid Burst Advertising In System ON

Example:

- `examples/BLE/AdvertisingLowPower/BleAdvertiserHybridDutyCycle/BleAdvertiserHybridDutyCycle.ino`

Intent:

- send short bursts instead of a single event cadence
- stay in `System ON`
- collapse the RF path and idle with `WFI` between bursts

Why it exists:

- lower average current than a persistent advertiser
- easier to detect than a burst-plus-`SYSTEM OFF` design
- no cold boot on every burst

Default sketch profile:

- `2` events per burst
- `20 ms` gap inside the burst
- one burst every `10 s`
- `-10 dBm`
- `ADV_IND`

Status:

- compiled successfully in this turn
- reflashed and revalidated on hardware in this turn as `X54-HYBRID`

## Mode 4: Burst Advertising Plus Timed SYSTEM OFF

Example:

- `examples/BLE/AdvertisingLowPower/BleAdvertiserBurstSystemOff/BleAdvertiserBurstSystemOff.ino`

Intent:

- advertise briefly
- shut down board-side RF path
- enter true timed `SYSTEM OFF`
- cold boot on wake and repeat

Tradeoff:

- lowest average current
- not continuously visible to scanners
- visibility depends on burst density and scan timing

Observed result in local testing:

- user-measured in the `tens of uA`

Validated baseline:

- `6` events per wake
- `20 ms` burst gaps
- `1000 ms` timed `SYSTEM OFF`
- fixed low-power WFI board default
- default boot profile
- change `kSystemOffIntervalMs` in the sketch to `5000UL` if you want a sparser wake cadence

## Mode 5: Long-Sleep Phone-Tuned Beacon

Example:

- `examples/BLE/AdvertisingLowPower/BleAdvertiserPhoneBeacon15s/BleAdvertiserPhoneBeacon15s.ino`

Intent:

- wake infrequently
- advertise long enough to be caught by phone-style scanners
- keep all useful data in the primary advertising packet
- avoid scan-response dependence
- return to true timed `SYSTEM OFF`

Why it exists:

- the shortest burst-plus-`SYSTEM OFF` pattern is excellent for average current, but easy for phones to miss
- many battery beacons behave more like "wake, advertise for a real window, sleep long" than "emit a tiny burst and disappear"

Validated sketch profile:

- `ADV_NONCONN_IND`
- short local name in the primary ADV payload
- `0 dBm`
- `14` advertising events per wake
- `70 ms` gaps
- `14000 ms` timed `SYSTEM OFF`
- RF path collapsed while idle

Observed behavior:

- shows up in clusters during the wake window, then disappears during the long sleep window
- better fit for low-average-current beacon use than the older short-burst system-off example when practical scanner visibility matters

## Choosing A Mode

Use mode 1 when:

- you need a continuously running beacon-style advertiser
- scan responses and connections are unnecessary
- lowest `System ON` advertising current matters more than sending on all three channels in each interval

Use mode 2 when:

- you need practical continuous discoverability
- `~100 uA` class current is acceptable

Use mode 3 when:

- you want a middle ground
- scanners should still have a reasonable chance to catch advertisements
- you do not want a cold boot every cycle

Use mode 4 when:

- the lowest average current matters more than continuous discoverability
- burst beaconing is acceptable
- cold-boot behavior on every cycle is acceptable

Use mode 5 when:

- you want long battery-style sleep intervals
- scanners should still have a realistic chance to catch the device inside a longer scan window
- connectability and scan responses are unnecessary

## Important Constraint

These patterns are still built on the raw Arduino BLE path in this core, not a full
Zephyr-style BLE controller scheduler. The power floor now comes from matching the
board-level RF gating and `SYSTEM OFF` behavior, not from controller parity.

## Discussion #71 AdvCurrent Finding

The May 17, 2026 AdvCurrent comparison showed that the large current spikes were
from a loose PPK2/battery-pad connection, not from the BLE controller itself. The
remaining Arduino-side gap was the charge immediately before a
connectable/scannable foreground advertising event.

The cause was Bluefruit scheduler state, not RF_SW:

- foreground Bluefruit advertising uses `advertiseInteractEvent()`, not the
  non-connectable DPPI background advertiser
- `advertiseInteractEvent()` applies the BLE random advertising delay before TX
- Bluefruit updated `next_adv_due_us_` after the radio call, so during that
  random delay the core considered advertising overdue
- the low-power `delay()` path then skipped WFI and busy-spun the CPU for that
  random-delay window

The fix is to keep the BLE random advertising delay in the Bluefruit foreground
scheduler instead of inside the immediate radio call. The scheduler now wakes at
the final event time and asks the radio to transmit without an additional
internal random-delay sleep. That removes the visible pre-TX plateau while still
preserving the BLE random advertising delay between foreground events.
