#!/usr/bin/env python3
"""Smoke test for the North Pole USB CDC diagnostic shell.

Without --port this prints the command plan and exits. With --port it requires
pyserial and sends the current pre-hardware command set to the virtual COM port.
"""

from __future__ import annotations

import argparse
import asyncio
import sys
import time


TARGET_COMMANDS = [
    "version",
    "status",
    "pins verify",
    "safe check",
    "faults",
    "settings show",
    "rgb off",
    "motor off",
    "audio status",
    "ip5209 status",
]

DEV_BOARD_COMMANDS = [
    "help",
    "version",
    "status",
    "faults",
    "settings show",
    "audio status",
    "pins verify",
    "safe check",
    "motor status",
]


def read_response(ser, timeout_s: float) -> str:
    deadline = time.monotonic() + timeout_s
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            data.extend(chunk)
            deadline = time.monotonic() + 0.25
        else:
            time.sleep(0.02)
    return data.decode("utf-8", errors="replace")


def port_present(port: str) -> bool:
    try:
        import serial.tools.list_ports
    except ImportError:
        return False

    target = port.upper()
    return any(item.device.upper() == target for item in serial.tools.list_ports.comports())


def reset_recovery_check(port: str, baud: int, timeout_s: float) -> int:
    try:
        import serial
    except ImportError:
        print("pyserial is required for --reset-recovery", file=sys.stderr)
        return 2

    print("\n>>> reset")
    try:
        with serial.Serial(port, baudrate=baud, timeout=0.05, write_timeout=1.0) as ser:
            time.sleep(0.3)
            ser.reset_input_buffer()
            ser.write(b"reset\r\n")
            ser.flush()
            try:
                response = read_response(ser, 1.0)
                if response.strip():
                    print(response.rstrip())
            except serial.SerialException as exc:
                print(f"serial disconnected during reset: {exc}")
    except serial.SerialException as exc:
        print(f"reset open/write failed: {exc}", file=sys.stderr)
        return 1

    saw_disconnect = False
    start = time.monotonic()
    while time.monotonic() - start < 20.0:
        present = port_present(port)
        if not present:
            saw_disconnect = True
        if present and (saw_disconnect or time.monotonic() - start > 3.0):
            break
        time.sleep(0.25)

    if not port_present(port):
        print(f"{port} did not reappear after reset", file=sys.stderr)
        return 1
    if not saw_disconnect:
        print(f"{port} stayed present; reset disconnect was not observed", file=sys.stderr)
        return 1

    time.sleep(1.0)
    with serial.Serial(port, baudrate=baud, timeout=0.05, write_timeout=1.0) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.write(b"version\r\n")
        ser.flush()
        response = read_response(ser, timeout_s)
        print("\n>>> version after reset")
        print(response.rstrip() if response else "<no response>")
        if "version=" not in response:
            return 1
    return 0


async def scan_ble_name(expected_name: str, timeout_s: float) -> int:
    try:
        from bleak import BleakScanner
    except ImportError:
        print("bleak is required for --ble-name: python -m pip install bleak", file=sys.stderr)
        return 2

    devices = await BleakScanner.discover(timeout=timeout_s)
    matches = []
    for device in devices:
        name = device.name or ""
        if expected_name in name:
            matches.append((device.address, name, getattr(device, "rssi", None)))

    print(f"\n>>> BLE scan for {expected_name!r}")
    for address, name, rssi in matches:
        print(f"{address} name={name} rssi={rssi}")
    print(f"scan_total={len(devices)} matches={len(matches)}")
    return 0 if matches else 1


def run(port: str, baud: int, timeout_s: float, commands: list[str]) -> int:
    try:
        import serial
    except ImportError:
        print("pyserial is required when --port is used: python -m pip install pyserial", file=sys.stderr)
        return 2

    failures = []
    with serial.Serial(port, baudrate=baud, timeout=0.05, write_timeout=1.0) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        for command in commands:
            ser.write((command + "\r\n").encode("ascii"))
            ser.flush()
            response = read_response(ser, timeout_s)
            print(f"\n>>> {command}")
            print(response.rstrip() if response else "<no response>")
            if not response.strip():
                failures.append(command)

    if failures:
        print(f"USB CDC smoke test failed: no response for {', '.join(failures)}", file=sys.stderr)
        return 1
    print("USB CDC smoke test complete")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="North Pole USB CDC shell smoke test")
    parser.add_argument("positional_port", nargs="?", help="Optional COM port, for example COM19")
    parser.add_argument("--port", help="COM port or tty device, for example COM7 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="Ignored by USB CDC but accepted by pyserial")
    parser.add_argument("--timeout", type=float, default=2.0, help="Seconds to wait for each response")
    parser.add_argument("--reset-recovery", action="store_true", help="Send reset and verify the CDC port returns")
    parser.add_argument("--ble-name", help="Optional BLE advertisement name to scan for after CDC checks")
    parser.add_argument("--ble-timeout", type=float, default=6.0, help="Seconds to scan for --ble-name")
    parser.add_argument(
        "--profile",
        choices=("target", "dev-board"),
        default="target",
        help="Use target-board commands or CH592 dev-board-safe commands",
    )
    args = parser.parse_args()

    commands = DEV_BOARD_COMMANDS if args.profile == "dev-board" else TARGET_COMMANDS
    port = args.port or args.positional_port
    if not port:
        print("No --port supplied. Planned USB CDC shell commands:")
        for command in commands:
            print(command)
        return 0

    rc = run(port, args.baud, args.timeout, commands)
    if rc != 0:
        return rc

    if args.reset_recovery:
        rc = reset_recovery_check(port, args.baud, args.timeout)
        if rc != 0:
            return rc

    if args.ble_name:
        rc = asyncio.run(scan_ble_name(args.ble_name, args.ble_timeout))
        if rc != 0:
            return rc

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
