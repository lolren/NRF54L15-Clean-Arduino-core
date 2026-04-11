#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SKETCH_DIR="${SCRIPT_DIR}/SerialMonitorViaPico"
BUILD_DIR="${SCRIPT_DIR}/serial_build"
LOG_DIR="${SCRIPT_DIR}/logs"
FQBN="${FQBN:-nrf54l15clean:nrf54l15clean:holyiot_25007_nrf54l15}"
PORT="${PICO_PORT:-/dev/ttyACM0}"
PYOCD_UID="${PICO_UID:-}"

mkdir -p "${BUILD_DIR}" "${LOG_DIR}"

CONFIG_FILE="$(mktemp)"
TTY_SAVED="$(mktemp)"
cleanup() {
  stty -F "${PORT}" "$(cat "${TTY_SAVED}" 2>/dev/null)" 2>/dev/null || true
  rm -f "${CONFIG_FILE}" "${TTY_SAVED}"
}
trap cleanup EXIT

cat >"${CONFIG_FILE}" <<EOF
directories:
  data: ${HOME}/.arduino15
  downloads: ${HOME}/.arduino15/staging
  user: ${REPO_ROOT}
EOF

if [[ -z "${PYOCD_UID}" ]] && [[ -e /dev/serial/by-id ]]; then
  serial_match="$(
    ls /dev/serial/by-id 2>/dev/null \
      | sed -n 's/^.*Raspberry_Pi_Debugprobe_on_Pico__CMSIS-DAP__\([0-9A-Fa-f][0-9A-Fa-f]*\)-if01$/\1/p' \
      | head -n1
  )"
  if [[ -n "${serial_match}" ]]; then
    PYOCD_UID="${serial_match}"
  fi
fi

echo "Repo root : ${REPO_ROOT}"
echo "Sketch    : ${SKETCH_DIR}"
echo "Build dir : ${BUILD_DIR}"
echo "Log dir   : ${LOG_DIR}"
echo "FQBN      : ${FQBN}"
echo "Port      : ${PORT}"
echo "Probe UID : ${PYOCD_UID:-auto}"
echo
echo "Extra UART wiring for serial monitor:"
echo "  target P2.08 / Serial TX -> Pico GP5 / Debugprobe UART RX"
echo "  target P2.07 / Serial RX -> Pico GP4 / Debugprobe UART TX"
echo "  target GND              -> Pico GND"
echo "  SWD stays wired as GP2 -> SWCLK, GP3 -> SWDIO, plus VTREF and GND"
echo "  Optional: GP1 -> nRESET"
echo

arduino-cli compile \
  --config-file "${CONFIG_FILE}" \
  --fqbn "${FQBN}" \
  --board-options clean_serial=header \
  --build-path "${BUILD_DIR}" \
  "${SKETCH_DIR}" \
  2>&1 | tee "${LOG_DIR}/serial_compile.log"

upload_args=(
  --config-file "${CONFIG_FILE}"
  --build-path "${BUILD_DIR}"
  -p "${PORT}"
  --fqbn "${FQBN}"
  --board-options clean_upload=pyocd
  --board-options clean_serial=header
)
upload_args+=("${SKETCH_DIR}")

echo
echo "Uploading serial monitor test..."
arduino-cli upload "${upload_args[@]}" 2>&1 | tee "${LOG_DIR}/serial_upload.log"

stty -F "${PORT}" -g > "${TTY_SAVED}"
stty -F "${PORT}" 115200 raw -echo -echoe -echok -echoctl -echoke

echo
echo "Capturing initial serial output..."
timeout 4s cat "${PORT}" | tee "${LOG_DIR}/serial_capture.log" || true

echo
echo "Sending a probe line..."
printf 'ping-from-host\r\n' > "${PORT}"
timeout 2s cat "${PORT}" | tee "${LOG_DIR}/serial_echo.log" || true

echo
echo "Logs written to ${LOG_DIR}"
echo "Use a serial terminal at 115200 on ${PORT} for interactive monitoring."
