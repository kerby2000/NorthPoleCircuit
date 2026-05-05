# Firmware

Firmware for the CH592X BLE/audio revision has not been written yet.

The original upstream firmware still exists in `../Code/`, but it targets the original NorthPoleCircuit architecture. Treat it as reference material only.

## Planned Bring-Up Order

1. Power-on smoke test.
2. CH592X boot/programming test.
3. BLE advertising test.
4. GPIO and status LED/debug output.
5. Capacitive touch raw reading and threshold tuning.
6. RGB LED chain test.
7. Hall sensor interrupt/polling test.
8. WT2003H4 UART/audio playback test.
9. DRV8837 low-duty motor-driver smoke test.
10. Sled movement and speed-control experiments.
11. Integrated BLE/touch/audio/light/motion demo.

## Planned Responsibilities

- BLE control and configuration.
- Capacitive touch controls.
- Motor phase timing and speed control.
- Hall checkpoint processing.
- RGB animation.
- UART control of the audio playback IC.
- Power/status handling for the battery/boost subsystem.
