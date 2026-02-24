# BLE CLI Matrix Report

- Timestamp: 2026-02-23T06:27:50+00:00
- Port: `/dev/ttyACM0`
- FQBN: `nrf54l15clean:nrf54l15clean:xiao_nrf54l15`
- Bluetooth command: `sudo -n bluetoothctl`
- Scan timeout/retries: `12s x 3`

## Results

| Example | Mode | Compile | Upload | Scan | Connect | Info Connected | Extra | Note |
|---|---|---|---|---|---|---|---|---|
| `BleAdvertiser` | `scan_nonconn` | `pass` | `pass` | `pass` | `skip` | `skip` | `skip` | `` |
| `BlePassiveScanner` | `serial` | `pass` | `pass` | `skip` | `skip` | `skip` | `pass` | `` |
| `BleConnectableScannableAdvertiser` | `connect` | `pass` | `pass` | `pass` | `pass` | `fail` | `skip` | `` |
| `BleConnectionPeripheral` | `connect` | `pass` | `pass` | `pass` | `pass` | `pass` | `skip` | `` |
| `BleGattBasicPeripheral` | `gatt` | `pass` | `pass` | `pass` | `pass` | `pass` | `pass` | `` |
| `BleBatteryNotifyPeripheral` | `notify` | `pass` | `pass` | `pass` | `pass` | `fail` | `pass` | `` |
| `BlePairingEncryptionStatus` | `pair` | `pass` | `pass` | `pass` | `pass` | `pass` | `fail` | `` |
| `BleBondPersistenceProbe` | `bond` | `pass` | `pass` | `pass` | `pass` | `pass` | `fail` | `` |
| `BleConnectionTimingMetrics` | `connect` | `pass` | `pass` | `pass` | `pass` | `pass` | `skip` | `` |

## Logs

All raw logs are in this directory:

- `measurements/ble_cli_matrix_gpt52_20260223_062013`

Packet capture logs:

- `btmon_C0_DE_54_15_00_21.log,btmon_connect_C0_DE_54_15_00_21.log btmon_disconnect_C0_DE_54_15_00_21.log`
