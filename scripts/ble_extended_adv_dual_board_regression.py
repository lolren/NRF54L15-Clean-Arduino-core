#!/usr/bin/env python3
"""Run a two-board extended advertising TX/RX regression on XIAO nRF54L15.

This script compiles one extended advertising advertiser example and the
BleExtendedScanner example, uploads them to two boards, captures serial from
both sides, and asserts that the scanner reassembles the expected payload.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FQBN_DEFAULT = "nrf54l15clean:nrf54l15clean:xiao_nrf54l15"
UPLOAD_SCRIPT = (
    ROOT
    / "hardware"
    / "nrf54l15clean"
    / "nrf54l15clean"
    / "tools"
    / "upload.py"
)
ADVERTISER_EXAMPLES = {
    "251": {
        "path": ROOT
        / "hardware"
        / "nrf54l15clean"
        / "nrf54l15clean"
        / "libraries"
        / "Nrf54L15-Clean-Implementation"
        / "examples"
        / "BLE"
        / "BleExtendedAdv251",
        "length": 236,
        "name": "X54-EXT-ADV",
    },
    "499": {
        "path": ROOT
        / "hardware"
        / "nrf54l15clean"
        / "nrf54l15clean"
        / "libraries"
        / "Nrf54L15-Clean-Implementation"
        / "examples"
        / "BLE"
        / "BleExtendedAdv499",
        "length": 499,
        "name": "X54-EXT-499",
    },
    "995": {
        "path": ROOT
        / "hardware"
        / "nrf54l15clean"
        / "nrf54l15clean"
        / "libraries"
        / "Nrf54L15-Clean-Implementation"
        / "examples"
        / "BLE"
        / "BleExtendedAdv995",
        "length": 995,
        "name": "X54-EXT-995",
    },
}
SCANNER_EXAMPLE = (
    ROOT
    / "hardware"
    / "nrf54l15clean"
    / "nrf54l15clean"
    / "libraries"
    / "Nrf54L15-Clean-Implementation"
    / "examples"
    / "BLE"
    / "BleExtendedScanner"
)


@dataclass
class PortInfo:
    port: str
    uid: str


class SerialCapture:
    def __init__(self, port: str, baud: int) -> None:
        import serial  # type: ignore

        self.port = port
        self._serial = serial.Serial(port, baud, timeout=0.05)
        self._chunks: list[bytes] = []
        self._stop = False
        self._thread = threading.Thread(target=self._reader, daemon=True)

    def start(self) -> None:
        self._thread.start()

    def _reader(self) -> None:
        while not self._stop:
            data = self._serial.read(4096)
            if data:
                self._chunks.append(data)
            time.sleep(0.02)

    def close(self) -> None:
        self._stop = True
        self._thread.join(timeout=0.5)
        self._serial.close()

    def text(self) -> str:
        return b"".join(self._chunks).decode("utf-8", errors="replace")


def run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(cmd, text=True, capture_output=True)
    if check and result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result


def detect_uid(port: str) -> str:
    result = run(["udevadm", "info", "-q", "property", "-n", port])
    for line in result.stdout.splitlines():
        if line.startswith("ID_SERIAL_SHORT="):
            return line.split("=", 1)[1].strip()
    raise SystemExit(f"Could not resolve probe UID for {port}")


def compile_example(example_dir: Path, fqbn: str, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    for existing in output_dir.iterdir():
        if existing.is_file():
            existing.unlink()
    run([
        "arduino-cli",
        "compile",
        "--output-dir",
        str(output_dir),
        "-b",
        fqbn,
        str(example_dir),
    ])
    hex_files = sorted(output_dir.glob("*.hex"))
    if len(hex_files) != 1:
        raise SystemExit(f"Expected one hex in {output_dir}, found {len(hex_files)}")
    return hex_files[0]


def upload_hex(hex_path: Path, uid: str) -> None:
    run([
        sys.executable,
        str(UPLOAD_SCRIPT),
        "--hex",
        str(hex_path),
        "--uid",
        uid,
    ])


def save_logs(outdir: Path, advertiser_log: str, scanner_log: str) -> None:
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "advertiser.log").write_text(advertiser_log, encoding="utf-8")
    (outdir / "scanner.log").write_text(scanner_log, encoding="utf-8")


def assert_scanner_log(scanner_log: str, expected_length: int, expected_name: str) -> None:
    if "BleExtendedScanner start" not in scanner_log:
        raise SystemExit("Scanner log does not contain the boot banner")
    if f"data_len={expected_length}" not in scanner_log:
        raise SystemExit(
            f"Scanner log does not contain expected extended payload length {expected_length}"
        )
    if f"name={expected_name}" not in scanner_log:
        raise SystemExit(
            f"Scanner log does not contain expected extended advertiser name {expected_name}"
        )


def assert_advertiser_log(advertiser_log: str, expected_length: int) -> None:
    if "BLE init: OK" not in advertiser_log:
        raise SystemExit("Advertiser did not report successful BLE init")
    if f"ext_len={expected_length}" not in advertiser_log:
        raise SystemExit(
            f"Advertiser log does not contain expected extended payload length {expected_length}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--advertiser",
        choices=sorted(ADVERTISER_EXAMPLES.keys()),
        default="995",
        help="Which extended advertiser example to validate against the scanner",
    )
    parser.add_argument(
        "--adv-port", default="/dev/ttyACM0", help="Advertiser board CDC port"
    )
    parser.add_argument(
        "--scan-port", default="/dev/ttyACM1", help="Scanner board CDC port"
    )
    parser.add_argument("--fqbn", default=FQBN_DEFAULT, help="Board FQBN")
    parser.add_argument("--baud", type=int, default=115200, help="CDC baud rate")
    parser.add_argument(
        "--capture-s",
        type=float,
        default=12.0,
        help="Serial capture time after upload",
    )
    parser.add_argument(
        "--outdir",
        type=Path,
        default=ROOT / "measurements" / "ble_extended_adv_dual_board_latest",
        help="Directory where advertiser/scanner logs are written",
    )
    parser.add_argument(
        "--skip-upload",
        action="store_true",
        help="Skip compile/upload and only capture current firmware",
    )
    args = parser.parse_args()

    try:
        import serial  # type: ignore  # noqa: F401
    except Exception as exc:
        raise SystemExit(f"pyserial is required: {exc}") from exc

    adv = PortInfo(port=args.adv_port, uid=detect_uid(args.adv_port))
    scan = PortInfo(port=args.scan_port, uid=detect_uid(args.scan_port))
    advertiser_info = ADVERTISER_EXAMPLES[args.advertiser]

    if not args.skip_upload:
        build_root = ROOT / "measurements" / "build_ble_extended_adv_regression"
        adv_hex = compile_example(
            advertiser_info["path"],
            args.fqbn,
            build_root / f"advertiser_{args.advertiser}",
        )
        scan_hex = compile_example(
            SCANNER_EXAMPLE,
            args.fqbn,
            build_root / "scanner",
        )
        upload_hex(adv_hex, adv.uid)
        upload_hex(scan_hex, scan.uid)

    adv_cap = SerialCapture(adv.port, args.baud)
    scan_cap = SerialCapture(scan.port, args.baud)
    adv_cap.start()
    scan_cap.start()
    try:
        time.sleep(args.capture_s)
    finally:
        adv_cap.close()
        scan_cap.close()

    advertiser_log = adv_cap.text()
    scanner_log = scan_cap.text()
    save_logs(args.outdir, advertiser_log, scanner_log)

    assert_advertiser_log(advertiser_log, int(advertiser_info["length"]))
    assert_scanner_log(
        scanner_log,
        int(advertiser_info["length"]),
        str(advertiser_info["name"]),
    )

    print("ble_extended_adv dual-board regression OK")
    print(f"advertiser example: {args.advertiser}")
    print(f"advertiser port: {adv.port} uid={adv.uid}")
    print(f"scanner port: {scan.port} uid={scan.uid}")
    print(f"logs: {args.outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
