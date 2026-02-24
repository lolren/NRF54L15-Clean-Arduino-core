# BLE CLI Matrix Report

- Timestamp: 2026-02-22T22:36:58+00:00
- Port: `/dev/ttyACM0`
- FQBN: `nrf54l15clean:nrf54l15clean:xiao_nrf54l15`
- Bluetooth command: `sudo -n bluetoothctl`

## Results

| Example | Mode | Compile | Upload | Scan | Connect | Info Connected | Extra | Note |
|---|---|---|---|---|---|---|---|---|
| `BleAdvertiser` | `scan` | `pass` | `pass` | `pass` | `fail` | `fail` | `skip` | `` |
| `BlePassiveScanner` | `serial` | `pass` | `pass` | `skip` | `skip` | `skip` | `pass` | `` |
| `BleConnectableScannableAdvertiser` | `connect` | `pass` | `pass` | `pass` | `fail` | `pass` | `skip` | `` |
| `BleConnectionPeripheral` | `connect` | `pass` | `pass` | `pass` | `pass` | `pass` | `skip` | `` |
| `BleGattBasicPeripheral` | `gatt` | `pass` | `pass` | `pass` | `fail` | `pass` | `pass` | `` |
| `BleBatteryNotifyPeripheral` | `notify` | `pass` | `pass` | `pass` | `fail` | `pass` | `pass` | `` |
| `BlePairingEncryptionStatus` | `connect` | `pass` | `pass` | `fail` | `skip` | `skip` | `skip` | `scan_miss` |
| `BleBondPersistenceProbe` | `connect` | `pass` | `pass` | `fail` | `skip` | `skip` | `skip` | `scan_miss` |
| `BleConnectionTimingMetrics` | `connect` | `pass` | `pass` | `pass` | `pass` | `pass` | `skip` | `` |

## Logs

All raw logs are in this directory:

- `measurements/ble_cli_matrix_latest`

Packet capture logs:

- `btmon_C0_DE_54_15_00_21.log,btmon_connect_C0_DE_54_15_00_21.log btmon_disconnect_C0_DE_54_15_00_21.log`
