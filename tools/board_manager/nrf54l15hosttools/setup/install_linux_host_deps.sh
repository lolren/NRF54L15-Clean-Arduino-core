#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RULE_SRC="${SCRIPT_DIR}/60-seeed-xiao-nrf54-cmsis-dap.rules"
RULE_DST="/etc/udev/rules.d/60-seeed-xiao-nrf54-cmsis-dap.rules"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required." >&2
  exit 1
fi

python3 -m pip install --user --upgrade pip -r "${SCRIPT_DIR}/../requirements-pyocd.txt"

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
