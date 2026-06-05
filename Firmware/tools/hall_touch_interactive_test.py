#!/usr/bin/env python3
"""Interactive Hall and touch-pad polling over the USB CDC shell."""

from __future__ import annotations

import argparse
import re
import sys
import time
from dataclasses import dataclass


HALL_RE = re.compile(r"hall(?P<idx>\d+)\s+level=(?P<level>\d+)\s+edges=(?P<edges>\d+)\s+last_ms=(?P<last_ms>\d+)")
TOUCH_RE = re.compile(
    r"touch\s+(?P<name>\S+)\s+raw=(?P<raw>\d+)\s+baseline=(?P<baseline>\d+)\s+"
    r"threshold=(?P<threshold>\d+)\s+pressed=(?P<pressed>\d+)"
)


@dataclass(frozen=True)
class HallState:
    level: int
    edges: int
    last_ms: int


@dataclass(frozen=True)
class TouchState:
    raw: int
    baseline: int
    threshold: int
    pressed: int


def read_response(ser, timeout_s: float, quiet_s: float = 0.08) -> str:
    deadline = time.monotonic() + timeout_s
    quiet_deadline = None
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            data.extend(chunk)
            quiet_deadline = time.monotonic() + quiet_s
        elif quiet_deadline is not None and time.monotonic() >= quiet_deadline:
            break
        else:
            time.sleep(0.01)
    return data.decode("utf-8", errors="replace")


def send_command(ser, command: str, timeout_s: float) -> str:
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode("ascii"))
    ser.flush()
    return read_response(ser, timeout_s=timeout_s)


def parse_hall(text: str) -> dict[str, HallState]:
    states: dict[str, HallState] = {}
    for match in HALL_RE.finditer(text):
        key = f"hall{match.group('idx')}"
        states[key] = HallState(
            level=int(match.group("level")),
            edges=int(match.group("edges")),
            last_ms=int(match.group("last_ms")),
        )
    return states


def parse_touch(text: str) -> dict[str, TouchState]:
    states: dict[str, TouchState] = {}
    for match in TOUCH_RE.finditer(text):
        states[match.group("name")] = TouchState(
            raw=int(match.group("raw")),
            baseline=int(match.group("baseline")),
            threshold=int(match.group("threshold")),
            pressed=int(match.group("pressed")),
        )
    return states


def now_label() -> str:
    return time.strftime("%H:%M:%S")


def format_hall(states: dict[str, HallState]) -> str:
    return " ".join(f"{name}:level={state.level},edges={state.edges}" for name, state in sorted(states.items()))


def format_touch(states: dict[str, TouchState]) -> str:
    return " ".join(
        f"{name}:raw={state.raw},pressed={state.pressed}" for name, state in sorted(states.items())
    )


def print_state_changes(
    label: str,
    current: dict[str, HallState] | dict[str, TouchState],
    previous: dict[str, HallState] | dict[str, TouchState] | None,
) -> bool:
    changed = False
    if previous is None:
        for name, state in sorted(current.items()):
            print(f"{now_label()} initial {label} {name}: {state}")
        return bool(current)

    for name, state in sorted(current.items()):
        old = previous.get(name)
        if old != state:
            print(f"{now_label()} {label} {name}: {old} -> {state}")
            changed = True
    return changed


def all_touch_zero(states: dict[str, TouchState]) -> bool:
    return bool(states) and all(
        state.raw == 0 and state.baseline == 0 and state.threshold == 0 and state.pressed == 0
        for state in states.values()
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Print Hall/touch changes while physically actuating the board")
    parser.add_argument("--port", required=True, help="USB CDC serial port, for example COM19")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate accepted by pyserial")
    parser.add_argument("--timeout", type=float, default=0.35, help="Seconds to wait for each shell response")
    parser.add_argument("--interval", type=float, default=0.15, help="Polling interval in seconds")
    parser.add_argument("--print-unchanged-every", type=float, default=2.0, help="Periodic unchanged summary interval")
    parser.add_argument("--no-hall", action="store_true", help="Do not poll hall read")
    parser.add_argument("--no-touch", action="store_true", help="Do not poll touch raw")
    parser.add_argument("--once", action="store_true", help="Read once and exit")
    args = parser.parse_args()

    if args.no_hall and args.no_touch:
        print("Nothing to poll: remove --no-hall or --no-touch", file=sys.stderr)
        return 2

    try:
        import serial
    except ImportError:
        print("pyserial is required: python -m pip install pyserial", file=sys.stderr)
        return 2

    print("Interactive Hall/touch test")
    print("Move a magnet near Hall sensors and touch pads. Only changed states are printed.")
    print("Press Ctrl+C to stop.\n")

    prev_hall: dict[str, HallState] | None = None
    prev_touch: dict[str, TouchState] | None = None
    last_summary = time.monotonic()
    touch_zero_warning = False
    start = time.monotonic()

    try:
        with serial.Serial(args.port, baudrate=args.baud, timeout=0.05, write_timeout=1.0) as ser:
            time.sleep(0.4)
            ser.reset_input_buffer()
            while True:
                loop_start = time.monotonic()
                changed = False

                if not args.no_hall:
                    response = send_command(ser, "hall read", timeout_s=args.timeout)
                    hall_states = parse_hall(response)
                    if hall_states:
                        changed |= print_state_changes("hall", hall_states, prev_hall)
                        prev_hall = hall_states
                    elif response.strip():
                        print(f"{now_label()} hall raw response: {response.rstrip()}")
                    else:
                        print(f"{now_label()} hall: <no response>")

                if not args.no_touch:
                    response = send_command(ser, "touch raw", timeout_s=args.timeout)
                    touch_states = parse_touch(response)
                    if touch_states:
                        changed |= print_state_changes("touch", touch_states, prev_touch)
                        prev_touch = touch_states
                        if (
                            not touch_zero_warning
                            and time.monotonic() - start > 5.0
                            and all_touch_zero(touch_states)
                        ):
                            print(
                                f"{now_label()} warning: touch values are still all zero; "
                                "touch sensing may still need firmware/backend tuning."
                            )
                            touch_zero_warning = True
                    elif response.strip():
                        print(f"{now_label()} touch raw response: {response.rstrip()}")
                    else:
                        print(f"{now_label()} touch: <no response>")

                if args.once:
                    return 0

                if time.monotonic() - last_summary >= args.print_unchanged_every:
                    if not changed:
                        parts = []
                        if prev_hall:
                            parts.append("hall " + format_hall(prev_hall))
                        if prev_touch:
                            parts.append("touch " + format_touch(prev_touch))
                        print(f"{now_label()} unchanged: {' | '.join(parts) if parts else '<no parsed state>'}")
                    last_summary = time.monotonic()

                elapsed = time.monotonic() - loop_start
                time.sleep(max(0.0, args.interval - elapsed))
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except serial.SerialException as exc:
        print(f"serial error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
