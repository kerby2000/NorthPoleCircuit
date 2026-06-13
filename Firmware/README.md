# Firmware

Firmware for the CH592X BLE/audio revision now has a MounRiver/WCH EVT based bring-up project under `Firmware/northpole_ch592_bringup/`.

The original upstream firmware still exists in `../Code/`, but it targets the original NorthPoleCircuit architecture. Treat it as reference material only. New code should live under `Firmware/` and should be written around the current KiCad design: CH592X BLE MCU, 3 x DRV8837 phase drivers, WT2003H4 audio playback IC, PY25Q64HA audio flash, IP5209 LiPo charge/boost PMIC, ME6211 3.3 V regulator, 2 x DRV5032 Hall sensors, 4 capacitive touch pads, and 6 x addressable RGB LEDs.

## Current Implementation Status

The bring-up framework has been started under this folder and the active target project is copied from the WCH CH592 EVT BLE `Peripheral` example.

- KiCad hardware audit script: `tools/extract_kicad_pinmap.py`
- Generated hardware audit: `docs/hardware_pin_audit.md`
- Generated firmware hardware notes: `northpole_ch592_bringup/APP/include/board_pins_autogen_notes.h`
- MounRiver project: `northpole_ch592_bringup/`
- Safe board pin framework: `northpole_ch592_bringup/APP/include/board.h`, `northpole_ch592_bringup/APP/include/board_pins.h`, `northpole_ch592_bringup/APP/northpole/board.c`
- Bring-up and production entry split: `northpole_ch592_bringup/APP/northpole/app_bringup.c`, `northpole_ch592_bringup/APP/northpole/app_production.c`
- Diagnostic shell scaffold: `northpole_ch592_bringup/APP/northpole/shell.c`
- Peripheral driver scaffolds: motor, Hall, touch, RGB, audio, IP5209, battery, BLE, settings

The selected first target toolchain direction is WCH CH592 EVT SDK native style through MounRiver Studio. See `docs/toolchain.md`.

The unmodified WCH EVT BLE `Peripheral` example builds locally with the MounRiver-bundled `riscv-none-embed-gcc`. The copied North Pole bring-up project also builds locally.

## CH592 Upload Path Status

Primary development/debug path:

```text
MounRiver Studio + WCH-LinkE
```

MounRiver/WCH-LinkE remains the primary development path. A MounRiver LED probe under `mounriver_ch592_led_probe/` builds, downloads, verifies, resets, and runs through MounRiver.

The official native USB ISP GUI path is also proven on the CH592X-EVT-R1-LinkE board:

```text
WCHISPStudio V3.9
Internal tool: WCHISPTool_CH57x-59x V3.10
CH592 USB Download device: 4348:55E0
Driver: official WCH CH375/WCHLink driver stack
```

Secondary and experimental paths are tracked separately:

```text
docs/ch592_upload_matrix.md
docs/ch592_usb_isp_test_plan.md
docs/wlink_attach_debug.md
```

Current policy:

- Daily development/debug: MounRiver, WCH-LinkUtility, or PlatformIO custom `wlink` with manual Download-mode entry.
- Emergency/field flashing: WCHISPStudio GUI native USB ISP is proven; a Windows official CMD tool has not been found yet.
- PlatformIO: `wlink` upload is usable after manual Download-mode entry; native USB ISP through `wchisp` remains experimental.
- PlatformIO bundled `wchisp -V` / no-verify is not acceptable because a no-verify image did not run.
- Do not swap the CH592 USB Download driver with Zadig during normal work. WCHISPStudio and `wchisp` need different driver bindings for `4348:55E0`.

Critical recovery note:

If WCH-LinkUtility, MounRiver, or `wlink` suddenly stop identifying the CH592 after ISP/config
experiments, the chip may be protected. WCHISPStudio can still flash in this state, but WCH-Link
debug attach fails. The recovery that worked was:

```powershell
# Put CH592 into USB Download mode.
# Use Zadig only on USB Module / 4348:55E0, not WCH-Link / 1A86:8010.
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

See `docs/ch592_unprotect_recovery.md`. Keep `Code and data protection mode` unchecked during
bring-up.

If the bootloader no longer stays visible long enough for normal tools, see
`docs/ch592_brick_recovery.md` and the polling recovery script
`tools/ch592_recovery/revive_ch59x.py`.

## Commands

Regenerate the hardware audit:

```powershell
python Firmware\tools\extract_kicad_pinmap.py --repo-root .
```

Run the build wrapper in audit-only mode:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -AuditOnly
```

Build wrappers for the intended profiles:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile evt-baseline
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile production
powershell -ExecutionPolicy Bypass -File Firmware\tools\build_mvp_demo.ps1
powershell -ExecutionPolicy Bypass -File Firmware\tools\build_matrix.ps1
```

These wrappers default to `C:\WCH\CH592EVT` and `C:\MounRiver\MounRiver_Studio2`. Build outputs are expected under `Firmware/build/`, which is ignored by git.

The MVP demo wrapper writes its flashable image to:

```text
Firmware/build/mvp_demo/northpole_ch592_bringup.hex
```

Use this only with the documented Rev-A rework state:
`docs/mvp_demo.md`, `docs/rev_a_mvp_rework_state.md`, and
`docs/first_integrated_demo_checklist.md`.

Run host-side logic tests:

```powershell
python -m unittest discover Firmware\host_tests
```

Smoke-test scripts for first-board validation:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --port COM7
python Firmware\tools\ble_diag_smoke_test.py --scan
```

## Hardware Audit Result

The current generated audit passes the major firmware-facing connections. The suspected WT2003 UART issue is not a board blocker in the current PCB: U2 UART nets reach WT2003 RX/TX through R12/R13 series resistors. The WCH UART backend and WT2003HX V2.00 protocol layer exist in the MounRiver bring-up project, but WT2003 command responses, BUSY timing, and playback still need hardware validation before production use.

## Firmware Strategy

Use two top-level firmware entry points with shared drivers:

- `main_production.c` - the final user-facing firmware. It should expose only safe controls, run quiet logging, enforce current/time limits, handle faults cleanly, and default to low-power idle.
- `main_bringup.c` - a diagnostic firmware with explicit test hooks for board validation. It should expose raw sensor readings, manual peripheral commands, motor safety interlocks, and verbose status output.

Both builds should share the same board support and peripheral drivers. The split should be at the application layer, not by duplicating low-level code.

Recommended structure:

```text
Firmware/
  README.md
  platformio.ini or wch_project/
  include/
    board.h
    build_profile.h
    fault.h
  src/
    main_production.c
    main_bringup.c
    board.c
    clock.c
    log.c
    fault.c
    power_ip5209.c
    battery.c
    touch.c
    rgb_ws2812.c
    hall.c
    motor_drv8837.c
    motor_track.c
    audio_wt2003.c
    audio_assets.c
    ble_service.c
    settings.c
    diagnostics.c
  tools/
    flash.ps1 or flash.sh
    read_log.ps1 or read_log.sh
    audio_asset_pack.py
```

If the selected CH592X toolchain makes separate entry files awkward, use one entry file plus a compile-time profile:

```c
#if defined(FIRMWARE_PROFILE_BRINGUP)
    app_bringup_run();
#else
    app_production_run();
#endif
```

## Shared Firmware Rules

- All outputs must start in the safest state: motor driver inputs low, motor enables off, RGB LED output idle, audio stopped or muted, unused pins configured deliberately.
- Production and bring-up builds must use the same `board.c` pin map so a wiring fix is made once.
- Motor commands must have a timeout/deadman. If the main loop stalls, BLE disconnects, or the command stream stops, PWM should go to zero and the DRV8837s should disable.
- Keep BLE, LED timing, touch sampling, and motor PWM from blocking each other. Use timer/DMA/peripheral support where possible, especially for WS2812 timing.
- Do not allow raw motor bridge commands in production BLE services.
- Keep current limits conservative until real board measurements exist.
- Store board revision, firmware profile, firmware version, and build date in a readable diagnostic command.
- Every bring-up command that can heat the coils, stress the battery, or make sound must be explicit and bounded.

## Toolchain And Project Setup

1. Select the CH592X build path.
   - Prefer the WCH-supported CH592X SDK/toolchain if it gives reliable BLE examples and flashing.
   - Confirm the linker script, startup code, BLE stack library, USB/UART support, and flash programming path before writing board-specific code.
   - Record the exact SDK version and flashing adapter in this README once selected.

2. Create a minimal firmware project.
   - Build an empty CH592X app.
   - Flash it to a known-good dev board or first PCB if available.
   - Confirm reset, boot mode, programming connection, and a basic delay loop.

3. Add build profiles.
   - `production`: optimized, limited logs, watchdog enabled, safe feature set.
   - `bringup`: verbose logs, diagnostic command interface, asserts enabled, extra telemetry.

4. Add repeatable commands.
   - Build production firmware.
   - Build bring-up firmware.
   - Flash selected profile.
   - Capture serial logs.
   - Stamp firmware version and git commit into the build.

5. Create the board pin map.
   - Extract every CH592X pin assignment from `../PCB/NorthPoleCircuit_PCB.kicad_sch`.
   - Define pins once in `board.h` and `board.c`.
   - Include signal direction, reset state, active polarity, alternate function, and safety notes.
   - Verify peripheral mux conflicts before writing drivers.

## Production Firmware Plan

The production firmware should behave like a finished BLE audio postcard, not like a test fixture.

Core production behavior:

- Boot into a safe idle state.
- Run a short self-test that checks firmware identity, stored settings, battery status if available, and peripheral readiness.
- Advertise BLE with a stable device name.
- Support touch controls for local operation.
- Play predefined scenes that coordinate audio, RGB animation, and track motion.
- Use Hall sensor events to synchronize or correct sled movement when possible.
- Use conservative motor acceleration, speed, duty, and runtime limits.
- Enter a low-power idle mode when inactive.
- Expose clear fault behavior: low battery, motor overrun timeout, audio failure, BLE command invalid, storage/settings invalid.

Suggested production touch mapping:

- Touch 1: play/pause current scene.
- Touch 2: next scene or next track.
- Touch 3: brightness or volume step.
- Touch 4: BLE/config action, long-press for pairing/config reset.

Suggested production BLE services:

- Device information: firmware version, board revision, profile, battery/status.
- Playback control: play, pause, stop, next scene, volume.
- Lighting control: brightness, selected palette, animation mode.
- Motion control: safe scene-level speed/intensity only. No raw phase commands.
- Settings: persistent volume, brightness, demo mode, shipping/low-power mode.
- Diagnostics: read-only fault/status counters.

Production safety requirements:

- Motor PWM starts disabled and only enables inside a scene or validated motion command.
- Motor runtime has a hard maximum per activation.
- Motor duty and acceleration are capped by constants measured during bring-up.
- BLE disconnect or invalid command leaves the active scene in a known state.
- Low battery or unstable supply prevents motor start.
- Watchdog resets into motor-off/audio-off/RGB-off state.

## Bring-Up Firmware Plan

The bring-up firmware should make the board easy to validate one subsystem at a time. It should be allowed to expose unsafe low-level commands, but each risky command must require arming and must time out automatically.

Preferred interfaces:

- Serial diagnostic shell over the available debug UART or USB CDC if practical.
- Optional BLE diagnostic service after BLE is proven.
- Human-readable command responses with machine-readable values where possible.

Suggested diagnostic commands:

```text
help
version
status
faults
pins
rail status
rail battery
touch raw
touch stream
touch threshold <pad> <value>
rgb off
rgb one <index> <r> <g> <b>
rgb chase <brightness>
hall read
hall stream
audio status
audio version
audio qstatus
audio qcount-ext
audio qperiph
audio volume <0-31>
audio play-index <file_id>
audio play-name <name-no-ext>
audio stop
i2c scan
ip5209 read
motor arm <seconds>
motor off
motor phase <driver> <a|b|coast|brake> <duty>
motor pwm <driver> <forward|reverse> <duty>
motor step <direction> <steps> <duty>
motor sweep <direction> <duty> <seconds>
scene demo
sleep
reset
```

Bring-up firmware guardrails:

- `motor arm` is required before any motor command.
- The arm window expires automatically.
- Motor duty has a compile-time bring-up maximum that starts very low.
- The shell prints the active current/duty/time limit before enabling motor outputs.
- Audio starts at low volume.
- RGB brightness starts low to limit current.
- Commands that change persistent settings should require explicit confirmation.

## Board Bring-Up Checklist

Use the bring-up firmware only after the hardware passes basic power checks. Do not connect a LiPo cell until the USB power path, charge behavior, and battery pad polarity are verified.

Required bench equipment:

- Current-limited bench supply.
- USB-C power meter or current-limited USB source.
- Digital multimeter.
- Oscilloscope or logic analyzer.
- CH592X-compatible programmer/debug adapter.
- BLE scanner phone or desktop BLE tool.
- Known-good small speaker or dummy load as appropriate.
- Magnet or assembled sled for Hall testing.
- Thermal camera or contact thermometer for motor/PMIC checks.

### Stage 0: Pre-Power Inspection

1. Confirm board revision and assembly options.
2. Inspect USB-C, IP5209, ME6211, CH592X, WT2003H4, DRV8837s, Hall sensors, RGB LEDs, and crystal orientation.
3. Check for solder bridges around the QFN/SSOP/QFP fine-pitch parts.
4. Measure resistance to ground on VBUS, battery pad, boosted rail if exposed, and 3.3 V.
5. Confirm battery pad polarity against board markings.
6. Confirm no visible copper damage around the antenna and matching network.
7. Record photos before applying power.

Exit criteria:

- No hard short on the main rails.
- Battery polarity is confirmed.
- Visual inspection has no unresolved assembly defects.

### Stage 1: Power Path Smoke Test

1. Power from USB-C with a conservative current limit and no battery installed.
2. Measure VBUS at the connector and downstream power path.
3. Measure battery pad voltage and confirm charge behavior is sane with no battery attached.
4. Measure the ME6211 3.3 V output.
5. Check rail ripple and startup shape with the oscilloscope.
6. Touch-check or thermal-check the IP5209, regulator, MCU, and audio IC for abnormal heating.
7. Repeat USB-C plug orientation tests.
8. If using a battery simulator or protected cell, verify charge current and termination behavior before installing a real cell.

Exit criteria:

- 3.3 V is within tolerance.
- No component heats unexpectedly.
- USB-C orientation does not change behavior.
- Battery/charger behavior is understood before a LiPo is attached.

### Stage 2: CH592X Programming And Clock Bring-Up

1. Connect the programmer/debug adapter.
2. Confirm the target can be identified.
3. Flash a minimal bring-up image.
4. Verify reset behavior and boot mode.
5. Start a debug UART or USB log if available.
6. Confirm the system clock and 32 MHz crystal behavior.
7. Blink or toggle a harmless GPIO if one is available, or print a heartbeat log.

Exit criteria:

- Firmware can be flashed repeatedly.
- Reset and boot are reliable.
- A stable debug log or heartbeat exists.

### Stage 3: Safe Pin Initialization

1. Boot the bring-up firmware with all high-power outputs disabled.
2. Verify DRV8837 input pins and enables are inactive after reset and after firmware startup.
3. Verify the RGB data line does not produce random LED colors at boot.
4. Verify audio control pins do not start playback or pop the speaker.
5. Confirm unused pins are configured to avoid floating high-current states.
6. Save oscilloscope captures of sensitive outputs during reset.

Exit criteria:

- Firmware startup does not move the sled, flash the LEDs unexpectedly, or start audio.
- All unsafe outputs remain off until commanded.

### Stage 4: Low-Speed Digital Interfaces

1. Run `i2c scan`.
2. Confirm the IP5209 responds if its I2C interface is present and enabled on this board.
3. Read battery/charge/status registers if supported.
4. Log fallback ADC or GPIO status if the PMIC interface is not usable.
5. Verify pullups, bus idle levels, and clock/data waveforms.

Exit criteria:

- I2C bus does not hang.
- Power/battery status path is either working or documented as unavailable.

### Stage 5: Capacitive Touch

1. Read raw counts from all 4 pads with the board untouched.
2. Read raw counts while touching each pad individually.
3. Repeat with USB powered, battery powered, and while audio/RGB/motor are idle.
4. Tune baseline tracking, debounce, hysteresis, and long-press timing.
5. Check false touches near the track, battery, speaker, and antenna areas.
6. Store initial thresholds as conservative defaults.

Exit criteria:

- Each pad has clear touch/no-touch separation.
- False triggers are rare during idle.
- Long press and short press behavior is repeatable.

### Stage 6: RGB LED Chain

1. Start with global brightness capped low.
2. Light one LED red, green, and blue to verify color order.
3. Step through all 6 LEDs to verify chain order and soldering.
4. Run a low-brightness chase pattern.
5. Check supply ripple and current draw at increasing brightness levels.
6. Confirm BLE and touch still work while LEDs update.

Exit criteria:

- All 6 LEDs respond in the expected order.
- Color order is documented.
- Production brightness limit is chosen from measured current draw.

### Stage 7: Hall Sensors

1. Read idle state from HALL1 and HALL2.
2. Move a magnet over each sensor and record transition polarity.
3. Test with the actual sled magnet orientation.
4. Decide whether polling or interrupts are more reliable.
5. Tune debounce and minimum interval filtering.
6. Log events while manually moving the sled around the track.

Exit criteria:

- Both Hall sensors detect the sled reliably.
- Event polarity and timing are documented.
- Production filtering constants are chosen.

### Stage 8: Audio Playback

1. Confirm WT2003H4 power rails and reset state.
2. Verify UART idle level and baud rate.
3. Run `audio version`, `audio qperiph`, and `audio qcount-ext`.
4. After the J4 +5 V update-power issue is fixed or a rework path is documented, program or install one minimal known-good `0001.mp3` asset, then disconnect WT2003 USB.
5. Set `audio volume 5` and play a short low-volume test file with `audio play-index 1`.
6. Test stop, volume, next-file, and error/status responses.
7. Measure current draw and check for speaker pops at startup/shutdown.
8. Document the audio file naming, indexing, and flash programming process.

Exit criteria:

- At least one audio file plays reliably.
- Volume and stop commands work.
- Audio asset packaging is repeatable.

### Stage 9: BLE Bring-Up

1. Advertise a bring-up device name that includes the board/profile.
2. Confirm discovery with a phone or desktop BLE scanner.
3. Connect, disconnect, and reconnect repeatedly.
4. Expose read-only version/status first.
5. Add safe commands for RGB, audio, and touch status.
6. Add production-level playback/settings controls after the basic service is stable.
7. Test BLE while RGB animations and touch scanning are active.

Exit criteria:

- Advertising and reconnect are reliable.
- GATT characteristics are documented.
- BLE does not break timing-sensitive peripherals.

### Stage 10: DRV8837 And Track Motor Bring-Up

Do this last. Use a current-limited supply and start with the lowest possible duty cycle. Keep the board accessible for temperature checks.

1. Confirm motor/coil resistance and expected current before enabling firmware control.
2. Scope each DRV8837 input while outputs are still disabled or unloaded if practical.
3. Enable one DRV8837 at a time with very low duty and short duration.
4. Test coast, brake, forward, and reverse behavior where applicable.
5. Verify no shoot-through command sequence is possible in the driver abstraction.
6. Measure supply current and component temperature after each short pulse.
7. Increase duty only after current and heating are understood.
8. Bring up phase stepping without the sled, then with the sled.
9. Determine phase order, direction sign, starting duty, running duty, acceleration ramp, and maximum safe runtime.
10. Use Hall events to validate speed and position assumptions.
11. Save measured safe limits into production constants.

Exit criteria:

- All 3 DRV8837 channels respond correctly.
- The sled can move under firmware control.
- Thermal/current limits are measured and documented.
- Production motor constants are conservative.

### Stage 11: Integrated Demo

1. Build one simple scene: touch starts audio, RGB animation, and low-speed sled movement.
2. Use Hall checkpoints to log movement timing during the scene.
3. Test scene stop from touch and BLE.
4. Test fault paths: BLE disconnect, low battery condition if simulated, motor timeout, audio failure.
5. Run a repeated demo loop and monitor battery voltage, current, and component temperature.
6. Tune scene timing, brightness, volume, and motor intensity.

Exit criteria:

- The board runs an integrated demo without resets or unsafe heating.
- The user-facing controls are understandable.
- Fault handling returns to a safe state.

## Driver Implementation Notes

### `board.c` / `board.h`

- Own all pin names, alternate functions, active polarity, and safe reset states.
- Provide `board_init_safe_pins()` before any subsystem driver starts.
- Provide `board_print_pin_map()` for bring-up firmware.

### `fault.c`

- Centralize faults and warnings.
- Track sticky faults until reset or explicit clear.
- Production faults should shut down motor/audio where appropriate.

### `log.c`

- Bring-up profile: verbose serial logs and command responses.
- Production profile: compact event counters and optional BLE-readable status.

### `power_ip5209.c` / `battery.c`

- Read PMIC status if available.
- Provide battery state abstraction even if the first revision only supports coarse status.
- Gate motor start on acceptable supply conditions.

### `touch.c`

- Support raw reads, baseline tracking, thresholds, debounce, short press, and long press.
- Bring-up firmware should stream raw values.
- Production firmware should emit semantic events only.

### `rgb_ws2812.c`

- Keep timing isolated from BLE and motor PWM.
- Provide global brightness limiting.
- Include color order and LED order constants discovered during bring-up.

### `hall.c`

- Support direct reads, optional interrupts, timestamped events, debounce, and event counters.
- Production code should consume events without depending on exact mechanical timing.

### `audio_wt2003.c`

- Wrap WT2003HX UART protocol details behind clear commands: init, status, query, play file, stop, pause, next/previous, output mode, sleep, and volume.
- Keep frame encode/parser tests for datasheet examples, plus timeouts and error reporting.
- Keep the audio asset manifest in firmware or generated from the audio packaging tool.

### `motor_drv8837.c` / `motor_track.c`

- Low-level driver owns each DRV8837 input state and PWM duty.
- Track driver owns phase stepping, direction, ramping, run timeout, and Hall feedback.
- Production code commands motion intent, not raw bridge state.
- Bring-up code may command raw bridge state only while armed.

### `ble_service.c`

- Start with read-only status.
- Add safe write characteristics one group at a time.
- Keep raw diagnostics out of the production profile unless they are read-only.

### `settings.c`

- Store persistent user settings with versioning and checksum.
- Include factory reset path from touch long-press and diagnostic command.
- Never let corrupt settings produce unsafe motor or audio behavior.

## Test And Validation Plan

Bench validation:

- Each driver has a bring-up command that proves it independently.
- Each risky command has a timeout.
- Every subsystem logs pass/fail and measured values.
- Capture rail voltage/current notes for each stage.

Firmware validation:

- Production and bring-up profiles both build from a clean checkout.
- Static pin map review matches the schematic.
- Startup state leaves high-power outputs disabled.
- Watchdog or main-loop health monitor is tested.
- Settings corruption path is tested.
- BLE reconnect path is tested.

System validation:

- Integrated demo runs from USB power.
- Integrated demo runs from battery after charger validation.
- Touch works while audio and RGB are active.
- BLE remains usable during a running scene.
- Motor current and temperature remain inside measured limits.
- The board can recover from reset during a scene without leaving outputs enabled.

Release validation:

- Tag firmware version.
- Save build artifacts for production and bring-up profiles.
- Record audio asset pack version.
- Record board revision and any required hardware rework.
- Update this README with measured limits and known issues.

## Initial Milestones

1. Create CH592X firmware skeleton and confirm flashing.
2. Add `main_bringup.c`, logging, and safe pin initialization.
3. Add board pin map from the KiCad schematic.
4. Bring up power/status and basic digital interfaces.
5. Bring up touch, RGB, Hall, audio, then BLE.
6. Bring up DRV8837/motor control last.
7. Create `main_production.c` with safe scene-level behavior.
8. Tune production constants from measured board data.
9. Run integrated demo and fault testing.
10. Freeze firmware release candidate and keep bring-up firmware available for future board revisions.

## Known Risks To Track

- WT2003H4 sourcing, UART protocol details, and audio asset layout.
- CH592X peripheral mux conflicts once exact pins are confirmed.
- WS2812 timing interference from BLE stack interrupts.
- Capacitive touch thresholds changing with battery state, enclosure, humidity, or hand position.
- DRV8837 current and heating during track operation.
- BLE antenna tuning and nearby copper/mechanical effects.
- IP5209 telemetry availability and behavior on this exact variant.
- LiPo charge current, battery protection, and safe shipping/storage behavior.
