# nRF54L15 Hardware Implementation Gap Map

This document tracks the remaining nRF54L15 hardware surface that is either:

- already implemented and usable
- present in the silicon but not wrapped cleanly yet
- present in the silicon but low-value on the XIAO board
- often confused with "hardware missing" even though the real gap is still higher-level software

The goal is to keep the next hardware work focused on the parts of the SoC that
still unlock meaningful capability for this core, not to treat every datasheet
block as equally urgent.

Status baseline:

- repo release line: `0.4.1`
- board target: Seeed XIAO nRF54L15
- implementation style: direct register-level core, no Zephyr runtime, no NCS runtime

## Already Covered Well

The current hardware surface is already strong for normal Arduino and wireless
bring-up work.

Implemented or exposed today:

- GPIO / interrupts / GPIOTE
- SPI controller and SPI target
- I2C controller plus I2C target through `Wire`
- UART / serial routing on the XIAO board
- SAADC / VBAT / TEMP
- TIMER / PWM / GRTC / watchdog / low-power paths
- COMP / LPCOMP / QDEC
- PDM / I2S
- DPPI helpers
- CRACEN RNG, AAR, ECB, CCM
- raw RADIO plus BLE / Zigbee / proprietary helpers

Evidence:

- [Nrf54L15-Clean-Implementation README](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/README.md)
- [Development Notes](development.md)
- [Wire.h](../hardware/nrf54l15clean/nrf54l15clean/cores/nrf54l15/Wire.h)
- [Hardware Implementation Phases](HARDWARE_IMPLEMENTATION_PHASES.md)

## Highest Priority

These are the remaining hardware-facing gaps that are worth doing first because
they unlock real nRF54-specific value, not just cosmetic parity.

### P0: KMU

Current state:

- the silicon has a real Key Management Unit
- the repo has register definitions only
- there is no clean wrapper, provisioning flow, or sketch/example surface

Why it matters:

- this is the right way to move sensitive key material toward CRACEN
- it matters for secure product work, not just demos
- it is one of the clearest "nRF54-specific hardware advantage" gaps still open

What should exist:

- `Kmu` HAL wrapper
- slot provisioning / erase / status helpers
- controlled CRACEN key handoff path
- one or two examples:
  - provision a test key
  - run CRACEN-backed crypto without exposing raw key bytes again

Suggested validation:

- slot write/read-status checks
- CRACEN/ECB or CRACEN/CCM path using KMU-loaded key material
- persistence and reset behavior validation

### P0: VPR / MAILBOX / SoftPeripheral Foundation

Current state:

- the datasheet exposes the RISC-V coprocessor and mailbox path
- the repo currently only has low-level type definitions and a small linker reservation
- there is no usable runtime layer for VPR tasks, mailbox handoff, or software-defined peripherals

Why it matters:

- this is one of the biggest differentiators of the nRF54 family
- a lot of future capability depends on it, including soft peripherals like sQSPI
- without this, the core is still leaving one of the major SoC features unused

What should exist first:

- minimal `Vpr` / `Mailbox` infrastructure
- boot / reset / trigger / status control
- safe memory ownership contract between Cortex-M33 and VPR
- one simple proof-of-life example before any ambitious soft peripheral work

Suggested validation:

- message roundtrip through the mailbox
- deterministic trigger / completion flow
- retained-context behavior across low-power modes

### P0: TAMPC / Tamper Controller

Current state:

- the silicon exposes tamper detection, active shield control, and reset policy
- the core touches related protection setup during system init
- there is no public HAL wrapper or real example set for tamper behavior

Why it matters:

- this is a real security hardware block, not a marketing extra
- it belongs with KMU/CRACEN in the "serious secure product" path
- it is one of the obvious datasheet features still missing from the public surface

What should exist:

- `TamperController` or `Tampc` HAL wrapper
- status / clear / enable helpers
- internal tamper reset enable
- external tamper and active shield configuration
- one diagnostic example that reports cause/status cleanly

Suggested validation:

- controlled status flag tests
- reset-enable behavior validation
- interaction with secure/non-secure startup policy

## Medium Priority

These are real hardware gaps, but they are more about breadth and completeness
than missing flagship capability.

### P1: Full Serial-Fabric Instance Coverage

Current state:

- the SoC exposes five serial interfaces with HS and regular variants
- the current core uses the right pieces for the XIAO board, but it does not expose the whole instance map cleanly
- some instance constants and wrapper defaults are still board-focused rather than SoC-complete

What is still missing or partial:

- cleaner exposure of more `SPIM`, `SPIS`, `TWIM`, `TWIS`, and `UARTE` instances
- clearer HS-instance support in the public HAL
- better multi-instance examples instead of only default-route examples

Why it matters:

- this improves advanced board ports and custom pin-routing work
- it reduces the gap between "works on XIAO defaults" and "usable as a general nRF54L15 platform"

### P1: Multi-Instance Peripheral Breadth

Current state:

- the wrappers are good, but some peripheral families are effectively represented by one validated instance even when the SoC has more

Candidates here:

- more `PWM` instances
- more `TIMER` instances
- `PDM21`
- cleaner exposure of secondary `GPIOTE` / fabric helper instances
- broader `WDT` instance handling where relevant

This is not blocked by missing hardware knowledge. It is mostly wrapper cleanup,
example coverage, and validation effort.

### P1: EGU Wrapper

Current state:

- the SoC exposes event generator units
- the core already uses DPPI/PPIB heavily internally
- there is no public `Egu` wrapper

Why it matters:

- it is useful glue hardware for low-latency event architectures
- it fits the style of the existing `Timer`, `Gpiote`, and `Dppic` helpers

This is a good "clean HAL expansion" task once the P0 blocks are in better shape.

## Lower Priority

These blocks are real, but they are not the next best use of time on the XIAO board.

### P2: NFCT

Current state:

- the SoC has `NFCT`
- the core explicitly does not wrap it today

Why it is low priority:

- the XIAO board does not expose a practical NFC antenna path for normal sketches
- implementing it now would increase maintenance without helping the main board target much

This becomes worth revisiting only if:

- a different nRF54L15 board target is added
- or there is a specific need for NFC-A listener/tag work

### P2: Native USB Device Surface

Current state:

- the XIAO board routes serial through the external SAMD11 bridge
- the current core model is built around that board reality

Why it is low priority:

- native nRF54 USB device support is not the practical board path here
- it would be more relevant for another board variant than for the shipped XIAO setup

### P2: Trace / Debug Specialty Blocks

Examples:

- ETM
- ITM
- TPIU
- advanced authenticated debug plumbing beyond what is needed for normal development

These are real features, but they are not the next product-value unlocks for the core.

## Not Primarily Hardware Gaps

These are important, but the main work is above the raw peripheral layer.

### BLE

The major remaining BLE work is now mostly controller/host-stack behavior, not
missing silicon wrappers:

- broader generic central/client behavior
- more complete pairing/bond persistence coverage
- fuller Bluefruit parity on less-common flows
- finished user-facing channel sounding

The current channel-sounding code itself already documents that the RTT side is
still incomplete enough to be unreliable as a clean-core ranging feature.

### Zigbee

The current gap is not "the radio hardware is missing." The remaining work is:

- fuller Zigbee 3.0 device/cluster coverage
- richer HA personalities
- better sleepy remote/device typing
- more coordinator/interoperability validation breadth

### Thread and Matter

These are future stack projects, not missing peripheral wrappers.

## Recommended Implementation Order

If the goal is "maximum value per unit of engineering time", the order should be:

1. `KMU`
2. `VPR` / `MAILBOX` foundation
3. `TAMPC`
4. serial-fabric breadth and multi-instance cleanup
5. `EGU`
6. optional board-limited blocks like `NFCT`

For the concrete execution breakdown, see:

- [`HARDWARE_IMPLEMENTATION_PHASES.md`](HARDWARE_IMPLEMENTATION_PHASES.md)

## Suggested Phase Breakdown

### Phase 1: Security Hardware

- `Kmu` wrapper
- `Tampc` wrapper
- examples and diagnostics
- secure key path integration with CRACEN helpers

### Phase 2: Coprocessor Foundation

- VPR boot/trigger/mailbox layer
- one proof-of-life example
- retained-context and low-power interaction checks

### Phase 3: Serial-Fabric Completion

- complete instance map exposure
- HS-instance documentation
- validation examples for non-default routes

### Phase 4: Remaining Peripheral Breadth

- `Egu`
- second-instance cleanup for selected peripherals
- lower-priority board-specific extras

## Practical Conclusion

The core is already past the point where "basic hardware support" is the main
problem. The high-value remaining hardware work is concentrated in:

- `KMU`
- `VPR` / mailbox / softperipheral groundwork
- `TAMPC`

Everything else is either:

- already implemented enough to use
- mainly a completeness pass
- or a higher-level BLE/Zigbee/Thread/Matter software effort
