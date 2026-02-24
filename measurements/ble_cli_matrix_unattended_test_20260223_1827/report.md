# BLE CLI Matrix Report

- Timestamp: 2026-02-23T18:33:24+00:00
- Port: `/dev/ttyACM0`
- FQBN: `nrf54l15clean:nrf54l15clean:xiao_nrf54l15`
- Bluetooth command: `bluetoothctl --agent NoInputNoOutput`
- Scan timeout/retries: `6s x 1`
- Connect retries: `1`
- Pair retries: `1`
- Auto agent setup: `1`
- Auto trust: `1`

## Results

| Example | Mode | Compile | Upload | Scan | Connect | Info Connected | Extra | Note |
|---|---|---|---|---|---|---|---|---|
| `BleAdvertiser` | `scan_nonconn` | `pass` | `pass` | `pass` | `skip` | `skip` | `skip` | `` |
| `BlePassiveScanner` | `serial` | `pass` | `pass` | `skip` | `skip` | `skip` | `pass` | `` |
| `BleConnectableScannableAdvertiser` | `connect` | `pass` | `pass` | `pass` | `pass` | `fail` | `skip` | `trust_inconclusive` |
| `BleConnectionPeripheral` | `connect` | `pass` | `pass` | `pass` | `fail` | `fail` | `skip` | `trust_inconclusive` |
| `BleGattBasicPeripheral` | `gatt` | `pass` | `pass` | `fail` | `skip` | `skip` | `skip` | `scan_miss` |
| `BleBatteryNotifyPeripheral` | `notify` | `pass` | `pass` | `pass` | `pass` | `pass` | `pass` | `connect_output_inconclusive` |
| `BlePairingEncryptionStatus` | `pair` | `pass` | `pass` | `pass` | `pass` | `pass` | `fail` | `` |
| `BleBondPersistenceProbe` | `bond` | `pass` | `pass` | `fail` | `skip` | `skip` | `skip` | `scan_miss` |
| `BleConnectionTimingMetrics` | `connect` | `pass` | `pass` | `pass` | `pass` | `pass` | `skip` | `trust_inconclusive` |

## Logs

All raw logs are in this directory:

- `measurements/ble_cli_matrix_unattended_test_20260223_1827`

Packet capture logs:

- `btmon_C0_DE_54_15_00_21.log,btmon_connect_C0_DE_54_15_00_21.log btmon_disconnect_C0_DE_54_15_00_21.log`
