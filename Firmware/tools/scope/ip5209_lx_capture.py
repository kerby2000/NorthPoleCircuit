#!/usr/bin/env python3
"""Capture IP5209 CSIN/LX waveforms with an OWON/HANMATEK DOS1104 scope."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass, field
from datetime import datetime, timezone
import json
import math
import statistics
import time
from pathlib import Path
import sys
from typing import Any

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(Path(__file__).resolve().parent))

from scope_driver_adapter import ChannelConfig, OwonScopeAdapter, ScopeConnection


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_EVIDENCE_DIR = REPO_ROOT / "Firmware" / "docs" / "ip5209_scope_evidence"
DEFAULT_REPORT_PATH = REPO_ROOT / "Firmware" / "docs" / "ip5209_lx_scope_report.md"
DEFAULT_FIRMWARE_IMAGE = "Firmware/build/bringup/northpole_ch592_bringup.hex"


@dataclass
class PulseCandidate:
    channel: int
    start_s: float
    end_s: float
    width_s: float
    baseline_v: float
    min_v: float
    max_v: float
    amplitude_v: float
    direction: str
    sample_count: int


@dataclass
class WaveformStats:
    channel: int
    signal: str
    sample_count: int
    min_v: float
    max_v: float
    mean_v: float
    peak_to_peak_v: float
    edge_count: int = 0
    estimated_frequency_hz: float | None = None
    pulse_count: int = 0
    first_pulse_s: float | None = None
    largest_pulse_v: float | None = None
    largest_pulse_width_s: float | None = None


@dataclass
class ScaleCheck:
    channel: int
    signal: str
    dmm_reference_v: float
    raw_mean_v: float
    ratio: float
    status: str


@dataclass
class CaptureResult:
    label: str
    mode: str
    screenshot_path: Path | None = None
    csv_path: Path | None = None
    trigger_status: str | None = None
    classification: str = "PENDING_CAPTURE"
    stats: list[WaveformStats] = field(default_factory=list)
    pulses: dict[int, list[PulseCandidate]] = field(default_factory=dict)
    scale_checks: list[ScaleCheck] = field(default_factory=list)
    scope_settings: dict[str, Any] = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode",
        choices=["idle", "wake-single", "steady-switching", "load-test"],
        help="Capture mode.",
    )
    parser.add_argument("--check-scope", action="store_true", help="Only open the scope and query *IDN?.")
    parser.add_argument(
        "--rewrite-report-only",
        action="store_true",
        help="Reanalyze existing capture CSV files from the manifest and rebuild the report without opening the scope.",
    )
    parser.add_argument("--out-dir", default=str(DEFAULT_EVIDENCE_DIR), help="Evidence output directory.")
    parser.add_argument("--report", default=str(DEFAULT_REPORT_PATH), help="Markdown report path.")
    parser.add_argument(
        "--instrumentkit-src",
        default=str(Path.home() / "Documents" / "VS code projects" / "InstrumentKit" / "src"),
        help="Path to local InstrumentKit src.",
    )
    parser.add_argument("--vid", default="0x5345", help="Scope USB VID.")
    parser.add_argument("--pid", default="0x1234", help="Scope USB PID.")
    parser.add_argument("--timeout-s", type=float, default=2.0, help="Scope USB timeout.")
    parser.add_argument("--probe", type=int, default=1, help="Probe attenuation configured on the scope.")
    parser.add_argument("--ch1-scale", type=float, default=2.0, help="CH1 volts/div.")
    parser.add_argument("--ch2-scale", type=float, default=2.0, help="CH2 volts/div.")
    parser.add_argument("--ch3", choices=["off", "vout"], default="off", help="Optional CH3 signal.")
    parser.add_argument("--ch4", choices=["off", "key", "vreg"], default="off", help="Optional CH4 signal.")
    parser.add_argument("--ch3-scale", type=float, default=2.0, help="CH3 volts/div.")
    parser.add_argument("--ch4-scale", type=float, default=1.0, help="CH4 volts/div.")
    parser.add_argument("--ch1-dmm-v", type=float, default=None, help="Optional DMM reference voltage for CH1.")
    parser.add_argument("--ch2-dmm-v", type=float, default=None, help="Optional DMM reference voltage for CH2.")
    parser.add_argument("--ch3-dmm-v", type=float, default=None, help="Optional DMM reference voltage for CH3.")
    parser.add_argument("--ch4-dmm-v", type=float, default=None, help="Optional DMM reference voltage for CH4.")
    parser.add_argument(
        "--pulse-min-vpp",
        type=float,
        default=0.5,
        help="Minimum peak-to-peak voltage before a channel can be treated as pulsing.",
    )
    parser.add_argument(
        "--pulse-threshold-v",
        type=float,
        default=0.5,
        help="Minimum excursion from baseline for pulse candidate detection.",
    )
    parser.add_argument("--idle-timebase", type=float, default=0.010, help="Idle seconds/div.")
    parser.add_argument("--wake-timebase", type=float, default=0.000100, help="Wake seconds/div.")
    parser.add_argument("--trigger-level", type=float, default=2.0, help="CH2 falling-edge trigger level.")
    parser.add_argument("--wait-trigger-s", type=float, default=10.0, help="Wake trigger wait time.")
    parser.add_argument("--no-waveform", action="store_true", help="Skip waveform CSV reads.")
    parser.add_argument("--no-screenshot", action="store_true", help="Skip screenshot capture.")
    parser.add_argument("--no-prompt", action="store_true", help="Do not wait for user prompts.")
    parser.add_argument("--board-revision", default="north-pole-ble-audio-current-pcb")
    parser.add_argument("--firmware-image", default=DEFAULT_FIRMWARE_IMAGE)
    parser.add_argument("--battery-voltage", default="unknown")
    parser.add_argument("--usb-state", default="unknown", help="connected, disconnected, or unknown.")
    parser.add_argument("--load-ohms", default="220", help="Load value for load-test report text.")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not args.check_scope and not args.rewrite_report_only and args.mode is None:
        parser.error("--mode is required unless --check-scope is used")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.rewrite_report_only:
        records = load_manifest(out_dir)
        records = reanalyze_records(records, args)
        write_manifest(out_dir, records)
        write_report(Path(args.report), records)
        print(f"report={Path(args.report)}")
        return 0

    connection = ScopeConnection(
        instrumentkit_src=Path(args.instrumentkit_src),
        vid=int(str(args.vid), 0),
        pid=int(str(args.pid), 0),
        timeout_s=args.timeout_s,
    )

    with OwonScopeAdapter(connection) as scope:
        scope_idn = scope.identify()
        if args.check_scope:
            print(f"scope_idn={scope_idn}")
            return 0

        if args.mode == "idle":
            results = [capture_idle(scope, args, out_dir)]
        elif args.mode == "wake-single":
            results = [capture_wake_single(scope, args, out_dir, label_suffix="wake_single")]
        elif args.mode == "load-test":
            results = [capture_load_test(scope, args, out_dir)]
        else:
            results = capture_steady_switching(scope, args, out_dir)

    records = update_manifest(out_dir, args, scope_idn, results)
    write_report(Path(args.report), records)
    for result in results:
        print(f"{result.label}: classification={result.classification}")
        if result.screenshot_path:
            print(f"  screenshot={result.screenshot_path}")
        if result.csv_path:
            print(f"  csv={result.csv_path}")
    return 0


def capture_idle(scope: OwonScopeAdapter, args: argparse.Namespace, out_dir: Path) -> CaptureResult:
    label = timestamped_label("idle")
    configure_channels(scope, args)
    scope.set_timebase_scale(args.idle_timebase)
    scope.run()
    time.sleep(0.5)
    scope.stop()
    return finish_capture(scope, args, out_dir, label, mode="idle")


def capture_wake_single(
    scope: OwonScopeAdapter,
    args: argparse.Namespace,
    out_dir: Path,
    *,
    label_suffix: str,
) -> CaptureResult:
    label = timestamped_label(label_suffix)
    configure_channels(scope, args)
    scope.set_timebase_scale(args.wake_timebase)
    scope.configure_edge_trigger(
        source_channel=2,
        slope="falling",
        level_v=args.trigger_level,
        sweep="SINGle",
    )
    scope.single()
    if not args.no_prompt:
        input("Scope is armed. Press SW3 briefly, wait for the trigger, then press Enter here.")
    status = wait_for_trigger(scope, args.wait_trigger_s)

    result = finish_capture(scope, args, out_dir, label, mode="wake-single", trigger_status=status)
    if result.classification == "NO_LX_SWITCHING_DETECTED":
        result.notes.append(
            "Falling-edge trigger did not show LX activity. Try a rising-edge capture at 4.5 V if needed."
        )
    return result


def capture_load_test(scope: OwonScopeAdapter, args: argparse.Namespace, out_dir: Path) -> CaptureResult:
    if not args.no_prompt:
        input(f"Confirm {args.load_ohms} ohm load is connected from +5V to GND, then press Enter.")
    return capture_wake_single(scope, args, out_dir, label_suffix=f"load_{args.load_ohms}R_wake_single")


def capture_steady_switching(
    scope: OwonScopeAdapter,
    args: argparse.Namespace,
    out_dir: Path,
) -> list[CaptureResult]:
    if not args.no_prompt:
        input("Press SW3 and confirm VREG is awake if possible, then press Enter for steady captures.")

    results: list[CaptureResult] = []
    for timebase in (500e-9, 1e-6, 2e-6, 5e-6):
        label = timestamped_label(f"steady_{format_timebase(timebase)}div")
        configure_channels(scope, args)
        scope.set_timebase_scale(timebase)
        scope.configure_edge_trigger(
            source_channel=2,
            slope="falling",
            level_v=args.trigger_level,
            sweep="NORMal",
        )
        scope.run()
        time.sleep(0.5)
        scope.stop()
        results.append(finish_capture(scope, args, out_dir, label, mode="steady-switching"))
    return results


def configure_channels(scope: OwonScopeAdapter, args: argparse.Namespace) -> None:
    configs = [
        ChannelConfig(1, True, args.ch1_scale, probe_attenuation=args.probe),
        ChannelConfig(2, True, args.ch2_scale, probe_attenuation=args.probe),
        ChannelConfig(3, args.ch3 != "off", args.ch3_scale, probe_attenuation=args.probe),
        ChannelConfig(4, args.ch4 != "off", args.ch4_scale, probe_attenuation=args.probe),
    ]
    for config in configs:
        scope.setup_channel(config)


def wait_for_trigger(scope: OwonScopeAdapter, timeout_s: float) -> str | None:
    deadline = time.monotonic() + max(timeout_s, 0.0)
    last_status: str | None = None
    while time.monotonic() < deadline:
        last_status = scope.trigger_status()
        if last_status and any(token in last_status.upper() for token in ("STOP", "TRIG", "TD")):
            return last_status
        time.sleep(0.2)
    return last_status


def finish_capture(
    scope: OwonScopeAdapter,
    args: argparse.Namespace,
    out_dir: Path,
    label: str,
    *,
    mode: str,
    trigger_status: str | None = None,
) -> CaptureResult:
    screenshot_path: Path | None = None
    if not args.no_screenshot:
        screenshot_path = scope.save_screenshot(out_dir / f"{label}.png")

    waveforms: dict[int, tuple[list[float], list[float]]] = {}
    if not args.no_waveform:
        for channel in enabled_channels(args):
            waveform = scope.read_waveform(channel)
            if waveform is not None:
                waveforms[channel] = waveform

    csv_path: Path | None = None
    if waveforms:
        csv_path = out_dir / f"{label}_{channel_suffix(sorted(waveforms))}.csv"
        scope.save_waveforms_csv(csv_path, waveforms)

    stats, pulses = analyze_waveforms(waveforms, args)
    scale_checks, scale_warnings = build_scale_checks(stats, args)
    classification = classify_capture(stats, pulses, scale_warnings)
    scope_settings = scope.read_scope_settings(enabled_channels(args))
    return CaptureResult(
        label=label,
        mode=mode,
        screenshot_path=screenshot_path,
        csv_path=csv_path,
        trigger_status=trigger_status,
        classification=classification,
        stats=stats,
        pulses=pulses,
        scale_checks=scale_checks,
        scope_settings=scope_settings,
        warnings=scale_warnings,
    )


def enabled_channels(args: argparse.Namespace) -> list[int]:
    channels = [1, 2]
    if args.ch3 != "off":
        channels.append(3)
    if args.ch4 != "off":
        channels.append(4)
    return channels


def channel_suffix(channels: list[int]) -> str:
    return "_".join(f"ch{channel}" for channel in channels)


def channel_signal(channel: int, args_or_record: argparse.Namespace | dict[str, Any] | None = None) -> str:
    if channel == 1:
        return "CSIN"
    if channel == 2:
        return "LX"
    if channel == 3:
        if isinstance(args_or_record, argparse.Namespace) and getattr(args_or_record, "ch3", "off") == "vout":
            return "VOUT"
        return "VOUT"
    if channel == 4:
        if isinstance(args_or_record, argparse.Namespace):
            value = getattr(args_or_record, "ch4", "off")
            if value == "key":
                return "KEY"
            if value == "vreg":
                return "VREG"
        return "VREG/KEY"
    return f"CH{channel}"


def analyze_waveforms(
    waveforms: dict[int, tuple[list[float], list[float]]],
    args: argparse.Namespace,
) -> tuple[list[WaveformStats], dict[int, list[PulseCandidate]]]:
    stats: list[WaveformStats] = []
    pulses: dict[int, list[PulseCandidate]] = {}
    for channel in sorted(waveforms):
        channel_stats, channel_pulses = analyze_channel(
            channel,
            *waveforms[channel],
            signal=channel_signal(channel, args),
            pulse_min_vpp=args.pulse_min_vpp,
            pulse_threshold_v=args.pulse_threshold_v,
        )
        stats.append(channel_stats)
        pulses[channel] = channel_pulses
    return stats, pulses


def analyze_channel(
    channel: int,
    times: list[float],
    volts: list[float],
    *,
    signal: str = "",
    pulse_min_vpp: float = 0.5,
    pulse_threshold_v: float = 0.5,
) -> tuple[WaveformStats, list[PulseCandidate]]:
    count = min(len(times), len(volts))
    values = volts[:count]
    if count == 0:
        return (
            WaveformStats(channel, signal or f"CH{channel}", 0, math.nan, math.nan, math.nan, math.nan),
            [],
        )
    min_v = min(values)
    max_v = max(values)
    peak_to_peak_v = max_v - min_v
    edge_count = 0
    frequency_hz: float | None = None
    pulses: list[PulseCandidate] = []
    if peak_to_peak_v >= pulse_min_vpp:
        edge_count, frequency_hz = estimate_edges_and_frequency(
            times[:count],
            values,
            min_vpp=pulse_min_vpp,
        )
        pulses = find_pulse_candidates(
            channel,
            times[:count],
            values,
            threshold_v=pulse_threshold_v,
            min_vpp=pulse_min_vpp,
        )
    return (
        WaveformStats(
            channel=channel,
            signal=signal or f"CH{channel}",
            sample_count=count,
            min_v=min_v,
            max_v=max_v,
            mean_v=statistics.fmean(values),
            peak_to_peak_v=peak_to_peak_v,
            edge_count=edge_count,
            estimated_frequency_hz=frequency_hz,
            pulse_count=len(pulses),
            first_pulse_s=pulses[0].start_s if pulses else None,
            largest_pulse_v=max((pulse.amplitude_v for pulse in pulses), default=None),
            largest_pulse_width_s=max((pulse.width_s for pulse in pulses), default=None),
        ),
        pulses,
    )


def estimate_edges_and_frequency(
    times: list[float],
    volts: list[float],
    *,
    min_vpp: float = 0.5,
) -> tuple[int, float | None]:
    if len(times) < 3 or len(volts) < 3:
        return 0, None
    v_min = min(volts)
    v_max = max(volts)
    vpp = v_max - v_min
    if vpp < min_vpp:
        return 0, None
    low_threshold = v_min + vpp * 0.35
    high_threshold = v_min + vpp * 0.65
    high = volts[0] >= high_threshold
    edge_times: list[float] = []
    falling_times: list[float] = []
    for time_s, voltage in zip(times[1:], volts[1:]):
        if high:
            now_high = voltage > low_threshold
        else:
            now_high = voltage >= high_threshold
        if now_high != high:
            edge_times.append(time_s)
            if high and not now_high:
                falling_times.append(time_s)
            high = now_high
    frequency = None
    if len(falling_times) >= 2:
        periods = [b - a for a, b in zip(falling_times, falling_times[1:]) if b > a]
        if periods:
            frequency = 1.0 / statistics.fmean(periods)
    elif len(edge_times) >= 4:
        periods = [b - a for a, b in zip(edge_times, edge_times[2:]) if b > a]
        if periods:
            frequency = 1.0 / statistics.fmean(periods)
    return len(edge_times), frequency


def find_pulse_candidates(
    channel: int,
    times: list[float],
    volts: list[float],
    *,
    threshold_v: float,
    min_vpp: float,
) -> list[PulseCandidate]:
    if len(times) < 2 or len(volts) < 2:
        return []
    baseline = statistics.median(volts)
    if (max(volts) - min(volts)) < min_vpp:
        return []

    active = [abs(value - baseline) >= threshold_v for value in volts]
    candidates: list[PulseCandidate] = []
    index = 0
    while index < len(active):
        if not active[index]:
            index += 1
            continue
        start = index
        while index + 1 < len(active) and active[index + 1]:
            index += 1
        end = index
        if end > start:
            segment_volts = volts[start : end + 1]
            seg_min = min(segment_volts)
            seg_max = max(segment_volts)
            high_excursion = seg_max - baseline
            low_excursion = baseline - seg_min
            direction = "high" if high_excursion >= low_excursion else "low"
            amplitude = max(high_excursion, low_excursion)
            if amplitude >= threshold_v:
                candidates.append(
                    PulseCandidate(
                        channel=channel,
                        start_s=times[start],
                        end_s=times[end],
                        width_s=max(0.0, times[end] - times[start]),
                        baseline_v=baseline,
                        min_v=seg_min,
                        max_v=seg_max,
                        amplitude_v=amplitude,
                        direction=direction,
                        sample_count=end - start + 1,
                    )
                )
        index += 1
    return candidates


def dmm_reference_for(channel: int, args: argparse.Namespace) -> float | None:
    return getattr(args, f"ch{channel}_dmm_v", None)


def build_scale_checks(
    stats: list[WaveformStats],
    args: argparse.Namespace,
) -> tuple[list[ScaleCheck], list[str]]:
    checks: list[ScaleCheck] = []
    warnings: list[str] = []
    for stat in stats:
        dmm_v = dmm_reference_for(stat.channel, args)
        if dmm_v is None or stat.sample_count == 0 or not math.isfinite(stat.mean_v) or abs(stat.mean_v) < 1e-9:
            continue
        ratio = dmm_v / stat.mean_v
        status = "OK" if 0.8 <= ratio <= 1.25 else "CSV_SCALE_SUSPECT"
        checks.append(
            ScaleCheck(
                channel=stat.channel,
                signal=stat.signal,
                dmm_reference_v=dmm_v,
                raw_mean_v=stat.mean_v,
                ratio=ratio,
                status=status,
            )
        )
        if status != "OK":
            warnings.append(
                f"CSV_SCALE_SUSPECT CH{stat.channel} {stat.signal}: "
                f"DMM={dmm_v:.4g} V, raw_mean={stat.mean_v:.4g} V, ratio={ratio:.3g}"
            )
    return checks, warnings


def classify_capture(
    stats: list[WaveformStats],
    pulses: dict[int, list[PulseCandidate]] | None = None,
    warnings: list[str] | None = None,
) -> str:
    ch2 = next((item for item in stats if item.channel == 2), None)
    if ch2 is None or ch2.sample_count == 0:
        return "PENDING_CAPTURE"
    ch2_pulses = (pulses or {}).get(2, [])
    if ch2.edge_count == 0 and not ch2_pulses:
        return "NO_LX_SWITCHING_DETECTED"
    if ch2_pulses and ch2.edge_count < 6:
        return "LX_STARTUP_ATTEMPT_THEN_STOP"
    ch3 = next((item for item in stats if item.channel == 3), None)
    if ch3 is not None and ch3.max_v < 4.5:
        return "LX_SWITCHING_BUT_VOUT_LOW"
    return "LX_SWITCHING_PRESENT"


def update_manifest(
    out_dir: Path,
    args: argparse.Namespace,
    scope_idn: str,
    results: list[CaptureResult],
) -> list[dict[str, Any]]:
    records = load_manifest(out_dir)

    generated_utc = datetime.now(timezone.utc).isoformat(timespec="seconds")
    for result in results:
        records.append(result_to_record(result, args, scope_idn, generated_utc))

    write_manifest(out_dir, records)
    return records


def load_manifest(out_dir: Path) -> list[dict[str, Any]]:
    manifest_path = out_dir / "capture_manifest.json"
    if not manifest_path.exists():
        return []
    try:
        loaded = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return []
    if not isinstance(loaded, list):
        return []
    return [item for item in loaded if isinstance(item, dict)]


def write_manifest(out_dir: Path, records: list[dict[str, Any]]) -> None:
    manifest_path = out_dir / "capture_manifest.json"
    manifest_path.write_text(json.dumps(records, indent=2), encoding="utf-8")


def reanalyze_records(records: list[dict[str, Any]], args: argparse.Namespace) -> list[dict[str, Any]]:
    reanalyzed: list[dict[str, Any]] = []
    for record in records:
        updated = dict(record)
        csv_path_text = str(updated.get("csv_path", ""))
        if csv_path_text:
            csv_path = Path(csv_path_text)
            if csv_path.exists():
                waveforms = load_waveforms_csv(csv_path)
                if waveforms:
                    stats, pulses = analyze_waveforms(waveforms, args)
                    scale_checks, warnings = build_scale_checks(stats, args)
                    updated["stats"] = [waveform_stat_to_record(stat) for stat in stats]
                    updated["pulse_candidates"] = {
                        str(channel): [pulse_to_record(pulse) for pulse in channel_pulses]
                        for channel, channel_pulses in pulses.items()
                    }
                    updated["scale_checks"] = [scale_check_to_record(check) for check in scale_checks]
                    updated["warnings"] = warnings
                    updated["classification"] = classify_capture(stats, pulses, warnings)
        reanalyzed.append(updated)
    return reanalyzed


def load_waveforms_csv(path: Path) -> dict[int, tuple[list[float], list[float]]]:
    waveforms: dict[int, tuple[list[float], list[float]]] = {}
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            return waveforms
        channels: list[int] = []
        for field in reader.fieldnames:
            if field.startswith("ch") and field.endswith("_time_s"):
                try:
                    channels.append(int(field[2 : field.index("_")]))
                except ValueError:
                    continue
        for channel in channels:
            waveforms[channel] = ([], [])
        for row in reader:
            for channel in channels:
                time_text = row.get(f"ch{channel}_time_s", "")
                voltage_text = row.get(f"ch{channel}_voltage_v", "")
                if time_text == "" or voltage_text == "":
                    continue
                try:
                    waveforms[channel][0].append(float(time_text))
                    waveforms[channel][1].append(float(voltage_text))
                except ValueError:
                    continue
    return {channel: waveform for channel, waveform in waveforms.items() if waveform[0] and waveform[1]}


def result_to_record(
    result: CaptureResult,
    args: argparse.Namespace,
    scope_idn: str,
    generated_utc: str,
) -> dict[str, Any]:
    return {
        "generated_utc": generated_utc,
        "board_revision": args.board_revision,
        "firmware_image": args.firmware_image,
        "battery_voltage": args.battery_voltage,
        "usb_state": args.usb_state,
        "scope_idn": scope_idn,
        "label": result.label,
        "mode": result.mode,
        "screenshot_path": str(result.screenshot_path) if result.screenshot_path else "",
        "csv_path": str(result.csv_path) if result.csv_path else "",
        "trigger_status": result.trigger_status or "",
        "classification": result.classification,
        "warnings": list(result.warnings),
        "notes": list(result.notes),
        "scope_settings": result.scope_settings,
        "scale_checks": [scale_check_to_record(check) for check in result.scale_checks],
        "pulse_candidates": {
            str(channel): [pulse_to_record(pulse) for pulse in channel_pulses]
            for channel, channel_pulses in result.pulses.items()
        },
        "stats": [waveform_stat_to_record(stat) for stat in result.stats],
    }


def waveform_stat_to_record(stat: WaveformStats) -> dict[str, Any]:
    return {
        "channel": stat.channel,
        "signal": stat.signal,
        "sample_count": stat.sample_count,
        "min_v": stat.min_v,
        "max_v": stat.max_v,
        "mean_v": stat.mean_v,
        "peak_to_peak_v": stat.peak_to_peak_v,
        "edge_count": stat.edge_count,
        "estimated_frequency_hz": stat.estimated_frequency_hz,
        "pulse_count": stat.pulse_count,
        "first_pulse_s": stat.first_pulse_s,
        "largest_pulse_v": stat.largest_pulse_v,
        "largest_pulse_width_s": stat.largest_pulse_width_s,
    }


def pulse_to_record(pulse: PulseCandidate) -> dict[str, Any]:
    return {
        "channel": pulse.channel,
        "start_s": pulse.start_s,
        "end_s": pulse.end_s,
        "width_s": pulse.width_s,
        "baseline_v": pulse.baseline_v,
        "min_v": pulse.min_v,
        "max_v": pulse.max_v,
        "amplitude_v": pulse.amplitude_v,
        "direction": pulse.direction,
        "sample_count": pulse.sample_count,
    }


def scale_check_to_record(check: ScaleCheck) -> dict[str, Any]:
    return {
        "channel": check.channel,
        "signal": check.signal,
        "dmm_reference_v": check.dmm_reference_v,
        "raw_mean_v": check.raw_mean_v,
        "ratio": check.ratio,
        "status": check.status,
    }


def write_report(path: Path, records: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    latest = records[-1] if records else {}
    lines = [
        "# IP5209 LX Scope Report",
        "",
        f"- Generated UTC: `{timestamp}`",
        f"- Board revision: `{latest.get('board_revision', 'unknown')}`",
        f"- Firmware image: `{latest.get('firmware_image', 'unknown')}`",
        f"- Battery voltage: `{latest.get('battery_voltage', 'unknown')}`",
        f"- USB state: `{latest.get('usb_state', 'unknown')}`",
        f"- Scope IDN: `{latest.get('scope_idn', 'unknown')}`",
        "",
        "## Probe Connections",
        "",
        "- CH1: L2 CSIN / battery side of boost inductor",
        "- CH2: L2 LX side / IP5209 LX switching node",
        "- CH3: +5V/VOUT when enabled",
        "- CH4: VREG or KEY when enabled",
        "- Scope ground: local board GND near U7/IP5209",
        "",
        "Safety notes: use a short ground spring or very short ground lead if possible. "
        "Scope ground is earth-referenced; connect only to board GND. "
        "Do not connect scope ground to LX, VBAT, VOUT, or any non-ground node. "
        "Start with 1x probes.",
        "",
        "## Voltage Scale Sanity",
        "",
        "Earlier CH1/CH2 CSV files reported CH2 near 7.8 V while the scope screen and DMM indicated "
        "about 3.9 V on the LX node. Treat raw CSV voltages as suspect until the per-capture scale "
        "checks below agree with DMM references. The analyzer now flags `CSV_SCALE_SUSPECT` instead "
        "of silently treating doubled CSV values as real hardware voltage.",
        "",
        "## Captures",
        "",
    ]
    for record in records:
        lines.extend(render_capture_record(record, path.parent))
    lines.extend(["", "## Conclusion", "", conclusion_for_records(records), ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def render_capture_record(record: dict[str, Any], report_dir: Path) -> list[str]:
    lines = [
        f"### {record.get('label', 'capture')}",
        "",
        f"- Captured UTC: `{record.get('generated_utc', 'unknown')}`",
        f"- Mode: `{record.get('mode', 'unknown')}`",
        f"- Classification: `{record.get('classification', 'unknown')}`",
        f"- Trigger status: `{record.get('trigger_status') or 'unknown'}`",
    ]
    screenshot_path = Path(str(record.get("screenshot_path", ""))) if record.get("screenshot_path") else None
    csv_path = Path(str(record.get("csv_path", ""))) if record.get("csv_path") else None
    if screenshot_path:
        lines.append(f"- Screenshot: [{screenshot_path.name}]({relative_link(screenshot_path, report_dir)})")
    if csv_path:
        lines.append(f"- Waveform CSV: [{csv_path.name}]({relative_link(csv_path, report_dir)})")
    for warning in record.get("warnings", []):
        lines.append(f"- Warning: `{warning}`")
    for note in record.get("notes", []):
        lines.append(f"- Note: {note}")
    scope_settings = record.get("scope_settings", {})
    if isinstance(scope_settings, dict) and scope_settings:
        lines.extend(
            [
                f"- Scope timebase: `{scope_settings.get('timebase_scale_s_div', 'unknown')}` s/div",
                f"- Scope trigger: source `{scope_settings.get('trigger_source', 'unknown')}`, "
                f"slope `{scope_settings.get('trigger_slope', 'unknown')}`, "
                f"level `{scope_settings.get('trigger_level', 'unknown')}`",
            ]
        )
    lines.extend(
        [
            "",
            "| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |",
            "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|",
        ]
    )
    stats = record.get("stats", [])
    for stat in stats:
        freq_value = stat.get("estimated_frequency_hz")
        freq = "" if freq_value is None else f"{float(freq_value):.3g}"
        lines.append(
            f"| CH{stat.get('channel')} | {stat.get('signal') or channel_signal(int(stat.get('channel', 0)))} | "
            f"{stat.get('sample_count')} | {float(stat.get('min_v')):.4g} | "
            f"{float(stat.get('max_v')):.4g} | {float(stat.get('mean_v')):.4g} | "
            f"{float(stat.get('peak_to_peak_v')):.4g} | {stat.get('edge_count')} | "
            f"{stat.get('pulse_count', 0)} | {freq} |"
        )
    if not stats:
        lines.append("| n/a | n/a | 0 | | | | | | | |")
    scale_checks = record.get("scale_checks", [])
    if scale_checks:
        lines.extend(
            [
                "",
                "| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |",
                "|---|---:|---:|---:|---|",
            ]
        )
        for check in scale_checks:
            lines.append(
                f"| CH{check.get('channel')} {check.get('signal', '')} | "
                f"{float(check.get('dmm_reference_v')):.4g} | {float(check.get('raw_mean_v')):.4g} | "
                f"{float(check.get('ratio')):.4g} | `{check.get('status')}` |"
            )
    pulse_candidates = record.get("pulse_candidates", {})
    if isinstance(pulse_candidates, dict):
        lx_pulses = pulse_candidates.get("2", [])
        if lx_pulses:
            lines.extend(
                [
                    "",
                    "| CH2 LX Pulse Candidate | Start s | Width s | Direction | Amplitude V | Min V | Max V |",
                    "|---|---:|---:|---|---:|---:|---:|",
                ]
            )
            for index, pulse in enumerate(lx_pulses[:10], start=1):
                lines.append(
                    f"| {index} | {float(pulse.get('start_s')):.6g} | {float(pulse.get('width_s')):.6g} | "
                    f"{pulse.get('direction')} | {float(pulse.get('amplitude_v')):.4g} | "
                    f"{float(pulse.get('min_v')):.4g} | {float(pulse.get('max_v')):.4g} |"
                )
            if len(lx_pulses) > 10:
                lines.append(f"| ... | | | | {len(lx_pulses) - 10} more candidates omitted | | |")
    lines.append("")
    return lines


def conclusion_for_records(records: list[dict[str, Any]]) -> str:
    classifications = {str(record.get("classification")) for record in records}
    modes = {str(record.get("mode")) for record in records}
    scale_suspect = any(
        "CSV_SCALE_SUSPECT" in str(warning)
        for record in records
        for warning in record.get("warnings", [])
    )
    scale_note = (
        " One or more captures have `CSV_SCALE_SUSPECT`; use the screenshot and DMM reference "
        "for absolute voltage until the scope CSV scaling is calibrated."
        if scale_suspect
        else ""
    )
    if records and modes <= {"idle"}:
        return (
            "Only idle baseline captures are present. No SW3 wake or steady-switching conclusion has been made yet. "
            "Run `wake-single` next to determine whether LX switches when IP5209 is asked to boost."
            + scale_note
        )
    if "LX_SWITCHING_BUT_VOUT_LOW" in classifications:
        return (
            "LX switching is present but VOUT does not reach 5 V. Focus next on L2 value/current rating, "
            "R9 sense path, U7 LX/VOUT soldering, output capacitors, and possible IP5209 power-stage damage."
            + scale_note
        )
    if "LX_SWITCHING_PRESENT" in classifications:
        return "LX switching is present. Compare VOUT and load behavior before changing hardware." + scale_note
    if "LX_STARTUP_ATTEMPT_THEN_STOP" in classifications:
        return (
            "LX starts switching briefly, then stops. Focus next on protection conditions, NTC voltage, "
            "light-load shutdown, current limit, and output load behavior."
            + scale_note
        )
    if "NO_LX_SWITCHING_DETECTED" in classifications:
        return (
            "LX did not switch after SW3 wake. IP5209 wakes VREG but does not enable boost switching. "
            "Focus next on KEY, NTC, RSET, LIGHT/VSET straps, IP5209 soldering, wrong/damaged chip, "
            "and register diagnostics under USB power."
            + scale_note
        )
    return "No waveform classification is available yet. Run idle and wake-single captures."


def relative_link(path: Path, report_dir: Path) -> str:
    try:
        return path.resolve().relative_to(report_dir.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def timestamped_label(suffix: str) -> str:
    return f"{datetime.now().strftime('%Y%m%d_%H%M%S')}_{suffix}"


def format_timebase(seconds: float) -> str:
    if seconds < 1e-6:
        return f"{seconds * 1e9:.0f}ns"
    return f"{seconds * 1e6:.0f}us"


if __name__ == "__main__":
    raise SystemExit(main())
