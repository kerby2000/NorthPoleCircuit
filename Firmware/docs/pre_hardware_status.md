# Pre-Hardware Status

This table is the freeze-review status before target hardware arrives. It is deliberately conservative.

| Area | Status | Notes |
|---|---|---|
| USB CDC | DEV_BOARD_PASS / NEEDS_TARGET_BOARD | Windows CDC shell passed on CH592X-EVT-R1-LinkE, including reset recovery. Still validate enumeration and safety behavior on the target PCB and Linux. |
| BLE advertising | DEV_BOARD_PASS / NEEDS_TARGET_BOARD | `NorthPole BLE` advertising passed on CH592X-EVT-R1-LinkE. Still validate RF behavior on the target PCB. |
| BLE GATT | DEV_BOARD_PASS / NEEDS_TARGET_BOARD | Diagnostic service `0xFD90` reads and clear-faults write passed on CH592X-EVT-R1-LinkE. No motor writes are exposed. Still validate on the target PCB. |
| GPIO safe state | NEEDS_TARGET_BOARD | Firmware forces safe pins before BLE/HAL/app init and after HAL sleep blanket GPIO config. Scope target pins. |
| DRV8837 `/SLEEP` | NEEDS_TARGET_BOARD | KiCad audit passes PB0 `/SLEEP`; firmware drives idle/fault low. Requires target scope check. |
| Motor PWM backend | BUILDS_ONLY | PWM backend builds when enabled. Do not tune motion before hardware. |
| WS2812 timing | NEEDS_TARGET_BOARD | Bit-bang assumes 60 MHz and needs logic-analyzer validation. |
| Hall inputs | NEEDS_TARGET_BOARD | GPIO/counter scaffold builds; magnet/sensor behavior untested. |
| Touch sensing | NEEDS_TARGET_BOARD | Placeholder/raw path only; thresholds are not assumed. |
| WT2003 UART/audio | HOST_TESTED | Frame encoder/parser and command queue logic are host-tested; UART response, BUSY behavior, and playback still need target validation. |
| WT2003 USB update connector | BLOCKED | KiCad audit shows J4 pin 1 is unconnected, so J4 currently has D+/D-/GND only and cannot provide the expected WT2003 USB/update +5V path. Do not use J4 for WT2003 USB update until the schematic/PCB is fixed or a rework power path is documented. |
| IP5209 I2C | TARGET_PARTIAL | Target board ACKs at `0x75` after `i2c release-debug`; `ip5209 read 0x00` and `0x01` work. Register decoding is being added from the Injoinic IP5209/IP5109/IP5207/IP5108 I2C register PDF. |
| Settings persistence | BUILDS_ONLY | Defaults/CRC/corruption recovery are host-tested. Flash persistence is disabled by default and not implemented/tested. |
