#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORM_DIR="${ROOT_DIR}/hardware/nrf54l15clean/nrf54l15clean"
EXAMPLE_ROOT="${PLATFORM_DIR}/libraries/Nrf54L15-Clean-Implementation/examples/Zigbee"
FQBN_DEFAULT="localnrf54l15clean:nrf54l15clean:xiao_nrf54l15"

FQBN="${FQBN_DEFAULT}"
OUTDIR=""
KEEP_TEMP=0
TEMP_DIR=""

usage() {
  cat <<'USAGE'
Usage:
  scripts/zigbee_joinable_compile_matrix.sh [options]

Options:
  --fqbn <fqbn>       Board FQBN (default: localnrf54l15clean:nrf54l15clean:xiao_nrf54l15)
  --outdir <path>     Output directory (default: measurements/zigbee_joinable_compile_matrix_<timestamp>)
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

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli is required but was not found in PATH." >&2
  exit 1
fi

if [[ -z "${OUTDIR}" ]]; then
  OUTDIR="${ROOT_DIR}/measurements/zigbee_joinable_compile_matrix_$(date +%Y%m%d_%H%M%S)"
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

EXAMPLES=(
  "ZigbeeHaOnOffLightJoinable"
  "ZigbeeHaDimmableLightJoinable"
  "ZigbeeHaTemperatureSensorJoinable"
)

POLICIES=(
  "default"
  "strict_external"
)

strict_external_flags() {
  printf '%s' \
    '-DNRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK=0x00000800UL '\
    '-DNRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK=0x00001000UL '\
    '-DNRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE=1 '\
    '-DNRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY=0 '\
    '-DNRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY=1 '\
    '-DNRF54L15_CLEAN_ZIGBEE_ALLOW_DEMO_PLAINTEXT_TC_CMDS=0 '\
    '-DNRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE=0x00124B0001AA5501ULL '\
    '-DNRF54L15_CLEAN_ZIGBEE_PREFERRED_EXTENDED_PAN_ID=0x1122334455667788ULL'
}

policy_flags() {
  case "$1" in
    default)
      return 0
      ;;
    strict_external)
      strict_external_flags
      ;;
    *)
      echo "Unknown policy: $1" >&2
      return 1
      ;;
  esac
}

RESULTS_CSV="${OUTDIR}/results.csv"
{
  echo "example,policy,compile,log"
} > "${RESULTS_CSV}"

log() {
  echo "[$(date +%H:%M:%S)] $*"
}

run_compile() {
  local example="$1"
  local policy="$2"
  local flags
  local sketch="${EXAMPLE_ROOT}/${example}/${example}.ino"
  local logfile="${OUTDIR}/${example}.${policy}.compile.log"
  local build_dir="${TEMP_DIR}/build/${example}.${policy}"
  local -a cmd=(
    arduino-cli
    compile
    --config-file "${CLI_CONFIG}"
    --build-path "${build_dir}"
    --fqbn "${FQBN}"
    "${sketch}"
  )

  flags="$(policy_flags "${policy}")"
  if [[ -n "${flags}" ]]; then
    cmd+=(--build-property "build.extra_flags=${flags}")
  fi

  log "Compiling ${example} (${policy})"
  if "${cmd[@]}" >"${logfile}" 2>&1; then
    echo "${example},${policy},pass,${logfile}" >>"${RESULTS_CSV}"
    log "PASS ${example} (${policy})"
  else
    echo "${example},${policy},fail,${logfile}" >>"${RESULTS_CSV}"
    log "FAIL ${example} (${policy})"
    return 1
  fi
}

for example in "${EXAMPLES[@]}"; do
  if [[ ! -f "${EXAMPLE_ROOT}/${example}/${example}.ino" ]]; then
    echo "Example sketch not found: ${EXAMPLE_ROOT}/${example}/${example}.ino" >&2
    exit 1
  fi
done

log "Using FQBN: ${FQBN}"
log "Using temporary arduino-cli config: ${CLI_CONFIG}"
log "Writing logs to: ${OUTDIR}"

for policy in "${POLICIES[@]}"; do
  for example in "${EXAMPLES[@]}"; do
    run_compile "${example}" "${policy}"
  done
done

log "All Zigbee joinable compile-matrix cases passed."
printf '\nResults:\n'
if command -v column >/dev/null 2>&1; then
  column -s, -t "${RESULTS_CSV}"
else
  cat "${RESULTS_CSV}"
fi
