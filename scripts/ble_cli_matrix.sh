#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FQBN_DEFAULT="nrf54l15clean:nrf54l15clean:xiao_nrf54l15"
PORT_DEFAULT=""
USE_SUDO=0
ONLY_COMPILE=0
OUTDIR=""
SERIAL_BAUD=115200
SCAN_TIMEOUT=12
SCAN_RETRIES=3

usage() {
  cat <<'USAGE'
Usage:
  scripts/ble_cli_matrix.sh [options]

Options:
  --port <device>        Serial port (default: auto-detect)
  --fqbn <fqbn>          Board FQBN (default: nrf54l15clean:nrf54l15clean:xiao_nrf54l15)
  --sudo                 Run bluetoothctl/btmon with sudo -n
  --only-compile         Compile sketches only (skip upload/runtime checks)
  --scan-timeout <sec>   Scan duration per attempt in seconds (default: 12)
  --scan-retries <n>     Number of scan attempts per case (default: 3)
  --outdir <path>        Output folder (default: measurements/ble_cli_matrix_<timestamp>)
  --help                 Show this help
USAGE
}

FQBN="${FQBN_DEFAULT}"
PORT="${PORT_DEFAULT}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="$2"
      shift 2
      ;;
    --fqbn)
      FQBN="$2"
      shift 2
      ;;
    --sudo)
      USE_SUDO=1
      shift
      ;;
    --only-compile)
      ONLY_COMPILE=1
      shift
      ;;
    --scan-timeout)
      SCAN_TIMEOUT="$2"
      shift 2
      ;;
    --scan-retries)
      SCAN_RETRIES="$2"
      shift 2
      ;;
    --outdir)
      OUTDIR="$2"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${OUTDIR}" ]]; then
  OUTDIR="${ROOT_DIR}/measurements/ble_cli_matrix_$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "${OUTDIR}"

BTCTL_CMD=(bluetoothctl)
BTCTL_AGENT_CMD=(bluetoothctl --agent NoInputNoOutput)
BTMON_CMD=(btmon)
if [[ "${USE_SUDO}" -eq 1 ]]; then
  BTCTL_CMD=(sudo -n bluetoothctl)
  BTCTL_AGENT_CMD=(sudo -n bluetoothctl --agent NoInputNoOutput)
  BTMON_CMD=(sudo -n btmon)
fi

if [[ -z "${PORT}" ]]; then
  PORT="$(arduino-cli board list | awk 'NR > 1 && $1 ~ /^\/dev\// {print $1; exit}')"
fi
if [[ -z "${PORT}" && -e /dev/ttyACM0 ]]; then
  PORT="/dev/ttyACM0"
fi
if [[ -z "${PORT}" ]]; then
  echo "Could not auto-detect board port. Use --port." >&2
  exit 1
fi

EXAMPLE_ROOT="${ROOT_DIR}/hardware/nrf54l15clean/0.1.0/libraries/Nrf54L15-Clean-Implementation/examples"

declare -a CASES=(
  "BleAdvertiser|C0:DE:54:15:00:01|XIAO-54-CLN|scan_nonconn"
  "BlePassiveScanner||BlePassiveScanner|serial"
  "BleConnectableScannableAdvertiser|C0:DE:54:15:00:11|XIAO-54|connect"
  "BleConnectionPeripheral|C0:DE:54:15:00:21|XIAO54-LINK|connect"
  "BleGattBasicPeripheral|C0:DE:54:15:00:31|X54-GATT|gatt"
  "BleBatteryNotifyPeripheral|C0:DE:54:15:00:41|X54-BATT|notify"
  "BlePairingEncryptionStatus|C0:DE:54:15:00:51|X54-PAIR|pair"
  "BleBondPersistenceProbe|C0:DE:54:15:00:61|X54-BOND|bond"
  "BleConnectionTimingMetrics|C0:DE:54:15:00:71|XIAO54-TIMING|connect"
)

RESULTS_CSV="${OUTDIR}/results.csv"
{
  echo "example,mode,compile,upload,scan,connect,info_connected,extra,note"
} > "${RESULTS_CSV}"

log() {
  echo "[$(date +%H:%M:%S)] $*"
}

run_bt_scan() {
  local logfile="$1"
  timeout "$((SCAN_TIMEOUT + 2))s" "${BTCTL_CMD[@]}" --timeout "${SCAN_TIMEOUT}" scan on >"${logfile}" 2>&1 || true
}

run_bt_devices() {
  local logfile="$1"
  timeout 8s "${BTCTL_CMD[@]}" --timeout 6 devices >"${logfile}" 2>&1 || true
}

run_bt_connect() {
  local addr="$1"
  local logfile="$2"
  timeout 14s "${BTCTL_CMD[@]}" --timeout 12 connect "${addr}" >"${logfile}" 2>&1 || true
}

run_bt_info() {
  local addr="$1"
  local logfile="$2"
  timeout 10s "${BTCTL_CMD[@]}" --timeout 8 info "${addr}" >"${logfile}" 2>&1 || true
}

run_bt_disconnect() {
  local addr="$1"
  local logfile="$2"
  timeout 8s "${BTCTL_CMD[@]}" --timeout 6 disconnect "${addr}" >"${logfile}" 2>&1 || true
}

check_scan_hit() {
  local scan_log="$1"
  local devices_log="$2"
  local addr="$3"
  local name="$4"

  if [[ -n "${addr}" ]] && rg -q "${addr}" "${scan_log}" "${devices_log}"; then
    echo "pass"
    return
  fi
  if [[ -n "${name}" ]] && rg -q "${name}" "${scan_log}" "${devices_log}"; then
    echo "pass"
    return
  fi
  echo "fail"
}

scan_until_found() {
  local addr="$1"
  local name="$2"
  local prefix="$3"
  local attempt
  for attempt in $(seq 1 "${SCAN_RETRIES}"); do
    local scan_log="${prefix}.scan.${attempt}.log"
    local devices_log="${prefix}.devices.${attempt}.log"
    run_bt_scan "${scan_log}"
    run_bt_devices "${devices_log}"
    if [[ "$(check_scan_hit "${scan_log}" "${devices_log}" "${addr}" "${name}")" == "pass" ]]; then
      echo "pass"
      return
    fi
    sleep 1
  done
  echo "fail"
}

check_connect_hit() {
  local logfile="$1"
  if rg -q "Connection successful|Connected:\\s+yes|AlreadyConnected|already connected" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

check_info_connected() {
  local logfile="$1"
  if rg -q "Connected:\\s+yes" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

compile_example() {
  local example="$1"
  local sketch="${EXAMPLE_ROOT}/${example}/${example}.ino"
  arduino-cli compile --fqbn "${FQBN}" "${sketch}" >"${OUTDIR}/${example}.compile.log" 2>&1
}

upload_example() {
  local example="$1"
  local sketch="${EXAMPLE_ROOT}/${example}/${example}.ino"
  arduino-cli upload -p "${PORT}" --fqbn "${FQBN}" "${sketch}" >"${OUTDIR}/${example}.upload.log" 2>&1
}

check_serial_scanner() {
  local example="$1"
  local serial_log="${OUTDIR}/${example}.serial.log"
  stty -F "${PORT}" raw -echo "${SERIAL_BAUD}" -crtscts -ixon -ixoff || true
  timeout 10s cat "${PORT}" >"${serial_log}" 2>&1 || true
  if rg -q "ADV_|SCAN_REQ|advA=" "${serial_log}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

check_notify_flow() {
  local addr="$1"
  local logfile="$2"
  timeout 28s "${BTCTL_CMD[@]}" --timeout 24 <<EOF2 >"${logfile}" 2>&1 || true
connect ${addr}
menu gatt
select-attribute 00002a19-0000-1000-8000-00805f9b34fb
notify on
EOF2
  if rg -q "notify on|XIAO nRF54L15 Battery|X54-BATT|Connected:\\s+yes|Connection successful" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

check_gatt_discovery() {
  local addr="$1"
  local logfile="$2"
  timeout 24s "${BTCTL_CMD[@]}" --timeout 20 <<EOF2 >"${logfile}" 2>&1 || true
connect ${addr}
menu gatt
list-attributes
EOF2
  if rg -q "0000180f|Battery Service|00001801|Generic Attribute Profile" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

check_pair_flow() {
  local addr="$1"
  local logfile="$2"
  timeout 40s "${BTCTL_AGENT_CMD[@]}" --timeout 34 <<EOF2 >"${logfile}" 2>&1 || true
connect ${addr}
pair ${addr}
info ${addr}
EOF2
  if rg -q "Paired:\\s+yes|Bonded:\\s+yes|Pairing successful|Already Paired|AlreadyExists" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

check_bond_flow() {
  local addr="$1"
  local logfile="$2"
  timeout 52s "${BTCTL_AGENT_CMD[@]}" --timeout 46 <<EOF2 >"${logfile}" 2>&1 || true
connect ${addr}
pair ${addr}
disconnect ${addr}
connect ${addr}
info ${addr}
EOF2
  if rg -q "Paired:\\s+yes|Bonded:\\s+yes|Pairing successful" "${logfile}" && \
     rg -q "Connected:\\s+yes|Connection successful" "${logfile}"; then
    echo "pass"
  else
    echo "fail"
  fi
}

do_btmon_capture() {
  local addr="$1"
  local outfile="${OUTDIR}/btmon_${addr//:/_}.log"
  timeout 20s "${BTMON_CMD[@]}" >"${outfile}" 2>&1 &
  local mon_pid=$!
  sleep 1
  timeout 10s "${BTCTL_CMD[@]}" --timeout 8 connect "${addr}" >"${OUTDIR}/btmon_connect_${addr//:/_}.log" 2>&1 || true
  sleep 3
  run_bt_disconnect "${addr}" "${OUTDIR}/btmon_disconnect_${addr//:/_}.log"
  wait "${mon_pid}" || true
}

log "Output directory: ${OUTDIR}"
log "Using board port: ${PORT}"
log "Using FQBN: ${FQBN}"
log "Bluetooth command: ${BTCTL_CMD[*]}"
log "Scan timeout/retries: ${SCAN_TIMEOUT}s x ${SCAN_RETRIES}"

for case_def in "${CASES[@]}"; do
  IFS="|" read -r example addr name mode <<<"${case_def}"
  log "=== ${example} (${mode}) ==="

  compile_status="pass"
  upload_status="skip"
  scan_status="skip"
  connect_status="skip"
  info_status="skip"
  extra_status="skip"
  note=""

  if ! compile_example "${example}"; then
    compile_status="fail"
    note="compile_failed"
    echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
    continue
  fi

  if [[ "${ONLY_COMPILE}" -eq 1 ]]; then
    echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
    continue
  fi

  if ! upload_example "${example}"; then
    upload_status="fail"
    note="upload_failed"
    echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
    continue
  fi
  upload_status="pass"
  sleep 2

  if [[ "${mode}" == "serial" ]]; then
    extra_status="$(check_serial_scanner "${example}")"
    echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
    continue
  fi

  scan_status="$(scan_until_found "${addr}" "${name}" "${OUTDIR}/${example}")"

  if [[ "${mode}" == "scan_nonconn" ]]; then
    if [[ "${scan_status}" == "fail" ]]; then
      note="scan_miss"
    fi
    echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
    continue
  fi

  if [[ "${scan_status}" == "pass" && -n "${addr}" ]]; then
    connect_log="${OUTDIR}/${example}.connect.log"
    run_bt_connect "${addr}" "${connect_log}"
    connect_status="$(check_connect_hit "${connect_log}")"

    info_log="${OUTDIR}/${example}.info.log"
    run_bt_info "${addr}" "${info_log}"
    info_status="$(check_info_connected "${info_log}")"

    if [[ "${connect_status}" == "fail" && "${info_status}" == "pass" ]]; then
      connect_status="pass"
      note="connect_output_inconclusive"
    fi
  else
    note="scan_miss"
  fi

  if [[ "${mode}" == "notify" && "${scan_status}" == "pass" ]]; then
    extra_status="$(check_notify_flow "${addr}" "${OUTDIR}/${example}.notify.log")"
  elif [[ "${mode}" == "gatt" && "${scan_status}" == "pass" ]]; then
    extra_status="$(check_gatt_discovery "${addr}" "${OUTDIR}/${example}.gatt.log")"
  elif [[ "${mode}" == "pair" && "${scan_status}" == "pass" ]]; then
    extra_status="$(check_pair_flow "${addr}" "${OUTDIR}/${example}.pair.log")"
  elif [[ "${mode}" == "bond" && "${scan_status}" == "pass" ]]; then
    extra_status="$(check_bond_flow "${addr}" "${OUTDIR}/${example}.bond.log")"
  else
    extra_status="skip"
  fi

  if [[ -n "${addr}" ]]; then
    run_bt_disconnect "${addr}" "${OUTDIR}/${example}.disconnect.log"
  fi

  echo "${example},${mode},${compile_status},${upload_status},${scan_status},${connect_status},${info_status},${extra_status},${note}" >> "${RESULTS_CSV}"
done

if [[ "${ONLY_COMPILE}" -eq 0 ]]; then
  log "Capturing btmon trace for BleConnectionPeripheral..."
  upload_example "BleConnectionPeripheral" || true
  sleep 2
  do_btmon_capture "C0:DE:54:15:00:21" || true
fi

REPORT_MD="${OUTDIR}/report.md"
{
  echo "# BLE CLI Matrix Report"
  echo
  echo "- Timestamp: $(date -Iseconds)"
  echo "- Port: \`${PORT}\`"
  echo "- FQBN: \`${FQBN}\`"
  echo "- Bluetooth command: \`${BTCTL_CMD[*]}\`"
  echo "- Scan timeout/retries: \`${SCAN_TIMEOUT}s x ${SCAN_RETRIES}\`"
  echo
  echo "## Results"
  echo
  echo "| Example | Mode | Compile | Upload | Scan | Connect | Info Connected | Extra | Note |"
  echo "|---|---|---|---|---|---|---|---|---|"
  tail -n +2 "${RESULTS_CSV}" | while IFS=, read -r ex m c u s co i e n; do
    echo "| \`${ex}\` | \`${m}\` | \`${c}\` | \`${u}\` | \`${s}\` | \`${co}\` | \`${i}\` | \`${e}\` | \`${n}\` |"
  done
  echo
  echo "## Logs"
  echo
  echo "All raw logs are in this directory:"
  echo
  echo "- \`${OUTDIR}\`"
  echo
  echo "Packet capture logs:"
  echo
  echo "- \`$(ls "${OUTDIR}" | rg '^btmon' | sed -n '1,20p' | paste -sd ', ' -)\`"
} > "${REPORT_MD}"

log "Wrote CSV: ${RESULTS_CSV}"
log "Wrote report: ${REPORT_MD}"
log "Done."
