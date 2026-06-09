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
rgb backend
rgb brightness <0-255>
rgb diag-pa14 high [ms]
rgb diag-pa14 low [ms]
rgb diag-pa14 square <hz> <ms>
rgb idle-low
rgb off
rgb one <index> <r> <g> <b>
rgb all <r> <g> <b>
rgb chase <brightness>
rgb order test
rgb show
```

`rgb idle-low` only forces the CH592 `/LED` data pin low and does not transmit a WS2812 frame. Use it before WS2812 timing is trusted on a new board.

Brightness is capped by `APP_RGB_BRINGUP_BRIGHTNESS_LIMIT`. Use `rgb backend`
to confirm whether the build is using the normal PA15 bit-bang backend, the
PA14/SPI0 MOSI backend, or the PA14 GPIO bit-bang backend.

The CH592 PA15 WS2812 bit-bang backend is implemented, but timing still needs
logic-analyzer validation with BLE interrupts active. For the Rev-A MVP rework,
cut/isolated WT2003 `BUSY`, jumper MCU-side PA14 to the WS2812 data input, and
isolate the old PA15 `/LED` trace. The old PA15 path was observed to pull the
reworked LED data net down until it was cut. Preferred Rev-A test build:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1','APP_MOTOR_PWM_BACKEND_ENABLE=1') }"
```

The PA14 GPIO bit-bang experiment remains available with
`APP_RGB_WS2812_USE_PA14_BITBANG=1`, but bench captures showed invalid-looking
WS2812 frames and all LEDs could latch white. Do not use that backend as the
current LED reference. The SPI0 MOSI backend is the preferred Rev-A test path
because the pinmux experiment proved PA14/MOSI can emit WS2812-compatible
pulses. The SPI backend now sends explicit SPI zero-byte reset/latch windows
before and after every LED frame so any SPI-output enable artifact is followed
by a valid reset-low period before LED0 data begins.

For the Rev-A PA14/BUSY-to-LED jumper, use `rgb diag-pa14 high 5000` and
`rgb diag-pa14 square 1000 5000` with the scope on the jumper before debugging
WS2812 timing. These commands bypass SPI and WS2812 encoding and directly drive
PA14 as GPIO. If they are flat at the jumper, check the cut/jumper/probe point
before changing RGB firmware.

For WS2812 captures, the idle/reset portions before and after the data burst
must be low. A long high level before the burst is not part of a valid WS2812
message. If LED0 stays green while later LEDs respond, capture the first 50 us
of `rgb off` on the LED data net and check for a startup pulse before the first
encoded `0` bit.

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
motor wave-status
motor wave-stop
motor wave-start <electrical_hz_x1000> <amplitude_permille> [AB|A|B|G|all] [sleep0|sleep1] [fwd|rev] [guard-off|guard-fwd|guard-rev|guard-a|guard-b] [guard-duty <permille>]
motor wave-run <electrical_hz_x1000> <amplitude_permille> <ms> [AB|A|B|G|all] [sleep0|sleep1] [fwd|rev] [guard-off|guard-fwd|guard-rev|guard-a|guard-b] [guard-duty <permille>]
motion status
motion start
motion stop
motion speed <signed_hz_x1000>
motion step <signed_delta_hz_x1000>
motion guard <off|forward|reverse|phase-a|phase-b> [duty_permille]
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

For first real shuttle/sledge motion, drive physical phases `A` and `B`
together. `A1/A2` are only the two polarity legs of the A DRV8837 bridge, not
two track phases. Likewise `B1/B2` are the two polarity legs of B. The live
motion command should therefore target `AB`, normally with `B` phase shifted
90 degrees from `A`.

Initial finite live-motion commands with `/SLEEP` high:

```text
motor off
motor wave-run 500 50 1000 AB sleep1
motor wave-run 1000 50 1000 AB sleep1
motor off
```

If this is too weak for motion, rebuild with a higher
`APP_MOTOR_PWM_MAX_DUTY_PERMILLE` and keep the command duration short while
watching coil temperature and supply current.

2026-06-06 first-motion build used:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1','APP_MOTOR_PWM_BACKEND_ENABLE=1','APP_MOTOR_PWM_MAX_DUTY_PERMILLE=1000') }"
```

This removes the software duty cap for finite `sleep1` wave commands. It does
not make the motor run indefinitely; use short `wave-run` commands and finish
with `motor off`.

2026-06-06 first real shuttle/sledge motion milestone:

```text
motor wave-run 3000 1000 20000 all sleep1
```

Result: the sledge moved within the `G` guardrails in one direction. USB-C
input current rose to about `900 mA`. This is the first confirmed Rev-A target
board motion with the DRV8837 bridges enabled.

Important interpretation:

- `3000` means `3.000 Hz` electrical sine-table frequency, not a 3000 Hz sine.
- `1000` means `1000 permille`, or full-scale duty envelope.
- `all` drives A, B, and G.
- A uses sine phase 0.
- B uses sine phase +90 degrees.
- G now defaults to fixed guard mode, not a sine phase.

With `all`, the guard bridge is now driven as a fixed inward-force guard by
default: `guard-fwd` means G1 PWM / G2 low. Use `guard-rev` if the physical
force is the wrong direction. `guard-a` and `guard-b` remain diagnostic modes
that phase-link G to A or B; they are not the default motion strategy.

Finite forward/reverse tests:

```text
motor wave-run 3000 1000 20000 all sleep1 fwd guard-fwd
motor wave-run 3000 1000 20000 all sleep1 rev guard-fwd
motor wave-run 3000 1000 20000 all sleep1 fwd guard-rev
motor off
```

Continuous motion controller:

```text
motion status
motion start
motion step 500
motion step -500
motion speed -3000
motion guard forward 1000
motion stop
```

Touch-pad behavior in the continuous controller:

- RUN toggles continuous motion on/off.
- SPD+ increases signed electrical frequency by the configured step.
- SPD- decreases signed electrical frequency by the configured step.
- If SPD- pushes a positive speed through zero, direction flips and the speed
  continues increasing in the reverse sign. SPD+ does the symmetric operation
  from reverse back through zero.
- Motion defaults to A/B propulsion plus fixed G guard and `/SLEEP` high.

The current bring-up default is 3 Hz electrical frequency, 0.5 Hz button step,
12 Hz max, full-scale A/B duty envelope, and full-scale G guard duty. Watch USB
input current and bridge/coil temperature during early runs.

## Measuring Real Coil Current

The USB-C meter shows total input current. It is useful for safety, but it is
not the same as current through an individual coil/track segment.

Better ways to see the physical coil current:

- Use a current probe around one bridge-output/coil conductor if available.
- Insert a low-value current shunt in series with one coil/track feed and
  measure differential voltage across it. Example: `0.05 ohm` or `0.1 ohm`,
  adequate power rating. Current is `I = Vshunt / Rshunt`.
- If using a grounded oscilloscope, do not clip scope ground to a floating
  H-bridge output. Use a differential probe or a known safe differential
  measurement setup.
- Probing `A1_OUT`, `B2_OUT`, or `G1_OUT` relative to board GND shows bridge
  voltage and inductive ringing. It does not directly show coil current.

The scope waveform from bridge outputs is not expected to look like an analog
sine. The sine model is encoded as duty and polarity commands into chopped
H-bridge outputs. When probing `B2_OUT`, `G1_OUT`, or `A1_OUT` relative to GND,
the scope shows bridge switching, coil/track inductance, flyback behavior, and
ringing. The reference sine plots document the intended input-duty envelope and
phase order; they are not literal analog voltages expected on the bridge output
pads.

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
