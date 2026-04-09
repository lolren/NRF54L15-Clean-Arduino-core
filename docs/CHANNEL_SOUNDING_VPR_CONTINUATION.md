# Channel Sounding VPR Continuation

This note is the resume point for the current Channel Sounding and VPR transport work.

## Current State

The clean core now has a real VPR-backed transport path for controller-style Channel Sounding bring-up.

Implemented locally in the main repo:

- shared-memory VPR transport and boot helpers:
  - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_vpr.h`
  - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_vpr.cpp`
  - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_vpr_transport_shared.h`
- linker reservations for CPUAPP <-> VPR transport windows:
  - `hardware/nrf54l15clean/nrf54l15clean/cores/nrf54l15/nrf54l15_linker_script.ld`
- controller-facing CS helpers:
  - HCI CS command builders
  - HCI event parsers
  - HCI subevent result reassembly
  - RTT step decode from controller-style packets
  - workflow / session / host / H4 / stream host layers
  - VPR-backed host wrapper
  - files:
    - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/ble_channel_sounding.h`
    - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/ble_channel_sounding.cpp`
- VPR stub firmware source and generated image:
  - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/tools/vpr/vpr_cs_transport_stub.c`
  - `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/vpr_cs_transport_stub_firmware.h`

## Validated Result

The important milestone already passed:

- the VPR-backed CS transport boots
- the VPR-backed host wrapper drives the CS workflow
- the VPR stub has built-in fallback responders for the supported CS opcode set
- the initiator no longer needs to preload script responses from the sketch for the working demo path

Working demo command on the initiator sketch:

- `hcivprtransportdemo`

Validated logs:

- `/home/lolren/Desktop/Nrf54L15/.build/cs_vpr_final_run_initiator.log`
- `/home/lolren/Desktop/Nrf54L15/.build/cs_vpr_final_run_reflector.log`
- `/home/lolren/Desktop/Nrf54L15/.build/cs_vpr_builtin_run_initiator.log`
- `/home/lolren/Desktop/Nrf54L15/.build/cs_vpr_builtin_run_reflector.log`

The key proof line from the built-in responder path is:

- `hcivprtransportdemo ok=1 pumped=12 wrote=6/88 read=212/63 phase=ready ... proc=7 dist_m=0.7501`

That proves:

- commands were sent through the VPR transport
- the workflow reached `ready`
- the supported opcode set was answered correctly by the VPR stub fallback
- subevent data flowed back far enough to produce an estimate

## Built-In VPR Stub Behavior

The VPR stub now has built-in fallback responders for these CS opcodes when no script entry matches:

- `0x208A` Read Remote Supported Capabilities
- `0x208D` Set Default Settings
- `0x2090` Create Config
- `0x208C` Security Enable
- `0x2093` Set Procedure Parameters
- `0x2094` Procedure Enable

The `Procedure Enable` fallback also emits built-in local CS subevent result and continuation packets so the host workflow can complete without sketch-side scripted injection.

## Known Good Design Choice

The stable path is:

- shared-memory windows
- host polling
- VPR stub firmware built from `vpr_cs_transport_stub.c`

One attempted variant should stay disabled unless reworked carefully:

- direct VPR -> host event signaling through the VPR event CSRs

That path regressed command flow after the second command even though shared memory stayed alive. The poll-based design is the proven baseline.

## Remaining Gaps

This is still not a production BLE controller runtime.

Still missing:

- a real connected BLE controller service on VPR instead of the current CS demo responder
- binding the CS workflow to real link state rather than demo-scripted behavior
- broader result and error handling around real procedures
- reliable raw RADIO RTT AUXDATA decode on the non-controller path
- broader validation across more boards and phone hosts

## Suggested Next Steps

1. Keep the poll-based transport as the baseline.
2. Preserve the built-in responder path as the working demo harness.
3. Move one real controller function at a time from host-side synthetic behavior into VPR-side service code.
4. Add an explicit transport self-test example that only checks boot, heartbeat, command roundtrip, and memory ownership.
5. Add a second VPR firmware mode for real command dispatch instead of hardcoded fallback responses.
6. Only revisit CSR event signaling after the controller service is stable on shared-memory polling.

## Resume Checklist

When resuming this work:

1. Regenerate the VPR stub header:
   - `python3 hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/tools/generate_vpr_cs_transport_stub.py`
2. Rebuild the initiator example.
3. Flash initiator and reflector.
4. Run `hcivprtransportdemo`.
5. Confirm the built-in responder path still reaches `phase=ready`.
6. Only then start changing transport or controller behavior.

## Notes

- The generated firmware header path bug was already fixed in the generator.
- The initiator sketch still contains some now-unused demo helper code from the earlier script-driven phase. That is cleanup work, not a functional blocker.
- The current repo docs already describe the feature as partial. Keep that wording until a real controller/runtime exists.
