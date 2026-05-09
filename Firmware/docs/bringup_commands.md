# Bring-Up Commands

The bring-up firmware exposes a USB CDC diagnostic shell. UART1 is reserved for the WT2003 audio path and is not the default host console.

On the current PCB, UART1 is routed to the WT2003 audio IC. The original EVT debug UART used PA9, which is `/INT` on this board, so UART shell/log I/O is compiled off by default. Enable `NORTHPOLE_ENABLE_UART1_LOG=1` only on reworked hardware or a fixture where PB12/PB13 are connected to a host instead of the audio IC.

## Core

```text
help
version
status
faults
pins verify
safe check
reset
settings show
settings reset
settings save
settings corrupt
```

`settings save` reports `flash=0` in the default pre-hardware builds because `APP_SETTINGS_FLASH_ENABLE` is disabled. CRC/default/corruption recovery logic exists, but persistent storage is not proven until a flash/SNV backend is implemented and target-tested.

## Power

```text
i2c scan
i2c read <addr7> <reg>
i2c write <addr7> <reg> <value>
ip5209 status
ip5209 probe
ip5209 read <reg>
ip5209 write <reg> <value>
```

I2C transfers use the WCH master peripheral with short timeouts. IP5209 register meaning is still conservative/unknown until the exact PMIC behavior is validated on hardware.

## Touch And Hall

```text
touch raw
hall read
```

Touch measurement currently returns a GPIO-level placeholder through the WCH board port. The old CH32V003 charge-time idea can be reused conceptually, but the CH592X touch/ADC peripheral should be preferred if it is stable.

## RGB

```text
rgb off
rgb one <index> <r> <g> <b>
rgb all <r> <g> <b>
rgb chase <brightness>
rgb order test
rgb show
```

Brightness is capped by `APP_RGB_BRINGUP_BRIGHTNESS_LIMIT`. The CH592 PA15 WS2812 bit-bang backend is implemented, but timing still needs logic-analyzer validation with BLE interrupts active.

## Audio

```text
audio status
audio raw <hex bytes>
audio version
audio qvol
audio qstatus
audio qcount-ext
audio qperiph
audio busy
audio volume <0-31>
audio play-index <1-65535>
audio play-name <name-no-ext>
audio stop
audio pause
audio next
audio prev
audio mode <single|single-loop|all-loop|random>
audio output <spk|dac>
audio sleep <idle|deep>
audio format-ext-flash CONFIRM
```

`audio ping` and `audio play <id>` remain accepted as compatibility aliases for `audio qstatus` and `audio play-index <id>`.

The KiCad audit currently shows the UART path is present through R12/R13. The WT2003HX V2.00 frame encoder/parser is implemented and host-tested against datasheet examples, but WT2003 UART responses, BUSY behavior, output mode, file indexing, and volume persistence still need target-board validation.

`audio format-ext-flash CONFIRM` is rejected unless the firmware is built with `APP_AUDIO_ALLOW_FORMAT_COMMAND=1`. Do not enable that flag for production builds.

## Motor

```text
motor status
motor arm <seconds>
motor off
motor pwm <A|B|G> <forward|reverse|coast|brake> <duty_permille> <ms>
```

Rules:

- `motor arm` is required before any non-coast command.
- Arm duration is capped by `APP_BRINGUP_MOTOR_ARM_MAX_MS`.
- `motor arm` wakes the DRV8837s by driving PB0 `/SLEEP` high after all IN pins are low.
- Duty is capped by `APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE` in bring-up builds.
- Command duration is capped by `APP_MOTOR_COMMAND_TIMEOUT_MS`.
- `motor off` immediately disarms, coasts all DRV8837 inputs, then drives PB0 `/SLEEP` low.
- The timer/PWM backend is present but compile-time disabled by default with `APP_MOTOR_PWM_BACKEND_ENABLE=0`.

`safe check` must report `MOTOR_SLEEP` on U2 pad 3 `/SLEEP` with expected safe state `output_low`.

## Debug And Update Connectors

J3 is the WCH-LinkE/debug connector, not a generic ARM SWD header:

```text
1 = 3.3V target reference
2 = TIO / SWDIO / PB14 / SDA-side MCU net
3 = NC unless reset is later wired
4 = TCK / SWDCK / PB15 / SCL-side MCU net
5 = GND
6 = NC
```

PB14/PB15 are shared with IP5209 I2C through R15/R14 0 ohm links. Normal firmware uses them as SDA/SCL. WCH-LinkE uses them as TIO/TCK. Do not expect IP5209 I2C access during active WCH-Link debugging.

J4 uses the Tag-Connect footprint only as a WT2003 USB update connector:

```text
1 = +5V
2 = WT2003 D+
3 = NC
4 = WT2003 D-
5 = GND
6 = NC
```

J4 is not ARM SWD. Use only a custom USB adapter/cable.

## BLE Diagnostic Service

The firmware registers a minimal North Pole diagnostic GATT service `0xFD90`:

- `0xFD91` firmware version, read
- `0xFD92` board revision, read
- `0xFD96` build profile, read
- `0xFD93` status packet, read
- `0xFD94` Hall/touch counters, read
- `0xFD95` safe control, write: RGB all, clear faults, audio stop, audio play index, audio volume, audio pause/resume, audio query status

No raw motor commands are exposed over BLE.
