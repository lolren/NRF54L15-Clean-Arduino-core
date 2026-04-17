# BLE / CS Power Characterization

This note defines the current bench path for measuring BLE and Channel Sounding
current on the `XIAO nRF54L15 / Sense`.

Scope:

- this is the real hardware measurement path for the shipped BLE / CS runtime
- it does not claim any current numbers by itself
- the current machine snapshot used for this update did not have a PPK2,
  Joulescope, or Otii attached, so the measurement harness is now implemented
  but the actual current table still needs one instrumented bench run

## Measurement Harness

Example:

- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/ChannelSounding/BleChannelSoundingVprServicePowerProbe/BleChannelSoundingVprServicePowerProbe.ino`

This sketch is intentionally quiet during the measured window. It runs four
phases, then prints a repeated summary line afterward:

1. generic VPR service idle, no BLE link
2. encrypted BLE link connected, no CS workflow
3. encrypted BLE link with repeated nominal CS workflow runs
4. generic VPR service idle again after disconnect

The CS phase uses a fixed inter-run gap so the measurement is reproducible and
does not collapse into a flat-out synthetic stress loop.

The output line looks like:

```text
power_probe ok=1 svc=1.16 opmask=0x7FFFFF phase_ms=5000/5002/10240/5002 total_ms=25262 cs_gap_ms=250 cs_runs=40 proc=120 last_q4=7537 last_nominal_dist_m=0.7537 last_sub=2/3 last_steps=5/5 final=0/0/0/0/0#13
```

Important:

- `last_nominal_dist_m` is still the nominal synthetic CS regression output
- this sketch is for current measurement, not physical-distance validation

## Recommended FQBN

For the first real current sweep, use:

```text
nrf54l15clean:nrf54l15clean:xiao_nrf54l15:clean_cpu=cpu64,clean_power=low,clean_autogate=balanced,clean_ble=on,clean_ble_timing=balanced,clean_vpr=on
```

Keep the same FQBN across all runs in one comparison table.

## Serial Handling

For current capture:

- keep the serial monitor closed during the 25-second measured window
- open it after the window if you want the repeated summary line

If you want to log the summary automatically:

```bash
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200 > cs_power_probe.log
python3 scripts/parse_cs_power_probe.py --log cs_power_probe.log
```

## Optional Marker Pin

The sketch has an optional marker pulse output:

- disabled by default for measurement purity
- if you need it, set `kEnableMarkerPin = true`
- the default marker pin is `D10`

Pulse counts on phase transitions:

- `1` pulse: service idle phase started
- `2` pulses: BLE-connected phase started
- `3` pulses: BLE + CS phase started
- `4` pulses: final idle phase started

Do not enable the marker unless you actually need external alignment. It adds a
small, intentional GPIO overhead.

## What Still Needs A Real Bench Run

The next real instrumented pass should record:

- average current for each phase
- 95th percentile current for each phase
- peak current for each phase
- instrument type and power path (`VBAT` or `VBUS`)

Once that is done, the checklist item
`Measured BLE / CS power characterization on real hardware`
can be closed honestly.

## Current Validation Status

The harness itself is validated on attached hardware:

- it compiles and uploads cleanly
- it runs the full four-phase sequence
- it emits a stable repeated summary line after the measured window
- the parser accepts that emitted line and produces the expected phase summary

What is still missing is the external current instrument capture, not the
firmware/control path.
