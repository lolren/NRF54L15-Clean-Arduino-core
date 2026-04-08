#!/usr/bin/env python3
import argparse
import json
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
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


class HaClient:
    def __init__(self, base_url: str, username: str, password: str):
        self.base_url = base_url.rstrip("/")
        self.username = username
        self.password = password
        self._token = None

    def authenticate(self):
        redirect_uri = f"{self.base_url}/?auth_callback=1"
        req = urllib.request.Request(
            self.base_url + "/auth/login_flow",
            data=json.dumps(
                {
                    "client_id": self.base_url + "/",
                    "redirect_uri": redirect_uri,
                    "handler": ["homeassistant", None],
                }
            ).encode(),
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=20) as response:
            flow_id = json.load(response)["flow_id"]

        req = urllib.request.Request(
            self.base_url + f"/auth/login_flow/{flow_id}",
            data=json.dumps(
                {
                    "username": self.username,
                    "password": self.password,
                    "client_id": self.base_url + "/",
                }
            ).encode(),
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=20) as response:
            code = json.load(response)["result"]

        req = urllib.request.Request(
            self.base_url + "/auth/token",
            data=urllib.parse.urlencode(
                {
                    "grant_type": "authorization_code",
                    "code": code,
                    "client_id": self.base_url + "/",
                }
            ).encode(),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        with urllib.request.urlopen(req, timeout=20) as response:
            self._token = json.load(response)["access_token"]

    def _request(self, path: str, *, method="GET", data=None):
        if self._token is None:
            self.authenticate()
        body = None
        headers = {"Authorization": f"Bearer {self._token}"}
        if data is not None:
            body = json.dumps(data).encode()
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(
            self.base_url + path, data=body, headers=headers, method=method
        )
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                if response.status == 204:
                    return None
                return json.load(response)
        except urllib.error.HTTPError as error:
            payload = error.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HA {method} {path} failed: {error.code} {payload}")

    def call_service(self, domain: str, service: str, payload: dict):
        return self._request(
            f"/api/services/{domain}/{service}", method="POST", data=payload
        )

    def states(self):
        return self._request("/api/states")


def _find_matching_entities(states, target_ieee: str):
    target_ieee = target_ieee.lower()
    matches = []
    for state in states:
        entity_id = state.get("entity_id", "")
        attrs = state.get("attributes", {})
        blob = json.dumps({"entity_id": entity_id, "attributes": attrs}).lower()
        if target_ieee in blob or "x54-sleep-cl15" in blob:
            matches.append(state)
    return matches


def _summarize_entities(states):
    lines = []
    for state in states:
        entity_id = state.get("entity_id", "")
        status = state.get("state", "")
        attrs = state.get("attributes", {})
        friendly_name = attrs.get("friendly_name", "")
        lines.append(f"{entity_id} state={status} name={friendly_name}")
    return lines


def main():
    parser = argparse.ArgumentParser(
        description="Validate sleepy Zigbee climate sensor against Home Assistant."
    )
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--ha-url", default="http://192.168.1.100:8123")
    parser.add_argument("--ha-user", default="lolren")
    parser.add_argument("--ha-pass", default="lolren")
    parser.add_argument("--target-ieee", default="0xd0acf9feff59226e")
    parser.add_argument("--permit-join-entity", default="switch.zigbee2mqtt_bridge_permit_join")
    parser.add_argument("--timeout-s", type=int, default=150)
    parser.add_argument("--outdir", default="/home/lolren/Desktop/Nrf54L15/.build/zigbee_sleepy_ha_validation")
    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    serial_log = outdir / "serial.log"
    summary_file = outdir / "summary.txt"
    entities_before_file = outdir / "entities_before.txt"
    entities_after_file = outdir / "entities_after.txt"

    ha = HaClient(args.ha_url, args.ha_user, args.ha_pass)
    before_states = _find_matching_entities(ha.states(), args.target_ieee)
    entities_before_file.write_text("\n".join(_summarize_entities(before_states)) + "\n")

    # Remove the stale Zigbee2MQTT device entry if it exists.
    ha.call_service(
        "mqtt",
        "publish",
        {
            "topic": "zigbee2mqtt/bridge/request/device/remove",
            "payload": json.dumps({"id": args.target_ieee, "force": True}),
        },
    )
    time.sleep(2.0)

    # Open permit join on the bridge.
    ha.call_service("switch", "turn_on", {"entity_id": args.permit_join_entity})

    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    stop_flag = {"stop": False}
    serial_parts = []
    thread = threading.Thread(
        target=_pump_serial, args=(ser, serial_parts, serial_log, stop_flag), daemon=True
    )
    thread.start()

    try:
        time.sleep(3.0)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        serial_parts.clear()

        _send(ser, "c")
        clear_deadline = time.time() + 10.0
        while time.time() < clear_deadline:
            time.sleep(0.25)
            if "state cleared" in _joined(serial_parts):
                break

        time.sleep(1.0)
        serial_parts.clear()
        _send(ser, "j")

        join_ok = False
        transport_key_ok = False
        announce_ok = False
        boot_report_ok = False
        timeout_ok = False
        alive_joined = False
        sleep_logged = False
        ha_entities_promoted = False
        found_entities_after = []

        deadline = time.time() + args.timeout_s
        while time.time() < deadline:
            time.sleep(1.0)
            text = _joined(serial_parts)
            join_ok = join_ok or ("join OK ch=" in text)
            transport_key_ok = transport_key_ok or ("transport_key seq=" in text)
            announce_ok = announce_ok or ("device_announce OK" in text)
            boot_report_ok = boot_report_ok or ("boot_report temp=OK" in text and "hum=OK" in text)
            timeout_ok = timeout_ok or ("end_device_timeout_req OK" in text or "end_device_timeout status=0x0" in text)
            alive_joined = alive_joined or ("alive ch=" in text and "joined=yes" in text)
            sleep_logged = sleep_logged or ("sleep_cycle system_off_ms=" in text)

            states = ha.states()
            found_entities_after = _find_matching_entities(states, args.target_ieee)
            promoted = False
            for state in found_entities_after:
                if state.get("entity_id") == f"switch.{args.target_ieee}":
                    if state.get("state") != "unknown":
                        promoted = True
                elif state.get("entity_id", "").startswith(("sensor.", "binary_sensor.", "switch.")):
                    promoted = True
            ha_entities_promoted = ha_entities_promoted or promoted

            if (
                join_ok
                and transport_key_ok
                and announce_ok
                and boot_report_ok
                and timeout_ok
                and alive_joined
                and ha_entities_promoted
            ):
                break

        _send(ser, "s")
        time.sleep(1.0)
    finally:
        try:
            ha.call_service("switch", "turn_off", {"entity_id": args.permit_join_entity})
        except Exception:
            pass
        stop_flag["stop"] = True
        thread.join(timeout=1.0)
        ser.close()

    after_states = _find_matching_entities(ha.states(), args.target_ieee)
    entities_after_file.write_text("\n".join(_summarize_entities(after_states)) + "\n")

    serial_text = _joined(serial_parts)
    summary = {
        "join_ok": "join OK ch=" in serial_text,
        "transport_key_ok": "transport_key seq=" in serial_text,
        "announce_ok": "device_announce OK" in serial_text,
        "boot_report_ok": "boot_report temp=OK" in serial_text and "hum=OK" in serial_text,
        "timeout_ok": "end_device_timeout_req OK" in serial_text or "end_device_timeout status=0x0" in serial_text,
        "alive_joined": "alive ch=" in serial_text and "joined=yes" in serial_text,
        "sleep_cycle_logged": "sleep_cycle system_off_ms=" in serial_text,
        "ha_entities_found": len(after_states) > 0,
        "ha_entities_promoted": any(
            (
                state.get("entity_id") != f"switch.{args.target_ieee}"
                or state.get("state") != "unknown"
            )
            for state in after_states
        ),
    }

    with open(summary_file, "w") as handle:
        for key, value in summary.items():
            handle.write(f"{key}={str(value).lower()}\n")

    print(summary_file)
    for key, value in summary.items():
        print(f"{key}={str(value).lower()}")
    if after_states:
        print("entities_after:")
        for line in _summarize_entities(after_states):
            print(line)


if __name__ == "__main__":
    main()
