# BLE Pair/Bond Regression Runbook

Purpose: run repeatable BLE security checks with minimal manual interaction.

## Prerequisites

- XIAO nRF54L15 connected over USB (appears as `/dev/ttyACM*`).
- Host BLE adapter available as `hci0`.
- `arduino-cli`, `bluetoothctl`, and `btmon` installed.
- `sudo -n` access for `btmon/bluetoothctl` (recommended for unattended runs).

## Recommended command (pair/bond)

```bash
cd Nrf54L15-Clean-BoardPackage
./scripts/ble_pair_bond_regression.sh \
  --sudo \
  --attempts 10 \
  --mode pair-bond \
  --example BlePairingEncryptionStatus \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15:clean_ble_trace=on
```

## Bonded reconnect command

```bash
cd Nrf54L15-Clean-BoardPackage
./scripts/ble_pair_bond_regression.sh \
  --sudo \
  --attempts 5 \
  --mode bonded-reconnect \
  --example BleBondPersistenceProbe
```

## Optional adapter selection

Use these options when multiple host adapters are present:

```bash
--controller AA:BB:CC:DD:EE:FF   # bluetoothctl controller address (select)
--btmon-iface hci1               # btmon capture interface
```

Outputs:

- `measurements/ble_pair_bond_regression_<timestamp>/attempts.csv`
- `measurements/ble_pair_bond_regression_<timestamp>/summary.txt`
- per-attempt logs (`serial.log`, `btmon.log`, `pair.log`, `info.log`, etc.)

## Interpretation

- `pair_ok=yes` means host observed successful pairing outcome.
- `bonded=yes` means host reported bonded state.
- `enc_change_success=yes` means `Encryption Change` success event was seen.
- `reconnect_connected/reconnect_bonded/reconnect_enc_seen` are used in `bonded-reconnect` mode.
- `target_trace_error=yes` indicates target-side BLE trace failures seen on serial logs.
- `mic_failure=yes` indicates explicit MIC failure reason in btmon.
- `host_crash=yes` indicates host adapter instability signature (`Hardware Error 0x0c` / Intel reset).
- `host_unstable=yes` tags host-contaminated runs.
- `target_verdict` classifies target behavior (`pass`, `fail`, `unknown_host`).
- `overall_verdict` is the final attempt verdict (`pass`, `fail_target`, `inconclusive_host`).

When `host_unstable=yes`, treat the attempt as host-contaminated evidence.

## Common issues

- `Device ... not available`: scan window did not catch target; rerun or increase `--scan-timeout`.
- `Failed to register agent object`: usually benign if a system agent already exists.
- early script exit with no summary: inspect `<outdir>/attempt_*/setup.log` and `<outdir>/attempt_*/btmon.log`.

## A/B workflow

1. Run baseline (current `main`) for 10 attempts.
2. Apply one BLE security change.
3. Re-run 10 attempts with identical command/options.
4. Compare `attempts.csv` counters (`bonded`, `mic_failure`, `host_crash`) before deciding to keep the patch.
