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


def _joined(parts):
    return "".join(parts)


def _cluster_report_seen(coord_text, cluster_hex):
    return "report short=" in coord_text and f"cluster={cluster_hex}" in coord_text


def _reporting_verified(coord_text, cluster_hex):
    return (
        "cfg_reporting_rsp short=" in coord_text
        and f"cluster={cluster_hex} status=OK" in coord_text
        and "read_reporting_rsp short=" in coord_text
        and f"cluster={cluster_hex} verify=OK" in coord_text
    )


def main():
    parser = argparse.ArgumentParser(
        description="Validate coordinator + sleepy Zigbee climate sensor join flow."
    )
    parser.add_argument("--coord-port", default="/dev/ttyACM1")
    parser.add_argument("--sensor-port", default="/dev/ttyACM0")
    parser.add_argument(
        "--outdir",
        default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_sleepy_climate_validation",
    )
    parser.add_argument("--timeout-s", type=int, default=120)
    parser.add_argument("--expect-sleep-ms", type=int, default=15000)
    parser.add_argument("--expect-humidity", action="store_true")
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    coord_log = outdir / "coord.log"
    sensor_log = outdir / "sensor.log"
    summary_file = outdir / "summary.txt"

    coord = serial.Serial(args.coord_port, 115200, timeout=0.05)
    sensor = serial.Serial(args.sensor_port, 115200, timeout=0.05)
    stop_flag = {"stop": False}
    coord_parts = []
    sensor_parts = []

    threads = [
        threading.Thread(
            target=_pump_serial,
            args=(coord, coord_parts, coord_log, stop_flag),
            daemon=True,
        ),
        threading.Thread(
            target=_pump_serial,
            args=(sensor, sensor_parts, sensor_log, stop_flag),
            daemon=True,
        ),
    ]
    for thread in threads:
        thread.start()

    try:
        time.sleep(4.0)
        coord.reset_input_buffer()
        sensor.reset_input_buffer()
        coord.reset_output_buffer()
        sensor.reset_output_buffer()
        coord_parts.clear()
        sensor_parts.clear()
        _send(coord, "c")
        time.sleep(0.5)
        _send(coord, "p")
        _send(sensor, "c")
        time.sleep(1.0)
        coord_parts.clear()
        sensor_parts.clear()

        _send(sensor, "j")
        start = time.time()
        join_seen = False
        while time.time() - start < args.timeout_s:
            time.sleep(0.25)
            coord_text = _joined(coord_parts)
            sensor_text = _joined(sensor_parts)
            if (
                "join OK ch=" in sensor_text
                and "transport_key seq=" in sensor_text
                and "device_announce short=" in coord_text
            ):
                join_seen = True

            if not join_seen:
                continue

            temp_reporting_ok = _reporting_verified(
                coord_text, "0x402"
            ) or _cluster_report_seen(coord_text, "0x402")
            humidity_reporting_ok = (
                not args.expect_humidity
                or _reporting_verified(coord_text, "0x405")
                or _cluster_report_seen(coord_text, "0x405")
            )
            power_reporting_ok = _reporting_verified(
                coord_text, "0x1"
            ) or _cluster_report_seen(coord_text, "0x1")
            pending_cleared = "pending=no" in coord_text
            timeout_ok = "end_device_timeout status=0x0" in sensor_text
            interview_complete = (
                timeout_ok
                and temp_reporting_ok
                and humidity_reporting_ok
                and power_reporting_ok
                and pending_cleared
            )
            if not interview_complete:
                continue

            sleep_ok = (
                args.expect_sleep_ms <= 0
                or f"sleep_cycle system_off_ms={args.expect_sleep_ms}" in sensor_text
            )
            if sleep_ok:
                break

        _send(sensor, "s")
        _send(coord, "l")
        time.sleep(1.0)
    finally:
        stop_flag["stop"] = True
        for thread in threads:
            thread.join(timeout=1.0)
        coord.close()
        sensor.close()

    coord_text = _joined(coord_parts)
    sensor_text = _joined(sensor_parts)
    expected_sleep = f"sleep_cycle system_off_ms={args.expect_sleep_ms}"
    summary = {
        "scan_ok": "scan_done found=yes" in sensor_text,
        "assoc_ok": "assoc_ok short=0x" in sensor_text,
        "join_ok": "join OK ch=" in sensor_text,
        "transport_key_ok": "transport_key seq=" in sensor_text,
        "device_announce_ok": "device_announce short=" in coord_text,
        "timeout_negotiated": "end_device_timeout status=0x0" in sensor_text,
        "status_joined": (
            "state joined=yes mode=joined" in sensor_text
            or "alive ch=" in sensor_text
        ),
        "temp_reporting_ok": _reporting_verified(coord_text, "0x402")
        or _cluster_report_seen(coord_text, "0x402"),
        "humidity_reporting_ok": (
            not args.expect_humidity
            or _reporting_verified(coord_text, "0x405")
            or _cluster_report_seen(coord_text, "0x405")
        ),
        "power_reporting_ok": _reporting_verified(coord_text, "0x1")
        or _cluster_report_seen(coord_text, "0x1"),
        "temp_report_seen": _cluster_report_seen(coord_text, "0x402"),
        "humidity_report_seen": (
            not args.expect_humidity
            or _cluster_report_seen(coord_text, "0x405")
        ),
        "power_report_seen": _cluster_report_seen(coord_text, "0x1"),
        "pending_cleared": "pending=no" in coord_text,
        "sleep_cycle_logged": expected_sleep in sensor_text,
    }

    with open(summary_file, "w") as handle:
        for key, value in summary.items():
            handle.write(f"{key}={str(value).lower()}\n")

    print(summary_file)
    for key, value in summary.items():
        print(f"{key}={str(value).lower()}")


if __name__ == "__main__":
    main()
