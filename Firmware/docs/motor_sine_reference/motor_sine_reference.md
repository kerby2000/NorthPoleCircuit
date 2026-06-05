# Motor Sine Reference

- Speed: `1` electrical cycles/s
- Amplitude: `50` permille
- `50` permille means `5%` duty, not `50%` duty.
- Table: 32 samples per electrical cycle
- Firmware step delay: about `31` ms per sample
- A phase: sample `phase`
- B phase: sample `phase + 8`, a 90-degree shift
- Positive sample drives `IN1` PWM and holds `IN2` low.
- Negative sample drives `IN2` PWM and holds `IN1` low.

![Motor sine reference](motor_sine_reference.png)

Scope hookup for the 4-channel check:

```text
CH1 = /PWM_A1, DRV A IN1
CH2 = /PWM_A2, DRV A IN2
CH3 = /PWM_B1, DRV B IN1
CH4 = /PWM_B2, DRV B IN2
Scope ground = board GND only
```

Alternate physical hookup used on the target board because it is easier to
reach around the PCB:

```text
CH1 = /PWM_B2, DRV B IN2
CH2 = /PWM_A2, DRV A IN2
CH3 = /PWM_B1, DRV B IN1
CH4 = /PWM_A1, DRV A IN1
```

Use `--channel-map ab-physical` with `motor_bridge_pwm_capture.py` for that
probe order.

Non-energizing firmware diagnostic:

```text
motor sine-diag-inputs 1 5000 AB
```

This uses the same 32-sample A/B phase sequence, but drives only logic-level
input pins and keeps DRV8837 `/SLEEP` low. It is intended to prove firmware
phase ordering on the scope before enabling bridge outputs.

Expected scope result for `sine-diag-inputs`: square/meander logic levels.
This command does not generate the stepped duty plot below because it does not
run the 20 kHz PWM carrier.

Expected scope result for `sine-demo`: 20 kHz PWM pulses whose duty follows the
stepped sine envelope shown in the reference plot.

Important scope interpretation:

- The reference image is generated from the firmware sine table and duty model.
- A normal slow 1 s/div scope screenshot of `sine-demo` will show narrow 20 kHz
  pulse clusters, not a smooth sine curve.
- With the bring-up safety limit, amplitude `50` means max `50` permille duty,
  so the carrier pulse is only about 5% high at the sine peak.
- `sine-demo` is intentionally limited by firmware bring-up safety. It drives
  the real DRV8837 bridges and should stay conservative until motor currents
  and thermal behavior are validated.
- Use `sine-diag-inputs` to prove phase ordering, and `sine-phase` to prove
  stable PWM carrier/duty on a specific input pin.

Scope-only high-visibility PWM diagnostic:

```text
motor sine-pwm-inputs <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]
```

This generates the same 20 kHz PWM carrier and stepped A/B sine duty sequence
as `sine-demo`, but forces DRV8837 `/SLEEP` low. The bridge outputs should stay
disabled, so this command can use a larger amplitude for oscilloscope
visibility.

To make the reference shape obvious on the scope, use `500` permille (`50%`
peak duty) or `800` permille (`80%` peak duty):

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-pwm-inputs --sine-target AB --channel-map ab-physical --speed-hz 1 --amplitude-permille 500 --duration-ms 1200 --timebase 100e-3 --stop-during-active-s 1.05 --no-waveform --verbose-shell
```

Expected result: a visible PWM duty envelope on the DRV input pins while
`/SLEEP` remains low.

Scope-screen reference visualization:

```text
motor sine-scope-plot <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]
```

This command is deliberately different from the real 20 kHz motor PWM. It keeps
DRV8837 `/SLEEP` low and stretches each 32-sample PWM slot to millisecond scale
so the oscilloscope screen can reproduce the reference bottom plot. Use it to
verify phase order and duty-envelope shape on the input pins. Do not treat it as
the production motor drive waveform.

For the current physical probe hookup, use maximum diagnostic amplitude:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-plot --sine-target AB --channel-map ab-physical --speed-hz 1 --amplitude-permille 1000 --duration-ms 1200 --timebase 100e-3 --stop-during-active-s 1.05 --no-waveform --verbose-shell
```

Expected result: the scope screen should show visible variable-width pulses on
the A/B input channels over roughly one electrical cycle. With the physical
hookup, channels are `CH1=B2`, `CH2=A2`, `CH3=B1`, `CH4=A1`.

Triggered paired-overlay capture:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-plot --sine-target AB --channel-map ab-physical --speed-hz 1 --amplitude-permille 1000 --duration-ms 1200 --acquire-mode single-trigger --trigger-channel 3 --trigger-slope rising --trigger-level 1.5 --trigger-sweep SINGle --scope-arm-mode single --scope-horizontal-offset-div 6 --scope-acquire-mode sample --scope-memory-depth 5000 --trigger-holdoff-ns 100 --pair-overlay --no-waveform --verbose-shell
```

This places the physical B pair on one baseline and the physical A pair on
another:

```text
CH1 B2 and CH3 B1 share the lower Y position.
CH2 A2 and CH4 A1 share the upper Y position.
```

The current physical overlay intentionally shifts the rows down from the first
successful capture so the full sequence uses the screen area more efficiently:

```text
CH1/CH3 B row position = -3.5 div
CH2/CH4 A row position = +0.5 div
```

That makes the screen read like the reference bottom plot: one visible B row
and one visible A row, instead of four separated traces. The trigger setup is
based on the working Magnetic-Levitation-Chess-Hardware scope workflow:

```text
single edge trigger
CH3/B1 rising edge by default for AB physical hookup
single arm mode with SINGle sweep; `run` is available but did not trigger
reliably in the 2026-06-03 NorthPole capture
100 ns trigger holdoff
explicit acquisition mode
explicit memory depth
horizontal offset to show the post-trigger waveform
```

If the interesting part appears off-screen, adjust only this value first:

```powershell
--scope-horizontal-offset-div 6
```

Try `4`, `6`, `8`, and `-6` if the scope firmware interprets offset direction
oppositely.

Continuous fast clock-div style scope visualization:

```text
motor sine-scope-clkdiv <fast_clkdiv> <amplitude_permille> <ms> [AB|A|B|G|all]
```

This is the same visual waveform generator as `sine-scope-plot`, but it repeats
the 32-sample sequence continuously for the requested duration. In this
diagnostic, `fast_clkdiv` maps directly to the width of one sine-table sample in
microseconds:

```text
one electrical cycle = 32 * fast_clkdiv us
smaller fast_clkdiv = same shape compressed into a shorter time window
```

Start with a slow compressed capture:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 1024 --amplitude-permille 1000 --duration-ms 5000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --verbose-shell
```

Then compress further:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 64 --amplitude-permille 1000 --duration-ms 5000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --verbose-shell
```

And the 20 us/div-style view:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 8 --amplitude-permille 1000 --duration-ms 5000 --timebase 50e-6 --display-channels 3 --scope-memory-depth 20000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --pair-overlay --no-waveform --verbose-shell
```

Recommended divider sweep:

```text
1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1
```

The high-speed `sine-scope-clkdiv` traces should have the same logical shape as
the slow scope plot, only compressed in time. This is true for the diagnostic
scope-plot commands because they intentionally generate visible GPIO pulses
instead of the real motor carrier. It is still diagnostic GPIO visualization
with `/SLEEP` low, not the production DRV8837 motor waveform.

Values below about `fast_clkdiv=256` are already approaching the practical
limit of the current software-GPIO visualization path. They are useful for
finding whether the pins still toggle and for scope setup experiments, but they
are not a production timing guarantee. A real high-rate production engine will
need a timer/PWM/DMA-style backend rather than shell-driven GPIO bit-banging.

That is different from `sine-demo` / `sine-pwm-inputs`: those use the real
20 kHz PWM carrier. On the scope, the real motor waveform is narrow carrier
pulses whose duty changes every sine-table sample. To see the duty envelope
clearly you either need a long timebase plus persistence/visual interpretation,
or a slow diagnostic like `sine-scope-plot`.

Scope sample-rate notes from the Magnetic-Levitation-Chess-Hardware bench:

- do not treat yellow versus white sample-rate text as a pass/fail signal; the
  local notes do not document that color meaning
- trust trigger status, visible waveform, timebase readback, numeric sample
  rate, and memory depth instead
- the DOS1104 sample rate depends on horizontal timebase, memory depth, and
  number of displayed channels
- for high-rate zooms, enable fewer channels with `--display-channels`; a
  single-channel capture can reach a higher sample rate than a four-channel
  overview
- the four-channel overview is best for phase/order; single-channel zoom is
  best for edge-shape or carrier timing
- `M:<time/div>` and the numeric sample rate are the useful readouts; the local
  notes do not define what white/yellow text color means on this scope
- if a high-speed screenshot still reports a low numeric rate, treat it as an
  undersampled screen capture even if the trigger fired

Useful high-speed zoom pattern:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 8 --amplitude-permille 1000 --duration-ms 5000 --timebase 50e-6 --display-channels 3 --scope-memory-depth 20000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --pair-overlay --no-waveform --verbose-shell
```

This sacrifices the full four-channel picture to increase sampling headroom on
one known-good trigger channel.

2026-06-04 continuous `sine-scope-clkdiv` evidence after flashing the
`Jun 4 2026 08:22:04` bring-up firmware:

```text
CH1 = B2
CH2 = A2
CH3 = B1
CH4 = A1
/SLEEP = low
amplitude = 1000 permille diagnostic visibility
```

The scope adapter now sends raw OWON/HANMATEK timebase commands after the
InstrumentKit property setter. Without that fallback, the DOS1104 sometimes
kept showing the old `M:5.0ms` scale even though the capture script requested a
smaller timebase.

| fast_clkdiv | Scope screen | Evidence | Interpretation |
|---:|---:|---|---|
| `1024` | `5 ms/div` | [20260604_212233_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_212233_sine-scope-clkdiv_ab_physical_AB.png) | Clean reference-like continuous A/B pattern. |
| `512` | `2 ms/div` | [20260604_213012_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_213012_sine-scope-clkdiv_ab_physical_AB.png) | Clean compressed continuous A/B pattern with zero horizontal offset. |
| `256` | `1 ms/div` | [20260604_212336_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_212336_sine-scope-clkdiv_ab_physical_AB.png) | Phase/order still visible, but software overhead is becoming visible. |
| `64` | `500 us/div` | [20260604_212439_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_212439_sine-scope-clkdiv_ab_physical_AB.png) | Qualitative compressed activity, not exact requested timing. |
| `16` | `100 us/div` | [20260604_212543_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_212543_sine-scope-clkdiv_ab_physical_AB.png) | Stress capture; CPU/GPIO overhead dominates the requested 16 us slot. |
| `8` | `50 us/div` | [20260604_212614_sine-scope-clkdiv_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_212614_sine-scope-clkdiv_ab_physical_AB.png) | Stress capture; proves toggling, not production-speed fidelity. |

The 5-second shell step counts show the overhead clearly:

```text
fast_clkdiv=1024 -> 3209 steps in 5 s, about 1.56 ms/step observed
fast_clkdiv=512  -> 4777 steps in 5 s, about 1.05 ms/step observed
fast_clkdiv=256  -> 6325 steps in 5 s, about 0.79 ms/step observed
fast_clkdiv=64   -> 8353 steps in 5 s, about 0.60 ms/step observed
fast_clkdiv=16   -> 9116 steps in 5 s, about 0.55 ms/step observed
fast_clkdiv=8    -> 9253 steps in 5 s, about 0.54 ms/step observed
```

So the current command is correct as a reference-shape visualization, but it
cannot prove a 20 kHz or 40 kHz production update engine. The next motor-control
firmware step is to move this A/B duty sequence into a timer/PWM-driven loop
and then repeat the same scope workflow with `/SLEEP` still held low.

2026-06-04 scope evidence with the target still running the older
`sine-scope-plot-us` firmware:

```text
Target firmware version observed:
version=0.1.0-bringup ... built=Jun  3 2026 19:17:36

motor sine-scope-clkdiv 1024 1000 250 AB
bad motor command
```

Because that firmware does not yet include `sine-scope-clkdiv`, the captures
below used the finite `sine-scope-plot-us` command. They still prove that the
same 32-sample A/B sign-and-duty pattern can be displayed on the real scope,
but at faster slots the finite command becomes too short for a perfectly
synchronized full-cycle overview. Flash the latest bring-up HEX before using
the continuous `sine-scope-clkdiv` workflow.

The sweep wrapper now checks this before touching the scope. On old firmware it
reports:

```text
version=0.1.0-bringup ... built=Jun  3 2026 19:17:36
bad motor command
Target firmware does not support 'motor sine-scope-clkdiv'.
```

| Slot width | Timebase | Evidence | Interpretation |
|---:|---:|---|---|
| `1024 us` | `5 ms/div` | [20260604_083055_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_083055_sine-scope-plot-us_ab_physical_AB.png) | Clean repeated reference-like A/B overview. |
| `512 us` | `2 ms/div` | [20260604_083504_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_083504_sine-scope-plot-us_ab_physical_AB.png) | Clean repeated reference-like A/B overview. |
| `256 us` | `1 ms/div` | [20260604_083634_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_083634_sine-scope-plot-us_ab_physical_AB.png) | Clean compressed overview; numeric scope rate showed `250 KS/s`. |
| `64 us` | `200 us/div` | [20260604_084059_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_084059_sine-scope-plot-us_ab_physical_AB.png) | Useful zoom, but not a full-cycle overview because the command is finite and trigger alignment is not continuous. |
| `32 us` | `100 us/div` | [20260604_084316_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_084316_sine-scope-plot-us_ab_physical_AB.png) | Fast zoom proving pulses remain present. |
| `16 us` | `50 us/div` | [20260604_084438_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_084438_sine-scope-plot-us_ab_physical_AB.png) | Fast zoom. |
| `8 us` | `50 us/div` | [20260604_084536_sine-scope-plot-us_ab_physical_AB.png](../motor_pwm_scope_evidence/20260604_084536_sine-scope-plot-us_ab_physical_AB.png) | Fast zoom; use continuous firmware for non-stop overview captures. |

Metadata readback can be enabled after a capture when checking sample-rate
behavior:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 8 --amplitude-permille 1000 --duration-ms 5000 --timebase 50e-6 --display-channels 3 --scope-memory-depth 20000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --pair-overlay --no-waveform --read-waveform-metadata --waveform-metadata-source screen --verbose-shell
```

Use `--waveform-metadata-source deep` or `--waveform-source deep` only when the
scope USB link is healthy. On 2026-06-03, querying deep metadata before arming
the scope wedged the DOS1104 USB protocol; normal captures now avoid metadata
queries before triggering.

Stable PWM phase-hold diagnostic:

```text
motor sine-phase <phase_0_31> <amplitude_permille> <ms> [AB|A|B|G|all]
```

With `AB` target and the physical scope hookup, these phases are most useful:

| Phase | Expected active input | Physical scope channel |
|---:|---|---|
| 0 | B1 PWM max, A off/zero | CH3 |
| 8 | A1 PWM max, B off/zero | CH4 |
| 16 | A2 PWM max, B off/zero | CH2 |
| 24 | B2 PWM max, A off/zero | CH1 |

Example carrier capture after flashing the latest bring-up HEX:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-phase --sine-target AB --channel-map ab-physical --phase 8 --amplitude-permille 50 --duration-ms 1200 --timebase 20e-6 --stop-during-active-s 0.12 --no-waveform --verbose-shell
```

Repeat with `--phase 0`, `--phase 16`, and `--phase 24` to verify B1, A2, and
B2 respectively. These are fast carrier captures, not full-cycle envelope
captures.

Automated capture for the physical hookup:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-diag-inputs --sine-target AB --channel-map ab-physical --speed-hz 1 --duration-ms 5000
```
