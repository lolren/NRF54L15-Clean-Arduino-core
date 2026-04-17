#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_ROOT="${TOOLS_ROOT_OVERRIDE:-$(cd -- "${SCRIPT_DIR}/.." && pwd)}"
RULE_SRC="${SCRIPT_DIR}/60-seeed-xiao-nrf54-cmsis-dap.rules"
RULE_DST="${RULE_DST:-/etc/udev/rules.d/60-seeed-xiao-nrf54-cmsis-dap.rules}"
UDEVADM_BIN="${UDEVADM_BIN:-udevadm}"
PYTHON_BIN="${PYTHON_BIN:-python3}"

usage() {
  cat <<'EOF'
Usage:
  install_linux_host_deps.sh           Install Python pyOCD into the tool-local runtime
  install_linux_host_deps.sh --python  Install Python pyOCD into the tool-local runtime
  install_linux_host_deps.sh --udev    Install Linux udev rules only
  install_linux_host_deps.sh --all     Install both Python deps and udev rules
EOF
}

ensure_python_pip() {
  if "${PYTHON_BIN}" -m pip --version >/dev/null 2>&1; then
    return 0
  fi

  if "${PYTHON_BIN}" -m ensurepip --upgrade >/dev/null 2>&1; then
    return 0
  fi

  echo "python3 pip support is required. Install python3-pip and retry." >&2
  exit 1
}

install_python_deps() {
  local py_tag
  local wheelhouse_dir
  local runtime_dir
  local site_dir
  local requirements_file
  local install_args

  if ! command -v "${PYTHON_BIN}" >/dev/null 2>&1; then
    echo "${PYTHON_BIN} is required for pyOCD installation." >&2
    exit 1
  fi

  ensure_python_pip

  py_tag="$("${PYTHON_BIN}" - <<'PY'
import sys
print(f"cp{sys.version_info.major}{sys.version_info.minor}")
PY
)"
  wheelhouse_dir="${TOOLS_ROOT}/wheelhouse/${py_tag}"
  runtime_dir="${TOOLS_ROOT}/runtime"
  site_dir="${runtime_dir}/pyocd-site"
  requirements_file="${TOOLS_ROOT}/requirements-pyocd.txt"
  install_args=(install --upgrade --disable-pip-version-check --ignore-installed --target "${site_dir}")

  rm -rf "${site_dir}"
  mkdir -p "${site_dir}"

  if [[ -d "${wheelhouse_dir}" ]]; then
    echo "Using bundled offline wheelhouse: ${wheelhouse_dir}"
    install_args+=(--no-index --find-links "${wheelhouse_dir}")
  fi

  if ! "${PYTHON_BIN}" -m pip "${install_args[@]}" -r "${requirements_file}"; then
    if [[ -d "${wheelhouse_dir}" ]]; then
      echo "Bundled wheelhouse install failed; retrying with online indexes..."
      "${PYTHON_BIN}" -m pip install --upgrade --disable-pip-version-check --ignore-installed --target "${site_dir}" -r "${requirements_file}"
    else
      exit 1
    fi
  fi

  "${PYTHON_BIN}" "${TOOLS_ROOT}/pyocd_shim.py" --version >/dev/null
}

install_udev_rules() {
  local rule_dir

  if ! command -v "${UDEVADM_BIN}" >/dev/null 2>&1; then
    echo "${UDEVADM_BIN} is required for --udev." >&2
    exit 1
  fi

  rule_dir="$(dirname -- "${RULE_DST}")"
  if [[ ! -d "${rule_dir}" ]]; then
    echo "udev rules directory does not exist: ${rule_dir}" >&2
    exit 1
  fi

  if [[ -w "${rule_dir}" ]]; then
    install -m 0644 "${RULE_SRC}" "${RULE_DST}"
    "${UDEVADM_BIN}" control --reload-rules
    "${UDEVADM_BIN}" trigger --attr-match=idVendor=2886 --attr-match=idProduct=0066
  elif command -v sudo >/dev/null 2>&1; then
    sudo install -m 0644 "${RULE_SRC}" "${RULE_DST}"
    sudo "${UDEVADM_BIN}" control --reload-rules
    sudo "${UDEVADM_BIN}" trigger --attr-match=idVendor=2886 --attr-match=idProduct=0066
  else
    echo "Need write access to ${rule_dir}. Re-run with sudo available or as root." >&2
    exit 1
  fi
}

want_python=0
want_udev=0

case "${1:-}" in
  "")
    want_python=1
    ;;
  --python)
    want_python=1
    ;;
  --udev)
    want_udev=1
    ;;
  --all)
    want_python=1
    want_udev=1
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  *)
    usage >&2
    exit 1
    ;;
esac

if (( want_python )); then
  install_python_deps
fi

if (( want_udev )); then
  install_udev_rules
fi

echo "Host upload dependencies are ready."
echo "Restart the Arduino IDE if it was already open."
