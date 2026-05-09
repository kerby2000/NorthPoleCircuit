# Pre-Hardware Status

This table is the freeze-review status before target hardware arrives. It is deliberately conservative.

| Area | Status | Notes |
|---|---|---|
| USB CDC | NEEDS_DEV_BOARD | Builds and has WCH example parity review, but enumeration and EP behavior are untested. |
| BLE advertising | NEEDS_DEV_BOARD | WCH BLE base builds; advertising needs RF/dev-board validation. |
| BLE GATT | NEEDS_DEV_BOARD | Diagnostic service builds; reads/writes need BLE client validation. No motor writes are exposed. |
| GPIO safe state | NEEDS_TARGET_BOARD | Firmware forces safe pins before BLE/HAL/app init and after HAL sleep blanket GPIO config. Scope target pins. |
| DRV8837 `/SLEEP` | NEEDS_TARGET_BOARD | KiCad audit passes PB0 `/SLEEP`; firmware drives idle/fault low. Requires target scope check. |
| Motor PWM backend | BUILDS_ONLY | PWM backend builds when enabled. Do not tune motion before hardware. |
| WS2812 timing | NEEDS_TARGET_BOARD | Bit-bang assumes 60 MHz and needs logic-analyzer validation. |
| Hall inputs | NEEDS_TARGET_BOARD | GPIO/counter scaffold builds; magnet/sensor behavior untested. |
| Touch sensing | NEEDS_TARGET_BOARD | Placeholder/raw path only; thresholds are not assumed. |
| WT2003 UART/audio | HOST_TESTED | Frame encoder/parser and command queue logic are host-tested; UART response, BUSY behavior, and playback still need target validation. |
| WT2003 USB update connector | NEEDS_TARGET_BOARD | KiCad audit passes J4 pinout; update cable/use remains untested. |
| IP5209 I2C | NEEDS_TARGET_BOARD | I2C framework builds; register meanings and debug-share behavior need target validation. |
| Settings persistence | BUILDS_ONLY | Defaults/CRC/corruption recovery are host-tested. Flash persistence is disabled by default and not implemented/tested. |
