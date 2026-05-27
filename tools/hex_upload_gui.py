#!/usr/bin/env python3
"""Simple GUI for flashing .hex files with the nRF54L15 core uploader."""

from __future__ import annotations

import argparse
import importlib.util
import queue
import shutil
import subprocess
import sys
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import tkinter as tk
from tkinter import filedialog, messagebox, ttk


RUNNER_LABELS = {
    "Auto Recover (recommended)": "auto",
    "pyOCD Recovery": "pyocd",
    "OpenOCD Direct": "openocd",
    "UF2 Bootloader": "uf2",
}

SAFE_MODE_LABELS = {
    "Auto": "auto",
    "On": "true",
    "Off": "false",
}

AUTO_PORT_LABEL = "Auto-select"


@dataclass(frozen=True)
class CoreAssets:
    upload_script: Path
    openocd_script: Path
    host_tools_path: Path | None
    openocd_bin: str
    source_label: str


@dataclass(frozen=True)
class PortInfo:
    device: str
    label: str
    uid: str | None
    is_cmsis_dap: bool


def linux_fallback_port_devices() -> list[str]:
    candidates: set[str] = set()
    for pattern in ("ttyACM*", "ttyUSB*"):
        for node in Path("/dev").glob(pattern):
            if node.exists():
                candidates.add(str(node))
    by_id_dir = Path("/dev/serial/by-id")
    if by_id_dir.is_dir():
        for entry in by_id_dir.iterdir():
            try:
                resolved = entry.resolve(strict=True)
            except OSError:
                continue
            if resolved.exists():
                candidates.add(str(resolved))
    return sorted(candidates)


def linux_tty_description(helper: Any, device: str) -> str:
    tty_name = Path(device).name
    base = Path("/sys/class/tty") / tty_name / "device"
    try:
        resolved = base.resolve(strict=True)
    except OSError:
        return ""

    for parent in (resolved, *resolved.parents):
        for field in ("product", "interface"):
            path = parent / field
            if not path.is_file():
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore").strip()
            except OSError:
                continue
            if text:
                return text
    return ""


def linux_fallback_ports(helper: Any, host_tools_path: Path | None) -> list[PortInfo]:
    ports: list[PortInfo] = []
    tty_identity = getattr(helper, "_sysfs_usb_identity_for_tty", None)
    for device in linux_fallback_port_devices():
        uid = helper.infer_uid_from_port(device, host_tools_path)
        vid = pid = None
        if callable(tty_identity):
            try:
                vid, pid = tty_identity(Path(device))
            except Exception:
                vid = pid = None
        details = []
        is_cmsis_dap = False
        if vid and pid:
            details.append(f"{vid.upper()}:{pid.upper()}")
            is_cmsis_dap = (vid, pid) == (
                helper.CMSIS_DAP_VENDOR_ID,
                helper.CMSIS_DAP_PRODUCT_ID,
            )
        if uid:
            details.append(uid)
        if is_cmsis_dap:
            details.append("CMSIS-DAP")

        label = device
        description = linux_tty_description(helper, device)
        if description:
            label += f" - {description}"
        if details:
            label += f" [{' | '.join(details)}]"

        ports.append(
            PortInfo(
                device=device,
                label=label,
                uid=uid,
                is_cmsis_dap=is_cmsis_dap,
            )
        )
    return ports


def _version_key(path: Path) -> tuple[int, ...]:
    parts = []
    for token in path.name.split("."):
        try:
            parts.append(int(token))
        except ValueError:
            parts.append(-1)
    return tuple(parts)


def _latest_child(path: Path) -> Path | None:
    if not path.is_dir():
        return None
    children = [child for child in path.iterdir() if child.is_dir()]
    if not children:
        return None
    return sorted(children, key=_version_key)[-1]


def _find_openocd_bin() -> str:
    explicit = (
        Path.home()
        / ".arduino15"
        / "packages"
        / "arduino"
        / "tools"
        / "openocd"
        / "0.11.0-arduino2"
        / "bin"
        / ("openocd.exe" if sys.platform.startswith("win") else "openocd")
    )
    if explicit.is_file():
        return str(explicit)
    return shutil.which("openocd") or "openocd"


def resolve_core_assets() -> CoreAssets:
    tools_dir = Path(__file__).resolve().parent
    core_root = tools_dir.parent
    local_pkg = core_root / "hardware" / "nrf54l15clean" / "nrf54l15clean"
    local_upload = local_pkg / "tools" / "upload.py"
    local_openocd = local_pkg / "tools" / "openocd" / "nrf54l15.cfg"
    local_host_tools = core_root / "tools" / "board_manager" / "nrf54l15hosttools"

    if local_upload.is_file() and local_openocd.is_file():
        return CoreAssets(
            upload_script=local_upload,
            openocd_script=local_openocd,
            host_tools_path=local_host_tools if local_host_tools.is_dir() else None,
            openocd_bin=_find_openocd_bin(),
            source_label="repo core",
        )

    installed_root = (
        Path.home()
        / ".arduino15"
        / "packages"
        / "nrf54l15clean"
        / "hardware"
        / "nrf54l15clean"
    )
    installed_version = _latest_child(installed_root)
    if installed_version is None:
        raise FileNotFoundError("Could not find local repo core or installed nrf54l15clean package.")

    upload_script = installed_version / "tools" / "upload.py"
    openocd_script = installed_version / "tools" / "openocd" / "nrf54l15.cfg"
    host_tools_path = (
        Path.home()
        / ".arduino15"
        / "packages"
        / "nrf54l15clean"
        / "tools"
        / "nrf54l15hosttools"
    )

    if not upload_script.is_file() or not openocd_script.is_file():
        raise FileNotFoundError("Installed nrf54l15clean package is missing upload tooling.")

    return CoreAssets(
        upload_script=upload_script,
        openocd_script=openocd_script,
        host_tools_path=host_tools_path if host_tools_path.is_dir() else None,
        openocd_bin=_find_openocd_bin(),
        source_label=f"installed package {installed_version.name}",
    )


def load_upload_helper(upload_script: Path):
    spec = importlib.util.spec_from_file_location("nrf54l15_upload_helper", upload_script)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load uploader module from {upload_script}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def enumerate_ports(helper: Any, host_tools_path: Path | None) -> list[PortInfo]:
    list_ports = helper.import_serial_list_ports(host_tools_path)
    if list_ports is None:
        if sys.platform.startswith("linux"):
            return linux_fallback_ports(helper, host_tools_path)
        return []

    cmsis_ports = set(helper.matching_cmsis_dap_serial_ports(host_tools_path))
    ports: list[PortInfo] = []
    for info in list_ports.comports():
        device = str(getattr(info, "device", "") or "")
        if not device:
            continue
        if sys.platform.startswith("linux") and Path(device).name.startswith("ttyS"):
            continue

        description = str(getattr(info, "description", "") or "")
        uid = helper.infer_uid_from_port(device, host_tools_path)
        vid = getattr(info, "vid", None)
        pid = getattr(info, "pid", None)
        details = []
        if vid is not None and pid is not None:
            details.append(f"{vid:04X}:{pid:04X}")
        if uid:
            details.append(uid)
        is_cmsis_dap = device in cmsis_ports
        if is_cmsis_dap:
            details.append("CMSIS-DAP")

        label = device
        if description and description.lower() != "n/a":
            label += f" - {description}"
        if details:
            label += f" [{' | '.join(details)}]"

        ports.append(
            PortInfo(
                device=device,
                label=label,
                uid=uid,
                is_cmsis_dap=is_cmsis_dap,
            )
        )

    ports.sort(key=lambda item: (not item.is_cmsis_dap, item.device.lower()))
    return ports


class HexUploadGui:
    def __init__(self, root: tk.Tk, assets: CoreAssets, helper: Any, initial_hex: str = ""):
        self.root = root
        self.assets = assets
        self.helper = helper
        self.log_queue: queue.Queue[str | None] = queue.Queue()
        self.process: subprocess.Popen[str] | None = None
        self.port_map: dict[str, PortInfo] = {}

        self.hex_var = tk.StringVar(value=initial_hex)
        self.port_var = tk.StringVar(value=AUTO_PORT_LABEL)
        self.uid_var = tk.StringVar(value="")
        self.runner_var = tk.StringVar(value="Auto Recover (recommended)")
        self.safe_mode_var = tk.StringVar(value="Auto")
        self.uid_hint_var = tk.StringVar(value="Probe UID will be inferred from the selected port when possible.")
        self.status_var = tk.StringVar(
            value=f"Uploader source: {assets.source_label} | OpenOCD: {assets.openocd_bin}"
        )

        self._build_ui()
        self.refresh_ports()
        self.root.after(100, self._pump_log_queue)

    def _build_ui(self) -> None:
        self.root.title("nRF54L15 HEX Uploader")
        self.root.geometry("900x640")
        self.root.minsize(760, 520)

        frame = ttk.Frame(self.root, padding=12)
        frame.grid(row=0, column=0, sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        frame.columnconfigure(1, weight=1)
        frame.rowconfigure(5, weight=1)

        ttk.Label(frame, text="HEX File").grid(row=0, column=0, sticky="w", padx=(0, 8), pady=(0, 8))
        ttk.Entry(frame, textvariable=self.hex_var).grid(row=0, column=1, sticky="ew", pady=(0, 8))
        ttk.Button(frame, text="Browse...", command=self.browse_hex).grid(row=0, column=2, sticky="ew", pady=(0, 8))

        ttk.Label(frame, text="Port").grid(row=1, column=0, sticky="w", padx=(0, 8), pady=(0, 8))
        self.port_combo = ttk.Combobox(frame, textvariable=self.port_var, state="readonly")
        self.port_combo.grid(row=1, column=1, sticky="ew", pady=(0, 8))
        self.port_combo.bind("<<ComboboxSelected>>", self._on_port_changed)
        ttk.Button(frame, text="Refresh", command=self.refresh_ports).grid(row=1, column=2, sticky="ew", pady=(0, 8))

        ttk.Label(frame, text="UID Override").grid(row=2, column=0, sticky="w", padx=(0, 8), pady=(0, 4))
        ttk.Entry(frame, textvariable=self.uid_var).grid(row=2, column=1, sticky="ew", pady=(0, 4))
        ttk.Label(frame, textvariable=self.uid_hint_var, foreground="#555555").grid(
            row=3, column=1, columnspan=2, sticky="w", pady=(0, 8)
        )

        options = ttk.Frame(frame)
        options.grid(row=4, column=0, columnspan=3, sticky="ew", pady=(0, 8))
        options.columnconfigure(1, weight=1)
        options.columnconfigure(3, weight=1)

        ttk.Label(options, text="Runner").grid(row=0, column=0, sticky="w", padx=(0, 8))
        ttk.Combobox(
            options,
            textvariable=self.runner_var,
            state="readonly",
            values=list(RUNNER_LABELS.keys()),
        ).grid(row=0, column=1, sticky="ew", padx=(0, 12))

        ttk.Label(options, text="pyOCD Safe").grid(row=0, column=2, sticky="w", padx=(0, 8))
        ttk.Combobox(
            options,
            textvariable=self.safe_mode_var,
            state="readonly",
            values=list(SAFE_MODE_LABELS.keys()),
        ).grid(row=0, column=3, sticky="ew")

        log_frame = ttk.LabelFrame(frame, text="Upload Log", padding=8)
        log_frame.grid(row=5, column=0, columnspan=3, sticky="nsew")
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, wrap="word", height=20)
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        log_scroll.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=log_scroll.set)

        buttons = ttk.Frame(frame)
        buttons.grid(row=6, column=0, columnspan=3, sticky="ew", pady=(10, 0))
        buttons.columnconfigure(2, weight=1)

        self.upload_button = ttk.Button(buttons, text="Upload", command=self.start_upload)
        self.upload_button.grid(row=0, column=0, sticky="w")
        self.cancel_button = ttk.Button(buttons, text="Cancel", command=self.cancel_upload, state="disabled")
        self.cancel_button.grid(row=0, column=1, sticky="w", padx=(8, 0))
        ttk.Label(buttons, textvariable=self.status_var).grid(row=0, column=2, sticky="e")

    def browse_hex(self) -> None:
        selected = filedialog.askopenfilename(
            title="Select HEX File",
            filetypes=[("HEX files", "*.hex"), ("All files", "*.*")],
        )
        if selected:
            self.hex_var.set(selected)

    def selected_port(self) -> PortInfo | None:
        return self.port_map.get(self.port_var.get())

    def refresh_ports(self) -> None:
        current_device = self.selected_port().device if self.selected_port() else None
        ports = enumerate_ports(self.helper, self.assets.host_tools_path)
        self.port_map = {port.label: port for port in ports}
        values = [AUTO_PORT_LABEL] + [port.label for port in ports]
        self.port_combo["values"] = values

        if current_device:
            for port in ports:
                if port.device == current_device:
                    self.port_var.set(port.label)
                    break
            else:
                self.port_var.set(AUTO_PORT_LABEL)
        elif not self.port_var.get() or self.port_var.get() not in values:
            self.port_var.set(AUTO_PORT_LABEL)

        self._update_uid_hint()

    def _on_port_changed(self, _event: object | None = None) -> None:
        self._update_uid_hint()

    def _update_uid_hint(self) -> None:
        selected = self.selected_port()
        if selected is None:
            self.uid_hint_var.set("Probe UID will be auto-selected unless you override it.")
            return
        if selected.uid:
            self.uid_hint_var.set(f"Inferred probe UID from port: {selected.uid}")
            return
        self.uid_hint_var.set("Selected port did not expose a unique UID. Auto-select may be ambiguous with multiple probes.")

    def append_log(self, text: str) -> None:
        self.log_text.insert("end", text)
        self.log_text.see("end")

    def build_command(self) -> list[str]:
        hex_path = Path(self.hex_var.get().strip()).expanduser()
        cmd = [
            sys.executable,
            str(self.assets.upload_script),
            "--hex",
            str(hex_path),
            "--target",
            "nrf54l",
            "--runner",
            RUNNER_LABELS[self.runner_var.get()],
            "--openocd-script",
            str(self.assets.openocd_script),
            "--openocd-speed",
            "4000",
            "--openocd-bin",
            self.assets.openocd_bin,
            "--pyocd-safe",
            SAFE_MODE_LABELS[self.safe_mode_var.get()],
        ]

        if self.assets.host_tools_path is not None:
            cmd.extend(["--host-tools-path", str(self.assets.host_tools_path)])

        selected = self.selected_port()
        if selected is not None:
            cmd.extend(["--port", selected.device])

        uid = self.uid_var.get().strip()
        if uid:
            cmd.extend(["--uid", uid])

        return cmd

    def start_upload(self) -> None:
        hex_path = Path(self.hex_var.get().strip()).expanduser()
        if not hex_path.is_file():
            messagebox.showerror("Missing HEX", f"HEX file not found:\n{hex_path}")
            return

        if self.process is not None and self.process.poll() is None:
            messagebox.showwarning("Upload Running", "An upload is already running.")
            return

        cmd = self.build_command()
        self.append_log("\n" + "=" * 72 + "\n")
        self.append_log("Running:\n")
        self.append_log(" ".join(cmd) + "\n\n")

        try:
            self.process = subprocess.Popen(
                cmd,
                cwd=str(self.assets.upload_script.parent),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
        except OSError as exc:
            messagebox.showerror("Upload Failed", str(exc))
            return

        self.status_var.set("Uploading...")
        self.upload_button.configure(state="disabled")
        self.cancel_button.configure(state="normal")

        thread = threading.Thread(target=self._read_process_output, daemon=True)
        thread.start()

    def _read_process_output(self) -> None:
        assert self.process is not None
        if self.process.stdout is not None:
            for line in self.process.stdout:
                self.log_queue.put(line)
        self.process.wait()
        self.log_queue.put(None)

    def _pump_log_queue(self) -> None:
        finished = False
        while True:
            try:
                item = self.log_queue.get_nowait()
            except queue.Empty:
                break
            if item is None:
                finished = True
                continue
            self.append_log(item)

        if finished and self.process is not None:
            rc = self.process.returncode
            self.append_log(f"\nUpload finished with exit code {rc}\n")
            if rc == 0:
                self.status_var.set("Upload complete")
            else:
                self.status_var.set(f"Upload failed ({rc})")
            self.upload_button.configure(state="normal")
            self.cancel_button.configure(state="disabled")
            self.process = None

        self.root.after(100, self._pump_log_queue)

    def cancel_upload(self) -> None:
        if self.process is None or self.process.poll() is not None:
            return
        self.process.terminate()
        self.append_log("\nUpload cancelled by user.\n")
        self.status_var.set("Cancelling...")


def run_check(initial_hex: str = "") -> int:
    assets = resolve_core_assets()
    helper = load_upload_helper(assets.upload_script)
    print(f"Uploader: {assets.upload_script}")
    print(f"OpenOCD config: {assets.openocd_script}")
    print(f"Host tools: {assets.host_tools_path or 'none'}")
    print(f"OpenOCD bin: {assets.openocd_bin}")
    if initial_hex:
        print(f"Initial hex: {initial_hex}")
    print("Ports:")
    ports = enumerate_ports(helper, assets.host_tools_path)
    if not ports:
        print("  (none found)")
    for port in ports:
        print(f"  {port.label}")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Simple GUI for flashing .hex files to nRF54L15 boards.")
    parser.add_argument("--hex", default="", help="Optional HEX path to prefill in the GUI.")
    parser.add_argument("--check", action="store_true", help="Print detected tools and ports, then exit.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.check:
        return run_check(args.hex)

    assets = resolve_core_assets()
    helper = load_upload_helper(assets.upload_script)

    root = tk.Tk()
    gui = HexUploadGui(root, assets, helper, initial_hex=args.hex)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
