# Fresh Session Handoff

This is the compact starting point for a new Codex/session. Use it before
loading long logs or old chat context.

## Current Milestone

The Rev-A target PCB has reached the integrated USB-powered MVP bring-up stage.
The board is not production-ready, but these paths are proven enough for the
next milestone:

- WCH-Link flashing works.
- USB CDC shell works.
- BLE advertising works; earlier bring-up confirmed the custom service can be
  read from nRF Connect.
- WT2003 audio works manually from the shell.
- RGB works after the Rev-A rework to drive WS2812 data from PA14/SPI0 MOSI.
- Touch pads work in standalone watch tests.
- Hall sensors work only after manual Rev-A rework; the original Rev-A
  footprint/symbol mapping is wrong.
- Sled motion is proven with A/B propulsion plus a fixed guard rail.
- IP5209 battery-only boost is unresolved and intentionally parked. Use USB
  power for MVP tests.

The current MVP build is RAM-tight: the current image uses about 96% of the
CH592 26 KB RAM once `.highcode`, `.data`, `.bss`, and the linker stack
reservation are counted. See `Firmware/docs/ram_usage_audit.md`.

## Current Build

Build the integrated MVP image with:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build_mvp_demo.ps1
```

Output:

```text
Firmware\build\mvp_demo\northpole_ch592_bringup.hex
Firmware\build\mvp_demo\northpole_ch592_bringup.elf
Firmware\build\mvp_demo\northpole_ch592_bringup.map
Firmware\build\mvp_demo\build_manifest.txt
```

The last audited MVP manifest was built from git commit `694ef3c` with these
important defines:

```text
APP_BUILD_PROFILE_NAME="mvp-demo"
APP_FIRMWARE_VERSION="0.1.0-mvp-demo"
APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1
APP_MOTOR_PWM_BACKEND_ENABLE=1
APP_MOTOR_PWM_MAX_DUTY_PERMILLE=1000
APP_DEMO_SCENE_ENABLE=1
APP_DEMO_USB_POWER_ONLY=1
APP_DEMO_DEFAULT_HALL_ENABLE=0
APP_DEMO_DEFAULT_AUDIO_VOLUME=31
APP_DEMO_DEFAULT_RGB_BRIGHTNESS=24
APP_DEMO_DEFAULT_RGB_EFFECT=1
APP_MOTOR_WAVE_TABLE_SIZE=256
APP_MOTOR_CONTROL_UPDATE_HZ=8000
APP_MOTOR_PWM_CARRIER_HZ=100000
APP_MOTION_RAMP_ENABLE=1
APP_MOTION_TOUCH_CONTROL_ENABLE=0
APP_MOTION_DEFAULT_SPEED_HZ_X1000=8000
APP_MOTION_DEFAULT_AMPLITUDE_PERMILLE=1000
APP_MOTION_GUARD_MODE=2
APP_MOTION_GUARD_DUTY_PERMILLE=600
```

## Physical Rev-A State

Read `Firmware/docs/rev_a_mvp_rework_state.md` first. The key points are:

- WT2003 `BUSY` trace is cut.
- PA14/SPI0 MOSI is jumpered to WS2812 data.
- Original PA15 `/LED` trace is isolated; before isolation it pulled the LED
  data net down.
- RGB backend should report/use PA14/SPI0 MOSI.
- Hall sensor footprint is wrong on Rev-A. User reworked at least one sensor
  enough for `hall watch` events.
- MVP operation is USB-powered. Battery-only IP5209 boost is not validated.

Do not assume an untouched Rev-A board will match the current bench results.

## Known Working Commands

After flashing, open the USB CDC shell and run:

```text
version
faults
demo status
```

Manual motion checkpoint:

```text
motor wave-run-smooth 8000 1000 10000 all sleep1 fwd guard-fwd guard-duty 600
```

Notes:

- This is the latest reported best mechanical behavior.
- Earlier tests also used `guard-rev`; if motion quality changes after
  mechanical rework, compare both polarities before changing defaults.
- Guard duty around `600` is quiet. Values around `800..1000` can create
  audible electrical/electromagnetic noise even with the sled removed.
- A/B amplitude below about `800` is usually too weak for the current sled;
  keep `1000` for movement experiments unless testing current/heat limits.

RGB checks:

```text
rgb backend
rgb brightness 24
rgb off
rgb all 255 0 0
rgb all 0 255 0
rgb all 0 0 255
rgb walk 24 0 0 250
rgb walk 0 24 0 250
rgb walk 0 0 24 250
rgb scene stars 8 24
rgb scene rainbow 10 24 120
rgb scene breathe 0 0 255 3 35
rgb scene strobe 8 24
rgb scene christmas 10 180
```

Audio checks:

```text
audio volume 31
audio play-index 1
audio status
```

WT2003 volume range is `0..31`; `31` is the chip-command maximum we currently
use. Playback start can be delayed by a few seconds by the WT2003/storage
state.

Touch/Hall checks:

```text
touch raw
touch watch 15000 20
hall read
hall watch 15000 20
```

The demo touch handling is edge-based: actions are applied on release, not on
the initial press.

Demo commands:

```text
demo clear-faults
demo status
demo rgb effect stars
demo audio volume 31
demo audio track 1
demo speed 8000
demo intensity 100
demo start
demo stop
```

If `demo start` appears to do nothing, check:

```text
faults
demo status
audio status
motion tune status
```

Recent firmware added demo audio/touch debug prints. If touch presses during
demo produce no logs, suspect foreground-loop starvation, USB CDC backpressure,
or a touch raw-state issue during motor operation.

## Important Documents

Start with these instead of reading chat history:

- `Firmware/docs/mvp_demo.md` - MVP build and operator commands.
- `Firmware/docs/mvp_runtime_timing.md` - current cooperative loop, interrupts,
  RGB/audio/motor timing, and headroom concerns.
- `Firmware/docs/ram_usage_audit.md` - current RAM breakdown and reduction plan.
- `Firmware/docs/production_firmware_architecture.md` - proposed production
  architecture and profile split.
- `Firmware/docs/rev_a_mvp_rework_state.md` - exact Rev-A bench rework.
- `Firmware/docs/first_integrated_demo_checklist.md` - integrated demo checklist.
- `Firmware/docs/motion_smoothness_tuning.md` - motor tuning observations.
- `Firmware/docs/motor_phase_model.md` - A/B/G phase and guard interpretation.
- `Firmware/docs/motor_sine_reference/motor_sine_reference.md` - waveform model.
- `Firmware/docs/spi0_mosi_pinmux_test_report.md` - PA14 RGB investigation.
- `Firmware/docs/ip5209_battery_boost_problem_statement.md` - parked battery
  boost problem.
- `Firmware/docs/ch592_upload_matrix.md` and
  `Firmware/docs/ch592_brick_recovery.md` - CH592 flashing/recovery notes.
- `Firmware/docs/hardware_pin_audit.md` - current net/pin audit.

## Current Open Problems

1. MVP demo integration under load:
   - Standalone RGB/audio/touch/motion work.
   - Need verify that demo can run motion, RGB, audio, and touch without losing
     foreground-loop service.
   - `mvp_runtime_timing.md` lists the likely bottlenecks.

2. RAM margin:
   - Current MVP image is acceptable for bench work but too tight for
     production.
   - `MEM_BUF`, USB CDC rings, motor DMA tables, and RAM-resident `.highcode`
     dominate the footprint.

3. Battery-only operation:
   - IP5209 VREG wakes, but VOUT/+5V boost behavior is unresolved.
   - Continue USB-powered MVP work until the power-stage issue is isolated.

4. Rev-B hardware:
   - Correct Hall footprint.
   - Preserve or deliberately redesign the PA14/SPI0 MOSI RGB route.
   - Revisit IP5209 boost/startup.
   - Consider timer/PWM pin mapping only after the current software motion
     architecture is settled.

## Verification Rules

Useful commands:

```powershell
python -B -m unittest Firmware.host_tests.test_firmware_logic
git diff --check
```

Do not run this unless the user explicitly asks:

```powershell
python Firmware\tools\repo_hygiene_check.py
```

Do not delete logs or generated evidence after a chat. The user wants logs kept
until an intentional cleanup before commit.

## Context Hygiene For New Sessions

- Do not paste old logs into the prompt unless they are directly needed.
- Use the docs listed above as the index.
- Rebuild from `build_mvp_demo.ps1` when behavior and source may be out of sync.
- If the user reports a physical result, record it in a doc before moving on.
- Keep generated `.hex`, `.elf`, `.map`, scope CSVs, screenshots, and large logs
  out of commits unless they are explicitly reference artifacts.
