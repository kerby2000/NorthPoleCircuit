# Motor Phase Model

The current PCB maps the DRV8837 inputs as:

| Driver | Ref | IN1 | IN2 | Sleep |
| --- | --- | --- | --- | --- |
| A | U10 | `/PWM_A1` | `/PWM_A2` | `/SLEEP` from PB0 |
| B | U11 | `/PWM_B1` | `/PWM_B2` | `/SLEEP` from PB0 |
| G | U9 | `/PWM_G1` | `/PWM_G2` | `/SLEEP` from PB0 |

PB0 controls the global DRV8837 `~SLEEP` net. The safe state is `/SLEEP` low and all six IN pins driven low, which keeps the DRV8837s asleep/coast.

## Next PCB Revision Notes

Rev-A target-board testing proved the static DRV8837 GPIO path for A and B:
the selected input reaches about 3.3 V, the opposite input stays low, and the
bridge output switches accordingly. The remaining implementation risk is the
mixed CH592 PWM resource mapping, especially A on TMR1/TMR2. A suspect scope
lead produced misleading flat captures during PWM bring-up, so future PWM
failures should first be checked against static GPIO output and probe integrity.

For the next PCB revision, prefer routing the main propulsion pair A/B to normal
CH592 PWMX-capable pins instead of timer pins:

- A/B are the propulsion phases and should have the easiest, most symmetric PWM implementation.
- G is the guard/centering rail and can tolerate the less convenient timer PWM path if pin count forces a tradeoff.
- Best case is all six DRV8837 input pins on normal PWMX-capable pads.
- Avoid using remapped `PB10/TMR1_` or `PB11/TMR2_` for motor control because those pads overlap the CH592 USB D-/D+ functions used on this board.

Do not cut current-board A traces until timer-PWM behavior has been isolated.
Static A GPIO and A bridge output behavior are already proven.

## Framework Behavior

- `board_init_safe_pins()` drives all six motor inputs low and PB0 `/SLEEP` low before any subsystem init.
- `motor_drv8837_arm()` coasts all inputs, drives `/SLEEP` high, waits the configured wake delay, then allows bounded commands.
- `motor_drv8837_off()` coasts all inputs, waits the configured settle delay, then drives `/SLEEP` low.
- `motor_drv8837_command()` rejects non-coast commands unless the motor system is armed.
- `motor_drv8837_command()` rejects duty above the active profile limit.
- `motor_track_step()` uses a quarter-wave sine lookup table for A/B phase drive and leaves G coasting for now.

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
