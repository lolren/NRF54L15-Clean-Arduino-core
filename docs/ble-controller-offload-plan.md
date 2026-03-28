# BLE Controller Offload Plan

This note captures the first realistic step toward moving BLE advertising away
from the sketch-driven loop and toward a controller-owned schedule on nRF54L15.

## Datasheet anchors

Based on `Nordic_nRF54L15_Datasheet_v1.0.pdf` in the local workspace:

- `8.10 GRTC — Global real-time counter`
  - ultra-low-power shared timer
  - 1 us resolution
  - compare channels
  - available in low-power modes
  - PPI connection
- `8.17.5 Radio states`
  - valid `TXEN -> READY -> START -> PHYEND -> DISABLE` flow
- `8.17.6 Transmit sequence`
  - shows why CPU-triggered `READY -> START` and `PHYEND -> DISABLE` gaps waste power
- `8.17.14 Register overview`
  - `TASKS_TXEN`, `TASKS_START`, `TASKS_DISABLE`
  - `SUBSCRIBE_TXEN`, `SUBSCRIBE_START`, `SUBSCRIBE_DISABLE`
  - `EVENTS_READY`, `EVENTS_PHYEND`, `EVENTS_DISABLED`
  - publish/subscribe support
- `8.17.14.89 SHORTS`
  - `TXREADY_START`
  - `PHYEND_DISABLE`
  - `READY_START`
  - `DISABLED_TXEN`
- `8.22 TIMER — Timer/counter`
  - optional higher-frequency local timing source if GRTC alone is not enough
- DPPI / PPI bridge sections
  - peripheral-to-peripheral triggering without CPU wakeups
- `8.26 VPR — RISC-V CPU`
  - future option for time-critical controller offload, but not required for the first milestone

## Current gap

Today the Arduino BLE path still does this at the sketch/library boundary:

1. The sketch calls one advertising event at a time.
2. The library enables radio activity.
3. Software waits through timing gaps.
4. The library tears radio activity back down.
5. The sketch calls `delay(...)`.

That means the sketch loop still owns cadence, and the CPU remains involved in
every advertising interval.

## First milestone

Implement a background legacy advertiser owned by the BLE library, not by the
sketch loop.

Scope:

- one non-connectable legacy advertising packet
- one channel at first
- fixed interval
- no scan response
- no connection support

Target behavior:

- sketch calls `startBackgroundLegacyAdvertising(...)` once
- BLE library arms the next wake on GRTC
- GRTC compare triggers `RADIO.TASKS_TXEN` through DPPI
- RADIO uses `SHORTS` for `TXREADY_START` and `PHYEND_DISABLE`
- `EVENTS_DISABLED` or equivalent completion path schedules the next interval
- CPU sleeps between intervals

This is the smallest step that proves controller-owned cadence and removes the
main sketch-loop dependency from advertising.

## Why start here

This milestone avoids the hardest parts of full BLE controller work:

- no three-channel primary rotation yet
- no extended advertising AUX scheduling yet
- no scan requests
- no connect requests
- no channel map / connection state machine

If this step fails to improve cadence ownership and sleep behavior, extending it
to full extended advertising will not help.

## Implementation entry points

Current code areas to evolve first:

- `BleRadio::advertiseEvent(...)`
- `BleRadio::beginUnconnectedRadioActivity(...)`
- existing BLE background GRTC service in `nrf54l15_hal.cpp`

The first code change should introduce a new background-advertising state
machine instead of expanding the existing per-call `advertiseEvent(...)` path.

## Acceptance criteria

- sketch no longer needs to call one advertising event per loop
- interval cadence comes from GRTC scheduling, not `delay(...)`
- PPK2 should show the same low idle floor between events as the fixed
  `delay()` path
- average current should drop compared with the sketch-driven advertiser

## Next after milestone one

1. Rotate across channels 37/38/39 from the background scheduler.
2. Add scan-response support.
3. Move extended advertising primary + AUX timing under the same scheduler.
4. Only after that consider VPR offload for tighter timing or lower active CPU cost.
