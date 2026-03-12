#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORM_DIR="${ROOT_DIR}/hardware/nrf54l15clean/nrf54l15clean"
EXAMPLE_SKETCH="${PLATFORM_DIR}/libraries/Nrf54L15-Clean-Implementation/examples/Zigbee/ZigbeeHaCoordinatorJoinDemo/ZigbeeHaCoordinatorJoinDemo.ino"
FQBN_DEFAULT="localnrf54l15clean:nrf54l15clean:xiao_nrf54l15"

FQBN="${FQBN_DEFAULT}"
OUTDIR=""
KEEP_TEMP=0
TEMP_DIR=""
POLICIES=()

usage() {
  cat <<'USAGE'
Usage:
  scripts/zigbee_coordinator_compile_matrix.sh [options]

Options:
  --fqbn <fqbn>       Board FQBN (default: localnrf54l15clean:nrf54l15clean:xiao_nrf54l15)
  --outdir <path>     Output directory (default: measurements/zigbee_coordinator_compile_matrix_<timestamp>)
  --policy <name>     Compile only the named policy (repeatable)
  --keep-temp         Preserve the temporary sketchbook/config folder for inspection
  --help              Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fqbn)
      FQBN="$2"
      shift 2
      ;;
    --outdir)
      OUTDIR="$2"
      shift 2
      ;;
    --policy)
      POLICIES+=("$2")
      shift 2
      ;;
    --keep-temp)
      KEEP_TEMP=1
      shift
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

if [[ ! -d "${PLATFORM_DIR}" ]]; then
  echo "Platform directory not found: ${PLATFORM_DIR}" >&2
  exit 1
fi

if [[ ! -f "${EXAMPLE_SKETCH}" ]]; then
  echo "Coordinator example sketch not found: ${EXAMPLE_SKETCH}" >&2
  exit 1
fi

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli is required but was not found in PATH." >&2
  exit 1
fi

if [[ -z "${OUTDIR}" ]]; then
  OUTDIR="${ROOT_DIR}/measurements/zigbee_coordinator_compile_matrix_$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "${OUTDIR}"

TEMP_DIR="$(mktemp -d)"
cleanup() {
  if [[ "${KEEP_TEMP}" -eq 0 && -n "${TEMP_DIR}" && -d "${TEMP_DIR}" ]]; then
    rm -rf "${TEMP_DIR}"
  fi
}
trap cleanup EXIT

SKETCHBOOK_DIR="${TEMP_DIR}/sketchbook"
LOCAL_VENDOR_DIR="${SKETCHBOOK_DIR}/hardware/localnrf54l15clean"
CLI_CONFIG="${TEMP_DIR}/arduino-cli.yaml"
mkdir -p "${LOCAL_VENDOR_DIR}"
ln -s "${PLATFORM_DIR}" "${LOCAL_VENDOR_DIR}/nrf54l15clean"
printf 'directories:\n  user: %s\n' "${SKETCHBOOK_DIR}" >"${CLI_CONFIG}"

DEFAULT_POLICIES=(
  "default"
  "strict_install_code_only"
)

strict_install_code_only_flags() {
  printf '%s' \
    '-DNRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY=0 '\
    '-DNRF54L15_CLEAN_ZIGBEE_COORDINATOR_IEEE=0x00124B0001AA5501ULL '\
    '-DNRF54L15_CLEAN_ZIGBEE_EXTENDED_PAN_ID=0x1122334455667788ULL '\
    '-DNRF54L15_CLEAN_ZIGBEE_ONOFF_LIGHT_IEEE=0x00124B0001AA1001ULL '\
    '-DNRF54L15_CLEAN_ZIGBEE_DIMMABLE_LIGHT_IEEE=0ULL '\
    '-DNRF54L15_CLEAN_ZIGBEE_TEMPERATURE_SENSOR_IEEE=0ULL'
}

policy_flags() {
  case "$1" in
    default)
      return 0
      ;;
    strict_install_code_only)
      strict_install_code_only_flags
      ;;
    *)
      echo "Unknown policy: $1" >&2
      return 1
      ;;
  esac
}

if [[ "${#POLICIES[@]}" -eq 0 ]]; then
  POLICIES=("${DEFAULT_POLICIES[@]}")
fi

RESULTS_CSV="${OUTDIR}/results.csv"
{
  echo "policy,compile,log"
} > "${RESULTS_CSV}"

log() {
  echo "[$(date +%H:%M:%S)] $*"
}

run_compile() {
  local policy="$1"
  local flags
  local logfile="${OUTDIR}/${policy}.compile.log"
  local build_dir="${TEMP_DIR}/build/${policy}"
  local -a cmd=(
    arduino-cli
    compile
    --config-file "${CLI_CONFIG}"
    --build-path "${build_dir}"
    --fqbn "${FQBN}"
    "${EXAMPLE_SKETCH}"
  )

  flags="$(policy_flags "${policy}")"
  if [[ -n "${flags}" ]]; then
    cmd+=(--build-property "build.extra_flags=${flags}")
  fi

  log "Compiling ZigbeeHaCoordinatorJoinDemo (${policy})"
  if "${cmd[@]}" >"${logfile}" 2>&1; then
    echo "${policy},pass,${logfile}" >>"${RESULTS_CSV}"
    log "PASS ZigbeeHaCoordinatorJoinDemo (${policy})"
  else
    echo "${policy},fail,${logfile}" >>"${RESULTS_CSV}"
    log "FAIL ZigbeeHaCoordinatorJoinDemo (${policy})"
    return 1
  fi
}

log "Using FQBN: ${FQBN}"
log "Using temporary arduino-cli config: ${CLI_CONFIG}"
log "Writing logs to: ${OUTDIR}"

for policy in "${POLICIES[@]}"; do
  run_compile "${policy}"
done

log "All Zigbee coordinator compile-matrix cases passed."
printf '\nResults:\n'
if command -v column >/dev/null 2>&1; then
  column -s, -t "${RESULTS_CSV}"
else
  cat "${RESULTS_CSV}"
fi
