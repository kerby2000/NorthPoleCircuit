# MVP Runtime Timing And Headroom

This note captures the current Rev-A MVP firmware timing model. It is based on
code inspection and the current `Firmware/tools/build_mvp_demo.ps1` defines, not
on cycle-accurate profiling.

For a fresh-session index, start with `Firmware/docs/fresh_session_handoff.md`.
For RAM details and the production split plan, see:

- `Firmware/docs/ram_usage_audit.md`
- `Firmware/docs/production_firmware_architecture.md`

## Current MVP Build

The MVP profile enables:

- USB CDC shell.
- BLE peripheral.
- WT2003 UART audio command queue.
- PA14/SPI0-MOSI WS2812 LED backend.
- 256-step A/B motor wave backend.
- TMR3 motor control update at 8 kHz.
- Motor PWM carrier at 100 kHz.
- Demo scene state machine, touch handling, RGB scenes, and audio start.

The current build is RAM-heavy. Recent MVP builds were around 96% static RAM
used on a 26 KB RAM device. This is the largest known headroom risk.

## Scheduling Model

There is no RTOS. There is no real task-switching overhead.

The application is one cooperative main loop plus hardware interrupts. The main
loop order is:

```text
shell_poll()
usb_cdc_shell_poll()
hall_poll()
touch_poll()
demo_scene_poll()
motion_control_poll()
audio_wt2003_poll()
motor_drv8837_poll()
ble_service_poll()
```

Interrupt work includes:

- USB endpoint interrupt handling for CDC.
- WCH BLE/TMOS-related timing from the SDK.
- TMR3 motor wave update interrupt.
- Timer/DMA hardware activity for the A phase waveform.

The main-loop functions are expected to be short and non-blocking. Long shell
prints, synchronous RGB frames, or excessive interrupt load can delay later
polling work such as audio and BLE service.

## RGB Timing

In the MVP build, RGB uses:

```text
backend = spi0-mosi-pa14
SPI0 MOSI bit rate = APP_WS2812_SPI0_MOSI_HZ = 3.2 MHz
LED count = 6
```

It is not DMA based. It is also not the old PA15 bit-bang path.

Each WS2812 data byte is encoded into 4 SPI bytes using:

```text
WS bit 1 = 1110
WS bit 0 = 1000
```

Approximate time for one six-LED frame:

```text
payload: 6 LEDs * 24 WS bits * 4 SPI bits = 576 SPI bits ~= 180 us
reset low before frame: ~= 240 us
reset low after frame inside platform_write(): ~= 240 us
extra idle-low from rgb_ws2812_show(): ~= 240 us
total typical blocking time: ~= 660-900 us plus polling overhead
```

The SPI backend does not disable interrupts. That is good for USB/BLE/motor
latency, but it means the TMR3 motor ISR can preempt between SPI bytes. If a
preemption gap ever becomes long enough, the LED strip can interpret it as a
reset/latch boundary and the frame can be corrupted. This is the main reason RGB
can be stable standalone yet less reliable during heavy motor activity.

The demo RGB scene updates at most every 120 ms, so RGB frame rate is low. The
risk is not average CPU load; the risk is a badly timed interrupt gap during a
WS2812 frame.

## Touch Timing

Touch is polled in the main loop before the demo state machine runs.

Demo touch actions are edge based:

- Press edge prints the pressed pad and current demo state.
- Release edge applies the action.
- `RUN` short release toggles start/stop.
- `RUN` long press is emergency stop.
- `SPD+` and `SPD-` change signed speed.
- `MUSIC` queues WT2003 next/play commands.

If touching pads during the demo prints nothing, the most likely causes are:

- main loop is not reaching `demo_scene_poll()` often enough;
- USB CDC logging is delayed or back-pressured;
- touch raw state is not changing in the firmware while motion is active;
- the log was produced but the host terminal missed it during disconnect/reopen.

It is not a separate task that can be preempted by an RTOS scheduler.

## Audio Timing

WT2003 audio commands are queued, then sent from `audio_wt2003_poll()`.

Relevant constants:

```text
APP_AUDIO_POWER_ON_DELAY_MS = 1000
APP_AUDIO_COMMAND_SPACING_MS = 250
APP_AUDIO_COMMAND_TIMEOUT_MS = 500
APP_AUDIO_RETRY_COUNT = 0
```

`demo start` queues volume and play commands. The firmware now logs the enqueue
status for both. That proves the firmware requested playback, but actual sound
still depends on the WT2003 accepting the UART command and its own playback
latency.

The WT2003 path should not be CPU-heavy. It is mostly UART TX/RX plus small
parser work in the main loop. If music does not start during demo while manual
`audio play-index 1` works, check:

- the new `demo audio start ... play_status=...` log line;
- `audio status` after `demo start`;
- whether the command queue is blocked by an earlier pending command timeout;
- whether demo stop/timeout quickly sends `audio stop`.

## Motor Timing

The known-good MVP motion uses:

```text
carrier_hz = 100000
control_update_hz = 8000
sine table = 256 phase positions
A/B propulsion = phase accumulator in TMR3 ISR
A phase = TMR1/TMR2 DMA-assisted where supported
B/G phase updates = register writes from the TMR3 ISR
guard = fixed direction/duty for the current MVP behavior
```

At 8 kHz update, TMR3 interrupts every 125 us. The ISR updates the phase
accumulator, applies A/B/G outputs, and increments counters. This is fast enough
for the current motor behavior, but it is the most important CPU-interrupt load
in the MVP image.

Average motor CPU load is unknown until measured. The critical thing for
USB/BLE/RGB is worst-case interrupt latency, not average load.

## BLE Headroom

BLE service is cooperative in the application loop plus SDK timing underneath.
The current firmware can advertise and expose the custom service in bring-up
profiles, but the integrated MVP image is tight:

- RAM use is already close to the 26 KB limit.
- Long shell output can delay BLE polling.
- Heavy motor ISR load can reduce foreground-loop service time.
- WS2812 frames are short but timing-sensitive.

The current MCU can probably serve the MVP if interrupts stay short and logs are
kept modest. It is not a comfortable production margin yet.

## Practical Conclusions

- RGB is SPI-assisted polling, not DMA.
- Touch, audio queue handling, demo state, shell, and BLE service are
  cooperative main-loop work.
- Motor wave timing is interrupt-driven and currently the dominant periodic
  preemption source.
- There is essentially no task switching overhead; there is interrupt latency
  and blocking foreground work.
- Static RAM headroom is the most visible resource risk.
- CPU headroom is not yet measured.

## Recommended Next Instrumentation

Add a lightweight runtime profiler command before increasing MVP complexity:

```text
runtime status
```

Suggested fields:

```text
loop_count
loop_hz
max_loop_gap_us
usb_tx_drop_or_busy_count
log_drop_count
motor_tick_count
motor_missed_update_count
rgb_frame_count
rgb_last_status
audio_queue_depth
audio_last_command
audio_last_error
ble_poll_count
```

For RGB specifically, consider one of these if demo corruption remains:

- temporarily pause the motor update around the <1 ms RGB frame;
- move WS2812 output to a DMA-fed SPI transfer if CH592 SPI DMA is practical;
- update RGB less often or only during low-motion moments;
- use a separate tiny LED controller in Rev-B if production effects must run
  while motor control is aggressive.
