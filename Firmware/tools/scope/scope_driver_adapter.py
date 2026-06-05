#!/usr/bin/env python3
"""Small OWON/HANMATEK DOS1104 adapter for North Pole scope captures."""

from __future__ import annotations

from dataclasses import dataclass
import csv
import io
import sys
from pathlib import Path
from typing import Any, Iterable

try:
    from PIL import Image
except ImportError:  # pragma: no cover - optional runtime dependency
    Image = None


DEFAULT_INSTRUMENTKIT_SRC = (
    Path.home() / "Documents" / "VS code projects" / "InstrumentKit" / "src"
)


class ScopeAdapterError(RuntimeError):
    """Scope adapter operation failed."""


@dataclass(frozen=True)
class ScopeConnection:
    instrumentkit_src: Path = DEFAULT_INSTRUMENTKIT_SRC
    vid: int = 0x5345
    pid: int = 0x1234
    timeout_s: float = 2.0


@dataclass(frozen=True)
class ChannelConfig:
    channel: int
    enabled: bool
    scale_v_div: float
    position_div: float = 0.0
    probe_attenuation: int = 1
    coupling: str = "DC"


@dataclass(frozen=True)
class ChannelState:
    channel: int
    display: str = "unknown"
    coupling: str = "unknown"
    scale_v_div: str = "unknown"
    position_div: str = "unknown"
    probe_attenuation: str = "unknown"


class OwonScopeAdapter:
    """Thin wrapper around the proven InstrumentKit OWON SDS1104 USB driver."""

    def __init__(self, connection: ScopeConnection) -> None:
        self.connection = connection
        self.instrument: Any | None = None

    def __enter__(self) -> "OwonScopeAdapter":
        self.open()
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        self.close()

    def open(self) -> None:
        _add_instrumentkit_path(self.connection.instrumentkit_src)
        try:
            from instruments.owon.sds1104 import OWONSDS1104
            from instruments.units import ureg as u
        except Exception as exc:  # pragma: no cover - depends on local tool install
            raise ScopeAdapterError(
                "Failed to import InstrumentKit OWON SDS1104 driver. "
                f"Checked: {self.connection.instrumentkit_src}"
            ) from exc

        try:
            self.instrument = OWONSDS1104.open_usb(
                vid=self.connection.vid,
                pid=self.connection.pid,
                timeout=float(self.connection.timeout_s) * u.second,
            )
        except Exception as exc:
            raise ScopeAdapterError(
                "Failed to open OWON/HANMATEK scope over USB. "
                "If this times out, physically reboot/replug the scope before retrying."
            ) from exc

    def close(self) -> None:
        if self.instrument is None:
            return
        close_method = getattr(self.instrument, "close", None)
        if callable(close_method):
            try:
                close_method()
            except Exception:
                pass
        self.instrument = None

    def identify(self) -> str:
        reply = self.query("*IDN?")
        return reply.strip() or "unknown"

    def send(self, command: str) -> None:
        instrument = self._require_instrument()
        sendcmd = getattr(instrument, "sendcmd", None)
        if not callable(sendcmd):
            raise ScopeAdapterError("Scope driver does not expose sendcmd().")
        sendcmd(str(command))

    def query(self, command: str) -> str:
        instrument = self._require_instrument()
        self._flush_input_best_effort()
        query = getattr(instrument, "query", None)
        if not callable(query):
            raise ScopeAdapterError("Scope driver does not expose query().")
        return str(query(str(command)))

    def query_optional(self, command: str) -> str:
        try:
            return self.query(command).strip()
        except Exception:
            return "unknown"

    def run(self) -> None:
        action = getattr(self._require_instrument(), "run", None)
        if callable(action):
            action()
        else:
            self.send(":RUN")

    def single(self) -> None:
        action = getattr(self._require_instrument(), "single", None)
        if callable(action):
            action()
        else:
            self.send(":SINGle")

    def stop(self) -> None:
        action = getattr(self._require_instrument(), "stop", None)
        if callable(action):
            action()
        else:
            self.send(":STOP")

    def trigger_status(self) -> str | None:
        try:
            return self.query(":TRIGger:STATUS?").strip()
        except Exception:
            return None

    def setup_channel(self, config: ChannelConfig) -> None:
        proxy = self._channel_proxy(config.channel)
        if proxy is None:
            self.send(f":CH{config.channel}:DISPlay {'ON' if config.enabled else 'OFF'}")
            if config.enabled:
                self.send(f":CH{config.channel}:COUPling {config.coupling.upper()}")
                self.send(f":CH{config.channel}:SCALe {config.scale_v_div}")
                self.send(f":CH{config.channel}:POSition {config.position_div}")
                self.send(f":CH{config.channel}:PROBe {config.probe_attenuation}")
            return

        if hasattr(proxy, "display"):
            proxy.display = bool(config.enabled)
        if not config.enabled:
            return

        if hasattr(proxy, "coupling"):
            proxy.coupling = self._resolve_enum("Coupling", config.coupling)
        if hasattr(proxy, "scale"):
            proxy.scale = float(config.scale_v_div)
        if hasattr(proxy, "position"):
            proxy.position = float(config.position_div)
        if hasattr(proxy, "probe_attenuation"):
            proxy.probe_attenuation = int(config.probe_attenuation)

    def set_timebase_scale(self, seconds_per_div: float) -> None:
        instrument = self._require_instrument()
        applied = False
        errors: list[str] = []
        attr = getattr(instrument, "timebase_scale", None)
        if callable(attr):
            try:
                attr(float(seconds_per_div))
                applied = True
            except Exception as exc:
                errors.append(f"timebase_scale(): {exc}")
        elif attr is not None or hasattr(instrument, "timebase_scale"):
            try:
                setattr(instrument, "timebase_scale", float(seconds_per_div))
                applied = True
            except Exception as exc:
                errors.append(f"timebase_scale property: {exc}")

        # The HANMATEK/DOS1104 sometimes accepts the InstrumentKit property
        # path without changing the visible horizontal scale. Send the small
        # raw command set used by OWON-compatible scopes as an explicit backup.
        raw_value = f"{float(seconds_per_div):.12g}"
        for command in (
            f":TIMebase:SCALe {raw_value}",
            f":TIMEBASE:SCALE {raw_value}",
            f":HORizontal:SCALe {raw_value}",
        ):
            try:
                self.send(command)
                applied = True
            except Exception as exc:
                errors.append(f"{command}: {exc}")

        if not applied:
            raise ScopeAdapterError(
                "Failed to set scope timebase scale. " + "; ".join(errors)
            )

    def set_horizontal_offset(self, offset_div: float) -> None:
        instrument = self._require_instrument()
        applied = False
        errors: list[str] = []
        attr = getattr(instrument, "horizontal_offset", None)
        if callable(attr):
            try:
                attr(float(offset_div))
                applied = True
            except Exception as exc:
                errors.append(f"horizontal_offset(): {exc}")
        elif attr is not None or hasattr(instrument, "horizontal_offset"):
            try:
                setattr(instrument, "horizontal_offset", float(offset_div))
                applied = True
            except Exception as exc:
                errors.append(f"horizontal_offset property: {exc}")

        raw_value = f"{float(offset_div):.12g}"
        for command in (
            f":TIMebase:OFFSet {raw_value}",
            f":TIMEBASE:OFFSET {raw_value}",
            f":HORizontal:OFFSet {raw_value}",
        ):
            try:
                self.send(command)
                applied = True
            except Exception as exc:
                errors.append(f"{command}: {exc}")

        if not applied:
            raise ScopeAdapterError(
                "Failed to set scope horizontal offset. " + "; ".join(errors)
            )

    def set_acquire_mode(self, mode: str) -> None:
        normalized = str(mode).strip().lower()
        if normalized in {"sample", "samp", "sampled"}:
            token = "SAMPle"
        elif normalized in {"average", "avg"}:
            token = "AVERage"
        elif normalized in {"peak", "peak_detect", "peakdetect"}:
            token = "PEAK"
        else:
            raise ScopeAdapterError(f"Unsupported acquisition mode: {mode!r}")
        self.send(f":ACQUire:Mode {token}")

    def set_memory_depth(self, depth: int) -> None:
        token = _memory_depth_token(int(depth))
        self.send(f":ACQUIRE:DEPMEM {token}")

    def set_trigger_holdoff_ns(self, holdoff_ns: int) -> None:
        self.send(f":TRIGger:SINGle:HOLDoff {int(holdoff_ns)}ns")

    def configure_edge_trigger(
        self,
        *,
        source_channel: int,
        slope: str,
        level_v: float,
        sweep: str = "SINGle",
    ) -> None:
        slope_token = "FALL" if slope.lower().startswith("fall") else "RISE"
        self.send(":TRIGger:TYPE SINGle")
        self.send(":TRIGger:SINGle:MODE EDGE")
        self.send(f":TRIGger:SINGle:EDGE:SOURce CH{int(source_channel)}")
        self.send(":TRIGger:SINGle:EDGE:COUPling DC")
        self.send(f":TRIGger:SINGle:EDGE:SLOPe {slope_token}")
        self.send(f":TRIGger:SINGle:EDGE:LEVel {float(level_v)}V")
        self.send(f":TRIGger:SINGle:SWEEp {sweep}")

    def read_channel_state(self, channel: int) -> ChannelState:
        channel = int(channel)
        return ChannelState(
            channel=channel,
            display=self.query_optional(f":CH{channel}:DISPlay?"),
            coupling=self.query_optional(f":CH{channel}:COUPling?"),
            scale_v_div=self.query_optional(f":CH{channel}:SCALe?"),
            position_div=self.query_optional(f":CH{channel}:POSition?"),
            probe_attenuation=self.query_optional(f":CH{channel}:PROBe?"),
        )

    def read_scope_settings(self, channels: Iterable[int], *, waveform_metadata: str = "none") -> dict[str, Any]:
        settings = {
            "timebase_scale_s_div": self.query_optional(":HORizontal:SCALe?"),
            "horizontal_offset_div": self.query_optional(":HORizontal:OFFSet?"),
            "acquire_mode": self.query_optional(":ACQUire:Mode?"),
            "memory_depth": self.query_optional(":ACQUIRE:DEPMEM?"),
            "trigger_status": self.trigger_status() or "unknown",
            "trigger_type": self.query_optional(":TRIGger:TYPE?"),
            "trigger_source": self.query_optional(":TRIGger:SINGle:EDGE:SOURce?"),
            "trigger_slope": self.query_optional(":TRIGger:SINGle:EDGE:SLOPe?"),
            "trigger_level": self.query_optional(":TRIGger:SINGle:EDGE:LEVel?"),
            "trigger_sweep": self.query_optional(":TRIGger:SINGle:SWEEp?"),
            "channels": {
                str(int(channel)): self.read_channel_state(int(channel)).__dict__
                for channel in channels
            },
        }
        metadata_mode = str(waveform_metadata).strip().lower()
        if metadata_mode in {"screen", "both"}:
            settings["screen_waveform"] = self.read_waveform_metadata_summary("screen")
        if metadata_mode in {"deep", "both"}:
            settings["deep_memory"] = self.read_waveform_metadata_summary("deep")
        return settings

    def save_screenshot(self, requested_path: Path) -> Path | None:
        instrument = self._require_instrument()
        reader = getattr(instrument, "read_screen_bmp", None)
        if not callable(reader):
            return None

        self._flush_input_best_effort()
        payload = reader()
        requested_path.parent.mkdir(parents=True, exist_ok=True)
        suffix = requested_path.suffix.lower()
        if suffix == ".bmp":
            requested_path.write_bytes(payload)
            return requested_path
        if Image is None:
            fallback = requested_path.with_suffix(".bmp")
            fallback.write_bytes(payload)
            return fallback

        with Image.open(io.BytesIO(payload)) as image:
            if suffix == ".png":
                image.save(requested_path, format="PNG")
            elif suffix in {".jpg", ".jpeg"}:
                image.convert("RGB").save(requested_path, format="JPEG", quality=95)
            else:
                raise ScopeAdapterError(f"Unsupported screenshot suffix: {suffix}")
        return requested_path

    def read_waveform(self, channel: int) -> tuple[list[float], list[float]] | None:
        instrument = self._require_instrument()
        reader = getattr(instrument, "read_waveform", None)
        result: Any | None = None
        if callable(reader):
            try:
                result = reader(int(channel))
            except Exception:
                result = None

        if result is None:
            proxy = self._channel_proxy(channel)
            proxy_reader = getattr(proxy, "read_waveform", None) if proxy is not None else None
            if callable(proxy_reader):
                try:
                    result = proxy_reader()
                except Exception:
                    result = None

        if not isinstance(result, (list, tuple)) or len(result) != 2:
            return None

        times = [_coerce_scalar(value, "second") for value in _to_list(result[0])]
        volts = [_coerce_scalar(value, "volt") for value in _to_list(result[1])]
        count = min(len(times), len(volts))
        if count <= 0:
            return None
        return times[:count], volts[:count]

    def read_deep_waveform(self, channel: int) -> tuple[list[float], list[float]] | None:
        instrument = self._require_instrument()
        reader = getattr(instrument, "read_deep_memory_channel", None)
        if not callable(reader):
            return None
        self._flush_input_best_effort()
        try:
            result = reader(int(channel))
        except Exception:
            return None
        if not isinstance(result, (list, tuple)) or len(result) != 2:
            return None
        times = [_coerce_scalar(value, "second") for value in _to_list(result[0])]
        volts = [_coerce_scalar(value, "volt") for value in _to_list(result[1])]
        count = min(len(times), len(volts))
        if count <= 0:
            return None
        return times[:count], volts[:count]

    def read_waveform_metadata_summary(self, source: str) -> dict[str, Any]:
        instrument = self._require_instrument()
        if str(source).lower().startswith("deep"):
            reader = getattr(instrument, "read_deep_memory_metadata", None)
            label = "deep"
        else:
            reader = getattr(instrument, "read_waveform_metadata", None)
            label = "screen"
        if not callable(reader):
            return {"source": label, "available": False, "error": "driver method unavailable"}
        self._flush_input_best_effort()
        try:
            metadata = reader()
        except Exception as exc:
            return {
                "source": label,
                "available": False,
                "error": f"{type(exc).__name__}: {exc}",
            }
        return _summarize_waveform_metadata(label, metadata)

    def save_waveforms_csv(self, path: Path, waveforms: dict[int, tuple[list[float], list[float]]]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        channels = sorted(waveforms)
        max_count = max((len(waveforms[ch][0]) for ch in channels), default=0)
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle)
            header: list[str] = []
            for channel in channels:
                header.extend([f"ch{channel}_time_s", f"ch{channel}_voltage_v"])
            writer.writerow(header)
            for index in range(max_count):
                row: list[str | float] = []
                for channel in channels:
                    times, volts = waveforms[channel]
                    if index < len(times):
                        row.extend([times[index], volts[index]])
                    else:
                        row.extend(["", ""])
                writer.writerow(row)

    def _require_instrument(self) -> Any:
        if self.instrument is None:
            raise ScopeAdapterError("Scope is not open.")
        return self.instrument

    def _flush_input_best_effort(self) -> None:
        if self.instrument is None:
            return
        filelike = getattr(self.instrument, "_file", None)
        flush_method = getattr(filelike, "flush_input", None)
        if callable(flush_method):
            try:
                flush_method()
            except Exception:
                pass

    def _channel_proxy(self, channel: int) -> Any | None:
        instrument = self._require_instrument()
        channels = getattr(instrument, "channel", None)
        if channels is None:
            channels = getattr(instrument, "channels", None)
        if channels is None:
            return None
        try:
            return channels[int(channel) - 1]
        except Exception:
            return None

    def _resolve_enum(self, enum_name: str, raw_value: str) -> Any:
        enum_cls = getattr(self._require_instrument(), enum_name, None)
        if enum_cls is None:
            raise ScopeAdapterError(f"Scope driver does not expose {enum_name}.")
        wanted = str(raw_value).strip().upper()
        for member in enum_cls:
            if member.name.upper() == wanted or str(member.value).upper() == wanted:
                return member
        raise ScopeAdapterError(f"Unsupported {enum_name} value: {raw_value!r}")


def _add_instrumentkit_path(path: Path) -> None:
    if path.exists():
        text = str(path)
        if text not in sys.path:
            sys.path.insert(0, text)


def _memory_depth_token(depth: int) -> str:
    if depth in {1000, 1_000}:
        return "1K"
    if depth in {5000, 5_000}:
        return "5K"
    if depth in {10000, 10_000}:
        return "10K"
    if depth in {20000, 20_000}:
        return "20K"
    if depth in {100000, 100_000}:
        return "100K"
    if depth in {1000000, 1_000_000}:
        return "1M"
    if depth in {10000000, 10_000_000}:
        return "10M"
    raise ScopeAdapterError(f"Unsupported OWON memory depth: {depth}")


def _summarize_waveform_metadata(source: str, metadata: Any) -> dict[str, Any]:
    if not isinstance(metadata, dict):
        return {"source": source, "available": False, "error": "metadata is not a dict"}

    sample = metadata.get("SAMPLE")
    timebase = metadata.get("TIMEBASE")
    channels = metadata.get("CHANNEL")

    summary: dict[str, Any] = {
        "source": source,
        "available": True,
        "sample_rate": "unknown",
        "data_len": "unknown",
        "timebase": "unknown",
        "horizontal_offset": "unknown",
        "displayed_channels": [],
    }

    if isinstance(sample, dict):
        summary["sample_rate"] = sample.get("SAMPLERATE", "unknown")
        summary["data_len"] = sample.get("DATALEN", "unknown")
    if isinstance(timebase, dict):
        summary["timebase"] = timebase.get("SCALE", timebase.get("TIMEBASE", "unknown"))
        summary["horizontal_offset"] = timebase.get("HOFFSET", "unknown")
    if isinstance(channels, list):
        displayed = []
        for index, channel in enumerate(channels, start=1):
            if not isinstance(channel, dict):
                continue
            display = str(channel.get("DISPLAY", "")).upper()
            if display == "ON":
                displayed.append(
                    {
                        "channel": index,
                        "scale": channel.get("SCALE", "unknown"),
                        "offset": channel.get("OFFSET", "unknown"),
                        "probe": channel.get("PROBE", "unknown"),
                    }
                )
        summary["displayed_channels"] = displayed
    return summary


def _to_list(values: Any) -> list[Any]:
    if isinstance(values, list):
        return values
    if isinstance(values, tuple):
        return list(values)
    if isinstance(values, Iterable):
        return list(values)
    return []


def _coerce_scalar(value: Any, unit_hint: str) -> float:
    if hasattr(value, "to") and hasattr(value, "magnitude"):
        try:
            return float(value.to(unit_hint).magnitude)
        except Exception:
            return float(value.magnitude)
    if hasattr(value, "magnitude"):
        return float(value.magnitude)
    return float(value)
