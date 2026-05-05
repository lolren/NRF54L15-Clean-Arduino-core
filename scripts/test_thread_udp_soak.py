#!/usr/bin/env python3
"""Two-board ThreadExperimentalUdpSoak host runner."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

try:
    import serial
except ImportError as exc:
    raise SystemExit(
        "pyserial is required: python3 -m pip install --user pyserial"
    ) from exc


DEFAULT_FQBN = "nrf54l15clean:nrf54l15clean:xiao_nrf54l15:clean_thread=stage"
DEFAULT_EXAMPLE = (
    "hardware/nrf54l15clean/nrf54l15clean/libraries/"
    "Nrf54L15-Clean-Implementation/examples/Thread/ThreadExperimentalUdpSoak"
)
PAYLOAD_SIZES = [8, 16, 31, 63, 95, 127, 191, 255, 512]


@dataclass
class BoardResults:
    port: str
    role: str = "unknown"
    rloc16: str = ""
    begin_ok: Optional[bool] = None
    multicast_subscribed: Optional[bool] = None
    dataset_hex: Optional[str] = None
    unicast: Dict[int, str] = field(default_factory=dict)
    multicast: Dict[int, str] = field(default_factory=dict)
    done: bool = False


def flash_board(port: str, fqbn: str, example: str) -> bool:
    print(f"flashing {port}")
    result = subprocess.run(
        ["arduino-cli", "upload", "-p", port, "-b", fqbn, example],
        capture_output=True,
        text=True,
        timeout=240,
    )
    if result.returncode == 0:
        return True
    print(result.stdout[-1000:])
    print(result.stderr[-1000:])
    return False


def parse_line(line: str, result: BoardResults) -> None:
    match = re.search(r"role=([A-Za-z0-9_]+)", line)
    if match:
        result.role = match.group(1)
    match = re.search(r"rloc16=0x([0-9a-fA-F]+)", line)
    if match:
        result.rloc16 = match.group(1)
    match = re.search(r"begin_ok=(\d)", line)
    if match:
        result.begin_ok = match.group(1) == "1"
    match = re.search(r"mcast_subscribed=(\d)", line)
    if match:
        result.multicast_subscribed = match.group(1) == "1"
    match = re.search(r"dataset_hex=([0-9a-fA-F]+)", line)
    if match:
        result.dataset_hex = match.group(1)

    match = re.search(r"soak_pass\s+len=(\d+)", line)
    if match:
        result.unicast[int(match.group(1))] = "pass"
    match = re.search(r"soak_fail\s+len=(\d+)\s+mode=([A-Za-z0-9_]+)", line)
    if match:
        result.unicast[int(match.group(1))] = f"fail_{match.group(2)}"
    match = re.search(r"soak_mcast_pass\s+len=(\d+)", line)
    if match:
        result.multicast[int(match.group(1))] = "pass"
    match = re.search(r"soak_mcast_fail\s+len=(\d+)\s+mode=([A-Za-z0-9_]+)", line)
    if match:
        result.multicast[int(match.group(1))] = f"fail_{match.group(2)}"
    if line.startswith("soak_done"):
        result.done = True


def read_available(ser: serial.Serial, result: BoardResults, seconds: float) -> List[str]:
    lines: List[str] = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            continue
        lines.append(line)
        parse_line(line, result)
    return lines


def print_matrix(title: str, values: Dict[int, str]) -> None:
    print(f"\n{title}")
    for size in PAYLOAD_SIZES:
        print(f"  {size:>3}: {values.get(size, '---')}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port1", default="/dev/ttyACM0")
    parser.add_argument("--port2", default="/dev/ttyACM1")
    parser.add_argument("--fqbn", default=DEFAULT_FQBN)
    parser.add_argument("--example", default=DEFAULT_EXAMPLE)
    parser.add_argument("--skip-flash", action="store_true")
    parser.add_argument("--timeout", type=float, default=180.0)
    args = parser.parse_args()

    if not args.skip_flash:
        if not flash_board(args.port1, args.fqbn, args.example):
            return 1
        if not flash_board(args.port2, args.fqbn, args.example):
            return 1
        time.sleep(5)

    board1 = BoardResults(args.port1)
    board2 = BoardResults(args.port2)
    ser1 = serial.Serial(args.port1, 115200, timeout=0.3)
    ser2 = serial.Serial(args.port2, 115200, timeout=0.3)
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            for line in read_available(ser1, board1, 0.5):
                print(f"{args.port1}: {line}")
            for line in read_available(ser2, board2, 0.5):
                print(f"{args.port2}: {line}")
            if board1.done or board2.done:
                break
    finally:
        ser1.close()
        ser2.close()

    sender = board1 if board1.unicast or board1.multicast else board2
    print_matrix("Unicast", sender.unicast)
    print_matrix("Multicast", sender.multicast)
    if sender.dataset_hex:
        print(f"\ndataset_hex={sender.dataset_hex}")

    any_unicast_pass = any(value == "pass" for value in sender.unicast.values())
    return 0 if any_unicast_pass else 2


if __name__ == "__main__":
    sys.exit(main())
