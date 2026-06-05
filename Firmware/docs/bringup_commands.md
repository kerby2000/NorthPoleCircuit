# Bring-Up Commands

The bring-up firmware exposes a USB CDC diagnostic shell. UART1 is reserved for the WT2003 audio path and is not the default host console.

On the current PCB, UART1 is routed to the WT2003 audio IC. The original EVT debug UART used PA9, which is `/INT` on this board, so UART shell/log I/O is compiled off by default. Enable `NORTHPOLE_ENABLE_UART1_LOG=1` only on reworked hardware or a fixture where PB12/PB13 are connected to a host instead of the audio IC.

## Automated Smoke Tests

CH592 dev-board smoke build, already validated on the CH592X-EVT-R1-LinkE:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --profile dev-board --port COM19 --timeout 3 --reset-recovery --ble-name "NorthPole BLE"
python Firmware\tools\ble_diag_smoke_test.py --scan --timeout 10
```

Target-board first pass, after replacing `COMxx` with the enumerated USB CDC port:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --profile target --port COMxx --timeout 3
python Firmware\tools\ble_diag_smoke_test.py --scan --timeout 10
```

The dev-board profile avoids commands that would touch NorthPole target-only hardware. Use the target profile only on the real NorthPole PCB.

Low-volume WT2003 audio smoke test, after copying at least one MP3 to the WT2003 external flash and disconnecting any WT2003 USB storage cable:

```powershell
python Firmware\tools\audio_wt2003_smoke_test.py --port COMxx --volume 5 --index 1 --play-seconds 3
```

Faster playback-only check after the control path is already proven:

```powershell
python Firmware\tools\audio_wt2003_smoke_test.py --port COMxx --quick-play --volume 31 --index 1 --play-seconds 3
```

Use `--verbose` when full shell responses are needed. Without `--verbose`, the script prints a concise command summary.

Interactive Hall/touch physical test:

```powershell
python Firmware\tools\hall_touch_interactive_test.py --port COMxx
```

The Hall/touch script prints initial state and then only changed values while you move a magnet or touch a pad. If touch values stay at zero, the pads are not validated yet; the firmware touch backend may still need implementation or threshold tuning.

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
i2c lines
i2c release-debug
i2c scan
i2c read <addr7> <reg>
i2c write <addr7> <reg> <value>
ip5209 status
ip5209 dump
ip5209 probe
ip5209 read <reg>
ip5209 write <reg> <value>
ip5209 boost <on|off>
ip5209 light-load <enable|disable>
ip5209 ntc <enable|disable>
```

I2C transfers use the WCH master peripheral with short timeouts. `ip5209 status` gives a short summary, while `ip5209 dump` prints each known IP5209 raw register byte in hex/binary and decodes the known bit fields from `PCB/datasheets/IP5209 IP5109 IP5207 IP5108 I2C registers.pdf`. The `boost_cfg` field is register `0x01` bit 2 only; it is not a physical VOUT measurement and does not prove that the +5 V rail is regulating. Writes remain manual only; do not change configuration registers casually because the datasheet marks reserved bits as stateful and requires read-modify-write.

The diagnostic write helpers are read-modify-write only and preserve reserved bits:

- `ip5209 boost on|off`: modifies `SYS_CTL0[0x01]` bit 2.
- `ip5209 light-load enable|disable`: modifies `SYS_CTL1[0x02]` bit 1.
- `ip5209 ntc enable|disable`: modifies `SYS_CTL5[0x07]` bit 6, where `0` means NTC enabled and `1` means NTC disabled.

Each helper prints register, mask, old value, new value, write result, and readback value.

`i2c lines` prints the raw CH592 PB15/PB14 SCL/SDA input levels and the I2C peripheral status registers. Use it before `i2c scan` when debugging a silent bus. Both lines should normally read high when idle.

`i2c release-debug` is a target-board diagnostic for the PB14/PB15 debug/I2C share. It disables the CH592 runtime two-wire debug function so SCL/SDA can own PB15/PB14, then reinitializes I2C. WCH-Link attach may require reset or download mode after this command.

The first target-board I2C pass proved that `i2c release-debug` changes `R16_PIN_ALTERNATE` from `0x0000` to `0x2000`, after which `ip5209 probe` ACKs at `0x75` and `i2c scan` finds `0x75`. This confirms the PCB I2C path, R14/R15 links, and pull-ups. Keep `i2c release-debug` manual during bring-up so WCH-LinkE access remains predictable.

## Touch And Hall

```text
touch raw
hall read
```

Touch measurement currently returns a GPIO-level placeholder through the WCH board port. The old CH32V003 charge-time idea can be reused conceptually, but the CH592X touch/ADC peripheral should be preferred if it is stable.

## RGB

```text
rgb idle-low
rgb off
rgb one <index> <r> <g> <b>
rgb all <r> <g> <b>
rgb chase <brightness>
rgb order test
rgb show
```

`rgb idle-low` only forces the CH592 `/LED` data pin low and does not transmit a WS2812 frame. Use it before WS2812 timing is trusted on a new board.

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
motor sine-demo <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]
```

Rules:

- `motor arm` is required before any non-coast command.
- Arm duration is capped by `APP_BRINGUP_MOTOR_ARM_MAX_MS`.
- `motor arm` wakes the DRV8837s by driving PB0 `/SLEEP` high after all IN pins are low.
- Duty is capped by `APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE` in bring-up builds.
- Command duration is capped by `APP_MOTOR_COMMAND_TIMEOUT_MS`.
- `motor off` immediately disarms, coasts all DRV8837 inputs, then drives PB0 `/SLEEP` low.
- The timer/PWM backend is present but compile-time disabled by default with `APP_MOTOR_PWM_BACKEND_ENABLE=0`.

`motor sine-demo` is a bounded diagnostic for checking pseudo-sine PWM envelopes on the DRV8837 input pins. It self-arms, steps a 32-sample sine table, then forces `motor off`. The default target is `AB`, where A uses phase 0 and B uses phase +90 degrees. Use `A`, `B`, or `G` to put the same signed sine envelope on one bridge while probing that bridge. Use low values first, for example:

```text
motor sine-demo 2 50 5000 A
motor sine-demo 2 50 5000 G
```

On the scope, the raw DRV8837 inputs remain digital 20 kHz PWM. The sine is visible as a slowly changing duty-cycle envelope, not as an analog sine voltage on the MCU pins.

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

J4 uses the Tag-Connect footprint only as a WT2003 USB update connector, but the current PCB revision is `BLOCKED` for update use because J4 pin 1 is unconnected:

```text
1 = BLOCKED: expected +5V, actual unconnected
2 = WT2003 D+
3 = NC
4 = WT2003 D-
5 = GND
6 = NC
```

J4 is not ARM SWD. Do not use it for WT2003 USB update until the missing +5 V path is fixed or a rework procedure is documented.

## BLE Diagnostic Service

The firmware registers a minimal North Pole diagnostic GATT service `0xFD90`:

- `0xFD91` firmware version, read
- `0xFD92` board revision, read
- `0xFD96` build profile, read
- `0xFD93` status packet, read
- `0xFD94` Hall/touch counters, read
- `0xFD95` safe control, write: RGB all, clear faults, audio stop, audio play index, audio volume, audio pause/resume, audio query status

No raw motor commands are exposed over BLE.
