# Production Firmware Architecture Proposal

The current firmware is a successful bring-up/MVP image, not a production
architecture. It intentionally contains shell commands, scope diagnostics,
register pokes, raw motor controls, and bench-only recovery paths. The next
milestone should split this into a minimal production application plus separate
test images.

## Goals

Production firmware should:

- Start deterministically from reset.
- Keep all outputs safe until the app intentionally enters RUN.
- Run the sled, RGB, audio, touch, Hall, and BLE service with bounded latency.
- Avoid arbitrary raw hardware commands.
- Leave meaningful RAM and stack margin.
- Keep diagnostics available through separate factory/bring-up builds.

It should not carry the full bench shell into the final demo/product image.

## Proposed Build Profiles

| Profile | Purpose | Keep |
|---|---|---|
| `bringup` | Hardware diagnosis | Full USB CDC shell, raw I2C/audio/motor/RGB commands, scope hooks |
| `mvp-demo` | Current Rev-A hand-reworked demo | USB CDC shell, selected debug prints, demo scene, motion/RGB/audio |
| `factory-test` | Board production test | Small command set: pins, RGB, audio, Hall/touch, motor-safe pulse, version |
| `production` | Final behavior | Demo app, BLE control/status if needed, touch controls, audio/RGB/motion |

The current `Firmware\tools\build_mvp_demo.ps1` should remain the bench MVP
wrapper. Add a future `Firmware\tools\build_production.ps1` with stricter
defines.

## Production Runtime Model

Keep the current no-RTOS model:

```text
interrupts:
  BLE/TMOS timing
  motor control update
  timer/PWM/DMA peripherals
  optional USB only in non-production

foreground loop:
  input scan
  demo state machine
  audio queue
  RGB scene scheduler
  BLE service/status update
  safety/fault handling
```

There is no task-switching overhead. The main constraints are interrupt
latency, foreground-loop blocking time, and RAM.

## Production Modules

### Safety Manager

Responsibilities:

- Own the global fault mask.
- Force motor off and `/SLEEP` low on faults.
- Bound run duration.
- Clamp motor duty, guard duty, and speed.
- Provide one emergency-stop path callable from touch/BLE/internal faults.

Production should not allow raw motor writes that bypass safety.

### Motion Engine

Use the proven smooth engine:

```text
sine table: 256 phase positions
control update: 8000 Hz baseline
carrier: 40000 or 100000 Hz after final tuning
A/B amplitude: up to 1000 permille for Rev-A sled
guard: fixed direction, default duty around 600 permille
```

Production commands should be high level:

```text
start
stop
speed up/down
set scene speed
set intensity within allowed bounds
```

Do not expose:

```text
raw phase
raw PWM pin
raw duty per bridge input
unbounded duration
unbounded carrier/update changes
```

### Input Manager

Touch behavior:

- `RUN`: start/stop; long press emergency stop.
- `SPD+`: increase signed speed.
- `SPD-`: decrease signed speed, cross through zero into reverse.
- `MUSIC`: next/start music depending on app state.

Hall behavior:

- Use only after Rev-B footprint is corrected or Rev-A manual rework is known.
- Production Hall use should be debounced and fail-safe.
- Hall is not a hard dependency for the current MVP profile.

### RGB Manager

Use the PA14/SPI0 MOSI WS2812 backend for the current Rev-A rework.

Production should keep a small set of effects:

- `stars` default.
- `rainbow` for blended-color visual.
- `breathe`.
- `strobe` only if desired.
- `off`.

Avoid updating WS2812 too frequently during aggressive motor motion. The current
SPI-assisted implementation blocks for under 1 ms, but a badly timed motor ISR
gap can corrupt WS2812 frames. If this remains visible in production, choose one:

- reduce RGB frame rate;
- schedule RGB updates during low-motion windows;
- temporarily mask/pause motor updates during the short LED frame only if the
  motion disturbance is acceptable;
- implement SPI DMA for RGB;
- move RGB to a small external LED controller in a later hardware revision.

### Audio Manager

Keep the WT2003 queue non-blocking.

Production should expose only:

- volume set within `0..31`;
- play known track index/name;
- stop;
- next/previous if needed.

Remove or compile out:

- raw WT2003 command;
- format external flash;
- verbose byte dumps.

### BLE Service

Production BLE should be small and explicit.

Suggested characteristics:

- identity/version;
- status summary;
- command/control with a limited command enum;
- optional counters/faults.

Do not expose raw motor or raw I2C over BLE.

If BLE is not needed for the demo, production can keep advertising/status only
or disable BLE entirely in a special show build. That decision has a large RAM
impact because the WCH BLE heap is `6144 B`.

## Remove From Production

Compile out or move to `bringup`/`factory-test`:

- USB CDC shell and its 2 KB TX ring.
- Shell command parser and report buffers.
- Raw `i2c read/write/scan` except maybe a factory probe.
- IP5209 full register dump and diagnostic bit writes.
- `hall watch`, `touch watch`, and long verbose polling output.
- RGB walk/order/scope diagnostic commands.
- Raw motor wave, static PWM, sine-scope, DMA debug, pin-level commands.
- Audio raw command and flash format command.
- Scope automation dependencies from firmware behavior.
- Long descriptive help text.

Keep docs and tools in the repository; just do not compile the bench surface into
the production image.

## Example Production Defines

This is a starting point, not final code:

```text
APP_BUILD_PROFILE_NAME="production"
APP_FIRMWARE_VERSION="0.2.0-production"
APP_USB_CDC_SHELL_ENABLE=0
APP_DEMO_SCENE_ENABLE=1
APP_DEMO_USB_POWER_ONLY=1
APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1
APP_MOTOR_PWM_BACKEND_ENABLE=1
APP_MOTOR_PWM_MAX_DUTY_PERMILLE=1000
APP_MOTOR_WAVE_TABLE_SIZE=256
APP_MOTOR_CONTROL_UPDATE_HZ=8000
APP_MOTOR_PWM_CARRIER_HZ=40000
APP_MOTION_RAMP_ENABLE=1
APP_MOTION_TOUCH_CONTROL_ENABLE=1
APP_DEMO_DEFAULT_AUDIO_VOLUME=31
APP_DEMO_DEFAULT_RGB_BRIGHTNESS=24
APP_DEMO_DEFAULT_RGB_EFFECT=1
APP_DEMO_DEFAULT_SPEED_HZ_X1000=8000
APP_DEMO_DEFAULT_AMPLITUDE_PERMILLE=1000
APP_DEMO_DEFAULT_GUARD_MODE=<latest proven polarity>
APP_DEMO_DEFAULT_GUARD_DUTY_PERMILLE=600
```

Add new compile gates as needed:

```text
APP_DIAG_COMMANDS_ENABLE=0
APP_RAW_MOTOR_COMMANDS_ENABLE=0
APP_RAW_I2C_COMMANDS_ENABLE=0
APP_AUDIO_FORMAT_COMMAND_ENABLE=0
APP_RGB_TEST_COMMANDS_ENABLE=0
APP_VERBOSE_DEMO_LOG_ENABLE=0
APP_FACTORY_TEST_COMMANDS_ENABLE=0
```

## RAM Targets

The current MVP image uses about `25580 / 26624 B` by linker accounting.
Production should not ship this close to the limit.

Targets:

```text
static/linker RAM <= 21 KB
static/linker RAM <= 80%
measured stack margin >= 2 KB after worst-case demo run
no unbounded shell/log buffers in production
```

Expected first savings:

- USB CDC shell off: about `2.6 KB` data plus possible `.highcode`.
- Remove shell/parser/debug buffers: several hundred bytes.
- Remove WCH SimpleProfile if not used: a few hundred bytes.
- Convert motor DMA tables to 16-bit if safe: about `1 KB`.

See `Firmware/docs/ram_usage_audit.md` for the detailed breakdown.

## Runtime Instrumentation Before Stripping

Before removing diagnostics, add one compact profiler/status path in MVP:

```text
runtime status
```

Suggested fields:

```text
loop_count
loop_hz
max_loop_gap_us
motor_tick_count
motor_missed_update_count
rgb_frame_count
rgb_last_status
audio_queue_depth
audio_last_error
ble_poll_count
usb_tx_busy_count
```

Once production is stable, keep only a minimal internal counter set and expose it
through BLE or a factory build.

## Migration Plan

1. Freeze the current MVP demo as the bench reference.
2. Add runtime loop/missed-update instrumentation to MVP.
3. Create `build_production.ps1` with USB CDC disabled and diagnostics compiled
   out.
4. Verify production image still:
   - boots safely;
   - advertises or intentionally does not advertise;
   - responds to touch controls;
   - starts/stops motion;
   - plays audio;
   - runs RGB effect;
   - faults safe on emergency stop.
5. Measure RAM again and record in `ram_usage_audit.md`.
6. Only after that, tune BLE heap and motor DMA table size.

## Design Rule

Bring-up firmware is allowed to be convenient. Production firmware must be
boring: few states, few commands, bounded timing, bounded memory, and one clear
safe-off path.
