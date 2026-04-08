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
        description="Validate coordinator + router-capable Zigbee HA join/interview/control."
    )
    parser.add_argument("--coord-port", default="/dev/ttyACM1")
    parser.add_argument("--router-port", default="/dev/ttyACM0")
    parser.add_argument(
        "--outdir",
        default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_ha_router_validation",
    )
    parser.add_argument("--timeout-s", type=int, default=90)
    parser.add_argument("--expect-onoff", action="store_true", default=True)
    parser.add_argument("--expect-level", action="store_true")
    parser.add_argument("--expect-color-hs", action="store_true")
    parser.add_argument("--expect-color-temp", action="store_true")
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    coord_log = outdir / "coord.log"
    router_log = outdir / "router.log"
    summary_file = outdir / "summary.txt"

    coord = serial.Serial(args.coord_port, 115200, timeout=0.05)
    router = serial.Serial(args.router_port, 115200, timeout=0.05)
    stop_flag = {"stop": False}
    coord_parts = []
    router_parts = []

    threads = [
        threading.Thread(
            target=_pump_serial,
            args=(coord, coord_parts, coord_log, stop_flag),
            daemon=True,
        ),
        threading.Thread(
            target=_pump_serial,
            args=(router, router_parts, router_log, stop_flag),
            daemon=True,
        ),
    ]
    for thread in threads:
        thread.start()

    try:
        time.sleep(4.0)
        _send(coord, "c")
        start = time.time()
        while time.time() - start < 5.0:
            if "nodes cleared" in _joined_text(coord_parts):
                break
            time.sleep(0.1)

        _send(coord, "p")
        _send(router, "c")
        start = time.time()
        while time.time() - start < 6.0:
            if "state cleared" in _joined_text(router_parts):
                break
            time.sleep(0.1)

        start = time.time()
        join_kicked = False
        commands_sent = False
        while time.time() - start < args.timeout_s:
            time.sleep(0.25)
            coord_text = _joined_text(coord_parts)
            router_text = _joined_text(router_parts)

            if (not join_kicked) and ("scan_join start" not in router_text) and (
                time.time() - start > 2.0
            ):
                _send(router, "j")
                join_kicked = True

            onoff_ready = ("bind_rsp short=" in coord_text and "cluster=0x6" in coord_text) or (
                "read_reporting_rsp short=" in coord_text and "cluster=0x6 verify=OK" in coord_text
            )
            level_ready = ("bind_rsp short=" in coord_text and "cluster=0x8" in coord_text) or (
                "read_reporting_rsp short=" in coord_text and "cluster=0x8 verify=OK" in coord_text
            )
            color_ready = ("bind_rsp short=" in coord_text and "cluster=0x300" in coord_text) or (
                "read_reporting_rsp short=" in coord_text and "cluster=0x300 verify=OK" in coord_text
            )
            interview_ready = ("simple_desc short=" in coord_text)
            if args.expect_onoff:
                interview_ready = interview_ready and onoff_ready
            if args.expect_level:
                interview_ready = interview_ready and level_ready
            if args.expect_color_hs or args.expect_color_temp:
                interview_ready = interview_ready and color_ready
            pending_clear = ("pending=no" in coord_text) or (
                "alive joined=1 pending=0" in coord_text
            )
            if interview_ready and pending_clear and not commands_sent:
                if args.expect_onoff:
                    _send(coord, "t")
                    time.sleep(2.0)
                if args.expect_level:
                    _send(coord, "U")
                    time.sleep(2.0)
                if args.expect_color_hs:
                    _send(coord, "h")
                    time.sleep(2.0)
                if args.expect_color_temp:
                    _send(coord, "A")
                commands_sent = True

            onoff_ok = (not args.expect_onoff) or ("queue_cmd OK" in coord_text)
            level_ok = (not args.expect_level) or ("queue_level OK" in coord_text)
            color_hs_ok = (not args.expect_color_hs) or (
                "queue_color_hs OK" in coord_text
            )
            color_temp_ok = (not args.expect_color_temp) or (
                "queue_color_temp OK" in coord_text
            )
            if commands_sent and onoff_ok and level_ok and color_hs_ok and color_temp_ok:
                break

        if ("pending=no" in _joined_text(coord_parts)) or (
            "alive joined=1 pending=0" in _joined_text(coord_parts)
        ):
            _send(coord, "l")
            time.sleep(1.0)
    finally:
        stop_flag["stop"] = True
        for thread in threads:
            thread.join(timeout=1.0)
        coord.close()
        router.close()

    coord_text = _joined_text(coord_parts)
    router_text = _joined_text(router_parts)
    summary = {
        "joined": "join OK" in router_text,
        "cap_ce": "cap=0xCE" in coord_text,
        "router_deliver_ok": "router_deliver OK type=aps" in coord_text,
        "transport_key_ok": "transport_key OK stage=prepared_tx" in coord_text,
        "device_announce_ok": ("device_announce OK" in coord_text)
        or ("device_announce OK" in router_text),
        "node_desc_ok": "node_desc short=" in coord_text,
        "active_ep_ok": "active_ep short=" in coord_text,
        "simple_desc_ok": "simple_desc short=" in coord_text,
        "pending_cleared": ("pending=no" in coord_text)
        or ("alive joined=1 pending=0" in coord_text),
        "queue_onoff_ok": (not args.expect_onoff) or ("queue_cmd OK" in coord_text),
        "queue_level_ok": (not args.expect_level) or ("queue_level OK" in coord_text),
        "queue_color_hs_ok": (not args.expect_color_hs)
        or ("queue_color_hs OK" in coord_text),
        "queue_color_temp_ok": (not args.expect_color_temp)
        or ("queue_color_temp OK" in coord_text),
    }

    with open(summary_file, "w") as handle:
        for key, value in summary.items():
            handle.write(f"{key}={str(value).lower()}\n")

    print(summary_file)
    for key, value in summary.items():
        print(f"{key}={str(value).lower()}")


if __name__ == "__main__":
    main()
