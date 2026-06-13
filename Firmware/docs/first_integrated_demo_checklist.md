# First Integrated Demo Checklist

Use this checklist before running the MVP demo in front of anyone.

## Build And Flash

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build_mvp_demo.ps1
```

Flash:

```text
Firmware\build\mvp_demo\northpole_ch592_bringup.hex
```

Use the proven WCH-Link/MounRiver/WCH-LinkUtility path. Keep the board on USB
power for this checklist.

## Pre-Run Checks

USB CDC shell:

```text
version
faults
demo status
rgb off
audio status
motor status
```

Expected:

- `version` reports `profile=mvp-demo`.
- No sticky fault bits.
- USB current is stable before motor start.
- RGB commands still control all six LEDs.
- WT2003 audio is ready enough for playback testing.

## Dry Run

Run without relying on Hall:

```text
demo hall off
demo duration 10000
demo speed 8000
demo intensity 100
demo audio volume 31
demo rgb brightness 24
demo status
```

Then:

```text
demo start
```

Watch:

- Sled motion.
- RGB activity.
- Audio start.
- USB current.
- DRV8837 temperature.

Emergency stop command:

```text
demo emergency-stop
```

Recovery:

```text
demo status
faults
motor off
rgb off
audio stop
```

## Touch Run

Only after the shell start/stop path is stable:

- Short `RUN`: start/stop.
- Long `RUN`: emergency stop.
- `SPD+`: increase speed.
- `SPD-`: decrease speed; crossing zero reverses direction.
- `MUSIC`: play/advance track.

## Stop Criteria

Stop immediately if:

- USB current rises unexpectedly above the bench norm.
- DRV8837 becomes too hot to comfortably touch.
- Sled leaves the rails.
- RGB data latches all-white unexpectedly.
- USB CDC disconnects repeatedly.
