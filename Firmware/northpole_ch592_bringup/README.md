# North Pole CH592 Bring-Up Project

This MounRiver project is copied from the WCH CH592 EVT BLE `Peripheral` example after verifying that the original example builds unchanged with the MounRiver-bundled RISC-V toolchain.

Baseline verified locally:

- EVT SDK: `C:\WCH\CH592EVT\`
- Example: `C:\WCH\CH592EVT\EVT\EXAM\BLE\Peripheral`
- Toolchain: MounRiver `RISC-V Embedded GCC`, `riscv-none-embed-gcc`
- Baseline output: `Firmware\build\evt_peripheral_unmodified\Peripheral.hex`

The copied project preserves the WCH startup file, linker script, HAL, StdPeriphDriver, BLE library linkage, and BLE system initialization through linked SDK resources in `.project`.

Only the `APP/` layer is changed:

- `APP/peripheral_main.c` keeps the WCH BLE init order but inserts North Pole safe pin bring-up.
- `APP/northpole/` contains the board/framework drivers.
- `APP/include/` contains the North Pole framework headers plus the original `peripheral.h`.
- The original WCH `APP/peripheral.c` remains as the known-good BLE peripheral role scaffold and now registers the North Pole diagnostic GATT service.

Implemented bring-up transports and test hooks:

- USB CDC diagnostic shell for host commands.
- `safe check` and `pins verify` board-state reports.
- Time-bounded motor commands with mandatory arming; WCH PWM/timer backend present but disabled by default.
- WS2812 write backend for 6 LEDs with low default brightness.
- Minimal diagnostic BLE service `0xFD90` with no raw motor writes.
- Settings defaults/CRC/corruption test path.
- WT2003 nonblocking state machine marked hardware-validation pending.
- I2C scan/read/write helpers and conservative IP5209 probe/register access.

## Build

From the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile evt-baseline
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

Outputs are written under `Firmware\build\` and are ignored by git.

## MounRiver Import

Import/open `Firmware\northpole_ch592_bringup` in MounRiver Studio. The project name is `northpole_ch592_bringup`.

The linked SDK paths currently assume:

```text
C:\WCH\CH592EVT\EVT\EXAM\BLE\HAL
C:\WCH\CH592EVT\EVT\EXAM\BLE\LIB
C:\WCH\CH592EVT\EVT\EXAM\SRC\Ld
C:\WCH\CH592EVT\EVT\EXAM\SRC\RVMSIS
C:\WCH\CH592EVT\EVT\EXAM\SRC\Startup
C:\WCH\CH592EVT\EVT\EXAM\SRC\StdPeriphDriver
```

## Safety Defaults

At firmware entry, `northpole_ch592_early_safe_pins()` drives all DRV8837 inputs low and keeps the RGB data line low before BLE/application initialization.

The original EVT UART debug setup is not used as-is because it drives PA9, which is `/INT` on this PCB. UART logging is compiled off by default with `NORTHPOLE_ENABLE_UART1_LOG=0`.
