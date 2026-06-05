# Logic Analyzer Tests

Use a current-limited supply. For PWM waveform tests, build with:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup -ExtraDefine APP_MOTOR_PWM_BACKEND_ENABLE=1
```

Default bring-up builds keep `APP_MOTOR_PWM_BACKEND_ENABLE=0`, so motor commands may appear as static bridge input states instead of timer PWM.

For automated scope setup/capture on bridge B:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --mode setup-only
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-forward --duty-permille 50 --duration-ms 300
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-reverse --duty-permille 50 --duration-ms 300
```

With CH1 on `/PWM_B2` and CH2 on `/PWM_B1`, `single-forward` should show CH2 PWM and CH1 low. `single-reverse` should show CH1 PWM and CH2 low.

The helper defaults to `--acquire-mode run-stop`, which starts the scope, starts
the motor command, and stops the scope while the waveform is active. Use this
for evidence captures. Use `--acquire-mode single-trigger` only when debugging
scope trigger behavior.

For PWMX register diagnostics while the command is active:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-forward --duty-permille 50 --duration-ms 1000 --pwm-debug --verbose-shell
```

The firmware shell command behind this is:

```text
motor pwm-debug
```

If both channels stay flat, first prove that the probed nets are the DRV8837
inputs and that the MCU can drive them as plain GPIO:

```text
motor diag-inputs B forward 5000
motor diag-inputs B reverse 5000
motor diag-inputs B brake 5000
```

Expected with bridge B:

- `forward`: `/PWM_B1` high, `/PWM_B2` low.
- `reverse`: `/PWM_B1` low, `/PWM_B2` high.
- `brake`: `/PWM_B1` high, `/PWM_B2` high.
- `/SLEEP` remains low for all three commands, so the DRV8837 power outputs stay disabled.

These are static GPIO checks. A DMM should read about 3.3 V on the high input and
0 V on the low input during the 5 second hold.

Rev-A target-board testing has proven static GPIO behavior at the DRV8837 inputs
and bridge outputs for B and A. On both bridges, one input reaches about 3.3 V,
the opposite input stays low, and the corresponding bridge output switches. If
PWM is missing on A while static A works, focus on TMR1/TMR2 PWM setup, probe
placement, and scope cabling rather than the bridge schematic. A faulty scope
lead has already produced misleading flat captures during this bring-up.

Rev-A PWM evidence after replacing the suspect scope lead:

- `motor pwm A forward 50 3000`: one A input showed about 3.4 Vpp, 20 kHz, 5.1% duty; the opposite input stayed low.
- `motor pwm A reverse 50 3000`: the active waveform moved to the opposite A input, about 3.3 Vpp, 20 kHz, 4.7% duty.
- This proves A-side timer PWM and the DRV8837 input path are working. If the generated scope report labels do not match the physical signal, verify whether CH1 is on `A1` or `A2`; the script assumes CH1 is the `*2` input and CH2 is the `*1` input.

For future PCB revisions, route the primary propulsion phases A/B to normal
PWMX-capable CH592 pins where possible, leaving timer PWM for the guard rail
only if necessary.

When `motor pwm-debug` is used with a PWM-enabled build, first check:

- `pwm expected init=1`: motor platform initialization ran.
- `pwm expected pwmx_div=4`: PWMX is using the same divider family as the WCH PWMX example.
- `pwm expected pwmx_cycle=750`: approximately 20 kHz at 60 MHz with divider 4.
- `pwm expected duty_50permille=37`: 5% bring-up duty limit.
- `pwm regs config` low nibble should include 16-bit PWM mode, normally `0x0c`.
- `pwm regs pwm9` should be nonzero for bridge B forward; `pwm7` should be nonzero for bridge B reverse.
- `tmr2 regs ctrl` should show count/output enable bits for bridge A forward.
- `tmr1 regs ctrl` should show count/output enable bits for bridge A reverse.

Automated scope capture for these GPIO checks:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-forward --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-reverse --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-brake --duration-ms 5000
```

## Reset Safe State

Trigger on reset release or 3.3 V rising.

Expected:

- `/PWM_A1`, `/PWM_A2`, `/PWM_B1`, `/PWM_B2`, `/PWM_G1`, `/PWM_G2` low after firmware entry.
- `/SLEEP` low after firmware entry.
- `/LED` low, with no unintended WS2812 frame.
- WT2003 UART TX not driven until an audio command opens UART1.
- I2C `/SCL` and `/SDA` idle high if pull-ups are fitted/powered.

Recommended sample rate: at least 10 MS/s for reset/safe-state checks.

## Motor A Forward

Commands:

```text
motor arm 2
motor pwm A forward 10 100
```

Expected with PWM backend enabled:

- `/PWM_A1`: PWM at `APP_MOTOR_PWM_DEFAULT_HZ`, currently 20 kHz.
- `/PWM_A1` duty: 10 permille, about 1%.
- `/PWM_A2`: low.
- `/SLEEP`: high during the armed window, then low after `motor off` or arm timeout.
- `/PWM_A1` returns low no later than 100 ms after command execution, plus shell/loop latency.
- All B/G inputs remain low.

## Motor B Reverse

Commands:

```text
motor arm 2
motor pwm B reverse 10 100
```

Expected with PWM backend enabled:

- `/PWM_B2`: PWM at about 20 kHz, about 1% duty.
- `/PWM_B1`: low.
- `/SLEEP`: high during the armed window, then low after `motor off` or arm timeout.
- `/PWM_B2` returns low no later than 100 ms after command execution, plus shell/loop latency.
- A/G inputs remain low.

## Motor G Forward

Commands:

```text
motor arm 2
motor pwm G forward 10 100
```

Expected with PWM backend enabled:

- `/PWM_G1`: PWM at about 20 kHz, about 1% duty.
- `/PWM_G2`: low.
- `/SLEEP`: high during the armed window, then low after `motor off` or arm timeout.
- `/PWM_G1` returns low no later than 100 ms after command execution, plus shell/loop latency.
- A/B inputs remain low.

## RGB One

Before sending a WS2812 frame on a new target board, run:

```text
rgb idle-low
```

Expected on `/LED`: steady low, with no LED color change. This is a GPIO-idle check only; it does not validate WS2812 timing.

Command:

```text
rgb one 0 16 0 0
```

Expected on `/LED`:

- WS2812-style 800 kHz bitstream.
- Backend assumes `FREQ_SYS = 60 MHz` and fixed NOP timing.
- Bit period should be about 1.25 us.
- XL-1010RGBC-WS2812B datasheet minimums are T0H 0.3 us, T0L 0.9 us, T1H 0.9 us, and T1L 0.3 us.
- The current 60 MHz bit-bang backend targets roughly T0H 0.3 us, T0L 0.9 us, T1H 0.9 us, and T1L 0.3 us.
- Default color order is GRB.
- Default brightness is low, so input red `16` scales to zero with the current integer brightness limit. The frame should still contain zero-bit timing pulses.
- Six LED frames are transmitted because the driver writes the full chain.
- Reset low period before and after the frame should exceed 200 us; firmware delays about 240 us.

Validate actual high/low widths with a logic analyzer before relying on the bit-bang backend with BLE active.

## WT2003 UART Frame

Command:

```text
audio status
audio stop
```

Expected:

- `audio status` should not drive UART if no command has opened the backend.
- `audio stop` enables UART1 remapped to PB12/PB13 at 9600 8N1 and sends the WT2003HX stop frame:

```text
7E 03 AB AE EF
```

Expected UART settings from the CDC line-coding default are independent of WT2003 UART. Confirm 9600 baud, 8 data bits, no parity, one stop bit, 3.3 V TTL, and idle-high TX. BUSY is idle low and should go high only while audio is playing.

## I2C Scan Transaction

Command:

```text
i2c scan
```

Expected:

- `/SCL` and `/SDA` idle high before scan.
- MCU-side nets are `/SWDCK` and `/SWDIO`; they reach IP5209 `/SCL` and `/SDA` through R14/R15 0 ohm links.
- Do not expect this test to pass while WCH-LinkE debug is actively using PB15/PB14.
- Firmware probes 7-bit addresses `0x08` through `0x77`.
- Each probe emits START, 7-bit address plus write bit, ACK/NACK sample, then STOP.
- IP5209 is expected at configured address `0x75` only if that address is correct for the assembled PMIC variant.
- Transfers should timeout and continue; the bus must not stay held low after a missing ACK.

Recommended sample rate: at least 2 MS/s for 100 kHz I2C, higher if later switched to 400 kHz.
