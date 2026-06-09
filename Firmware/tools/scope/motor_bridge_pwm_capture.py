#!/usr/bin/env python3
"""Scope capture helper for DRV8837 bridge input PWM bring-up."""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from scope_driver_adapter import ChannelConfig, OwonScopeAdapter, ScopeAdapterError, ScopeConnection


DEFAULT_OUT_DIR = Path("Firmware/docs/motor_pwm_scope_evidence")
MOTOR_RC_RE = re.compile(r"motor\s+(?P<driver>[ABG])\s+mode=(?P<mode>\S+)\s+duty=(?P<duty>\d+).*rc=(?P<rc>-?\d+)")
SINE_LIKE_MODES = (
    "sine-demo",
    "sine-phase",
    "sine-diag-inputs",
    "sine-pwm-inputs",
    "sine-scope-plot",
    "sine-scope-plot-us",
    "sine-scope-run-us",
    "sine-scope-clkdiv",
)
WAVE_MODES = ("wave-clkdiv", "wave-dma-a", "wave-dma-hybrid")
WAVEFORM_SEQUENCE_MODES = SINE_LIKE_MODES + WAVE_MODES

# DOS1104/HANMATEK accepted horizontal scales are discrete.  The
# InstrumentKit driver rejects intermediate values such as 6.4 ms/div, so
# quantize auto-selected and user-provided values before configuring the scope.
SUPPORTED_TIMEBASE_S_DIV = (
    1e-9, 2e-9, 5e-9,
    10e-9, 20e-9, 50e-9,
    100e-9, 200e-9, 500e-9,
    1e-6, 2e-6, 5e-6,
    10e-6, 20e-6, 50e-6,
    100e-6, 200e-6, 500e-6,
    1e-3, 2e-3, 5e-3,
    10e-3, 20e-3, 50e-3,
    100e-3, 200e-3, 500e-3,
    1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0,
)


@dataclass
class WaveSummary:
    channel: int
    label: str
    count: int
    min_v: float | None
    max_v: float | None
    mean_v: float | None
    peak_to_peak_v: float | None
    rising_edges: int
    estimated_frequency_hz: float | None
    estimated_duty_percent: float | None


def read_response(ser, timeout_s: float, quiet_s: float = 0.12) -> str:
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


def send_shell_command(ser, command: str, timeout_s: float, verbose: bool) -> str:
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode("ascii"))
    ser.flush()
    response = read_response(ser, timeout_s=timeout_s)
    if verbose:
        print(f"\n>>> {command}")
        print(response.rstrip() if response.strip() else "<no response>")
    else:
        rc_match = MOTOR_RC_RE.search(response)
        if rc_match:
            rc = int(rc_match.group("rc"))
            suffix = "ok" if rc == 0 else f"failed rc={rc}"
            print(
                f">>> {command}: {suffix} "
                f"mode={rc_match.group('mode')} duty={rc_match.group('duty')}"
            )
        else:
            print(f">>> {command}: {'response' if response.strip() else 'no response'}")
    return response


def shell_response_has_command_error(response: str) -> bool:
    normalized = response.lower()
    error_markers = (
        "bad motor command",
        "bad motor sine",
        "bad motor pwm",
        "bad motor diag",
        "bad motor wave",
        "unknown command",
    )
    return any(marker in normalized for marker in error_markers)


def raise_on_shell_command_error(command: str, response: str) -> None:
    if shell_response_has_command_error(response):
        raise RuntimeError(
            f"target rejected shell command {command!r}; response was: {response.strip()!r}. "
            "Flash the latest bring-up HEX before running this capture mode."
        )


def scope_status_hit_trigger(status: str | None) -> bool:
    text = "" if status is None else str(status).strip().lower()
    return text.startswith("trig") or text.startswith("stop") or text.endswith("trig") or text.endswith("stop")


def wait_for_scope_trigger(scope: OwonScopeAdapter, timeout_s: float, verbose: bool) -> str | None:
    deadline = time.monotonic() + max(0.0, float(timeout_s))
    last_status: str | None = None
    while time.monotonic() < deadline:
        last_status = scope.trigger_status()
        if verbose:
            print(f"scope trigger status: {last_status}")
        if scope_status_hit_trigger(last_status):
            return last_status
        time.sleep(0.02)
    return last_status


def open_serial(args):
    if not args.port:
        return None
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("pyserial is required when --port is used: python -m pip install pyserial") from exc
    return serial.Serial(args.port, args.baud, timeout=0.05)


def configure_scope(scope: OwonScopeAdapter, args) -> None:
    active_channels = channel_labels(args)
    if args.scope_acquire_mode:
        scope.set_acquire_mode(args.scope_acquire_mode)
    if args.scope_memory_depth:
        scope.set_memory_depth(args.scope_memory_depth)
    positions = {
        1: args.ch1_position,
        2: args.ch2_position,
        3: args.ch3_position,
        4: args.ch4_position,
    }
    scales = {
        1: args.ch1_scale,
        2: args.ch2_scale,
        3: args.ch3_scale,
        4: args.ch4_scale,
    }
    for channel, _label in active_channels:
        config = ChannelConfig(
            channel=channel,
            enabled=True,
            scale_v_div=scales[channel],
            position_div=positions[channel],
            probe_attenuation=args.probe,
            coupling="DC",
        )
        scope.setup_channel(config)
    for channel in (1, 2, 3, 4):
        if channel not in {item[0] for item in active_channels}:
            scope.setup_channel(ChannelConfig(channel=channel, enabled=False, scale_v_div=1.0))

    scope.set_timebase_scale(args.timebase)
    if args.scope_horizontal_offset_div is not None:
        scope.set_horizontal_offset(args.scope_horizontal_offset_div)
    scope.configure_edge_trigger(
        source_channel=args.trigger_channel,
        slope=args.trigger_slope,
        level_v=args.trigger_level,
        sweep=args.trigger_sweep,
    )
    if args.trigger_holdoff_ns is not None:
        scope.set_trigger_holdoff_ns(args.trigger_holdoff_ns)


def command_for_direction(driver: str, direction: str, duty: int, duration_ms: int) -> str:
    return f"motor pwm {driver} {direction} {duty} {duration_ms}"


def command_for_diag(driver: str, direction: str, duration_ms: int) -> str:
    return f"motor diag-inputs {driver} {direction} {duration_ms}"


def command_for_sine_demo(args) -> str:
    return f"motor sine-demo {args.speed_hz} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_sine_phase(args) -> str:
    return f"motor sine-phase {args.phase} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_sine_diag_inputs(args) -> str:
    return f"motor sine-diag-inputs {args.speed_hz} {args.duration_ms} {args.sine_target}"


def command_for_sine_pwm_inputs(args) -> str:
    return f"motor sine-pwm-inputs {args.speed_hz} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_sine_scope_plot(args) -> str:
    return f"motor sine-scope-plot {args.speed_hz} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_sine_scope_plot_us(args) -> str:
    return f"motor sine-scope-plot-us {args.slot_us} {args.amplitude_permille} {args.steps} {args.sine_target}"


def effective_slot_us(args) -> int:
    if args.mode in ("sine-scope-clkdiv", "wave-clkdiv", "wave-dma-a", "wave-dma-hybrid"):
        return max(1, int(args.fast_clkdiv))
    return max(1, int(args.slot_us))


def command_for_sine_scope_run_us(args) -> str:
    return f"motor sine-scope-run-us {effective_slot_us(args)} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_sine_scope_clkdiv(args) -> str:
    return f"motor sine-scope-clkdiv {args.fast_clkdiv} {args.amplitude_permille} {args.duration_ms} {args.sine_target}"


def command_for_wave_clkdiv(args) -> str:
    return f"motor wave-clkdiv {effective_slot_us(args)} {args.amplitude_permille} {args.sine_target} {args.wave_sleep}"


def command_for_wave_dma_a(args) -> str:
    return f"motor wave-dma-a {effective_slot_us(args)} {args.amplitude_permille} {args.wave_sleep}"


def command_for_wave_dma_hybrid(args) -> str:
    return f"motor wave-dma-hybrid {effective_slot_us(args)} {args.amplitude_permille} {args.sine_target} {args.wave_sleep}"


def command_for_sine_like_mode(args) -> str:
    if args.mode == "wave-dma-hybrid":
        return command_for_wave_dma_hybrid(args)
    if args.mode == "wave-dma-a":
        return command_for_wave_dma_a(args)
    if args.mode == "wave-clkdiv":
        return command_for_wave_clkdiv(args)
    if args.mode == "sine-demo":
        return command_for_sine_demo(args)
    if args.mode == "sine-phase":
        return command_for_sine_phase(args)
    if args.mode == "sine-pwm-inputs":
        return command_for_sine_pwm_inputs(args)
    if args.mode == "sine-scope-plot-us":
        return command_for_sine_scope_plot_us(args)
    if args.mode == "sine-scope-run-us":
        return command_for_sine_scope_run_us(args)
    if args.mode == "sine-scope-clkdiv":
        return command_for_sine_scope_clkdiv(args)
    if args.mode == "sine-scope-plot":
        return command_for_sine_scope_plot(args)
    return command_for_sine_diag_inputs(args)


def active_duration_s_for_args(args) -> float:
    if args.mode == "sine-scope-plot-us":
        return (args.slot_us * args.steps) / 1_000_000.0
    if args.mode in ("sine-scope-run-us", "sine-scope-clkdiv", "wave-clkdiv", "wave-dma-a", "wave-dma-hybrid"):
        return args.duration_ms / 1000.0
    if args.mode in ("sine-demo", "sine-phase", "sine-diag-inputs", "sine-pwm-inputs", "sine-scope-plot"):
        return args.duration_ms / 1000.0
    if args.mode.startswith("diag-"):
        return args.duration_ms / 1000.0
    return args.duration_ms / 1000.0


def quantize_timebase_scale(seconds_per_div: float) -> float:
    requested = max(0.0, float(seconds_per_div))
    for supported in SUPPORTED_TIMEBASE_S_DIV:
        if requested <= supported or math.isclose(requested, supported, rel_tol=1e-9, abs_tol=supported * 1e-9):
            return supported
    return SUPPORTED_TIMEBASE_S_DIV[-1]


def bridge_labels(driver: str) -> tuple[str, str]:
    return (
        f"{driver}2 / /PWM_{driver}2",
        f"{driver}1 / /PWM_{driver}1",
    )


def parse_channel_list(value: str | None) -> list[int]:
    text = "" if value is None else str(value).strip()
    if not text:
        return []
    channels: list[int] = []
    for token in text.replace(";", ",").split(","):
        token = token.strip().lower()
        if not token:
            continue
        if token.startswith("ch"):
            token = token[2:]
        channel = int(token)
        if channel < 1 or channel > 4:
            raise ValueError(f"scope channel out of range: {channel}")
        if channel not in channels:
            channels.append(channel)
    return channels


def channel_labels(args) -> list[tuple[int, str]]:
    if args.channel_map == "ab-reference":
        labels = [
            (1, "A1 / /PWM_A1"),
            (2, "A2 / /PWM_A2"),
            (3, "B1 / /PWM_B1"),
            (4, "B2 / /PWM_B2"),
        ]
    elif args.channel_map == "ab-physical":
        labels = [
            (1, "B2 / /PWM_B2"),
            (2, "A2 / /PWM_A2"),
            (3, "B1 / /PWM_B1"),
            (4, "A1 / /PWM_A1"),
        ]
    else:
        ch1_label, ch2_label = bridge_labels(args.driver)
        labels = [(1, ch1_label), (2, ch2_label)]

    selected_channels = getattr(args, "display_channels", None)
    if selected_channels:
        selected = set(selected_channels)
        labels = [item for item in labels if item[0] in selected]
    return labels


def default_sine_phase_trigger_channel(args) -> int | None:
    if args.mode in ("sine-scope-plot", "sine-scope-plot-us") and args.sine_target in ("AB", "all"):
        return 3 if args.channel_map in ("ab-physical", "ab-reference") else None
    if args.mode in ("sine-scope-run-us", "sine-scope-clkdiv", "wave-clkdiv", "wave-dma-hybrid") and args.sine_target in ("AB", "all"):
        return 3 if args.channel_map in ("ab-physical", "ab-reference") else None
    if args.mode != "sine-phase" or args.sine_target not in ("AB", "all"):
        return None
    phase = args.phase & 31
    if phase == 8:
        # A positive max: A1 active.
        return 4 if args.channel_map == "ab-physical" else 1
    if phase == 16:
        # A negative max: A2 active.
        return 2
    if phase == 0:
        # B positive max: B1 active.
        return 3 if args.channel_map in ("ab-physical", "ab-reference") else None
    if phase == 24:
        # B negative max: B2 active.
        return 1 if args.channel_map == "ab-physical" else 4
    return None


def run_motor_sequence(ser, args) -> list[str]:
    if ser is None:
        return []

    responses: list[str] = []
    responses.append(send_shell_command(ser, "motor status", args.command_timeout, args.verbose_shell))
    if args.mode in WAVEFORM_SEQUENCE_MODES:
        command = command_for_sine_like_mode(args)
        command_response = send_shell_command(ser, command, args.command_timeout, args.verbose_shell)
        raise_on_shell_command_error(command, command_response)
        responses.append(command_response)
        active_s = active_duration_s_for_args(args)
        time.sleep(active_s + args.post_command_wait)
        responses.append(read_response(ser, timeout_s=args.command_timeout))
        if args.mode in WAVE_MODES:
            responses.append(send_shell_command(ser, "motor wave-stop", args.command_timeout, args.verbose_shell))
        return responses

    if args.mode.startswith("diag-"):
        direction = args.mode[len("diag-"):]
        responses.append(
            send_shell_command(
                ser,
                command_for_diag(args.driver, direction, args.duration_ms),
                args.command_timeout,
                args.verbose_shell,
            )
        )
        time.sleep((args.duration_ms / 1000.0) + args.post_command_wait)
        responses.append(read_response(ser, timeout_s=args.command_timeout))
        return responses

    responses.append(send_shell_command(ser, f"motor arm {args.arm_seconds}", args.command_timeout, args.verbose_shell))
    time.sleep(args.command_gap)

    if args.mode == "sequence-forward-reverse":
        commands = [
            command_for_direction(args.driver, "forward", args.duty_permille, args.duration_ms),
            command_for_direction(args.driver, "reverse", args.duty_permille, args.duration_ms),
        ]
        for command in commands:
            responses.append(send_shell_command(ser, command, args.command_timeout, args.verbose_shell))
            time.sleep((args.duration_ms / 1000.0) + args.sequence_gap)
    else:
        direction = "reverse" if args.mode == "single-reverse" else "forward"
        responses.append(
            send_shell_command(
                ser,
                command_for_direction(args.driver, direction, args.duty_permille, args.duration_ms),
                args.command_timeout,
                args.verbose_shell,
            )
        )
        time.sleep((args.duration_ms / 1000.0) + args.post_command_wait)

    responses.append(send_shell_command(ser, "motor off", args.command_timeout, args.verbose_shell))
    return responses


def run_motor_sequence_run_stop(ser, args, scope: OwonScopeAdapter) -> list[str]:
    """Start the visible waveform, stop the scope while it is active, then clean up."""
    if ser is None:
        time.sleep(args.stop_during_active_s)
        scope.stop()
        return []

    responses: list[str] = []
    responses.append(send_shell_command(ser, "motor status", args.command_timeout, args.verbose_shell))

    if args.mode in WAVEFORM_SEQUENCE_MODES:
        command = command_for_sine_like_mode(args)
        command_response = send_shell_command(ser, command, args.command_timeout, args.verbose_shell)
        raise_on_shell_command_error(command, command_response)
        responses.append(command_response)
        time.sleep(args.stop_during_active_s)
        scope.stop()
        active_s = active_duration_s_for_args(args)
        remaining = max(0.0, active_s - args.stop_during_active_s)
        time.sleep(remaining + args.post_command_wait)
        responses.append(read_response(ser, timeout_s=args.command_timeout))
        if args.pwm_debug:
            responses.append(send_shell_command(ser, "motor pwm-debug", args.command_timeout, args.verbose_shell))
        stop_command = "motor wave-stop" if args.mode in WAVE_MODES else "motor off"
        responses.append(send_shell_command(ser, stop_command, args.command_timeout, args.verbose_shell))
        return responses

    if args.mode.startswith("diag-"):
        direction = args.mode[len("diag-"):]
        responses.append(
            send_shell_command(
                ser,
                command_for_diag(args.driver, direction, args.duration_ms),
                args.command_timeout,
                args.verbose_shell,
            )
        )
        time.sleep(args.stop_during_active_s)
        scope.stop()
        remaining = max(0.0, (args.duration_ms / 1000.0) - args.stop_during_active_s)
        time.sleep(remaining + args.post_command_wait)
        responses.append(read_response(ser, timeout_s=args.command_timeout))
        return responses

    responses.append(send_shell_command(ser, f"motor arm {args.arm_seconds}", args.command_timeout, args.verbose_shell))
    time.sleep(args.command_gap)

    if args.mode == "sequence-forward-reverse":
        first_direction = "forward"
    else:
        first_direction = "reverse" if args.mode == "single-reverse" else "forward"

    responses.append(
        send_shell_command(
            ser,
            command_for_direction(args.driver, first_direction, args.duty_permille, args.duration_ms),
            args.command_timeout,
            args.verbose_shell,
        )
    )
    time.sleep(args.stop_during_active_s)
    scope.stop()

    if args.pwm_debug:
        responses.append(send_shell_command(ser, "motor pwm-debug", args.command_timeout, args.verbose_shell))

    remaining = max(0.0, (args.duration_ms / 1000.0) - args.stop_during_active_s)
    time.sleep(remaining + args.post_command_wait)
    responses.append(send_shell_command(ser, "motor off", args.command_timeout, args.verbose_shell))
    return responses


def summarize_waveform(channel: int, label: str, waveform: tuple[list[float], list[float]] | None, threshold_v: float) -> WaveSummary:
    if waveform is None:
        return WaveSummary(channel, label, 0, None, None, None, None, 0, None, None)
    times, volts = waveform
    count = min(len(times), len(volts))
    if count <= 0:
        return WaveSummary(channel, label, 0, None, None, None, None, 0, None, None)

    times = times[:count]
    volts = volts[:count]
    min_v = min(volts)
    max_v = max(volts)
    mean_v = sum(volts) / count
    p2p = max_v - min_v
    edge_level = (min_v + max_v) / 2.0 if p2p >= threshold_v else threshold_v

    raw_rising_times: list[float] = []
    high_samples = 0
    previous_high = volts[0] >= edge_level
    if previous_high:
        high_samples += 1
    for index in range(1, count):
        high = volts[index] >= edge_level
        if high:
            high_samples += 1
        if high and not previous_high:
            raw_rising_times.append(times[index])
        previous_high = high

    rising_times = raw_rising_times
    raw_periods = [b - a for a, b in zip(raw_rising_times, raw_rising_times[1:]) if b > a]
    if len(raw_rising_times) >= 3 and raw_periods:
        # Narrow PWM pulses can create several threshold crossings on one edge.
        # Treat crossings much closer than the longest observed period as edge artifacts.
        min_edge_interval_s = max(raw_periods) * 0.25
        filtered: list[float] = []
        for edge_time in raw_rising_times:
            if not filtered or (edge_time - filtered[-1]) >= min_edge_interval_s:
                filtered.append(edge_time)
        rising_times = filtered

    frequency = None
    if len(rising_times) >= 2:
        periods = [b - a for a, b in zip(rising_times, rising_times[1:]) if b > a]
        if periods:
            median_period = sorted(periods)[len(periods) // 2]
            if median_period > 0:
                frequency = 1.0 / median_period

    duty = None
    if p2p >= threshold_v:
        duty = 100.0 * high_samples / count

    return WaveSummary(channel, label, count, min_v, max_v, mean_v, p2p, len(rising_times), frequency, duty)


def write_summary(path: Path, *, args, scope_id: str, settings: dict, screenshot: Path | None, csv_path: Path | None, summaries: list[WaveSummary], shell_log: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Motor Bridge PWM Scope Capture",
        "",
        f"- Timestamp: `{time.strftime('%Y-%m-%d %H:%M:%S')}`",
        f"- Scope: `{scope_id}`",
        f"- Driver command mode: `{args.mode}`",
        f"- Acquire mode: `{args.acquire_mode}`",
        f"- Bridge driver: `{args.driver}`",
        f"- Sine target: `{args.sine_target}`",
        f"- Channel map: `{args.channel_map}`",
        f"- Pair overlay: `{args.pair_overlay}`",
        f"- Duty: `{args.duty_permille}` permille",
        f"- Sine phase: `{args.phase}`",
        f"- Sine scope slot: `{args.slot_us}` us",
        f"- Effective scope slot: `{effective_slot_us(args)}` us",
        f"- Fast clkdiv: `{args.fast_clkdiv}`",
        f"- Sine scope steps: `{args.steps}`",
        f"- Duration: `{args.duration_ms}` ms",
        f"- Timebase: `{args.timebase}` s/div",
        f"- Requested timebase: `{getattr(args, 'requested_timebase', args.timebase)}` s/div",
        f"- Scope arm mode: `{args.scope_arm_mode}`",
        f"- Waveform source: `{args.waveform_source}`",
        f"- Trigger: CH{args.trigger_channel} {args.trigger_slope} at {args.trigger_level} V, sweep {args.trigger_sweep}",
    ]
    if getattr(args, "display_channels", None):
        lines.append(f"- Display channels: `{','.join(str(channel) for channel in args.display_channels)}`")
    for channel, label in channel_labels(args):
        lines.append(f"- CH{channel}: {label}")
    if screenshot:
        lines.append(f"- Screenshot: [{screenshot.name}]({screenshot.name})")
    if csv_path:
        lines.append(f"- CSV: [{csv_path.name}]({csv_path.name})")
    if args.mode == "sine-diag-inputs":
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-diag-inputs` is a logic-only sign-phase diagnostic. It intentionally",
            "keeps DRV8837 `/SLEEP` low, so the bridge outputs remain disabled. The",
            "expected waveform is a set of square/meander logic levels, not a sine",
            "envelope and not a 20 kHz PWM carrier.",
            "",
            "For this HANMATEK/DOS1104 capture mode, treat the screenshot as primary",
            "evidence. The exported waveform CSV can lose the displayed vertical",
            "offset/scale and may report misleading millivolt-level values even when",
            "the screenshot clearly shows 3.3 V logic transitions.",
        ])
    if args.mode == "sine-phase":
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-phase` holds one 32-sample sine-table phase so the expected PWM",
            "pin and duty stay stable long enough for a clean 20 kHz carrier capture.",
            "Use phases 8, 0, 16, and 24 to exercise A1, B1, A2, and B2 respectively",
            "with the default AB target and physical channel map.",
        ])
    if args.mode == "sine-pwm-inputs":
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-pwm-inputs` generates the 20 kHz PWM carrier and the same stepped",
            "A/B sine duty sequence as `sine-demo`, but it forces DRV8837 `/SLEEP` low.",
            "The bridge outputs should remain disabled; this is intended for scope",
            "visibility on the DRV input pins and can use amplitudes above the normal",
            "bring-up motor safety limit.",
        ])
    if args.mode == "sine-scope-plot":
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-scope-plot` is a scope visualization diagnostic, not the real motor",
            "carrier. It keeps DRV8837 `/SLEEP` low and deliberately stretches each",
            "32-sample sine-table PWM slot to millisecond scale so a normal oscilloscope",
            "screen can resemble the reference bottom plot. Use this to verify phase",
            "ordering and duty-envelope shape; use `sine-phase` for real 20 kHz carrier",
            "verification.",
        ])
    if args.mode == "sine-scope-plot-us":
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-scope-plot-us` is the clock-divided scope visualization path. It",
            "keeps DRV8837 `/SLEEP` low and emits the same 32-sample sign/duty sequence",
            "as `sine-scope-plot`, but with an explicit microsecond slot width. One",
            "electrical cycle is `32 * slot_us`; reducing `slot_us` compresses the same",
            "reference shape onto a shorter scope timebase.",
        ])
    if args.mode in ("sine-scope-run-us", "sine-scope-clkdiv"):
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`sine-scope-run-us` / `sine-scope-clkdiv` keep the same 32-sample",
            "A/B sine duty/sign sequence running continuously for the requested",
            "duration while DRV8837 `/SLEEP` stays low. The waveform is intentionally",
            "safe logic-level scope evidence for phase ordering and duty-envelope",
            "shape; it is not yet enabling the bridge outputs.",
            "",
            "For the divider workflow, `fast_clkdiv` maps directly to `slot_us` in",
            "this diagnostic. Reducing it from 1024 toward 1 compresses the same",
            "repeating shape onto smaller timebases. Values below about 4 us are",
            "software-GPIO stress tests, so measured timing should be treated as",
            "scope evidence rather than a production timing guarantee.",
        ])
    if args.mode in WAVE_MODES:
        lines.extend([
            "",
            "## Diagnostic Interpretation",
            "",
            "`wave-clkdiv` exercises the ISR-updated hardware-timed motor engine.",
            "`wave-dma-a` exercises the first DMA/offload experiment: A1/A2 only,",
            "fed by TMR2/TMR1 timer DMA while B/G remain off. `wave-dma-hybrid`",
            "keeps A1/A2 on TMR2/TMR1 DMA and updates B/G from the lower-rate",
            "phase scheduler because CH592 PWMX has no DMA registers. In all",
            "wave modes the carrier is the production-style PWM carrier;",
            "`fast_clkdiv` maps to one requested sine-table slot in microseconds,",
            "so one electrical cycle is `32 * fast_clkdiv` before hardware",
            "quantization.",
            "",
            "This capture intentionally keeps DRV8837 `/SLEEP` at the requested",
            "`wave_sleep` state. For bring-up captures we use `sleep0`, so bridge",
            "outputs remain disabled while the input PWM timing is proven.",
        ])
    lines.extend(["", "## Waveform Summary", ""])
    lines.append("| Channel | Label | Samples | Min V | Max V | Mean V | Vpp | Rising edges | Est. Hz | Est. duty |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    for summary in summaries:
        lines.append(
            "| CH{ch} | {label} | {count} | {min_v} | {max_v} | {mean_v} | {p2p} | {edges} | {freq} | {duty} |".format(
                ch=summary.channel,
                label=summary.label,
                count=summary.count,
                min_v=format_optional(summary.min_v),
                max_v=format_optional(summary.max_v),
                mean_v=format_optional(summary.mean_v),
                p2p=format_optional(summary.peak_to_peak_v),
                edges=summary.rising_edges,
                freq=format_optional(summary.estimated_frequency_hz),
                duty=format_optional(summary.estimated_duty_percent),
            )
        )
    lines.extend(["", "## Scope Settings", ""])
    screen_metadata = settings.get("screen_waveform")
    deep_metadata = settings.get("deep_memory")
    if isinstance(screen_metadata, dict):
        lines.append(f"- `screen_waveform`: `{screen_metadata}`")
    if isinstance(deep_metadata, dict):
        lines.append(f"- `deep_memory`: `{deep_metadata}`")
    for key, value in settings.items():
        if key not in {"channels", "screen_waveform", "deep_memory"}:
            lines.append(f"- `{key}`: `{value}`")
    for channel, state in settings.get("channels", {}).items():
        lines.append(f"- CH{channel}: `{state}`")
    if shell_log:
        lines.extend(["", "## Shell Responses", ""])
        for response in shell_log:
            text = response.strip().replace("\r", "")
            lines.append("```text")
            lines.append(text if text else "<no response>")
            lines.append("```")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def format_optional(value: float | None) -> str:
    if value is None or not math.isfinite(value):
        return ""
    return f"{value:.3f}"


def write_waveforms_csv(path: Path, waveforms: dict[int, tuple[list[float], list[float]]], labels: dict[int, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    channels = sorted(waveforms)
    max_count = max((len(waveforms[channel][0]) for channel in channels), default=0)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        header: list[str] = []
        for channel in channels:
            label = re.sub(r"[^A-Za-z0-9]+", "_", labels.get(channel, f"ch{channel}")).strip("_").lower()
            header.extend([f"ch{channel}_{label}_time_s", f"ch{channel}_{label}_voltage_v"])
        writer.writerow(header)
        for index in range(max_count):
            row: list[str | float] = []
            for channel in channels:
                waveform = waveforms.get(channel)
                if waveform and index < len(waveform[0]):
                    row.extend([waveform[0][index], waveform[1][index]])
                else:
                    row.extend(["", ""])
            writer.writerow(row)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Capture DRV8837 bridge input PWM waveforms")
    parser.add_argument(
        "--mode",
        choices=[
            "setup-only",
            "single-forward",
            "single-reverse",
            "sequence-forward-reverse",
            "sine-demo",
            "sine-phase",
            "sine-diag-inputs",
            "sine-pwm-inputs",
            "sine-scope-plot",
            "sine-scope-plot-us",
            "sine-scope-run-us",
            "sine-scope-clkdiv",
            "wave-clkdiv",
            "wave-dma-a",
            "wave-dma-hybrid",
            "diag-forward",
            "diag-reverse",
            "diag-brake",
        ],
        default="single-forward",
        help="Scope/motor workflow to run",
    )
    parser.add_argument("--port", help="USB CDC shell port, for example COM19. If omitted, only scope setup/capture runs.")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--driver", choices=["A", "B", "G"], default="B")
    parser.add_argument("--sine-target", choices=["AB", "A", "B", "G", "all"], default=None)
    parser.add_argument(
        "--channel-map",
        choices=["driver", "ab-reference", "ab-physical"],
        default="driver",
        help=(
            "Scope probe mapping. driver keeps legacy CH1=driver IN2, CH2=driver IN1. "
            "ab-reference uses CH1=A1 CH2=A2 CH3=B1 CH4=B2. "
            "ab-physical uses CH1=B2 CH2=A2 CH3=B1 CH4=A1."
        ),
    )
    parser.add_argument("--speed-hz", type=int, default=2, help="Sine-demo electrical cycles per second")
    parser.add_argument("--slot-us", type=int, default=31250, help="sine-scope-plot-us/run-us slot width; 32 slots make one electrical cycle")
    parser.add_argument("--steps", type=int, default=64, help="sine-scope-plot-us slot count to emit")
    parser.add_argument(
        "--fast-clkdiv",
        type=int,
        default=1024,
        help=(
            "Divider-style alias for sine-scope-clkdiv, wave-clkdiv, wave-dma-a, and wave-dma-hybrid. It maps "
            "directly to slot_us so 1024,512,...,1 compress the same 32-slot shape."
        ),
    )
    parser.add_argument(
        "--wave-sleep",
        choices=["sleep0", "sleep1"],
        default="sleep0",
        help="DRV8837 sleep state for wave-* modes. Use sleep0 for scope-only input validation.",
    )
    parser.add_argument(
        "--allow-wave-starvation-risk",
        action="store_true",
        help=(
            "Allow wave-clkdiv values below 128 us. 64 us/slot starved the "
            "USB shell on 2026-06-05 and may require manual reset/power cycle."
        ),
    )
    parser.add_argument("--phase", type=int, default=8, help="Sine-phase table index, 0..31")
    parser.add_argument("--amplitude-permille", type=int, default=50, help="Sine-demo peak duty permille")
    parser.add_argument("--duty-permille", type=int, default=50, help="Motor command duty, 50=5 percent bring-up limit")
    parser.add_argument("--duration-ms", type=int, default=300, help="Motor command duration")
    parser.add_argument("--arm-seconds", type=int, default=2)
    parser.add_argument("--command-timeout", type=float, default=0.5)
    parser.add_argument("--command-gap", type=float, default=0.1)
    parser.add_argument("--sequence-gap", type=float, default=0.15)
    parser.add_argument("--post-command-wait", type=float, default=0.25)
    parser.add_argument("--verbose-shell", action="store_true")
    parser.add_argument("--pwm-debug", action="store_true", help="Request motor pwm-debug before motor off")
    parser.add_argument(
        "--acquire-mode",
        choices=["run-stop", "single-trigger"],
        default="run-stop",
        help="run-stop captures the visible waveform without relying on single-trigger state",
    )
    parser.add_argument(
        "--stop-during-active-s",
        type=float,
        default=0.08,
        help="Delay after starting the motor command before stopping the scope in run-stop mode",
    )
    parser.add_argument("--probe", type=int, default=1)
    parser.add_argument("--ch1-scale", type=float, default=1.0)
    parser.add_argument("--ch2-scale", type=float, default=1.0)
    parser.add_argument("--ch3-scale", type=float, default=1.0)
    parser.add_argument("--ch4-scale", type=float, default=1.0)
    parser.add_argument("--ch1-position", type=float, default=-1.5)
    parser.add_argument("--ch2-position", type=float, default=1.5)
    parser.add_argument("--ch3-position", type=float, default=0.0)
    parser.add_argument("--ch4-position", type=float, default=3.0)
    parser.add_argument(
        "--display-channels",
        type=str,
        default=None,
        help=(
            "Comma-separated scope channels to enable, for example '1,2,3,4' or '3'. "
            "Use fewer channels for higher DOS1104 sample-rate zoom captures."
        ),
    )
    parser.add_argument("--timebase", type=float, default=None, help="Seconds/div. If omitted, a mode-specific value is chosen.")
    parser.add_argument(
        "--pair-overlay",
        dest="pair_overlay",
        action="store_true",
        default=None,
        help="Place A1/A2 on one vertical baseline and B1/B2 on another.",
    )
    parser.add_argument(
        "--no-pair-overlay",
        dest="pair_overlay",
        action="store_false",
        help="Keep individually separated channel baselines.",
    )
    parser.add_argument("--scope-horizontal-offset-div", type=float, default=None)
    parser.add_argument("--scope-acquire-mode", choices=["sample", "average", "peak"], default="sample")
    parser.add_argument("--scope-memory-depth", type=int, default=5000)
    parser.add_argument(
        "--scope-arm-mode",
        choices=["run", "single"],
        default="single",
        help=(
            "How to arm the scope for single-trigger captures. The NorthPole "
            "DOS1104 captures currently trigger most reliably with single; run "
            "is available for Magnetic-style experiments."
        ),
    )
    parser.add_argument(
        "--pre-arm-motor-off",
        dest="pre_arm_motor_off",
        action="store_true",
        default=True,
        help="Send motor off before arming a single-trigger capture so the trigger source starts low.",
    )
    parser.add_argument(
        "--no-pre-arm-motor-off",
        dest="pre_arm_motor_off",
        action="store_false",
        help="Do not send motor off before arming a single-trigger capture.",
    )
    parser.add_argument("--trigger-channel", type=int, choices=[1, 2, 3, 4], default=None)
    parser.add_argument("--trigger-slope", choices=["rising", "falling"], default="rising")
    parser.add_argument("--trigger-level", type=float, default=1.5)
    parser.add_argument("--trigger-holdoff-ns", type=int, default=100)
    parser.add_argument("--trigger-sweep", choices=["AUTO", "NORMal", "SINGle"], default="SINGle")
    parser.add_argument("--settle-s", type=float, default=0.2)
    parser.add_argument("--capture-wait-s", type=float, default=0.6)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--no-screenshot", action="store_true")
    parser.add_argument("--no-waveform", action="store_true")
    parser.add_argument(
        "--waveform-source",
        choices=["screen", "deep"],
        default="screen",
        help=(
            "CSV source when waveform capture is enabled. screen matches the "
            "displayed trace; deep uses DOS1104 deep-memory capture for "
            "higher-rate timing diagnostics."
        ),
    )
    parser.add_argument(
        "--read-waveform-metadata",
        dest="read_waveform_metadata",
        action="store_true",
        default=False,
        help="Read screen/deep-memory metadata after capture for sample-rate diagnostics.",
    )
    parser.add_argument(
        "--no-read-waveform-metadata",
        dest="read_waveform_metadata",
        action="store_false",
        help="Skip screen/deep-memory metadata queries if the scope is unstable.",
    )
    parser.add_argument(
        "--waveform-metadata-source",
        choices=["screen", "deep", "both"],
        default="screen",
        help=(
            "Metadata source when --read-waveform-metadata is used. screen is "
            "safer; deep is useful for high-rate checks but can be less stable "
            "on this DOS1104 firmware."
        ),
    )
    parser.add_argument("--edge-threshold-v", type=float, default=0.5)
    parser.add_argument("--instrumentkit-src", type=Path, default=ScopeConnection.instrumentkit_src)
    parser.add_argument("--vid", type=lambda text: int(text, 0), default=0x5345)
    parser.add_argument("--pid", type=lambda text: int(text, 0), default=0x1234)
    parser.add_argument("--timeout-s", type=float, default=2.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        args.display_channels = parse_channel_list(args.display_channels)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    if args.duty_permille < 0 or args.duty_permille > 1000:
        print("--duty-permille must be 0..1000", file=sys.stderr)
        return 2
    if args.sine_target is None:
        args.sine_target = "AB" if args.driver == "B" else args.driver
    if args.mode in WAVEFORM_SEQUENCE_MODES and args.sine_target not in ("AB", "all"):
        args.driver = args.sine_target
    if args.phase < 0 or args.phase > 31:
        print("--phase must be 0..31", file=sys.stderr)
        return 2
    if args.mode in ("wave-clkdiv", "wave-dma-hybrid") and effective_slot_us(args) < 128 and not args.allow_wave_starvation_risk:
        print(
            "--fast-clkdiv/slot below 128 us can starve the USB shell in ISR-updated wave modes; "
            "use --allow-wave-starvation-risk only for intentional stress captures.",
            file=sys.stderr,
        )
        return 2
    if args.trigger_channel is None:
        args.trigger_channel = default_sine_phase_trigger_channel(args)
    if args.trigger_channel is None:
        args.trigger_channel = 1 if args.mode in ("single-reverse", "diag-reverse") else 2
    if args.display_channels and args.trigger_channel not in args.display_channels:
        print("--trigger-channel must be one of --display-channels so the trigger source is visible/enabled", file=sys.stderr)
        return 2
    if args.timebase is None:
        if args.mode == "sequence-forward-reverse":
            args.timebase = 50e-3
        elif args.mode.startswith("diag-"):
            args.timebase = 100e-3
        elif args.mode in ("sine-demo", "sine-diag-inputs", "sine-pwm-inputs", "sine-scope-plot"):
            args.timebase = 100e-3
        elif args.mode in ("sine-scope-plot-us", "sine-scope-run-us", "sine-scope-clkdiv", "wave-clkdiv", "wave-dma-a", "wave-dma-hybrid"):
            slot_us = effective_slot_us(args)
            visible_slots = max(1, min(args.steps if args.mode == "sine-scope-plot-us" else 32, 32))
            visible_cycle_s = (visible_slots * slot_us) / 1_000_000.0
            args.timebase = max(20e-6, visible_cycle_s / 10.0)
        else:
            args.timebase = 20e-6
    requested_timebase = args.timebase
    args.timebase = quantize_timebase_scale(args.timebase)
    args.requested_timebase = requested_timebase
    if not math.isclose(float(requested_timebase), float(args.timebase), rel_tol=1e-9, abs_tol=1e-12):
        print(f"Adjusted timebase from {requested_timebase:g} to supported {args.timebase:g} s/div")
    if args.mode in ("sine-demo", "sine-diag-inputs", "sine-pwm-inputs", "sine-scope-plot") and args.stop_during_active_s == 0.08:
        args.stop_during_active_s = 0.6
    if args.mode == "sine-scope-plot-us" and args.stop_during_active_s == 0.08:
        args.stop_during_active_s = min(0.6, max(0.02, (args.slot_us * args.steps) / 2_000_000.0))
    if args.mode in ("sine-scope-run-us", "sine-scope-clkdiv", "wave-clkdiv", "wave-dma-a", "wave-dma-hybrid") and args.stop_during_active_s == 0.08:
        visible_cycle_s = (32 * effective_slot_us(args)) / 1_000_000.0
        active_s = active_duration_s_for_args(args)
        args.stop_during_active_s = min(max(0.03, visible_cycle_s * 1.5), max(0.03, active_s * 0.5), 1.0)
    if args.mode == "sine-phase" and args.stop_during_active_s == 0.08:
        args.stop_during_active_s = 0.12
    if args.pair_overlay is None:
        args.pair_overlay = args.channel_map != "driver" and args.mode in ("sine-scope-plot", "sine-scope-plot-us", "sine-scope-run-us", "sine-scope-clkdiv", "wave-clkdiv", "wave-dma-a", "wave-dma-hybrid")
    if args.channel_map != "driver":
        if args.pair_overlay:
            if args.channel_map == "ab-physical":
                # User's physical wiring: CH1=B2, CH2=A2, CH3=B1, CH4=A1.
                # Overlay each physical pair on one baseline:
                # B2/B1 lower row, A2/A1 upper row.
                args.ch1_position = -3.5
                args.ch3_position = -3.5
                args.ch2_position = 0.5
                args.ch4_position = 0.5
            else:
                args.ch1_position = 0.5
                args.ch2_position = 0.5
                args.ch3_position = -3.5
                args.ch4_position = -3.5
        else:
            if args.ch1_position == -1.5:
                args.ch1_position = -3.0
            if args.ch2_position == 1.5:
                args.ch2_position = -1.0
            if args.ch3_position == 0.0:
                args.ch3_position = 1.0
    if args.mode.startswith("diag-") and args.stop_during_active_s == 0.08:
        args.stop_during_active_s = 0.6
    if args.mode.startswith("diag-") and args.trigger_sweep == "SINGle":
        args.trigger_sweep = "AUTO"
    if args.acquire_mode == "run-stop" and args.trigger_sweep == "SINGle":
        args.trigger_sweep = "AUTO"
    if not channel_labels(args):
        print("--display-channels disabled all channels for the selected --channel-map", file=sys.stderr)
        return 2

    args.out_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    stem_target = args.sine_target if args.mode in WAVEFORM_SEQUENCE_MODES else args.driver
    stem_target = re.sub(r"[^A-Za-z0-9]+", "_", str(stem_target)).strip("_") or args.driver
    stem_map = re.sub(r"[^A-Za-z0-9]+", "_", args.channel_map).strip("_")
    stem = f"{stamp}_{args.mode}_{stem_map}_{stem_target}"
    screenshot_path = args.out_dir / f"{stem}.png"
    csv_path = args.out_dir / f"{stem}.csv"
    summary_path = args.out_dir / f"{stem}.md"

    connection = ScopeConnection(
        instrumentkit_src=args.instrumentkit_src,
        vid=args.vid,
        pid=args.pid,
        timeout_s=args.timeout_s,
    )

    shell_log: list[str] = []
    try:
        with OwonScopeAdapter(connection) as scope:
            scope_id = scope.identify()
            print(f"Scope: {scope_id}")
            configure_scope(scope, args)
            time.sleep(args.settle_s)
            active_channels = [channel for channel, _label in channel_labels(args)]
            settings = scope.read_scope_settings(active_channels, waveform_metadata="none")

            if args.mode == "setup-only":
                scope.run()
                print("Scope configured and left running.")
                write_summary(
                    summary_path,
                    args=args,
                    scope_id=scope_id,
                    settings=settings,
                    screenshot=None,
                    csv_path=None,
                    summaries=[],
                    shell_log=[],
                )
                print(f"Summary: {summary_path}")
                return 0

            ser = open_serial(args)
            try:
                if args.acquire_mode == "run-stop":
                    scope.run()
                    time.sleep(0.1)
                    shell_log = run_motor_sequence_run_stop(ser, args, scope)
                else:
                    if ser is not None and args.pre_arm_motor_off:
                        shell_log.append(send_shell_command(ser, "motor off", args.command_timeout, args.verbose_shell))
                        time.sleep(0.05)
                    if args.scope_arm_mode == "run":
                        scope.run()
                    else:
                        scope.single()
                    time.sleep(0.1)
                    started_at = time.monotonic()
                    active_s = active_duration_s_for_args(args)
                    needs_motor_off = False

                    if ser is None:
                        shell_log = []
                    else:
                        shell_log.append(send_shell_command(ser, "motor status", args.command_timeout, args.verbose_shell))
                        if args.mode in WAVEFORM_SEQUENCE_MODES:
                            command = command_for_sine_like_mode(args)
                        elif args.mode.startswith("diag-"):
                            direction = args.mode[len("diag-"):]
                            command = command_for_diag(args.driver, direction, args.duration_ms)
                        else:
                            shell_log.append(send_shell_command(ser, f"motor arm {args.arm_seconds}", args.command_timeout, args.verbose_shell))
                            time.sleep(args.command_gap)
                            direction = "reverse" if args.mode == "single-reverse" else "forward"
                            command = command_for_direction(args.driver, direction, args.duty_permille, args.duration_ms)
                            needs_motor_off = True
                        command_response = send_shell_command(ser, command, args.command_timeout, args.verbose_shell)
                        if shell_response_has_command_error(command_response):
                            scope.stop()
                        raise_on_shell_command_error(command, command_response)
                        shell_log.append(command_response)

                    trigger_status = wait_for_scope_trigger(
                        scope,
                        max(args.capture_wait_s, active_s + args.post_command_wait),
                        args.verbose_shell,
                    )
                    if not scope_status_hit_trigger(trigger_status):
                        print(f"Scope trigger was not observed before timeout; last status={trigger_status!r}")
                        scope.stop()

                    if ser is not None:
                        elapsed = time.monotonic() - started_at
                        remaining = max(0.0, active_s - elapsed)
                        time.sleep(remaining + args.post_command_wait)
                        shell_log.append(read_response(ser, timeout_s=args.command_timeout))
                        if args.mode in WAVE_MODES:
                            shell_log.append(send_shell_command(ser, "motor wave-stop", args.command_timeout, args.verbose_shell))
                        elif needs_motor_off:
                            shell_log.append(send_shell_command(ser, "motor off", args.command_timeout, args.verbose_shell))
            finally:
                if ser is not None:
                    ser.close()

            saved_screenshot = None
            if not args.no_screenshot:
                saved_screenshot = scope.save_screenshot(screenshot_path)

            waveforms: dict[int, tuple[list[float], list[float]]] = {}
            summaries: list[WaveSummary] = []
            if not args.no_waveform:
                labels = dict(channel_labels(args))
                for channel, label in channel_labels(args):
                    if args.waveform_source == "deep":
                        waveform = scope.read_deep_waveform(channel)
                    else:
                        waveform = scope.read_waveform(channel)
                    if waveform is not None:
                        waveforms[channel] = waveform
                    summaries.append(summarize_waveform(channel, label, waveform, args.edge_threshold_v))
                if waveforms:
                    write_waveforms_csv(csv_path, waveforms, labels)
                else:
                    csv_path = None
            else:
                csv_path = None

            write_summary(
                summary_path,
                args=args,
                scope_id=scope_id,
                settings=scope.read_scope_settings(
                    active_channels,
                    waveform_metadata=args.waveform_metadata_source if args.read_waveform_metadata else "none",
                ),
                screenshot=saved_screenshot,
                csv_path=csv_path,
                summaries=summaries,
                shell_log=shell_log,
            )
            print(f"Screenshot: {saved_screenshot}" if saved_screenshot else "Screenshot: not saved")
            print(f"CSV: {csv_path}" if csv_path else "CSV: not saved")
            print(f"Summary: {summary_path}")
            for summary in summaries:
                print(
                    f"CH{summary.channel} {summary.label}: "
                    f"vpp={format_optional(summary.peak_to_peak_v)}V "
                    f"edges={summary.rising_edges} "
                    f"freq={format_optional(summary.estimated_frequency_hz)}Hz "
                    f"duty={format_optional(summary.estimated_duty_percent)}%"
                )
    except ScopeAdapterError as exc:
        print(f"scope error: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
