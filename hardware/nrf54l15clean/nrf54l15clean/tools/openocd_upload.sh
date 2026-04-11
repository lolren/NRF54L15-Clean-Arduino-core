#!/usr/bin/env bash
set -euo pipefail

OPENOCD_BIN="${1:?openocd bin required}"
OPENOCD_SCRIPT="${2:?openocd script required}"
OPENOCD_SPEED="${3:?openocd speed required}"
HEX_PATH="${4:?hex path required}"

exec "${OPENOCD_BIN}" \
  -f "${OPENOCD_SCRIPT}" \
  -c "adapter speed ${OPENOCD_SPEED}" \
  -c "program {${HEX_PATH}} reset" \
  -c "shutdown"
