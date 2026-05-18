# First Board Firmware Bring-Up

This sequence is intentionally conservative. Do not run motor movement tests until safe states, USB, BLE, RGB, sensors, audio, and I2C have been checked.

## Starting Point

The CH592 firmware base has already passed on the CH592X-EVT-R1-LinkE dev board:

- BLE advertisement as `NorthPole BLE`.
- Diagnostic GATT service `0xFD90` reads.
- USB CDC shell on Windows.
- USB CDC `reset` recovery.
- Automated BLE advertisement scan after reset.

The first real NorthPole PCB session should therefore focus on electrical safety and pin behavior, not on proving the CH592 BLE stack from scratch.

## Build Target-Board Firmware

Use the normal bring-up profile, with no dev-board smoke defines:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

Expected output:

```text
BUILD_OK ...\Firmware\build\bringup\northpole_ch592_bringup.hex
```

Flash this HEX to the target board:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

Do not flash a dev-board smoke build to the target board for first electrical validation.

## Required Order

1. Inspect the board under magnification for solder bridges, reversed ICs, USB-C connector damage, and regulator/PMIC orientation.
2. Power the board from a current-limited supply or a protected USB port.
3. Verify 3.3 V before flashing application firmware.
4. Flash an unmodified WCH CH592 EVT example first, preferably `C:\WCH\CH592EVT\EVT\EXAM\BLE\Peripheral`.
5. Verify the USB bootloader or WCH-LinkE programming path through J3 is reliable.
6. Flash `northpole_ch592_bringup`.
7. Open the USB CDC shell.
8. Run `version`.
9. Run `safe check`.
10. Run `pins verify`.
11. Scope PB0 `/SLEEP` and all six DRV8837 inputs at reset and after `motor off`.
12. Scope the RGB data line idle state.
13. Confirm J4 is treated only as WT2003 USB update, not SWD/debug.
14. Test BLE advertising and read the diagnostic service.
15. Only then test RGB commands.
16. Test Hall inputs.
17. Test touch raw readings.
18. Test WT2003 audio status and protocol commands only after preparing one low-volume `0001.mp3` asset through J4 and disconnecting the WT2003 USB cable.
19. Test `i2c scan` and `ip5209 status` without WCH-LinkE actively debugging.
20. Test motor logic with no load or disconnected coils first.
21. Test motor PWM with current limiting and a logic analyzer before allowing any real motion.

## First-Flash Checklist

Use this checklist the first time the actual NorthPole PCB is flashed.

### 1. Pre-Power Inspection

- Confirm no obvious solder bridges on CH592X, DRV8837s, WT2003, IP5209, USB-C, and regulator pins.
- Confirm J3 is the WCH-LinkE/debug connector.
- Confirm J4 is the WT2003 USB update connector, not debug.
- If practical, leave the motor/load disconnected for the first power-up.
- If R14/R15 are populated, remember that PB14/PB15 are shared between WCH-LinkE debug and IP5209 I2C.

### 2. First Power

- Use a current-limited bench supply or protected USB source.
- Start with a conservative current limit.
- Verify `+3.3V` is present and stable.
- Stop immediately if current is unexpectedly high, the regulator heats, or 3.3 V is out of tolerance.

### 3. Flash

Preferred debug/programming path:

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" flash --chip CH59X --speed low --erase "Firmware\build\bringup\northpole_ch592_bringup.hex"
```

If `wlink` is unreliable, use MounRiver or WCH-LinkUtility with:

| Setting | Value |
|---|---|
| Core | RISC-V |
| Series | CH590/1/2 |
| Address | `0x00000000` |
| CLK Speed | Low |
| Operations | Erase All, Program, Verify, Reset and Run |

Do not change code-protection or debug-protection settings during normal bring-up.

### 4. USB CDC Shell

After reset, Windows should enumerate a CDC serial port. If it does, run:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --profile target --port COMxx --timeout 3
```

Replace `COMxx` with the target board's COM port. If the script is too broad for the first pass, manually run:

```text
version
status
faults
settings show
pins verify
safe check
audio status
motor status
```

Expected:

- `faults=0x00000000` unless a known, documented first-board condition exists.
- `motor_armed=0`.
- `safe check` reports `/SLEEP` and all six DRV8837 inputs expected low.
- `audio status` remains `HARDWARE_VALIDATION_PENDING`.

### 5. Mandatory Scope Checks Before Any Output Test

Do these with the board freshly reset and before running RGB, audio, I2C, or motor commands:

| Signal | Expected |
|---|---|
| PB0 `/SLEEP` | Low |
| `/PWM_A1` | Low |
| `/PWM_A2` | Low |
| `/PWM_B1` | Low |
| `/PWM_B2` | Low |
| `/PWM_G1` | Low |
| `/PWM_G2` | Low |
| `/LED` | Low, no boot pulse |
| WT2003 UART TX | Safe input before audio command; idle high only once UART is enabled |

Then run:

```text
motor off
safe check
```

Confirm the same safe states again.

### 6. BLE Diagnostic Check

After the electrical safe state passes, verify BLE:

```powershell
python Firmware\tools\ble_diag_smoke_test.py --scan --timeout 10
```

Expected:

- Device name `NorthPole BLE`.
- Version, board revision, build profile, status packet, and counters packet read successfully.
- Clear-faults write succeeds.

Do not use BLE RGB/audio writes until the relevant hardware path has been tested locally from USB CDC.

### 7. Stop Point For First Session

The first target-board session is successful if:

- 3.3 V is stable.
- Firmware flashes and boots.
- USB CDC or BLE diagnostics work.
- `/SLEEP`, all DRV8837 inputs, and `/LED` are safe at reset.
- `safe check` and `pins verify` match the PCB audit.

Stop there and save notes before moving to RGB, sensors, audio, I2C, or motor tests.

## Initial Shell Commands

```text
version
status
safe check
pins verify
faults
settings show
motor off
rgb off
audio status
audio version
audio qperiph
ip5209 status
```

## Probe Points And Expected Safe States

| Signal | CH592 pad/function | Net | Probe target | Expected safe state |
|---|---|---|---|---|
| DRV8837 A IN1 | pad 32 `TMR2` | `/PWM_A1` | U10 IN1 or MCU pad | Low after reset |
| DRV8837 A IN2 | pad 1 `TMR1_` | `/PWM_A2` | U10 IN2 or MCU pad | Low after reset |
| DRV8837 B IN1 | pad 17 `PWM9` | `/PWM_B1` | U11 IN1 or MCU pad | Low after reset |
| DRV8837 B IN2 | pad 18 `PWM7` | `/PWM_B2` | U11 IN2 or MCU pad | Low after reset |
| DRV8837 G IN1 | pad 30 `PWM5` | `/PWM_G1` | U9 IN1 or MCU pad | Low after reset |
| DRV8837 G IN2 | pad 31 `PWM4` | `/PWM_G2` | U9 IN2 or MCU pad | Low after reset |
| DRV8837 global SLEEP | pad 3 `PB0` | `/SLEEP` | U9/U10/U11 `~SLEEP` or MCU pad | Low after reset and after `motor off`; high only during armed bounded motor tests |
| RGB data | pad 28 `PA15` | `/LED` | first LED DIN | Low after reset, no boot pulse expected |
| WT2003 UART TX | pad 13 `TXD1_` | through R12 to `/RX1` | R12/WT RXD | Input/safe until audio command opens UART; idle high when UART enabled |
| WT2003 BUSY | pad 29 `PA14` | `/BUSY` | WT BUSY | Input, do not drive |
| Hall 1 | pad 2 `PB6` | `/HALL1` | sensor output | Input, raw level depends magnet/sensor |
| Hall 2 | pad 26 `PA4` | `/HALL2` | sensor output | Input, raw level depends magnet/sensor |
| Touch SPD- | pad 7 `AIN10` | `/SPD--` | touch pad | Analog/input, not driven |
| Touch RUN | pad 8 `AIN11` | `/RUN` | touch pad | Analog/input, not driven |
| Touch SPD+ | pad 9 `AIN12` | `/SPD++` | touch pad | Analog/input, not driven |
| Touch MUSIC | pad 27 `AIN1` | `/MUSIC` | touch pad | Analog/input, not driven |
| I2C SCL / WCH TCK | pad 11 `SCL` | `/SWDCK`, through R14 to `/SCL` | J3 pin 4, R14, U7 SCL | Pull-up/high when idle; unavailable during active WCH-LinkE debug |
| I2C SDA / WCH TIO | pad 12 `SDA` | `/SWDIO`, through R15 to `/SDA` | J3 pin 2, R15, U7 SDA | Pull-up/high when idle; unavailable during active WCH-LinkE debug |
| IP5209 INT | pad 10 `PA9` | `/INT` | U7 INT | Input; level depends PMIC |
| USB D+ / D- | pads 15/16 | `/DP`, `/DN` | USB connector | Enumerates as CDC when enabled |
| WT2003 USB update D+ | WT2003 pad 5 | `/DP2` | J4 pin 2 | Only for WT2003 update cable |
| WT2003 USB update D- | WT2003 pad 4 | `/DM2` | J4 pin 4 | Only for WT2003 update cable |

## Connector Pinouts

J3 WCH-LinkE/debug:

| Pin | Function |
|---:|---|
| 1 | 3.3V target reference |
| 2 | TIO / SWDIO / PB14 / SDA-side MCU net |
| 3 | NC unless reset is later wired |
| 4 | TCK / SWDCK / PB15 / SCL-side MCU net |
| 5 | GND |
| 6 | NC |

J4 WT2003 USB update:

| Pin | Function |
|---:|---|
| 1 | +5V |
| 2 | D+ |
| 3 | NC |
| 4 | D- |
| 5 | GND |
| 6 | NC |

J4 is not ARM SWD. Use only a custom USB adapter/cable.

## WT2003 Audio Validation

Run this only after USB CDC, safe pins, RGB idle, BLE advertising, Hall/touch raw checks, and I2C non-hanging behavior are understood.

1. Copy one file named `0001.mp3` to WT2003 external flash via J4.
2. Disconnect the WT2003 USB cable.
3. Power-cycle the board.
4. Wait at least 1 s after WT2003 power-up.
5. Run `audio version`.
6. Run `audio qperiph`.
7. Run `audio qcount-ext`.
8. Run `audio volume 5`.
9. Run `audio play-index 1`.
10. Check BUSY goes high while playing.
11. Run `audio stop`.
12. Check BUSY returns low.

Do not send serial commands while WT2003 is connected to a PC as USB storage. Do not assume file index order equals filename order.

## Stop Conditions

Stop testing immediately if:

- 3.3 V is out of tolerance.
- Any DRV8837 input is high before an explicit motor command.
- Any motor input remains active after its command timeout.
- The board heats unexpectedly.
- USB or BLE testing causes resets.
- I2C scan hangs longer than the command timeout expectations.
