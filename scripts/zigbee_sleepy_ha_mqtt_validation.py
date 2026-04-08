#!/usr/bin/env python3
import argparse
import subprocess
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


def _pump_proc_stdout(proc, sink, path, stop_flag):
    with open(path, "w", buffering=1) as handle:
        while not stop_flag["stop"]:
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    break
                continue
            sink.append(line)
            handle.write(line)


def _send(ser, text):
    ser.write(text.encode("ascii"))
    ser.flush()


def _joined(parts):
    return "".join(parts)


def _mqtt_cmd(args, *extra):
    return [
        "mosquitto_pub",
        "-h",
        args.mqtt_host,
        "-p",
        str(args.mqtt_port),
        "-u",
        args.mqtt_user,
        "-P",
        args.mqtt_pass,
        *extra,
    ]


def _mqtt_sub_cmd(args):
    return [
        "mosquitto_sub",
        "-h",
        args.mqtt_host,
        "-p",
        str(args.mqtt_port),
        "-u",
        args.mqtt_user,
        "-P",
        args.mqtt_pass,
        "-v",
        "-t",
        "zigbee2mqtt/bridge/#",
        "-t",
        f"zigbee2mqtt/{args.target_ieee}",
        "-t",
        "homeassistant/#",
    ]


def main():
    parser = argparse.ArgumentParser(
        description="Validate a sleepy Zigbee example against Zigbee2MQTT via MQTT."
    )
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--mqtt-host", default="192.168.1.100")
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--mqtt-user", default="lolren")
    parser.add_argument("--mqtt-pass", default="lolren")
    parser.add_argument("--target-ieee", default="0xd0acf9feff59226e")
    parser.add_argument("--permit-join-s", type=int, default=180)
    parser.add_argument("--timeout-s", type=int, default=240)
    parser.add_argument("--join-command", default="j")
    parser.add_argument("--clear-command", default="c")
    parser.add_argument(
        "--outdir",
        default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_sleepy_ha_mqtt_validation",
    )
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    serial_log = outdir / "serial.log"
    mqtt_log = outdir / "mqtt.log"
    summary_file = outdir / "summary.txt"

    stop_flag = {"stop": False}
    serial_parts = []
    mqtt_parts = []

    mqtt_proc = subprocess.Popen(
        _mqtt_sub_cmd(args),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    mqtt_thread = threading.Thread(
        target=_pump_proc_stdout,
        args=(mqtt_proc, mqtt_parts, mqtt_log, stop_flag),
        daemon=True,
    )
    mqtt_thread.start()

    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    serial_thread = threading.Thread(
        target=_pump_serial,
        args=(ser, serial_parts, serial_log, stop_flag),
        daemon=True,
    )
    serial_thread.start()

    try:
        time.sleep(2.0)
        subprocess.run(
            _mqtt_cmd(
                args,
                "-t",
                "zigbee2mqtt/bridge/request/device/remove",
                "-m",
                f'{{"id":"{args.target_ieee}","force":true}}',
            ),
            check=False,
        )
        time.sleep(1.5)
        subprocess.run(
            _mqtt_cmd(
                args,
                "-t",
                "zigbee2mqtt/bridge/request/permit_join",
                "-m",
                f'{{"value":true,"time":{args.permit_join_s}}}',
            ),
            check=False,
        )

        time.sleep(3.0)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        serial_parts.clear()
        mqtt_parts.clear()

        _send(ser, args.clear_command)
        clear_deadline = time.time() + 12.0
        while time.time() < clear_deadline:
            time.sleep(0.25)
            if "state cleared" in _joined(serial_parts):
                break

        time.sleep(1.0)
        serial_parts.clear()
        mqtt_parts.clear()
        _send(ser, args.join_command)

        join_ok = False
        transport_key_ok = False
        announce_ok = False
        boot_report_ok = False
        timeout_ok = False
        alive_joined = False
        sleep_logged = False
        z2m_joined = False
        z2m_interview_started = False
        z2m_interview_success = False
        z2m_interview_failed = False
        z2m_topic_seen = False
        ha_discovery_seen = False

        deadline = time.time() + args.timeout_s
        while time.time() < deadline:
            time.sleep(1.0)
            serial_text = _joined(serial_parts)
            mqtt_text = _joined(mqtt_parts)

            join_ok = join_ok or ("join OK ch=" in serial_text)
            transport_key_ok = transport_key_ok or ("transport_key seq=" in serial_text)
            announce_ok = announce_ok or ("device_announce OK" in serial_text)
            boot_report_ok = boot_report_ok or (
                "boot_report temp=OK" in serial_text
                or "boot_report state=OK" in serial_text
            )
            timeout_ok = timeout_ok or (
                "end_device_timeout_req OK" in serial_text
                or "end_device_timeout status=0x0" in serial_text
            )
            alive_joined = alive_joined or (
                ("alive ch=" in serial_text and "joined=yes" in serial_text)
                or "state joined=yes mode=joined" in serial_text
            )
            sleep_logged = sleep_logged or ("sleep_cycle system_off_ms=" in serial_text)

            z2m_joined = z2m_joined or (
                f'"ieee_address":"{args.target_ieee}"' in mqtt_text
                and '"type":"device_joined"' in mqtt_text
            )
            z2m_interview_started = z2m_interview_started or (
                f'"ieee_address":"{args.target_ieee}"' in mqtt_text
                and '"type":"device_interview"' in mqtt_text
                and '"status":"started"' in mqtt_text
            )
            z2m_interview_success = z2m_interview_success or (
                f'"ieee_address":"{args.target_ieee}"' in mqtt_text
                and '"type":"device_interview"' in mqtt_text
                and '"status":"successful"' in mqtt_text
            )
            z2m_interview_failed = z2m_interview_failed or (
                f'"ieee_address":"{args.target_ieee}"' in mqtt_text
                and '"type":"device_interview"' in mqtt_text
                and '"status":"failed"' in mqtt_text
            )
            z2m_topic_seen = z2m_topic_seen or (
                f"zigbee2mqtt/{args.target_ieee} " in mqtt_text
            )
            ha_discovery_seen = ha_discovery_seen or (
                args.target_ieee in mqtt_text and "homeassistant/" in mqtt_text
            )

            if (
                join_ok
                and transport_key_ok
                and announce_ok
                and timeout_ok
                and z2m_joined
                and z2m_interview_started
                and z2m_interview_success
            ):
                break

        _send(ser, "s")
        time.sleep(1.0)
    finally:
        subprocess.run(
            _mqtt_cmd(
                args,
                "-t",
                "zigbee2mqtt/bridge/request/permit_join",
                "-m",
                '{"value":false,"time":0}',
            ),
            check=False,
        )
        stop_flag["stop"] = True
        try:
            ser.close()
        except Exception:
            pass
        if mqtt_proc.poll() is None:
            mqtt_proc.terminate()
            try:
                mqtt_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                mqtt_proc.kill()
        mqtt_thread.join(timeout=1.0)
        serial_thread.join(timeout=1.0)

    summary = {
        "join_ok": join_ok,
        "transport_key_ok": transport_key_ok,
        "announce_ok": announce_ok,
        "boot_report_ok": boot_report_ok,
        "timeout_ok": timeout_ok,
        "alive_joined": alive_joined,
        "sleep_cycle_logged": sleep_logged,
        "z2m_joined": z2m_joined,
        "z2m_interview_started": z2m_interview_started,
        "z2m_interview_success": z2m_interview_success,
        "z2m_interview_failed": z2m_interview_failed,
        "z2m_topic_seen": z2m_topic_seen,
        "ha_discovery_seen": ha_discovery_seen,
    }

    with open(summary_file, "w") as handle:
        for key, value in summary.items():
            handle.write(f"{key}={str(value).lower()}\n")

    print(summary_file)
    for key, value in summary.items():
        print(f"{key}={str(value).lower()}")


if __name__ == "__main__":
    main()
