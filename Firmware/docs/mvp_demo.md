# MVP Demo Firmware

This profile is the integrated Rev-A demo image for the current hand-reworked
target board. It is intentionally separate from the generic bring-up build.

For a compact handoff to a new session, see
`Firmware/docs/fresh_session_handoff.md`.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build_mvp_demo.ps1
```

Output:

```text
Firmware\build\mvp_demo\northpole_ch592_bringup.hex
Firmware\build\mvp_demo\build_manifest.txt
```

The wrapper builds the normal CH592 bring-up project with these MVP defines:

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

Current build note: the MVP image is RAM-heavy, around 96% RAM used in the
current MounRiver/WCH SDK build. Treat this as acceptable for the demo but not
as a production memory budget.

Runtime/timing details are tracked in
`Firmware/docs/mvp_runtime_timing.md`. In short: the MVP firmware has one
cooperative main loop plus interrupts; RGB uses the PA14 SPI0-MOSI WS2812
backend without DMA; motor wave timing uses TMR3 interrupts; WT2003 audio
commands are queued from the main loop.

## Hardware Assumptions

This MVP image assumes the Rev-A rework state documented in
`Firmware/docs/rev_a_mvp_rework_state.md`:

- USB power connected.
- Battery-only IP5209 boost operation is not required.
- WT2003 `BUSY` trace is cut.
- PA14/SPI0 MOSI is jumpered to the WS2812 data input.
- Old PA15 `/LED` trace is isolated.
- Hall sensors are not relied on unless manually reworked. The MVP demo build
  defaults Hall handling off because Rev-A Hall output routing is not usable on
  the current board.

## Demo Commands

After flashing, open the USB CDC shell and check:

```text
version
demo status
faults
```

Start and stop:

```text
demo clear-faults
demo start
demo stop
demo emergency-stop
```

Tune before starting:

```text
demo duration 10000
demo speed 8000
demo intensity 100
demo audio track 1
demo audio volume 31
demo rgb brightness 24
demo rgb effect stars
demo hall off
demo clear-faults
demo start
```

`demo start` now uses the same direct smooth wave backend as the known-good
manual motion checkpoint below, instead of the higher-level `motion start`
wrapper. This is intentional for the MVP: the demo should exercise the proven
Rev-A movement path first, then the higher-level controller can be cleaned up
separately.

Expected RGB behavior during the demo:

- During running, the default MVP demo effect is `stars`: random dim
  white/blue twinkles across the six LEDs.
- The demo effect can be changed before or during the run:

```text
demo rgb effect stars
demo rgb effect rainbow
demo rgb effect chase
demo rgb effect breathe
demo rgb effect strobe
demo rgb effect christmas
```

- `rainbow` is the best demo effect for checking blended color order and
  brightness scaling.
- Hall-triggered white flashes only occur when Hall handling is enabled with
  `demo hall on`; the MVP profile defaults Hall off until the reworked sensors
  are verified.
- The startup all-blue fill was removed because it made it too easy to confuse
  a stuck pre-run state with a valid chase pattern.

Touch behavior when `APP_DEMO_SCENE_ENABLE=1`:

- Touch actions are taken on release edges. A press alone only logs the press;
  the command is applied when the pad is released after debounce.
- `RUN` short release toggles demo start/stop.
- `RUN` long press triggers emergency stop.
- `SPD+` increases signed electrical speed. In forward motion this means
  faster; in reverse motion it moves the signed speed back toward forward.
- `SPD-` decreases signed electrical speed. In forward motion this slows down,
  crosses through zero, then reverses; in reverse motion it becomes faster
  reverse.
- `MUSIC` starts the configured demo track when the demo is idle, and sends
  WT2003 `next` while the demo is running. WT2003 start latency is expected.
- Every demo touch press/release now prints what action is expected and which
  internal speed/audio/demo command was requested.

The demo is bounded by `APP_DEMO_MAX_RUNTIME_MS` and stops motor/audio/RGB on
timeout, command stop, fault, or emergency stop.

If `demo status` reports `state=FAULT` or a non-zero `faults=0x...`, run
`demo clear-faults` after confirming the board is safe. Stale sticky faults from
manual audio or motor experiments can otherwise block `demo start` even when the
individual subsystems work.

## Known Good Manual Motion Reference

Keep this command as the manual movement checkpoint while tuning the demo scene:

```text
motor wave-run-smooth 8000 1000 10000 all sleep1 fwd guard-rev guard-duty 600
```

This is the current MVP direction policy: A/B propulsion forward, guard rail
reverse. If the mechanical sled/track orientation changes, compare with
`guard-fwd` before changing firmware defaults.

## Component Checks Before Demo

Run these after flashing if one subsystem looks suspicious:

```text
rgb backend
rgb brightness 24
rgb walk 24 0 0 250
rgb walk 0 24 0 250
rgb walk 0 0 24 250
rgb scene stars 8 24
rgb scene rainbow 10 24 120
rgb scene breathe 0 0 255 3 35
rgb scene strobe 8 24
rgb scene christmas 10 180
rgb chase 24

hall read
hall watch 15000 20

audio volume 31
audio play-index 1
```

`hall watch` prints level/edge changes while you move a magnet near each sensor.
`rgb walk` clears the strip and lights one LED at a time, which is better than
`rgb chase` for checking physical LED order and stuck pixels.

RGB scene expectations:

- `stars`: random white/blue twinkles.
- `rainbow`: dim blended rainbow moves across the six LEDs.
- `breathe`: all LEDs fade in/out together in the requested color.
- `strobe`: all LEDs flash white together.
- `christmas`: red/green pattern rotates around the strip.

## Out Of Scope

Do not use this MVP profile to debug:

- IP5209 battery boost.
- Raw BLE motor control.
- New pin remaps.
- Production low-power behavior.

Those are separate workstreams.
