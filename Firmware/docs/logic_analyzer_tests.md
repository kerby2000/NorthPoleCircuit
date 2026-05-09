# Logic Analyzer Tests

Use a current-limited supply. For PWM waveform tests, build with:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup -ExtraDefine APP_MOTOR_PWM_BACKEND_ENABLE=1
```

Default bring-up builds keep `APP_MOTOR_PWM_BACKEND_ENABLE=0`, so motor commands may appear as static bridge input states instead of timer PWM.

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

Command:

```text
rgb one 0 16 0 0
```

Expected on `/LED`:

- WS2812-style 800 kHz bitstream.
- Backend assumes `FREQ_SYS = 60 MHz` and fixed NOP timing.
- Bit period should be about 1.25 us.
- A zero bit should be roughly T0H 0.35 us high followed by T0L 0.8 us low.
- A one bit should be roughly T1H 0.7 us high followed by T1L 0.6 us low.
- Default color order is GRB.
- Default brightness is low, so input red `16` scales to zero with the current integer brightness limit. The frame should still contain zero-bit timing pulses.
- Six LED frames are transmitted because the driver writes the full chain.
- Reset low period after the frame should exceed 50 us; firmware delays about 90 us.

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
