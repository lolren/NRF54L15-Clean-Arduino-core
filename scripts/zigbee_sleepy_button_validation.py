#!/usr/bin/env python3
import argparse
import threading
import time
from pathlib import Path

import serial


def _pump_serial(ser, sink, path, stop_flag):
    with open(path, "w", buffering=1) as handle:
        while not stop_flag["stop"]:
            try:
                data = ser.read(ser.in_waiting or 1)
            except Exception:
                break
            if not data:
                continue
            text = data.decode("utf-8", errors="replace")
            sink.append(text)
            handle.write(text)


def _send(ser, text):
    ser.write(text.encode("ascii"))
    ser.flush()


def _joined_text(parts):
    return "".join(parts)


def main():
    parser = argparse.ArgumentParser(
        description="Validate coordinator + sleepy Zigbee button join and action flow."
    )
    parser.add_argument("--coord-port", default="/dev/ttyACM1")
    parser.add_argument("--button-port", default="/dev/ttyACM0")
    parser.add_argument(
        "--outdir",
        default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_sleepy_button_validation",
    )
    parser.add_argument("--timeout-s", type=int, default=90)
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    coord_log = outdir / "coord.log"
    button_log = outdir / "button.log"
    summary_file = outdir / "summary.txt"

    coord = serial.Serial(args.coord_port, 115200, timeout=0.05)
    button = serial.Serial(args.button_port, 115200, timeout=0.05)
    stop_flag = {"stop": False}
    coord_parts = []
    button_parts = []

    threads = [
        threading.Thread(
            target=_pump_serial,
            args=(coord, coord_parts, coord_log, stop_flag),
            daemon=True,
        ),
        threading.Thread(
            target=_pump_serial,
            args=(button, button_parts, button_log, stop_flag),
            daemon=True,
        ),
    ]
    for thread in threads:
        thread.start()

    try:
        time.sleep(4.0)
        _send(coord, "p")
        _send(button, "c")
        time.sleep(1.0)
        coord_parts.clear()
        button_parts.clear()

        _send(button, "j")
        start = time.time()
        action_sent = False
        join_seen_at = None
        action_confirmed = False
        while time.time() - start < args.timeout_s:
            time.sleep(0.25)
            coord_text = _joined_text(coord_parts)
            button_text = _joined_text(button_parts)

            if join_seen_at is None and (
                "transport_key seq=" in button_text
                or "state joined=yes" in button_text
                or "mode=joined" in button_text
                or "join OK ch=" in button_text
            ):
                join_seen_at = time.time()

            if (
                not action_sent
                and (
                    "end_device_timeout status=0x0" in button_text
                    or (
                        join_seen_at is not None
                        and (time.time() - join_seen_at) >= 8.0
                    )
                )
            ):
                _send(button, "t")
                action_sent = True

            if action_sent and "button_action cmd=toggle" in button_text and (
                "remote_onoff_cmd" in coord_text
                or ("report short=" in coord_text and "cluster=0x6" in coord_text)
            ):
                action_confirmed = True

            if not action_confirmed:
                continue

            if (
                "state joined=yes mode=joined" in button_text
                and "pending=no" in coord_text
            ):
                break

        _send(button, "s")
        _send(coord, "l")
        time.sleep(2.0)
    finally:
        stop_flag["stop"] = True
        for thread in threads:
            thread.join(timeout=1.0)
        coord.close()
        button.close()

    coord_text = _joined_text(coord_parts)
    button_text = _joined_text(button_parts)
    summary = {
        "scan_ok": "scan_done found=yes" in button_text,
        "assoc_retry_ok": "assoc_req_retry_ok attempt=2" in button_text
        or "assoc_req_ok parent=0x" in button_text,
        "assoc_ok": "assoc_ok short=0x" in button_text,
        "join_ok": "join OK ch=" in button_text,
        "transport_key_ok": "transport_key seq=" in button_text,
        "device_announce_ok": "device_announce short=" in coord_text,
        "timeout_negotiated": "end_device_timeout status=0x0" in button_text,
        "status_joined": "state joined=yes mode=joined" in button_text,
        "action_triggered": "button_action cmd=toggle" in button_text,
        "action_report_seen": (
            "remote_onoff_cmd" in coord_text
            or ("report short=" in coord_text and "cluster=0x6" in coord_text)
        ),
    }

    with open(summary_file, "w") as handle:
        for key, value in summary.items():
            handle.write(f"{key}={str(value).lower()}\n")

    print(summary_file)
    for key, value in summary.items():
        print(f"{key}={str(value).lower()}")


if __name__ == "__main__":
    main()
