#!/usr/bin/env python3
"""Conservative WT2003 audio smoke test over the USB CDC shell."""

from __future__ import annotations

import argparse
import re
import sys
import time
from dataclasses import dataclass


AUDIO_QUEUE_RE = re.compile(r"busy=(?P<busy>\d+)\s+pending=(?P<pending>\d+)\s+queue=(?P<queue>\d+)")
AUDIO_LAST_RE = re.compile(
    r"last_command=(?P<command>\S+).*last_error=(?P<error>\S+).*result=0x(?P<result>[0-9a-fA-F]+)"
)
AUDIO_INFO_RE = re.compile(
    r"audio version=(?P<version>.*?)\s+volume=(?P<volume>\d+)\s+"
    r"playback_status=0x(?P<playback>[0-9a-fA-F]+)\s+"
    r"peripheral=0x(?P<peripheral>[0-9a-fA-F]+)\s+ext_count=(?P<ext_count>\d+)"
)
AUDIO_BUSY_RE = re.compile(r"audio busy=(?P<busy>\d+)")
AUDIO_STATUS_RE = re.compile(r"audio status=(?P<status>\S+)")


@dataclass
class AudioSnapshot:
    busy: int | None = None
    pending: int | None = None
    queue: int | None = None
    last_command: str | None = None
    last_error: str | None = None
    last_result: int | None = None
    version: str | None = None
    volume: int | None = None
    playback_status: int | None = None
    peripheral: int | None = None
    ext_count: int | None = None


def read_response(ser, timeout_s: float, quiet_s: float = 0.18) -> str:
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
            time.sleep(0.02)
    return data.decode("utf-8", errors="replace")


def parse_snapshot(text: str) -> AudioSnapshot:
    snapshot = AudioSnapshot()

    match = AUDIO_QUEUE_RE.search(text)
    if match:
        snapshot.busy = int(match.group("busy"))
        snapshot.pending = int(match.group("pending"))
        snapshot.queue = int(match.group("queue"))

    match = AUDIO_LAST_RE.search(text)
    if match:
        snapshot.last_command = match.group("command")
        snapshot.last_error = match.group("error")
        snapshot.last_result = int(match.group("result"), 16)

    match = AUDIO_INFO_RE.search(text)
    if match:
        snapshot.version = match.group("version")
        snapshot.volume = int(match.group("volume"))
        snapshot.playback_status = int(match.group("playback"), 16)
        snapshot.peripheral = int(match.group("peripheral"), 16)
        snapshot.ext_count = int(match.group("ext_count"))

    return snapshot


def parse_busy(text: str) -> int | None:
    match = AUDIO_BUSY_RE.search(text)
    if not match:
        return None
    return int(match.group("busy"))


def command_status(text: str) -> str | None:
    match = AUDIO_STATUS_RE.search(text)
    if not match:
        return None
    return match.group("status")


def send_command(ser, command: str, timeout_s: float, quiet_s: float = 0.18, verbose: bool = False) -> str:
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode("ascii"))
    ser.flush()
    response = read_response(ser, timeout_s=timeout_s, quiet_s=quiet_s)
    if verbose:
        print(f"\n>>> {command}")
        print(response.rstrip() if response.strip() else "<no response>")
    else:
        status = command_status(response)
        if status:
            print(f">>> {command}: {status}")
        elif command == "audio busy":
            busy = parse_busy(response)
            print(f">>> {command}: {busy if busy is not None else 'no response'}")
        elif response.strip():
            print(f">>> {command}: response")
        else:
            print(f">>> {command}: no response")
    return response


def wait_audio_idle(ser, timeout_s: float, command_timeout_s: float, verbose: bool) -> tuple[bool, AudioSnapshot]:
    deadline = time.monotonic() + timeout_s
    last = AudioSnapshot()
    while time.monotonic() < deadline:
        response = send_command(ser, "audio status", timeout_s=command_timeout_s, verbose=verbose)
        last = parse_snapshot(response)
        if last.pending == 0 and last.queue == 0:
            return True, last
        time.sleep(0.15)
    return False, last


def run_audio_command(ser, command: str, args) -> tuple[bool, AudioSnapshot]:
    response = send_command(ser, command, timeout_s=args.timeout, verbose=args.verbose)
    immediate_status = command_status(response)
    if immediate_status and immediate_status != "ok":
        return False, parse_snapshot(response)
    time.sleep(args.command_gap)
    idle, snapshot = wait_audio_idle(
        ser,
        timeout_s=args.queue_timeout,
        command_timeout_s=args.timeout,
        verbose=args.verbose,
    )
    if not idle:
        print(f"WARNING: audio queue did not become idle after {command!r}")
    if snapshot.last_error and snapshot.last_error != "ok":
        return False, snapshot
    return idle, snapshot


def main() -> int:
    parser = argparse.ArgumentParser(description="WT2003 audio smoke test through the North Pole USB CDC shell")
    parser.add_argument("--port", required=True, help="USB CDC serial port, for example COM19")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate accepted by pyserial")
    parser.add_argument("--timeout", type=float, default=1.5, help="Seconds to wait for each shell response")
    parser.add_argument("--queue-timeout", type=float, default=3.0, help="Seconds to wait for WT2003 command completion")
    parser.add_argument("--command-gap", type=float, default=0.25, help="Delay after commands that enqueue WT2003 work")
    parser.add_argument("--volume", type=int, default=5, help="Safe test volume, 0..31")
    parser.add_argument("--index", type=int, default=1, help="External flash file index to play")
    parser.add_argument("--play-seconds", type=float, default=3.0, help="Seconds to let audio play before stop")
    parser.add_argument("--output", choices=("none", "spk", "dac"), default="spk", help="Optional WT2003 output mode")
    parser.add_argument("--query-version", action="store_true", help="Also query WT2003 firmware version")
    parser.add_argument("--skip-play", action="store_true", help="Only query status/peripheral/count/volume")
    parser.add_argument("--quick-play", action="store_true", help="Skip preflight queries; set volume/output and play")
    parser.add_argument("--no-volume", action="store_true", help="Do not send audio volume before playback")
    parser.add_argument("--verbose", action="store_true", help="Print full shell responses for every command")
    args = parser.parse_args()

    if not args.no_volume and not 0 <= args.volume <= 31:
        print("--volume must be between 0 and 31", file=sys.stderr)
        return 2
    if not 1 <= args.index <= 65535:
        print("--index must be between 1 and 65535", file=sys.stderr)
        return 2

    try:
        import serial
    except ImportError:
        print("pyserial is required: python -m pip install pyserial", file=sys.stderr)
        return 2

    print("WT2003 audio smoke test")
    print("Make sure the WT2003 USB storage drive is not mounted while testing UART playback.")
    print("The script uses low volume and sends audio stop at the end.")

    failures: list[str] = []
    busy_seen = False
    final_busy: int | None = None
    latest = AudioSnapshot()

    try:
        with serial.Serial(args.port, baudrate=args.baud, timeout=0.05, write_timeout=1.0) as ser:
            time.sleep(0.5)
            if not args.quick_play:
                send_command(ser, "version", timeout_s=args.timeout, verbose=args.verbose)
            initial = send_command(ser, "audio status", timeout_s=args.timeout, verbose=args.verbose)
            latest = parse_snapshot(initial)

            if args.output != "none":
                ok, latest = run_audio_command(ser, f"audio output {args.output}", args)
                if not ok:
                    failures.append(f"audio output {args.output}")

            if not args.no_volume:
                ok, latest = run_audio_command(ser, f"audio volume {args.volume}", args)
                if not ok:
                    failures.append("audio volume")

            if not args.quick_play:
                for command in ("audio qperiph", "audio qcount-ext", "audio qvol", "audio qstatus"):
                    ok, latest = run_audio_command(ser, command, args)
                    if not ok:
                        failures.append(command)

            if args.query_version and not args.quick_play:
                ok, latest = run_audio_command(ser, "audio version", args)
                if not ok:
                    failures.append("audio version")

            if not args.skip_play:
                ok, latest = run_audio_command(ser, f"audio play-index {args.index}", args)
                if not ok:
                    failures.append("audio play-index")

                play_deadline = time.monotonic() + args.play_seconds
                while time.monotonic() < play_deadline:
                    response = send_command(
                        ser,
                        "audio busy",
                        timeout_s=args.timeout,
                        quiet_s=0.08,
                        verbose=args.verbose,
                    )
                    busy = parse_busy(response)
                    if busy is not None:
                        busy_seen = busy_seen or bool(busy)
                        final_busy = busy
                    time.sleep(0.25)

                ok, latest = run_audio_command(ser, "audio stop", args)
                if not ok:
                    failures.append("audio stop")

                stop_deadline = time.monotonic() + 2.0
                while time.monotonic() < stop_deadline:
                    response = send_command(
                        ser,
                        "audio busy",
                        timeout_s=args.timeout,
                        quiet_s=0.08,
                        verbose=args.verbose,
                    )
                    final_busy = parse_busy(response)
                    if final_busy == 0:
                        break
                    time.sleep(0.2)

            final_status = send_command(ser, "audio status", timeout_s=args.timeout, verbose=args.verbose)
            latest = parse_snapshot(final_status)
    except serial.SerialException as exc:
        print(f"serial error: {exc}", file=sys.stderr)
        return 1

    print("\nSummary")
    print(f"  failures={failures if failures else 'none'}")
    print(f"  peripheral=0x{latest.peripheral:02x}" if latest.peripheral is not None else "  peripheral=unknown")
    print(f"  ext_count={latest.ext_count}" if latest.ext_count is not None else "  ext_count=unknown")
    print(f"  volume={latest.volume}" if latest.volume is not None else "  volume=unknown")
    print(f"  last_error={latest.last_error or 'unknown'}")
    if not args.skip_play:
        print(f"  busy_seen_during_play={int(busy_seen)}")
        print(f"  busy_after_stop={final_busy if final_busy is not None else 'unknown'}")

    if not args.quick_play and latest.ext_count == 0:
        failures.append("external flash file count is zero")
    if not args.skip_play and not busy_seen:
        failures.append("BUSY never went high during playback window")
    if not args.skip_play and final_busy not in (0, None):
        failures.append("BUSY stayed high after stop")

    if failures:
        print("\nA UART/control problem is still possible. If BUSY toggles but audio is faint, focus on speaker/output wiring.")
        return 1

    print("\nPASS: WT2003 UART/control path responded. Listen locally to judge speaker/acoustic output.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
