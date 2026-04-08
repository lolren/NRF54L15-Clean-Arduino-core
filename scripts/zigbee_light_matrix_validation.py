#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path


REPO = Path("/home/lolren/Desktop/Nrf54L15/NRF54L15-Clean-Arduino-core")
ZIGBEE_EXAMPLES = REPO / (
    "hardware/nrf54l15clean/nrf54l15clean/libraries/"
    "Nrf54L15-Clean-Implementation/examples/Zigbee"
)
VALIDATOR = REPO / "scripts/zigbee_ha_router_validation.py"
COORDINATOR = (
    ZIGBEE_EXAMPLES
    / "ZigbeeHaCoordinatorJoinDemo"
)
FQBN = "nrf54l15clean:nrf54l15clean:xiao_nrf54l15"

DEVICES = {
    "porch": {
        "sketch": ZIGBEE_EXAMPLES / "Lights/ZigbeeHaOnOffPorchLight",
        "validator_flags": [],
    },
    "desk_lamp": {
        "sketch": ZIGBEE_EXAMPLES / "Lights/ZigbeeHaDimmableDeskLamp",
        "validator_flags": ["--expect-level"],
    },
    "rgb_mood": {
        "sketch": ZIGBEE_EXAMPLES / "Lights/ZigbeeHaRgbMoodLight",
        "validator_flags": ["--expect-level", "--expect-color-hs"],
    },
    "rgbw_ceiling": {
        "sketch": ZIGBEE_EXAMPLES / "Lights/ZigbeeHaRgbwCeilingLight",
        "validator_flags": ["--expect-level", "--expect-color-hs", "--expect-color-temp"],
    },
}


def run(cmd):
    subprocess.run(cmd, check=True)


def compile_sketch(sketch: Path, build_path: Path) -> None:
    build_path.mkdir(parents=True, exist_ok=True)
    run(
        [
            "arduino-cli",
            "compile",
            "--fqbn",
            FQBN,
            str(sketch),
            "--build-path",
            str(build_path),
        ]
    )


def upload_build(build_path: Path, port: str) -> None:
    run(
        [
            "arduino-cli",
            "upload",
            "--fqbn",
            FQBN,
            "--input-dir",
            str(build_path),
            "-p",
            port,
        ]
    )


def parse_summary(path: Path) -> dict:
    result = {}
    for line in path.read_text().splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key] = value.strip().lower() == "true"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile, flash, and validate the Zigbee HA light example matrix."
    )
    parser.add_argument("--coord-port", default="/dev/ttyACM1")
    parser.add_argument("--device-port", default="/dev/ttyACM0")
    parser.add_argument(
        "--outdir",
        default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_light_matrix_validation",
    )
    parser.add_argument(
        "--devices",
        nargs="*",
        choices=sorted(DEVICES.keys()),
        default=["porch", "desk_lamp", "rgb_mood", "rgbw_ceiling"],
    )
    parser.add_argument("--timeout-s", type=int, default=90)
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    coord_build = outdir / "build_coord"
    compile_sketch(COORDINATOR, coord_build)
    upload_build(coord_build, args.coord_port)
    time.sleep(4.0)

    overall = {}
    for device_name in args.devices:
        spec = DEVICES[device_name]
        device_build = outdir / f"build_{device_name}"
        compile_sketch(spec["sketch"], device_build)
        upload_build(device_build, args.device_port)
        time.sleep(5.0)

        validation_out = outdir / device_name
        run(
            [
                sys.executable,
                str(VALIDATOR),
                "--coord-port",
                args.coord_port,
                "--router-port",
                args.device_port,
                "--timeout-s",
                str(args.timeout_s),
                "--outdir",
                str(validation_out),
                *spec["validator_flags"],
            ]
        )
        overall[device_name] = parse_summary(validation_out / "summary.txt")

    (outdir / "summary.json").write_text(json.dumps(overall, indent=2, sort_keys=True))
    with open(outdir / "summary.txt", "w") as handle:
        for device_name in args.devices:
            handle.write(f"[{device_name}]\n")
            for key, value in sorted(overall[device_name].items()):
                handle.write(f"{key}={str(value).lower()}\n")
            handle.write("\n")

    print(outdir / "summary.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
