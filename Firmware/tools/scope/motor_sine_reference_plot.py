#!/usr/bin/env python3
"""Generate reference plots for the NorthPole motor sine-demo waveform."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt


SINE_Q15 = [
    0, 6393, 12540, 18204, 23170, 27245, 30273, 32137,
    32767, 32137, 30273, 27245, 23170, 18204, 12540, 6393,
    0, -6393, -12540, -18204, -23170, -27245, -30273, -32137,
    -32767, -32137, -30273, -27245, -23170, -18204, -12540, -6393,
]


def duty_permille(sample: int, amplitude_permille: int) -> float:
    return abs(sample) * amplitude_permille / 32767.0


def split_drive(sample: int, amplitude_permille: int) -> tuple[float, float, float]:
    duty = duty_permille(sample, amplitude_permille)
    if sample > 0:
        return duty, 0.0, duty
    if sample < 0:
        return 0.0, duty, -duty
    return 0.0, 0.0, 0.0


def build_waveforms(amplitude_permille: int) -> dict[str, list[float]]:
    phases = list(range(33))
    a_samples = [SINE_Q15[p & 31] for p in phases]
    b_samples = [SINE_Q15[(p + 8) & 31] for p in phases]

    a1: list[float] = []
    a2: list[float] = []
    b1: list[float] = []
    b2: list[float] = []
    a_signed: list[float] = []
    b_signed: list[float] = []
    for a, b in zip(a_samples, b_samples):
        a_pos, a_neg, a_value = split_drive(a, amplitude_permille)
        b_pos, b_neg, b_value = split_drive(b, amplitude_permille)
        a1.append(a_pos)
        a2.append(a_neg)
        b1.append(b_pos)
        b2.append(b_neg)
        a_signed.append(a_value)
        b_signed.append(b_value)
    return {
        "phase": phases,
        "A_signed": a_signed,
        "B_signed": b_signed,
        "A1": a1,
        "A2": a2,
        "B1": b1,
        "B2": b2,
    }


def stylized_pwm(duties: list[float], amplitude_permille: int, pulses_per_sample: int) -> tuple[list[float], list[float]]:
    x: list[float] = []
    y: list[float] = []
    high = 1.0
    for sample_index, duty in enumerate(duties[:-1]):
        duty_fraction = 0.0 if amplitude_permille == 0 else max(0.0, min(1.0, duty / amplitude_permille))
        for pulse in range(pulses_per_sample):
            start = sample_index + pulse / pulses_per_sample
            high_end = start + duty_fraction / pulses_per_sample
            end = sample_index + (pulse + 1) / pulses_per_sample
            x.extend([start, start, high_end, high_end, end])
            y.extend([0.0, high, high, 0.0, 0.0])
    return x, y


def generate_plot(out_path: Path, amplitude_permille: int, speed_hz: int, pulses_per_sample: int) -> None:
    wave = build_waveforms(amplitude_permille)
    phase = wave["phase"]

    fig, axes = plt.subplots(4, 1, figsize=(13, 9), sharex=True)
    fig.suptitle(
        f"NorthPole sine-demo reference: {speed_hz} Hz, amplitude {amplitude_permille} permille",
        fontsize=14,
    )

    axes[0].plot(phase, wave["A_signed"], color="tab:orange", label="A signed duty")
    axes[0].plot(phase, wave["B_signed"], color="limegreen", label="B signed duty, +90 deg")
    axes[0].axhline(0, color="0.55", linewidth=0.8)
    axes[0].set_ylabel("Signed duty\npermille")
    axes[0].legend(loc="upper right")
    axes[0].grid(True, alpha=0.25)

    axes[1].step(phase, wave["A1"], where="post", color="red", label="A1 / PWM_A1")
    axes[1].step(phase, wave["A2"], where="post", color="blue", label="A2 / PWM_A2")
    axes[1].set_ylabel("A input duty\npermille")
    axes[1].legend(loc="upper right")
    axes[1].grid(True, alpha=0.25)

    axes[2].step(phase, wave["B1"], where="post", color="orange", label="B1 / PWM_B1")
    axes[2].step(phase, wave["B2"], where="post", color="green", label="B2 / PWM_B2")
    axes[2].set_ylabel("B input duty\npermille")
    axes[2].legend(loc="upper right")
    axes[2].grid(True, alpha=0.25)

    a1_x, a1_y = stylized_pwm(wave["A1"], amplitude_permille, pulses_per_sample)
    a2_x, a2_y = stylized_pwm(wave["A2"], amplitude_permille, pulses_per_sample)
    axes[3].plot(a1_x, a1_y, color="red", linewidth=0.8, label="A1 stylized PWM")
    axes[3].plot(a2_x, a2_y, color="blue", linewidth=0.8, label="A2 stylized PWM")
    axes[3].plot(phase, [0.5 + 0.5 * math.sin(2.0 * math.pi * p / 32.0) for p in phase],
                 color="0.45", linewidth=1.0, label="A envelope guide")
    axes[3].set_ylabel("A input\nlogic")
    axes[3].set_xlabel("Firmware sine table phase sample, 32 samples per electrical cycle")
    axes[3].legend(loc="upper right")
    axes[3].grid(True, alpha=0.25)

    for ax in axes:
        ax.set_xlim(0, 32)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def write_report(out_dir: Path, image_path: Path, amplitude_permille: int, speed_hz: int) -> None:
    step_ms = max(1, 1000 // (speed_hz * 32))
    report = out_dir / "motor_sine_reference.md"
    report.write_text(
        "\n".join(
            [
                "# Motor Sine Reference",
                "",
                f"- Speed: `{speed_hz}` electrical cycles/s",
                f"- Amplitude: `{amplitude_permille}` permille",
                "- Table: 32 samples per electrical cycle",
                f"- Firmware step delay: about `{step_ms}` ms per sample",
                "- A phase: sample `phase`",
                "- B phase: sample `phase + 8`, a 90-degree shift",
                "- Positive sample drives `IN1` PWM and holds `IN2` low.",
                "- Negative sample drives `IN2` PWM and holds `IN1` low.",
                "",
                f"![Motor sine reference]({image_path.name})",
                "",
                "Scope hookup for the 4-channel check:",
                "",
                "```text",
                "CH1 = /PWM_A1, DRV A IN1",
                "CH2 = /PWM_A2, DRV A IN2",
                "CH3 = /PWM_B1, DRV B IN1",
                "CH4 = /PWM_B2, DRV B IN2",
                "Scope ground = board GND only",
                "```",
                "",
            ]
        ),
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate motor sine-demo reference plots")
    parser.add_argument("--speed-hz", type=int, default=1)
    parser.add_argument("--amplitude-permille", type=int, default=50)
    parser.add_argument("--pulses-per-sample", type=int, default=4)
    parser.add_argument("--out-dir", type=Path, default=Path("Firmware/docs/motor_sine_reference"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image_path = args.out_dir / "motor_sine_reference.png"
    generate_plot(image_path, args.amplitude_permille, args.speed_hz, args.pulses_per_sample)
    write_report(args.out_dir, image_path, args.amplitude_permille, args.speed_hz)
    print(image_path)
    print(args.out_dir / "motor_sine_reference.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
