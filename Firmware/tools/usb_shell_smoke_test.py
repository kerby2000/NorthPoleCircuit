#!/usr/bin/env python3
"""Smoke test for the North Pole USB CDC diagnostic shell.

Without --port this prints the command plan and exits. With --port it requires
pyserial and sends the current pre-hardware command set to the virtual COM port.
"""

from __future__ import annotations

import argparse
import sys
import time


COMMANDS = [
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


def run(port: str, baud: int, timeout_s: float) -> int:
    try:
        import serial
    except ImportError:
        print("pyserial is required when --port is used: python -m pip install pyserial", file=sys.stderr)
        return 2

    failures = []
    with serial.Serial(port, baudrate=baud, timeout=0.05, write_timeout=1.0) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        for command in COMMANDS:
            ser.write((command + "\r\n").encode("ascii"))
            ser.flush()
            response = read_response(ser, timeout_s)
            print(f"\n>>> {command}")
            print(response.rstrip() if response else "<no response>")
            if command in {"version", "status"} and not response.strip():
                failures.append(command)

    if failures:
        print(f"USB CDC smoke test failed: no response for {', '.join(failures)}", file=sys.stderr)
        return 1
    print("USB CDC smoke test complete")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="North Pole USB CDC shell smoke test")
    parser.add_argument("--port", help="COM port or tty device, for example COM7 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="Ignored by USB CDC but accepted by pyserial")
    parser.add_argument("--timeout", type=float, default=1.5, help="Seconds to wait for each response")
    args = parser.parse_args()

    if not args.port:
        print("No --port supplied. Planned USB CDC shell commands:")
        for command in COMMANDS:
            print(command)
        return 0

    return run(args.port, args.baud, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
