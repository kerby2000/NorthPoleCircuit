#!/usr/bin/env python3
"""Smoke test for the North Pole BLE diagnostic service.

Without --scan or --address this prints the planned BLE checks and exits.
With BLE access it requires bleak:

    python -m pip install bleak
"""

from __future__ import annotations

import argparse
import asyncio
import sys


def uuid16(value: int) -> str:
    return f"0000{value:04x}-0000-1000-8000-00805f9b34fb"


SERVICE_UUID = uuid16(0xFD90)
CHAR_VERSION = uuid16(0xFD91)
CHAR_BOARD = uuid16(0xFD92)
CHAR_STATUS = uuid16(0xFD93)
CHAR_COUNTERS = uuid16(0xFD94)
CHAR_CONTROL = uuid16(0xFD95)
CHAR_PROFILE = uuid16(0xFD96)

CONTROL_RGB_ALL = 0x01
CONTROL_CLEAR_FAULTS = 0x02


async def find_device(timeout: float):
    from bleak import BleakScanner

    devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
    for device, adv in devices.values():
        name = device.name or adv.local_name or ""
        services = {service.lower() for service in adv.service_uuids}
        if "NorthPole" in name or SERVICE_UUID in services:
            return device
    return None


def decode_text(data: bytes) -> str:
    return data.decode("utf-8", errors="replace").rstrip("\x00")


def le_u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def print_status_packet(data: bytes) -> None:
    if len(data) < 16:
        print(f"status: short packet {data.hex()}")
        return
    print(
        "status: "
        f"uptime_ms={le_u32(data, 0)} "
        f"faults=0x{le_u32(data, 4):08x} "
        f"battery=0x{data[8]:02x} "
        f"ip5209_present={data[9]} "
        f"ip5209_int={data[10]} "
        f"audio_hw={data[11]} "
        f"motor_armed={data[12]} "
        f"brightness={data[13]} "
        f"settings_valid={data[14]} "
        f"ble_state={data[15]}"
    )


def print_counters_packet(data: bytes) -> None:
    if len(data) < 12:
        print(f"counters: short packet {data.hex()}")
        return
    print(
        "counters: "
        f"hall1_edges={le_u32(data, 0)} "
        f"hall2_edges={le_u32(data, 4)} "
        f"hall1_level={data[8]} "
        f"hall2_level={data[9]} "
        f"touch_mask=0x{data[10]:02x} "
        f"touch_placeholder=0x{data[11]:02x}"
    )


async def run(args) -> int:
    try:
        from bleak import BleakClient
    except ImportError:
        print("bleak is required for BLE access: python -m pip install bleak", file=sys.stderr)
        return 2

    address = args.address
    if not address:
        device = await find_device(args.timeout)
        if not device:
            print("NorthPole BLE device not found", file=sys.stderr)
            return 1
        address = device.address
        print(f"found {device.name} {address}")

    async with BleakClient(address, timeout=args.timeout) as client:
        version = await client.read_gatt_char(CHAR_VERSION)
        board = await client.read_gatt_char(CHAR_BOARD)
        profile = await client.read_gatt_char(CHAR_PROFILE)
        status = await client.read_gatt_char(CHAR_STATUS)
        counters = await client.read_gatt_char(CHAR_COUNTERS)

        print(f"version: {decode_text(version)}")
        print(f"board: {decode_text(board)}")
        print(f"profile: {decode_text(profile)}")
        print_status_packet(bytes(status))
        print_counters_packet(bytes(counters))

        await client.write_gatt_char(CHAR_CONTROL, bytes([CONTROL_CLEAR_FAULTS]), response=True)
        print("wrote clear-faults command")

        if args.rgb:
            r, g, b = args.rgb
            await client.write_gatt_char(CHAR_CONTROL, bytes([CONTROL_RGB_ALL, r, g, b]), response=True)
            print(f"wrote RGB all command r={r} g={g} b={b}")

    return 0


def print_plan() -> None:
    print("No --scan or --address supplied. Planned BLE diagnostic checks:")
    print(f"scan for device name containing NorthPole or service {SERVICE_UUID}")
    print(f"read version {CHAR_VERSION}")
    print(f"read board revision {CHAR_BOARD}")
    print(f"read build profile {CHAR_PROFILE}")
    print(f"read status packet {CHAR_STATUS}")
    print(f"read counters packet {CHAR_COUNTERS}")
    print(f"write clear-faults command to {CHAR_CONTROL}")
    print("optional: write RGB test command; no BLE motor commands are used")


def main() -> int:
    parser = argparse.ArgumentParser(description="North Pole BLE diagnostic smoke test")
    parser.add_argument("--scan", action="store_true", help="Scan for a NorthPole BLE device")
    parser.add_argument("--address", help="Connect directly to a BLE address")
    parser.add_argument("--timeout", type=float, default=8.0, help="Scan/connect timeout in seconds")
    parser.add_argument("--rgb", nargs=3, type=int, metavar=("R", "G", "B"), help="Optional safe RGB all test")
    args = parser.parse_args()

    if args.rgb and any(value < 0 or value > 255 for value in args.rgb):
        parser.error("--rgb values must be 0..255")

    if not args.scan and not args.address:
        print_plan()
        return 0

    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
