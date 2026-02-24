# BLE CLI Matrix Report

- Timestamp: 2026-02-24T06:52:01+00:00
- Port: `/dev/ttyACM0`
- FQBN: `nrf54l15clean:nrf54l15clean:xiao_nrf54l15`
- Bluetooth command: `bluetoothctl --agent NoInputNoOutput`
- Scan timeout/retries: `12s x 3`
- Connect retries: `2`
- Pair retries: `3`
- Auto agent setup: `1`
- Auto trust: `1`

## Results

| Example | Mode | Compile | Upload | Scan | Connect | Info Connected | Extra | Note |
|---|---|---|---|---|---|---|---|---|
| `BleAdvertiser` | `scan_nonconn` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BlePassiveScanner` | `serial` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleActiveScanner` | `serial_active` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleConnectableScannableAdvertiser` | `connect` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleConnectionPeripheral` | `connect` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleGattBasicPeripheral` | `gatt` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleBatteryNotifyPeripheral` | `notify` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BlePairingEncryptionStatus` | `pair` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleBondPersistenceProbe` | `bond` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |
| `BleConnectionTimingMetrics` | `connect` | `pass` | `skip` | `skip` | `skip` | `skip` | `skip` | `` |

## Logs

All raw logs are in this directory:

- `/home/lolren/Desktop/Clean_nrf54_implementation/Nrf54L15-Clean-BoardPackage/measurements/ble_cli_matrix_20260224_065143`

Packet capture logs:

- ``
