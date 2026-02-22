#!/usr/bin/env python3
"""Arduino upload helper for XIAO nRF54L15 Zephyr-based core.

This wrapper keeps Arduino upload integration cross-platform while relying on
pyOCD for CMSIS-DAP flashing.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=False, text=True, capture_output=True)


def print_result(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode != 0 and result.stderr:
        print(result.stderr, file=sys.stderr, end="")


def tool_available(name: str) -> bool:
    return shutil.which(name) is not None


def resolve_tool(path_or_name: str) -> str | None:
    if not path_or_name:
        return None

    if "{" in path_or_name and "}" in path_or_name:
        # Unresolved Arduino property placeholder, fall back to PATH lookup.
        return shutil.which("openocd")

    candidate = Path(path_or_name)
    if candidate.is_file():
        return str(candidate)

    return shutil.which(path_or_name)


def find_probe_uid(requested_uid: str | None) -> str:
    if requested_uid:
        return requested_uid

    if not tool_available("pyocd"):
        raise RuntimeError("pyocd is not installed or not available in PATH")

    result = run(["pyocd", "list", "--probes", "--no-header"])
    if result.returncode != 0:
        raise RuntimeError(
            "pyocd list failed\n" + (result.stdout or "") + (result.stderr or "")
        )

    # Typical row:
    # 0   Seeed Studio ... CMSIS-DAP   E91217E8    n/a
    for line in result.stdout.splitlines():
        m = re.search(r"\b([0-9A-Fa-f]{8,})\b", line)
        if m:
            return m.group(1)

    raise RuntimeError("No CMSIS-DAP probe UID detected by pyocd")


def looks_like_locked_target(result: subprocess.CompletedProcess[str]) -> bool:
    details = ((result.stdout or "") + "\n" + (result.stderr or "")).lower()
    indicators = (
        "approtect",
        "memory transfer fault",
        "fault ack",
        "failed to read memory",
        "dp initialisation failed",
        "ap write error",
        "locked",
    )
    return any(token in details for token in indicators)


def flash_hex(target: str, uid: str, hex_path: str) -> subprocess.CompletedProcess[str]:
    cmd = ["pyocd", "load", "-t", target, "-u", uid, hex_path, "--format", "hex"]
    result = run(cmd)
    print_result(result)
    return result


def recover_target(target: str, uid: str) -> subprocess.CompletedProcess[str]:
    print("Detected protected target; attempting chip erase and retry...")
    cmd = ["pyocd", "erase", "--chip", "-t", target, "-u", uid]
    result = run(cmd)
    print_result(result)
    return result


def choose_runner(requested: str, openocd_bin: str) -> str:
    normalized = requested.strip().lower()
    if normalized != "auto":
        return normalized

    if tool_available("pyocd"):
        return "pyocd"
    if resolve_tool(openocd_bin):
        return "openocd"

    raise RuntimeError("No supported uploader found (need pyocd or openocd in PATH)")


def upload_pyocd(hex_path: str, target: str, requested_uid: str | None) -> int:
    if not tool_available("pyocd"):
        print("ERROR: pyocd is not installed or not available in PATH", file=sys.stderr)
        return 3

    try:
        uid = find_probe_uid(requested_uid)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 3

    print(f"Flashing {hex_path}")
    print(f"Runner: pyocd")
    print(f"Probe UID: {uid}")

    load_result = flash_hex(target, uid, hex_path)
    if load_result.returncode != 0 and looks_like_locked_target(load_result):
        erase_result = recover_target(target, uid)
        if erase_result.returncode == 0:
            load_result = flash_hex(target, uid, hex_path)

    if load_result.returncode != 0:
        return load_result.returncode

    reset_cmd = ["pyocd", "reset", "-t", target, "-u", uid]
    reset_result = run(reset_cmd)
    print_result(reset_result)
    return 0


def upload_openocd(hex_path: str, openocd_script: str, openocd_speed: int, openocd_bin: str) -> int:
    openocd_exe = resolve_tool(openocd_bin)
    if not openocd_exe:
        print(f"ERROR: openocd binary not found: {openocd_bin}", file=sys.stderr)
        return 4

    if not os.path.isfile(openocd_script):
        print(f"ERROR: OpenOCD config not found: {openocd_script}", file=sys.stderr)
        return 2

    print(f"Flashing {hex_path}")
    print("Runner: openocd")

    cmd = [
        openocd_exe,
        "-f",
        openocd_script,
        "-c",
        f"adapter speed {openocd_speed}",
        "-c",
        f'program "{hex_path}" verify reset exit',
    ]
    result = run(cmd)
    print_result(result)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hex", required=True, help="Path to firmware hex file")
    parser.add_argument("--port", default="", help="Arduino serial port (unused)")
    parser.add_argument("--target", default="nrf54l", help="pyOCD target name")
    parser.add_argument("--uid", default=None, help="Optional pyOCD probe UID")
    parser.add_argument(
        "--runner",
        default="auto",
        help="Upload runner: auto, pyocd, openocd",
    )
    parser.add_argument(
        "--openocd-script",
        default="",
        help="OpenOCD target config script path",
    )
    parser.add_argument(
        "--openocd-speed",
        type=int,
        default=4000,
        help="OpenOCD adapter speed in kHz",
    )
    parser.add_argument(
        "--openocd-bin",
        default="openocd",
        help="OpenOCD executable path or command name",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.hex):
        print(f"ERROR: HEX file not found: {args.hex}", file=sys.stderr)
        return 2

    try:
        runner = choose_runner(args.runner, args.openocd_bin)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 4

    if runner == "pyocd":
        rc = upload_pyocd(args.hex, args.target, args.uid)
    elif runner == "openocd":
        rc = upload_openocd(args.hex, args.openocd_script, args.openocd_speed, args.openocd_bin)
        if rc != 0 and tool_available("pyocd"):
            print("OpenOCD upload failed; falling back to pyocd...")
            rc = upload_pyocd(args.hex, args.target, args.uid)
    else:
        print(f"ERROR: Unsupported runner: {runner}", file=sys.stderr)
        return 4

    if rc != 0:
        return rc

    print("Upload complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
