# BLE Zephyr-Parity Implementation Plan

This plan is the working roadmap for getting the clean nRF54L15 Arduino BLE
path as close as practical to the Zephyr reference behavior for:

- connected idle current
- disconnected central reconnect current
- scan / connect / reconnect timing
- parity sketches used with PPK2

This is a local implementation plan. It is intentionally focused on the current
power and reconnect work, not on the full BLE compliance backlog.

## 1. What "All In Hardware" Actually Means

The nRF54L15 does not contain a complete BLE controller state machine that can
replace all link-layer software in this core.

So "do it in hardware" cannot mean:

- no software state machine
- no software scheduler
- no software LL/GATT logic

What it **can** mean here is:

- use `RADIO.SHORTS` wherever the silicon already supports the turnaround
- use `GRTC` compares for event timing instead of sketch-side or wall-clock
  pacing
- use IRQ-driven `WFI` sleep between radio milestones instead of CPU polling
- use DPPI/PPIB only where it is proven stable on the board/runtime
- keep the CPU out of scan / connect / idle wait windows unless software must
  actively construct the next packet or update controller state

That is the correct target: **hardware-assisted controller behavior**, not a
fictional "100% hardware BLE controller".

## 2. Current Known Gaps Versus Zephyr

These are the gaps that still matter for the parity work.

- [ ] Central reconnect path still has a software-heavy create-connection flow.
- [ ] A failed central connection attempt can fall back into an expensive
      reconnect-hunt pattern instead of behaving like Zephyr's scan-first
      controller path.
- [ ] Scan listen windows are still mostly CPU-polled instead of IRQ/WFI-driven.
- [ ] The current Arduino central create path uses a broad synchronous
      multi-channel hunt instead of a controller-style bounded create state.
- [ ] The generic IRQ-sleep radio wait experiment was too broad and touched
      timing-sensitive ADV / SCAN_REQ / connection paths together.
- [ ] The scan path and the connect path need to be offloaded separately, not
      in one large step.

## 3. Current Concrete Findings

These findings already came out of the local power work and should drive the
next code changes.

- [x] The broad RADIO-IRQ sleep conversion was not safe as a global change.
      It disturbed timing-sensitive BLE paths.
- [x] The current spike after disconnect is not just "BLE is expensive". A big
      part of it comes from software reconnect behavior.
- [x] `BluefruitCompatManager::maybeConnectCentral()` was using a large
      software connection hunt budget (`initiateConnection(..., 1200000UL)`).
- [x] Reconnect behavior was holding onto stale pending-connect state instead of
      always dropping cleanly back to scan mode like Zephyr does.
- [x] The scan path and the connection-create path need different offload
      strategies.

## 4. Design Rules For The Next Local Changes

These rules are here to prevent another round of regressions.

- [ ] Do not change UART/Serial while doing BLE power work.
- [ ] Do not convert all BLE waits to IRQ/WFI in one step again.
- [ ] Keep connectable advertising request/response timing on the currently
      proven path until scan/create parity is stable.
- [ ] Land scan-path offload and create-connection offload as separate slices.
- [ ] Every slice must be testable on the local XIAO pair plus Zephyr parity
      sketches.
- [ ] No release push until the local parity measurements are stable again.

## 5. Implementation Phases

## Phase A. Lock The Baseline

Goal: stop chasing moving targets.

- [ ] Keep one known-good Arduino parity central/peripheral pair in
      `sketches_parity_reached/`.
- [ ] Keep one known-good Zephyr parity pair beside it.
- [ ] Record exact roles by probe UID.
- [ ] Record connected current, disconnected current, reconnect current, and
      whether reconnect is automatic.
- [ ] Freeze sketch behavior before touching core timing again.

Exit criteria:

- [ ] Arduino and Zephyr parity sketches are functionally equivalent enough for
      current comparison.
- [ ] PPK2 measurements are repeatable on the same hardware pair.

## Phase B. Fix The Central Reconnect State Machine

Goal: stop burning current on stale software reconnect hunts.

- [ ] Clear stale `pending_connect_valid_` state after a failed create attempt.
- [ ] Resume scanner scheduling immediately after a failed create attempt.
- [ ] Bound the software connection-create budget to the current scan settings,
      not a giant hardcoded value.
- [ ] Make connection-create retries depend on fresh scan reports instead of
      looping forever on a stale report.
- [ ] Re-measure disconnected central current after this change alone.

Why this phase exists:

- Zephyr does not keep doing a large software reconnect hunt against stale
  state.
- This is the most likely cause of the `~1.2 mA` disconnected spikes.

Exit criteria:

- [ ] Disconnecting the peer no longer leaves the Arduino central in a long
      high-current reconnect-hunt plateau.
- [ ] Reconnect still succeeds automatically when the peer returns.

## Phase C. Move Scan Listen Windows To IRQ/WFI

Goal: make scan current look like controller behavior instead of CPU polling.

- [ ] Add a **scan-only** RADIO wait helper that uses IRQ/WFI for:
      - `ADDRESS`
      - `END` / `PHYEND`
- [ ] Use it only in:
      - passive scan receive
      - active scan receive
- [ ] Do not reuse it yet for:
      - connectable advertising response windows
      - connection event TX/RX
      - generic BLE waits
- [ ] Keep fallback polling if IRQ mode is not allowed or proves unstable.
- [ ] Compare disconnected central current against Zephyr again.

Why this phase exists:

- The scan path is the cleanest place to copy Zephyr-style sleep behavior.
- It gives most of the disconnected-current win without touching the most
  fragile connection timing yet.

Exit criteria:

- [ ] Scan-only current drops meaningfully toward the Zephyr disconnected
      current.
- [ ] No regression in scan callbacks, filtering, or basic central connect.

## Phase D. Replace The Synchronous Create-Connection Hunt

Goal: stop doing controller work in a large blocking software scan loop.

Target behavior:

- scan report arrives
- candidate is recorded
- create-connection path uses a bounded, controller-style wait strategy
- failure returns to scan mode quickly

Planned steps:

- [ ] Store richer candidate data from the last valid scan report:
      - advertiser address
      - address type
      - channel
      - event timing if available
      - PDU type / connectable flag
- [ ] Create a dedicated central create state instead of a broad "hunt all
      channels twice with a huge budget" loop.
- [ ] Prefer the last seen channel first.
- [ ] Use bounded retry windows based on scan settings, not large generic spin
      limits.
- [ ] Keep failure behavior Zephyr-like:
      - abandon stale candidate
      - resume scanning
      - wait for a fresh report

Exit criteria:

- [ ] Central reconnect behavior is stable.
- [ ] Connection creation no longer causes a large sustained current plateau.

## Phase E. Connected-Idle Event Sleep

Goal: close the remaining connected-idle gap after scan/create parity is fixed.

- [ ] Isolate connection-event wait points that are safe to move to IRQ/WFI.
- [ ] Keep the current proven callback/resync behavior for central setup until
      a narrower event-sleep version is validated.
- [ ] Use the same rule as the scan phase:
      IRQ/WFI only where the next hardware event is already fully determined.
- [ ] Re-test:
      - MTU 23 / Data Length 27
      - MTU 247 / Data Length 251 on explicit request
      - notify/write loops
      - reconnect after disconnect

Exit criteria:

- [ ] Connected current stays near the current good figure.
- [ ] No regressions in central/peripheral packet flow.

## Phase F. Extend The Same Model To ADV / SCAN_REQ / CONNECT_IND Paths

Goal: finish the offload work without breaking the already-working paths.

- [ ] Revisit connectable advertising request windows only after scan/create
      parity is stable.
- [ ] Introduce separate helpers for:
      - advertising listen window
      - scan-response turnaround
      - connect-indication acceptance
- [ ] Keep these isolated from scan-path helpers.

Why this is last:

- This is the most timing-sensitive area.
- The earlier broad IRQ conversion showed that changing this too early is how
  regressions get created.

## 6. Test Matrix For Every Slice

Every phase above must pass these local checks before moving on.

- [ ] Arduino parity central + Arduino parity peripheral
- [ ] Zephyr parity central + Zephyr parity peripheral
- [ ] Mixed Arduino/Zephyr pair in both role directions where possible
- [ ] Connected idle current
- [ ] Disconnected central current
- [ ] Reconnect latency and success rate
- [ ] NUS/BLEUart smoke test
- [ ] MTU/Data Length default behavior remains `23 / 27` without explicit
      sketch requests
- [ ] Explicit MTU/DLE requests still work

## 7. Priority Order

Do the work in this order:

1. Phase B: stale reconnect / oversized create budget
2. Phase C: scan-only IRQ/WFI offload
3. Phase D: dedicated create-connection state machine
4. Phase E: connected-idle event sleep
5. Phase F: advertising request-window offload

This order is intentional:

- it fixes the most obvious power bug first
- it narrows the hardware-offload work to the safest path first
- it avoids touching fragile ADV / CONNECT_IND timing until the scan/create
  behavior is already stable

## 8. Definition Of "Done"

This parity work is done when:

- [ ] disconnected central current is in the same practical range as the
      Zephyr parity reference
- [ ] reconnect behavior is controller-like instead of software-hunt-like
- [ ] connected current remains stable
- [ ] no regressions are introduced in normal Bluefruit central/peripheral
      operation
- [ ] the remaining software-owned BLE logic is limited to real controller
      state, not avoidable busy-wait timing windows

Until then, the correct direction is not "more random tweaks".
It is: isolate the scan path, isolate the create path, move each one onto
hardware-assisted sleep behavior, and verify each step with the same parity
pair and PPK2 setup.
