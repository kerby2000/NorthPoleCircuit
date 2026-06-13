# Motion Smoothness Tuning

This page tracks the move from coarse 32-step motor motion to the original-like
NorthPole approach:

- A/B sine phases are 90 degrees apart.
- The runtime wave engine uses a 32-bit phase accumulator and 256 phase
  positions per electrical cycle.
- The control update tick is fixed, default `4000 Hz`, so low electrical speeds
  do not collapse into audible 32-step updates.
- The guard rail defaults to fixed inward force instead of a third sine phase.
- Motion start/stop and speed changes ramp instead of jumping immediately.

## Baseline Command

Keep the coarse path as the regression reference:

```text
motor wave-run 3000 1000 20000 all sleep1 fwd guard-fwd
```

That command uses the proven 32-position timing path. The smoother path is
isolated behind `wave-run-smooth` and `motion`, so a failed smooth test should
not invalidate the moving baseline.

The first smooth-motion checkpoint should use the known-moving preset while the
motor is stopped:

```text
motion tune proven
motion tune status
motion start
```

The `proven` preset maps to the command that moved the sled on Rev-A:

```text
carrier_hz=20000
control_update_hz=4000
sine_table_size=256
speed_target_hz_x1000=3000
amplitude_target=1000
guard=forward
guard_duty=1000
ramp_start_ms=0
ramp_stop_ms=0
ramp_speed_ms=0
```

Use the original-like preset only after the `proven` preset is repeatable:

```text
motion tune original-like
motion tune status
motion start
```

The preset sets:

```text
carrier_hz=100000
control_update_hz=4000
sine_table_size=256
speed_target_hz_x1000=8000
amplitude_target=700
guard=forward
guard_duty=600
ramp_start_ms=500
ramp_stop_ms=300
ramp_speed_ms=500
```

Startup ramps both amplitude and speed from zero toward these targets. This is
important for the `8 Hz` original-like preset: jumping directly to full target
speed from rest failed to capture the sled reliably.

Bench note: on 2026-06-12 the `original-like` preset reported `running=1`,
`last_rc=0`, and advancing update ticks, but did not visibly move the sled. That
points to tuning/force limits rather than a parser or zero-output software bug.
The `proven` preset is the production checkpoint until the higher-carrier
original-like tuning is made to move reliably.

Implementation note: the low-level smooth wave engine cannot be started with
zero electrical frequency. The motion controller therefore arms the timer with
the target direction/frequency and zero amplitude, then immediately lets the
normal ramp update pull the effective speed and amplitude from zero toward the
target.

`motion stop` ramps amplitude down before disabling `/SLEEP`; it does not erase
the configured amplitude target for the next `motion start`.
`motor off` is still the immediate emergency stop.

## Tuning Commands

```text
motion tune status
motion tune proven
motion tune original-like
motion tune carrier <hz>
motion tune update <hz>
motion tune speed <signed_electrical_hz_x1000>
motion tune amplitude <permille>
motion tune guard <off|forward|reverse|phase-a|phase-b> <permille>
motion tune ramp <start_ms> <stop_ms> <speed_ms>
motor wave-run-smooth <electrical_hz_x1000> <amplitude_permille> <ms> all sleep1 fwd guard-fwd
```

Carrier changes and update-rate changes are intentionally only accepted while
motion is stopped. This avoids changing timer periods mid-run.

## Test Matrix

| Test | Carrier | Update Hz | Sine Samples | Electrical Hz | Amp | Guard | Result |
|---|---:|---:|---:|---:|---:|---|---|
| Current coarse reference | 20000 | 96 effective at 3 Hz | 32 | 3 | 1000 | fixed | Jerky/audible |
| Proven smooth checkpoint | 20000 | 4000 | 256 | 3 | 1000 | fixed 1000 | Moves on Rev-A |
| Carrier comparison | 20000 vs 100000 | 4000 | 256 | 3-10 | 1000 | fixed 1000 | No obvious sound/current improvement from higher carrier |
| Low-speed comparison | 20000/100000 | 4000 | 256 | 2 | 1000 | fixed 1000 | Visually jerkier than faster settings |
| Faster comparison | 20000/100000 | 4000 | 256 | 10 | 1000 | fixed 1000 | Moves, still audible |
| Reduced amplitude | 20000/100000 | 4000 | 256 | 3-10 | <800 | fixed 1000 | Too weak to move reliably on current sled |
| Higher update comparison | 100000 | 8000 | 256 | 8 | 1000 | fixed 1000 | TBD |
| Guard duty comparison | 40000 | 8000 | 256 | 8 | 1000 | fixed reverse 600 vs 900 | 600 quiet; 900 high-pitch/audible |
| Reverse | 100000 | 4000 | 256 | -8 | 700 | fixed 600 | TBD |

For each run, record:

- Short video.
- USB current.
- Whether the sled stalls, jumps, or runs continuously.
- Audible noise character.
- DRV8837 and coil heating after the run.
- `motion tune status` output before and during the run.

## Current Conclusions

2026-06-13 bench observations on Rev-A:

- Raising PWM carrier from `20 kHz` to `100 kHz` did not materially improve
  audible noise, visual smoothness, or USB current.
- Raising the control update tick from `4000 Hz` to `8000 Hz` removed the
  high-pitch audio component while keeping motion working. This shows that the
  audible high-pitch component was not mainly carrier-frequency related.
- Follow-up testing isolated the remaining high-pitch sound to guard rail
  duty. With `40 kHz` carrier and `8000 Hz` control update, this earlier
  polarity test was quiet:

  ```text
  motor wave-run-smooth 8000 1000 5000 all sleep1 fwd guard-rev guard-duty 600
  ```

  The same test with `guard-duty 900` produced audible high-pitch noise:

  ```text
  motor wave-run-smooth 8000 1000 5000 all sleep1 fwd guard-rev guard-duty 900
  ```

  Treat `600 permille` as the quiet guard-duty baseline regardless of polarity.
  Increase toward `700` only if the sled starts leaving the guide. Values
  around `800-1000` should be considered high-force/noisy for this Rev-A board.
- Repeating the high-duty guard test with the sled removed did not remove the
  sound. That means the noise is not only sled/rail friction. The remaining
  sound is likely electrical or electromagnetic: guard coil/PCB vibration,
  DRV8837/output ripple, or supply/load interaction under high guard current.
- USB current stayed below about `1 A`, typically around `0.8 A`.
- DRV8837 heating was moderate in short runs, with thermal images around the
  low `40 C` range.
- Lower electrical speed around `2 Hz` looked more visibly jerky than faster
  settings around `10 Hz`.
- Reducing A/B amplitude below about `800 permille` made force too weak for
  reliable sled motion.
- The current sled needs full or near-full A/B drive for repeatable motion, so
  the next useful tuning axis is not carrier frequency. Focus on electrical
  speed, guard force/polarity, update rate, and mechanical friction/contact.

Practical tuning stance:

- Keep A/B amplitude at `1000 permille` for now.
- Latest physical check moved the practical polarity back to:
  `fwd guard-fwd`.
- Use `40 kHz` carrier and `8000 Hz` control update as the current quiet
  baseline unless scope/current/heat later shows a reason to prefer different
  values.
- Use `guard-fwd guard-duty 600` as the current quiet guard baseline unless
  a specific sled/track placement proves otherwise.
- Treat audible noise as electrical/electromagnetic guard-load noise until
  proven otherwise. Sled/rail friction is not required because the high-duty
  sound remains when the sled is removed.

## Guard Rail Drive

The fixed guard rail is not bit-banged.

G1/G2 are driven through hardware PWMX channels:

```text
G1 = /PWM_G1 = pad 30 = PWM5
G2 = /PWM_G2 = pad 31 = PWM4
```

For the latest current moving polarity:

```text
guard-fwd guard-duty 600
```

the firmware commands a constant positive guard sample. That maps to:

```text
G1 = hardware PWM at current carrier frequency, 60% duty
G2 = low
```

For `guard-rev`, the polarity is reversed:

```text
G2 = hardware PWM at current carrier frequency
G1 = low
```

The guard PWM frequency is the motor carrier frequency selected by
`motion tune carrier <hz>` or by the `motor wave-*` command path. In the current
quiet baseline this is `40000 Hz`.

The `motion tune update 8000` value is a control update tick. It recomputes and
refreshes the A/B wave state at `8000 Hz`, but it is not the guard PWM waveform
itself. With fixed guard modes (`guard-fwd`/`guard-rev`), the guard duty is
applied when the wave starts or when guard settings change. It is not rewritten
on every A/B phase update. Only the diagnostic modes `guard-a` and `guard-b`
phase-update the guard channel.

## Original Track Control Comparison

The original `Code/Track_Control` implementation is simpler and more direct:

```text
TRACK_PWM_FREQ       = 100000 Hz
TRACK_PWM_POWER_PCT  = 95
GUARD_PWM_POWER_PCT  = 60
```

The guard rail is configured as a fixed PWM duty. In `track_enable()`, the
original code writes:

```c
TIM2->CH3CVR = GUARD_DUTY;
```

It does not step the guard phase during normal motion. The TIM2 interrupt fires
at the PWM carrier rate, but the interrupt only advances the A/B sine phase
after a software divider:

```text
phase step rate = 100000 Hz / (BASE_INTERVAL / abs(speed))
```

With original speed values:

```text
speed=20 -> about 2000 phase steps/s, approximately 7.8 electrical Hz
speed=35 -> about 3500 phase steps/s, approximately 13.7 electrical Hz
speed=50 -> about 5000 phase steps/s, approximately 19.5 electrical Hz
```

The original C condition is `speed_count > step_interval`, so the measured
value may be one PWM carrier tick slower than the ideal formula:

```text
speed=20 -> 100000 / 51 = 1961 phase steps/s
speed=35 -> 100000 / 29 = 3448 phase steps/s
speed=50 -> 100000 / 21 = 4762 phase steps/s
```

Our smooth engine instead uses a 32-bit phase accumulator with an independent
control update rate. For example:

```text
motion tune update 8000
motor wave-run-smooth 8000 ...    -> 8000 control updates/s
electrical speed 8 Hz             -> 8 * 256 = 2048 ideal phase-index steps/s
electrical speed 2 Hz             -> 2 * 256 = 512 ideal phase-index steps/s
electrical speed 10 Hz            -> 10 * 256 = 2560 ideal phase-index steps/s
```

So the original guard behavior is best modeled as:

```text
G rail = fixed 100 kHz PWM, 60% duty
A/B    = 100 kHz PWM carrier with 2-5 kHz phase-table updates
```

This matches the current observation that `guard-duty 600` is quiet and
`guard-duty 900-1000` is noisy: the original design never appears to run the
guard that hard by default.

## Scope Notes

To inspect coil outputs, use output-side probing, not just MCU PWM inputs.
For one direction, the useful three-channel set is:

```text
CH1 = B2_OUT
CH2 = G1_OUT
CH3 = A1_OUT
Scope GND = board GND
```

With fixed guard enabled, G should behave like a fixed inward force. A/B carry
the moving 90-degree phase relationship.

## Safety

- Keep initial runs short enough to observe current and heating.
- Stop with `motion stop` for a ramped stop.
- Stop immediately with `motor off`.
- Do not expose raw motor commands through BLE until bench behavior is stable.
