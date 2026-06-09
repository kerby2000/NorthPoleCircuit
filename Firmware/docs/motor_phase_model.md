# Motor Phase Model

The current PCB maps the DRV8837 inputs as:

| Driver | Ref | IN1 | IN2 | Sleep |
| --- | --- | --- | --- | --- |
| A | U10 | `/PWM_A1` | `/PWM_A2` | `/SLEEP` from PB0 |
| B | U11 | `/PWM_B1` | `/PWM_B2` | `/SLEEP` from PB0 |
| G | U9 | `/PWM_G1` | `/PWM_G2` | `/SLEEP` from PB0 |

PB0 controls the global DRV8837 `~SLEEP` net. The safe state is `/SLEEP` low and all six IN pins driven low, which keeps the DRV8837s asleep/coast.

## A/B Phases vs A1/A2 Bridge Legs

The physical propulsion phases are the PCB track phases `A` and `B`. They are
not `A1` and `A2`.

Firmware naming:

```text
A  phase = DRV A bridge output pair A1_OUT/A2_OUT
B  phase = DRV B bridge output pair B1_OUT/B2_OUT
A1/A2    = the two DRV8837 input legs used to choose A-phase polarity
B1/B2    = the two DRV8837 input legs used to choose B-phase polarity
```

For motion, drive the `AB` target. The firmware applies:

```text
A = sine phase
B = sine phase + 90 degrees
```

Positive A current uses A1/PWM_A1 and holds A2 low. Negative A current uses
A2/PWM_A2 and holds A1 low. B does the same with B1/B2, shifted by 90 degrees.

This matches the public NorthPole discussion and `A_B_diagram.png`: the track
is a two-phase A/B system similar to a stepper motor. A1/A2 are only the H-bridge
polarity controls for phase A.

## G Guard Phase

The `G` bridge is physically separate from the A/B propulsion phases. It is
intended for guard/centering behavior, not as a third propulsion sine phase.

Current firmware behavior in the live wave engine:

```text
target AB  -> A = sine phase, B = sine phase + 90 degrees, G off
target all -> A = sine phase, B = sine phase + 90 degrees, G fixed guard by default
```

The default guard mode for `all` is now `guard-fwd`: G1 is driven with a fixed
PWM duty and G2 stays low. This is intended to create a steady inward guard
force while A/B generate the travelling propulsion field. If the physical guard
force is the wrong direction on a particular board/fixture, use `guard-rev`.

Available guard modes:

```text
guard-off  -> G disabled
guard-fwd  -> fixed positive G drive, G1 PWM / G2 low
guard-rev  -> fixed negative G drive, G2 PWM / G1 low
guard-a    -> diagnostic only, G follows A sine phase
guard-b    -> diagnostic only, G follows B sine phase
```

The older first-motion command used G as a sine phase. That proved the hardware
could move the sledge, but it is no longer the default guardrail control law.

## Next PCB Revision Notes

Rev-A target-board testing proved the static DRV8837 GPIO path for A and B:
the selected input reaches about 3.3 V, the opposite input stays low, and the
bridge output switches accordingly. The remaining implementation risk is the
mixed CH592 PWM resource mapping, especially A on TMR1/TMR2. A suspect scope
lead produced misleading flat captures during PWM bring-up, so future PWM
failures should first be checked against static GPIO output and probe integrity.

For the next PCB revision, keep the proven A mapping on `TMR2/TMR1` unless a
better MCU/peripheral map is selected. Rev-A testing proved:

- A static GPIO and bridge-output behavior work.
- A timer PWM works.
- A1/A2 can be driven by TMR2/TMR1 DMA in `wave-dma-a`.

The local CH592 SDK/SFR headers only expose timer DMA for `TMR1/TMR2`.
`TMR0/TMR3` can generate PWM outputs, but they do not expose DMA registers or a
DMA API on CH592. Do not move B1/B2 to `TMR0/TMR3` expecting a second DMA phase.

For Rev-B, the practical options are:

- keep A on `TMR2/TMR1` for DMA experiments,
- keep B/G on PWMX pins and update them from the scheduler,
- or choose a different pin/peripheral plan only if the reference manual proves
  additional DMA-fed PWM channels.

Avoid using remapped `PB10/TMR1_` or `PB11/TMR2_` for motor control because
those pads overlap the CH592 USB D-/D+ functions used on this board.

Do not cut current-board A traces for PWM reasons. Static A GPIO, A bridge
output behavior, A timer PWM, and A DMA have all been proven.

## Framework Behavior

- `board_init_safe_pins()` drives all six motor inputs low and PB0 `/SLEEP` low before any subsystem init.
- `motor_drv8837_arm()` coasts all inputs, drives `/SLEEP` high, waits the configured wake delay, then allows bounded commands.
- `motor_drv8837_off()` coasts all inputs, waits the configured settle delay, then drives `/SLEEP` low.
- `motor_drv8837_command()` rejects non-coast commands unless the motor system is armed.
- `motor_drv8837_command()` rejects duty above the active profile limit.
- `motor_track_step()` uses a quarter-wave sine lookup table for A/B phase drive and leaves G coasting for now.
- `motion_control` runs continuous A/B motion with `/SLEEP` high and a fixed
  G guard while the RUN touch pad is enabled.
- SPD+ increases signed electrical frequency. SPD- decreases it; when the value
  crosses zero, direction reverses and frequency grows in the opposite sign.

The sine model is intentionally conservative. It is a phase-order test hook, not a tuned motion controller.

## Bring-Up Order

1. Confirm all motor inputs are low after reset.
2. Scope each input with no sled movement expected.
3. Arm for a short window.
4. Command one driver at a time with very low duty.
5. Confirm current and temperature.
6. Only then test A/B phase stepping.
7. Add G behavior after A/B movement is understood.

Measured limits should be copied into `app_config.h` before production scene work starts.
