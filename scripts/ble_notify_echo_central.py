#!/usr/bin/env python3
"""Host-side BLE central example for BleNotifyEchoPeripheral.

Install bleak first:
  python3 -m pip install bleak

Example:
  python3 scripts/ble_notify_echo_central.py --name X54-NOTIFY --write "hello from central"
"""

from __future__ import annotations

import argparse
import asyncio
import sys
from typing import Optional

from bleak import BleakClient, BleakScanner


SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
WRITE_CHAR_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
NOTIFY_CHAR_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"


def decode_payload(payload: bytearray) -> str:
    try:
        return payload.decode("utf-8")
    except UnicodeDecodeError:
        return payload.hex(" ")


async def find_device(name: Optional[str], address: Optional[str], timeout: float):
    if address:
        return await BleakScanner.find_device_by_address(address, timeout=timeout)

    if not name:
        raise ValueError("either --name or --address is required")

    def matcher(device, advertisement_data):
        return device.name == name or advertisement_data.local_name == name

    return await BleakScanner.find_device_by_filter(matcher, timeout=timeout)


async def run(args: argparse.Namespace) -> int:
    device = await find_device(args.name, args.address, args.scan_timeout)
    if device is None:
        target = args.address or args.name
        print(f"Could not find BLE peripheral: {target}", file=sys.stderr)
        return 1

    print(f"Connecting to {device.name or device.address} ({device.address})")
    first_notification = asyncio.Event()

    def handle_notification(_sender, data: bytearray):
        print(f"notify: {decode_payload(data)}")
        first_notification.set()

    async with BleakClient(device) as client:
        if hasattr(client, "get_services"):
            services = await client.get_services()
        else:
            services = client.services
        if services.get_service(SERVICE_UUID) is None:
            print(f"Service {SERVICE_UUID} not found", file=sys.stderr)
            return 1

        await client.start_notify(NOTIFY_CHAR_UUID, handle_notification)
        print(f"Subscribed to {NOTIFY_CHAR_UUID}")

        write_bytes = args.write.encode("utf-8")[:20]
        await client.write_gatt_char(WRITE_CHAR_UUID, write_bytes, response=True)
        print(f"Wrote to {WRITE_CHAR_UUID}: {write_bytes.decode('utf-8', errors='replace')}")

        try:
            await asyncio.wait_for(first_notification.wait(), timeout=args.first_notify_timeout)
        except asyncio.TimeoutError:
            print("No notification received before timeout", file=sys.stderr)

        deadline = asyncio.get_running_loop().time() + args.listen_seconds
        while asyncio.get_running_loop().time() < deadline:
            await asyncio.sleep(0.25)

        await client.stop_notify(NOTIFY_CHAR_UUID)

    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Connect to BleNotifyEchoPeripheral and print notifications."
    )
    parser.add_argument(
        "--name",
        default="X54-NOTIFY",
        help="advertising name to scan for (default: %(default)s)",
    )
    parser.add_argument(
        "--address",
        default=None,
        help="connect directly to a BLE address instead of scanning by name",
    )
    parser.add_argument(
        "--write",
        default="hello from central",
        help="text to write to characteristic 0xFFF1",
    )
    parser.add_argument(
        "--scan-timeout",
        type=float,
        default=10.0,
        help="seconds to wait while scanning",
    )
    parser.add_argument(
        "--first-notify-timeout",
        type=float,
        default=5.0,
        help="seconds to wait for the first notification after subscribing",
    )
    parser.add_argument(
        "--listen-seconds",
        type=float,
        default=12.0,
        help="seconds to stay connected after the initial write",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
