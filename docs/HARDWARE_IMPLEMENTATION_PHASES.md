# nRF54L15 Hardware Implementation Phases

This document turns the priority list from
[`HARDWARE_IMPLEMENTATION_GAP_MAP.md`](HARDWARE_IMPLEMENTATION_GAP_MAP.md)
into concrete execution phases.

The goal is to keep the next hardware work realistic for this codebase:

- each phase should end with a usable HAL surface
- each phase should have one or two proof examples
- each phase should have validation that works on real hardware, not just compile smoke

Status baseline:

- repo release line: `0.4.1`
- board target: Seeed XIAO nRF54L15
- execution style: direct-register Arduino core, no Zephyr runtime

## Phase 1: KMU Foundation

Goal:

- expose the nRF54L15 Key Management Unit as a safe internal HAL surface

Deliverables:

- `Kmu` wrapper with:
  - slot enumeration
  - slot status query
  - slot erase
  - slot program from explicit test material
- clear ownership rules between KMU and CRACEN consumers
- internal documentation for what key types are supported first

Out of scope for this phase:

- polished public Arduino API
- production provisioning workflow
- secure manufacturing tooling

Validation:

- compile smoke
- provision known test material into a slot
- read back slot status only, not raw secret bytes
- cold boot and warm reset persistence check

Exit criteria:

- a developer can deterministically provision and inspect KMU slot state on XIAO nRF54L15

## Phase 2: KMU -> CRACEN Path

Goal:

- prove that KMU material can be consumed by hardware crypto without falling back to raw in-memory key handling

Deliverables:

- ECB or CCM proof path using KMU-backed material
- example sketch:
  - provision test key
  - perform one CRACEN operation using that key path
- explicit zeroization of temporary staging buffers where relevant

Validation:

- deterministic crypto result against known test vector
- reset/reuse behavior
- negative test for empty or invalid slot selection

Exit criteria:

- the repo has one documented hardware-crypto example where KMU meaningfully participates

## Phase 3: MAILBOX / VPR Bring-Up

Goal:

- establish the minimum infrastructure needed to control the VPR side safely from the main core

Deliverables:

- `Mailbox` HAL wrapper
- minimal `VprControl` surface:
  - reset
  - start
  - stop
  - state query
- agreed shared-memory layout for one small message block
- linker/memory documentation for Cortex-M33 <-> VPR ownership

Out of scope for this phase:

- software-defined peripherals
- complex VPR firmware
- scheduler-like abstractions

Validation:

- mailbox message roundtrip
- deterministic trigger/completion sequence
- repeated start/stop cycle stability

Exit criteria:

- one proof-of-life example reliably exchanges a message with VPR and reports success back on serial

## Phase 4: First SoftPeripheral Proof

Goal:

- prove the MAILBOX / VPR infrastructure is useful beyond a synthetic ping

Preferred target:

- one very small software-defined peripheral or coprocessed helper path

Suggested options:

- software-defined serial helper
- tiny VPR offloaded waveform/timing task
- minimal sQSPI-oriented proof if practical

Validation:

- repeated roundtrip or signal-generation check
- low-power interaction sanity check
- no regressions in existing BLE / Zigbee smoke builds

Exit criteria:

- the repo demonstrates one concrete user-visible capability that depends on the VPR foundation

## Phase 5: TAMPC Base Wrapper

Goal:

- expose tamper status/control cleanly enough for diagnostics and secure-product experimentation

Deliverables:

- `Tampc` wrapper with:
  - enable/disable helpers
  - status read
  - flag clear
  - reset-policy configuration
- one serial diagnostic example

Validation:

- controlled status path exercise
- reset-policy behavior validation where safely testable
- startup reporting after reset cause

Exit criteria:

- tamper state can be queried and demonstrated from a sketch without direct register poking

## Phase 6: TAMPC Advanced Modes

Goal:

- extend the base wrapper to cover the parts of TAMPC that are actually useful for secure designs

Deliverables:

- external tamper configuration
- active shield configuration, if practical on the board target
- clear board-target caveats for XIAO limitations

Validation:

- feature-gated compile checks
- explicit documentation of what is real on XIAO vs what needs another board

Exit criteria:

- TAMPC support is honest, documented, and not limited to one narrow status register demo

## Phase 7: Serial-Fabric Breadth

Goal:

- improve whole-SoC coverage after the flagship hardware gaps are no longer blocking

Deliverables:

- cleaner exposure of more serial instances
- better HS-instance coverage where supported
- multi-instance examples

Validation:

- compile matrix over exposed instances
- at least one dual-instance runtime check

Exit criteria:

- the core feels less XIAO-default-only and more like a general nRF54L15 platform

## Phase 8: Secondary Hardware Breadth

Goal:

- finish the lower-risk expansion work after the major hardware-specific features are in place

Candidates:

- `EGU`
- broader `PWM` / `TIMER` / `PDM` instance coverage
- cleaner secondary `GPIOTE` / fabric helper exposure

Validation:

- compile sweep
- one real runtime example per new wrapper family

## Recommended Order

The practical order should stay:

1. KMU foundation
2. KMU -> CRACEN proof
3. MAILBOX / VPR bring-up
4. first softperipheral proof
5. TAMPC base wrapper
6. TAMPC advanced modes
7. serial-fabric breadth
8. secondary hardware breadth

This order keeps the work focused on the nRF54-specific features that most
differentiate the platform before spending time on breadth cleanup.

## Release Guidance

Release planning should follow implementation shape, not marketing pressure.

- `0.4.x` can carry internal refactors and one hardware phase at a time
- `0.5.0` would make sense once at least KMU and the first VPR/mailbox phase are real and tested
- `1.0` still depends more on overall BLE/Zigbee/platform maturity than on any single missing hardware block

## Practical Note

Not every datasheet block is worth equal effort on the XIAO board.

The best next work is the hardware that:

- is genuinely nRF54-specific
- unlocks secure/product-grade capability
- can be validated on the boards already in the lab

That is why KMU, VPR/MAILBOX, and TAMPC stay ahead of lower-value items like
NFCT or native USB surface work for this board target.
