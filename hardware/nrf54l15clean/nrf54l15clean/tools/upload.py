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
import time
from pathlib import Path

CMSIS_DAP_VENDOR_ID = "2886"
CMSIS_DAP_PRODUCT_ID = "0066"


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=False, text=True, capture_output=True)


def print_result(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode != 0 and result.stderr:
        print(result.stderr, file=sys.stderr, end="")


def tool_available(name: str) -> bool:
    return shutil.which(name) is not None


def normalize_tools_path(path: str | None) -> Path | None:
    if not path:
        return None
    if "{" in path and "}" in path:
        return None
    candidate = Path(path)
    return candidate if candidate.exists() else None


def bundled_pyocd_command(host_tools_path: Path | None) -> list[str] | None:
    if host_tools_path is None:
        return None

    candidates = []
    if sys.platform.startswith("win"):
        candidates.extend(
            [
                host_tools_path / "runtime" / "pyocd-venv" / "Scripts" / "pyocd.exe",
                host_tools_path / "runtime" / "pyocd-venv" / "Scripts" / "python.exe",
            ]
        )
    else:
        candidates.extend(
            [
                host_tools_path / "runtime" / "pyocd-venv" / "bin" / "pyocd",
                host_tools_path / "runtime" / "pyocd-venv" / "bin" / "python",
            ]
        )

    for candidate in candidates:
        if not candidate.is_file():
            continue
        if candidate.name.startswith("pyocd"):
            return [str(candidate)]
        return [str(candidate), "-m", "pyocd"]
    return None


def bundled_wheelhouse_path(host_tools_path: Path | None) -> Path | None:
    if host_tools_path is None:
        return None
    wheelhouse_root = host_tools_path / "wheelhouse"
    if not wheelhouse_root.is_dir():
        return None
    version_tag = f"cp{sys.version_info.major}{sys.version_info.minor}"
    candidate = wheelhouse_root / version_tag
    return candidate if candidate.is_dir() else None


def detect_pyocd_command(host_tools_path: Path | None = None) -> list[str] | None:
    pyocd_exe = shutil.which("pyocd")
    if pyocd_exe:
        return [pyocd_exe]

    bundled = bundled_pyocd_command(host_tools_path)
    if bundled is not None:
        return bundled

    module_probe = run([sys.executable, "-m", "pyocd", "--version"])
    if module_probe.returncode == 0:
        return [sys.executable, "-m", "pyocd"]

    return None


def host_setup_hint(host_tools_path: Path | None = None) -> str:
    tools_dir = None
    if host_tools_path is not None and (host_tools_path / "setup").is_dir():
        tools_dir = host_tools_path / "setup"
    else:
        tools_dir = Path(__file__).resolve().parent / "setup"
    if sys.platform.startswith("linux"):
        return (
            "Run "
            + str(tools_dir / "install_linux_host_deps.sh")
            + " --udev"
        )
    if sys.platform.startswith("win"):
        return (
            "Run PowerShell -ExecutionPolicy Bypass -File "
            + str(tools_dir / "install_windows_host_deps.ps1")
        )
    return "Install Python 3 and pyocd, then retry"


def install_pyocd(host_tools_path: Path | None = None) -> bool:
    print("Attempting to install pyocd for automatic target recovery...")

    if host_tools_path is not None:
        runtime_dir = host_tools_path / "runtime"
        venv_dir = runtime_dir / "pyocd-venv"
        requirements = host_tools_path / "requirements-pyocd.txt"
        wheelhouse = bundled_wheelhouse_path(host_tools_path)
        runtime_dir.mkdir(parents=True, exist_ok=True)

        if not venv_dir.exists():
            create = run([sys.executable, "-m", "venv", str(venv_dir)])
            print_result(create)
            if create.returncode != 0:
                return False

        if sys.platform.startswith("win"):
            pip = venv_dir / "Scripts" / "python.exe"
        else:
            pip = venv_dir / "bin" / "python"
        if not pip.is_file():
            return False

        install_cmd = [
            str(pip),
            "-m",
            "pip",
            "install",
            "--upgrade",
            "--disable-pip-version-check",
        ]
        if wheelhouse is not None:
            print(f"Using bundled offline wheelhouse: {wheelhouse}")
            install_cmd.extend(["--no-index", "--find-links", str(wheelhouse)])
        if requirements.is_file():
            install_cmd.extend(["-r", str(requirements)])
        else:
            install_cmd.append("pyocd")

        install = run(install_cmd)
        print_result(install)
        if install.returncode != 0 and wheelhouse is not None:
            print("Bundled wheelhouse install failed; retrying with online indexes...")
            online_cmd = [
                str(pip),
                "-m",
                "pip",
                "install",
                "--upgrade",
                "--disable-pip-version-check",
            ]
            if requirements.is_file():
                online_cmd.extend(["-r", str(requirements)])
            else:
                online_cmd.append("pyocd")
            install = run(online_cmd)
            print_result(install)
        return install.returncode == 0


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


def normalize_uid(requested_uid: str | None) -> str | None:
    if requested_uid is None:
        return None

    cleaned = requested_uid.strip()
    if not cleaned:
        return None
    return cleaned


def infer_uid_from_port(port: str | None) -> str | None:
    if not port:
        return None
    if not sys.platform.startswith("linux"):
        return None

    try:
        target_path = Path(port).resolve(strict=True)
    except OSError:
        return None

    by_id_dir = Path("/dev/serial/by-id")
    if not by_id_dir.is_dir():
        return None

    for entry in by_id_dir.iterdir():
        try:
            if entry.resolve(strict=True) != target_path:
                continue
        except OSError:
            continue

        match = re.search(r"_([0-9A-Fa-f]+)-if\d+$", entry.name)
        if match:
            return match.group(1)

    return None


def _sysfs_usb_identity_for_hidraw(node: Path) -> tuple[str | None, str | None]:
    sys_device = Path("/sys/class/hidraw") / node.name / "device"
    try:
        resolved = sys_device.resolve(strict=True)
    except OSError:
        return None, None

    for parent in (resolved, *resolved.parents):
        vid_file = parent / "idVendor"
        pid_file = parent / "idProduct"
        if not vid_file.is_file() or not pid_file.is_file():
            continue
        try:
            vendor = vid_file.read_text(encoding="utf-8").strip().lower()
            product = pid_file.read_text(encoding="utf-8").strip().lower()
        except OSError:
            return None, None
        return vendor, product

    return None, None


def matching_probe_hidraw_nodes() -> list[Path]:
    if not sys.platform.startswith("linux"):
        return []

    matches: list[Path] = []
    for node in sorted(Path("/dev").glob("hidraw*")):
        vendor, product = _sysfs_usb_identity_for_hidraw(node)
        if vendor == CMSIS_DAP_VENDOR_ID and product == CMSIS_DAP_PRODUCT_ID:
            matches.append(node)
    return matches


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


def looks_like_nrf54l_mass_erase_timeout(result: subprocess.CompletedProcess[str]) -> bool:
    details = ((result.stdout or "") + "\n" + (result.stderr or "")).lower()
    indicators = (
        "mass erase timeout waiting for eraseallstatus",
        "no cores were discovered",
    )
    return any(token in details for token in indicators)


def looks_like_no_probe_error(result: subprocess.CompletedProcess[str]) -> bool:
    details = ((result.stdout or "") + "\n" + (result.stderr or "")).lower()
    indicators = (
        "no connected debug probes",
        "no connected debug probe matches unique id",
        "no available debug probes",
        "unable to open cmsis-dap device",
        "unable to find a matching cmsis-dap device",
    )
    return any(token in details for token in indicators)


def force_nrf54l_unlock_workaround(
    pyocd_cmd: list[str], target: str, uid: str | None
) -> subprocess.CompletedProcess[str]:
    if target.strip().lower() != "nrf54l":
        return subprocess.CompletedProcess(
            args=[*pyocd_cmd, "commander"], returncode=2, stdout="", stderr=""
        )

    print("pyocd nRF54L unlock workaround: forcing CTRL-AP erase/reset sequence...")

    script_lines = [
        "initdp",
        "writeap 2 0x04 1",
        "sleep 500",
        "readap 2 0x08",
        "sleep 500",
        "readap 2 0x08",
        "writeap 2 0x00 2",
        "writeap 2 0x00 0",
        "sleep 500",
        "readap 2 0x08",
        "readap 0 0x00",
        "readap 1 0x00",
    ]

    import tempfile

    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as script_file:
        script_file.write("\n".join(script_lines))
        script_path = script_file.name

    try:
        cmd = append_uid(
            [*pyocd_cmd, "commander", "-N", "-O", "auto_unlock=false", "-x", script_path],
            uid,
        )
        result = run(cmd)
        print_result(result)
        return result
    finally:
        try:
            os.unlink(script_path)
        except OSError:
            pass


def print_linux_probe_permission_hint(
    result: subprocess.CompletedProcess[str], host_tools_path: Path | None = None
) -> None:
    if not looks_like_no_probe_error(result):
        return
    if not sys.platform.startswith("linux"):
        return
    if not tool_available("lsusb"):
        return

    lsusb_result = run(["lsusb"])
    probe_id = f"{CMSIS_DAP_VENDOR_ID}:{CMSIS_DAP_PRODUCT_ID}"
    if probe_id not in (lsusb_result.stdout or "").lower():
        return

    hidraw_nodes = matching_probe_hidraw_nodes()
    if not hidraw_nodes:
        return
    if any(os.access(node, os.R_OK | os.W_OK) for node in hidraw_nodes):
        return

    print(
        "HINT: CMSIS-DAP probe 2886:0066 is present but hidraw access is denied. "
        "Access must be granted on /dev/hidraw*, not only on /dev/ttyACM*.",
        file=sys.stderr,
    )
    print(
        f"HINT: {host_setup_hint(host_tools_path)}",
        file=sys.stderr,
    )


def append_uid(cmd: list[str], uid: str | None) -> list[str]:
    if uid:
        cmd.extend(["-u", uid])
    return cmd


def append_connect_mode(cmd: list[str], connect_mode: str | None) -> list[str]:
    if connect_mode:
        cmd.extend(["-M", connect_mode])
    return cmd


def retry_connect_mode(target: str, attempt: int) -> str | None:
    if target.strip().lower() == "nrf54l":
        return "under-reset"
    if attempt <= 1:
        return None
    return "halt"


def maybe_wait_before_retry(attempt: int, retries: int, retry_delay: float) -> None:
    if attempt >= retries:
        return
    delay = max(0.0, retry_delay)
    print(
        f"Attempt {attempt}/{retries} failed; retrying in {delay:.1f}s...",
        file=sys.stderr,
    )
    time.sleep(delay)


def flash_hex(
    pyocd_cmd: list[str], target: str, uid: str | None, hex_path: str,
    *, auto_unlock: bool = True, connect_mode: str | None = None
) -> subprocess.CompletedProcess[str]:
    cmd = append_uid([*pyocd_cmd, "load", "-W", "-t", target], uid)
    cmd = append_connect_mode(cmd, connect_mode)
    if not auto_unlock:
        cmd.extend(["-O", "auto_unlock=false"])
    cmd.extend([hex_path, "--format", "hex"])
    result = run(cmd)
    print_result(result)
    return result


def recover_target(
    pyocd_cmd: list[str], target: str, uid: str | None, *,
    connect_mode: str | None = None
) -> subprocess.CompletedProcess[str]:
    print("Detected protected target; attempting chip erase and retry...")
    cmd = append_uid([*pyocd_cmd, "erase", "-W", "--chip", "-t", target], uid)
    cmd = append_connect_mode(cmd, connect_mode)
    result = run(cmd)
    print_result(result)
    return result


def install_host_pyocd_fallback() -> bool:
    pip_check = run([sys.executable, "-m", "pip", "--version"])
    if pip_check.returncode != 0:
        ensurepip = run([sys.executable, "-m", "ensurepip", "--upgrade"])
        print_result(ensurepip)
        if ensurepip.returncode != 0:
            return False

    in_virtualenv = getattr(sys, "base_prefix", sys.prefix) != sys.prefix
    if in_virtualenv:
        install_cmds = [[sys.executable, "-m", "pip", "install", "--upgrade", "--disable-pip-version-check", "pyocd"]]
    else:
        install_cmds = [
            [sys.executable, "-m", "pip", "install", "--user", "--upgrade", "--disable-pip-version-check", "pyocd"],
            [sys.executable, "-m", "pip", "install", "--upgrade", "--disable-pip-version-check", "pyocd"],
        ]

    for cmd in install_cmds:
        install = run(cmd)
        print_result(install)
        if install.returncode == 0:
            return True
    return False


def choose_runner(requested: str, openocd_bin: str, host_tools_path: Path | None) -> str:
    normalized = requested.strip().lower()
    if normalized != "auto":
        return normalized

    if detect_pyocd_command(host_tools_path) is not None or host_tools_path is not None:
        return "pyocd"
    if resolve_tool(openocd_bin):
        return "openocd"

    raise RuntimeError("No supported uploader found (need pyocd or openocd in PATH)")


def upload_pyocd(
    hex_path: str,
    target: str,
    requested_uid: str | None,
    retries: int,
    retry_delay: float,
    allow_uid_fallback: bool = False,
    pyocd_cmd: list[str] | None = None,
    host_tools_path: Path | None = None,
) -> int:
    pyocd_cmd = pyocd_cmd if pyocd_cmd is not None else detect_pyocd_command(host_tools_path)
    if pyocd_cmd is None:
        print("ERROR: pyocd is not installed or not available in PATH", file=sys.stderr)
        print(f"HINT: {host_setup_hint(host_tools_path)}", file=sys.stderr)
        return 3

    uid = normalize_uid(requested_uid)

    print(f"Flashing {hex_path}")
    print(f"Runner: pyocd")
    print(f"Probe UID: {uid or 'auto-select'}")
    print(f"Retries: {retries}")

    load_result = subprocess.CompletedProcess(args=[*pyocd_cmd, "load"], returncode=1)
    retries = max(1, retries)
    last_connect_mode: str | None = None

    for attempt in range(1, retries + 1):
        connect_mode = retry_connect_mode(target, attempt)
        last_connect_mode = connect_mode
        print(
            f"Upload attempt {attempt}/{retries}"
            + (
                f" (pyocd, connect={connect_mode})"
                if connect_mode
                else " (pyocd)"
            )
        )

        load_result = flash_hex(
            pyocd_cmd,
            target,
            uid,
            hex_path,
            connect_mode=connect_mode,
        )
        if load_result.returncode != 0 and looks_like_locked_target(load_result):
            erase_result = recover_target(
                pyocd_cmd,
                target,
                uid,
                connect_mode=connect_mode,
            )
            if erase_result.returncode == 0:
                load_result = flash_hex(
                    pyocd_cmd,
                    target,
                    uid,
                    hex_path,
                    connect_mode=connect_mode,
                )
            elif looks_like_nrf54l_mass_erase_timeout(erase_result):
                workaround_result = force_nrf54l_unlock_workaround(pyocd_cmd, target, uid)
                if workaround_result.returncode == 0:
                    load_result = flash_hex(
                        pyocd_cmd,
                        target,
                        uid,
                        hex_path,
                        auto_unlock=False,
                        connect_mode=connect_mode,
                    )

        if (
            load_result.returncode != 0
            and allow_uid_fallback
            and uid is not None
            and looks_like_no_probe_error(load_result)
        ):
            print(
                f"Inferred probe UID '{uid}' did not match an accessible debug probe; "
                "retrying with auto-select...",
                file=sys.stderr,
            )
            uid = None
            allow_uid_fallback = False
            load_result = flash_hex(
                pyocd_cmd,
                target,
                uid,
                hex_path,
                connect_mode=connect_mode,
            )

        if load_result.returncode == 0:
            break
        maybe_wait_before_retry(attempt, retries, retry_delay)

    if load_result.returncode != 0:
        print_linux_probe_permission_hint(load_result, host_tools_path)
        return load_result.returncode

    reset_cmd = append_uid([*pyocd_cmd, "reset", "-W", "-t", target], uid)
    reset_cmd = append_connect_mode(reset_cmd, last_connect_mode)
    if target.strip().lower() == "nrf54l":
        reset_cmd.extend(["-O", "auto_unlock=false"])
    reset_result = run(reset_cmd)
    print_result(reset_result)
    return 0


def upload_openocd(
    hex_path: str,
    openocd_script: str,
    openocd_speed: int,
    openocd_bin: str,
    retries: int,
    retry_delay: float,
    host_tools_path: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    openocd_exe = resolve_tool(openocd_bin)
    if not openocd_exe:
        print(f"ERROR: openocd binary not found: {openocd_bin}", file=sys.stderr)
        return subprocess.CompletedProcess(args=[openocd_bin], returncode=4, stdout="", stderr="")

    if not os.path.isfile(openocd_script):
        print(f"ERROR: OpenOCD config not found: {openocd_script}", file=sys.stderr)
        return subprocess.CompletedProcess(args=[openocd_script], returncode=2, stdout="", stderr="")

    print(f"Flashing {hex_path}")
    print("Runner: openocd")
    print(f"Retries: {retries}")

    retries = max(1, retries)
    result = subprocess.CompletedProcess(args=[openocd_exe], returncode=1, stdout="", stderr="")
    for attempt in range(1, retries + 1):
        print(f"Upload attempt {attempt}/{retries} (openocd)")
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
        if result.returncode == 0:
            return result
        maybe_wait_before_retry(attempt, retries, retry_delay)

    if result.returncode != 0:
        print_linux_probe_permission_hint(result, host_tools_path)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hex", required=True, help="Path to firmware hex file")
    parser.add_argument("--port", default="", help="Arduino serial port (unused)")
    parser.add_argument("--target", default="nrf54l", help="pyOCD target name")
    parser.add_argument(
        "--uid",
        nargs="?",
        default=None,
        const="",
        help="Optional pyOCD probe UID",
    )
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
    parser.add_argument(
        "--host-tools-path",
        default="",
        help="Optional bundled host-tools package path",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=4,
        help="Number of upload attempts before failing (default: 4)",
    )
    parser.add_argument(
        "--retry-delay",
        type=float,
        default=0.6,
        help="Delay in seconds between upload attempts (default: 0.6)",
    )
    args = parser.parse_args()
    requested_runner = args.runner.strip().lower()
    host_tools_path = normalize_tools_path(args.host_tools_path)
    explicit_uid = normalize_uid(args.uid)
    inferred_uid = infer_uid_from_port(args.port) if explicit_uid is None else None
    selected_uid = explicit_uid if explicit_uid is not None else inferred_uid
    allow_inferred_uid_fallback = explicit_uid is None and inferred_uid is not None

    if not os.path.isfile(args.hex):
        print(f"ERROR: HEX file not found: {args.hex}", file=sys.stderr)
        return 2

    try:
        runner = choose_runner(requested_runner, args.openocd_bin, host_tools_path)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 4

    if runner == "pyocd":
        if detect_pyocd_command(host_tools_path) is None:
            if install_pyocd(host_tools_path):
                print("pyocd installation succeeded.")
            elif not install_host_pyocd_fallback():
                print("pyocd installation failed.", file=sys.stderr)
                print(f"HINT: {host_setup_hint(host_tools_path)}", file=sys.stderr)
        rc = upload_pyocd(
            args.hex,
            args.target,
            selected_uid,
            allow_uid_fallback=allow_inferred_uid_fallback,
            retries=args.retries,
            retry_delay=args.retry_delay,
            host_tools_path=host_tools_path,
        )
        if rc != 0 and requested_runner == "auto":
            print("pyocd upload failed in auto mode; trying openocd...")
            rc = upload_openocd(
                args.hex,
                args.openocd_script,
                args.openocd_speed,
                args.openocd_bin,
                retries=args.retries,
                retry_delay=args.retry_delay,
                host_tools_path=host_tools_path,
            ).returncode
    elif runner == "openocd":
        openocd_result = upload_openocd(
            args.hex,
            args.openocd_script,
            args.openocd_speed,
            args.openocd_bin,
            retries=args.retries,
            retry_delay=args.retry_delay,
            host_tools_path=host_tools_path,
        )
        rc = openocd_result.returncode

        if rc != 0 and looks_like_locked_target(openocd_result):
            pyocd_cmd = detect_pyocd_command(host_tools_path)
            if pyocd_cmd is None and requested_runner == "auto":
                if install_pyocd(host_tools_path) or (host_tools_path is None and install_host_pyocd_fallback()):
                    pyocd_cmd = detect_pyocd_command(host_tools_path)

            if pyocd_cmd is not None:
                print("OpenOCD indicates protected target; attempting pyocd recover/flash...")
                rc = upload_pyocd(
                    args.hex,
                    args.target,
                    selected_uid,
                    allow_uid_fallback=allow_inferred_uid_fallback,
                    retries=args.retries,
                    retry_delay=args.retry_delay,
                    pyocd_cmd=pyocd_cmd,
                    host_tools_path=host_tools_path,
                )
            elif requested_runner == "auto":
                print(
                    "ERROR: Target appears protected and OpenOCD cannot recover it. "
                    "Install pyocd and retry (or select pyOCD upload method).",
                    file=sys.stderr,
                )
                print(f"HINT: {host_setup_hint(host_tools_path)}", file=sys.stderr)
        elif rc != 0 and detect_pyocd_command(host_tools_path) is not None:
            print("OpenOCD upload failed; falling back to pyocd...")
            rc = upload_pyocd(
                args.hex,
                args.target,
                selected_uid,
                allow_uid_fallback=allow_inferred_uid_fallback,
                retries=args.retries,
                retry_delay=args.retry_delay,
                host_tools_path=host_tools_path,
            )
    else:
        print(f"ERROR: Unsupported runner: {runner}", file=sys.stderr)
        return 4

    if rc != 0:
        return rc

    print("Upload complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
