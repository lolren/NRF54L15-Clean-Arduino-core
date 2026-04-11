#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RULE_SRC="${SCRIPT_DIR}/60-seeed-xiao-nrf54-cmsis-dap.rules"
RULE_DST="/etc/udev/rules.d/60-seeed-xiao-nrf54-cmsis-dap.rules"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required." >&2
  exit 1
fi

PY_TAG="$(python3 - <<'PY'
import sys
print(f"cp{sys.version_info.major}{sys.version_info.minor}")
PY
)"
WHEELHOUSE_DIR="${SCRIPT_DIR}/../wheelhouse/${PY_TAG}"
INSTALL_ARGS=(--user --upgrade)

if [[ -d "${WHEELHOUSE_DIR}" ]]; then
  echo "Using bundled offline wheelhouse: ${WHEELHOUSE_DIR}"
  INSTALL_ARGS+=(--no-index --find-links "${WHEELHOUSE_DIR}")
fi

if ! python3 -m pip install "${INSTALL_ARGS[@]}" -r "${SCRIPT_DIR}/../requirements-pyocd.txt"; then
  if [[ -d "${WHEELHOUSE_DIR}" ]]; then
    echo "Bundled wheelhouse install failed; retrying with online indexes..."
    python3 -m pip install --user --upgrade -r "${SCRIPT_DIR}/../requirements-pyocd.txt"
  else
    exit 1
  fi
fi

if [[ "${1:-}" == "--udev" ]]; then
  if command -v sudo >/dev/null 2>&1; then
    sudo install -m 0644 "${RULE_SRC}" "${RULE_DST}"
    sudo udevadm control --reload-rules
    sudo udevadm trigger --attr-match=idVendor=2886 --attr-match=idProduct=0066
  else
    install -m 0644 "${RULE_SRC}" "${RULE_DST}"
    udevadm control --reload-rules
    udevadm trigger --attr-match=idVendor=2886 --attr-match=idProduct=0066
  fi
fi

echo "Host upload dependencies are ready."
echo "Restart the Arduino IDE if it was already open."
