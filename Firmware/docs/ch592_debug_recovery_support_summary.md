# CH592 Debug Recovery Support Summary

Date: 2026-05-17

This note summarizes the CH592X-EVT-R1-LinkE upload/debug investigation for external review.

## Hardware Under Test

- Board: CH592X-EVT-R1-LinkE development board
- Target MCU: CH592 / CH59x family
- Programmer/debug probe: onboard/attached WCH-LinkE
- Native USB download pin: PB22 Download button
- Native USB bootloader VID:PID observed by `wchisp`: `4348:55E0`
- WCH-LinkE VID:PID observed by `wlink`: `1A86:8010`
- WCH-Link firmware observed by `wlink`: `v2.18(v38)`, `WCH-LinkE-CH32V305`

## Tool Versions Seen

- MounRiver Studio: installed/reinstalled during investigation
- WCH-LinkUtility: V2.90
- WCHISPStudio: V3.9
- WCHISPTool_CH57x-59x inside WCHISPStudio: V3.10
- PlatformIO CH32V platform: `Community-PIO-CH32V/platform-ch32v`
- PlatformIO `tool-wlink` package: `0.23.241116+sha.f44158e`
- Bundled `wlink.exe` version: `wlink 0.1.1`
- External `wlink 0.1.2`:
  - x86 build worked like 0.1.1 in earlier test
  - x64 build listed the probe but failed with an incompatible-driver USB error
- PlatformIO `tool-wchisp` package: `0.23.240914`
- Standalone `wchisp 0.3.0` also tested
- WCHISPTool_CMD package:
  - Linux/macOS binaries and source exist
  - Windows folder only contained a README pointing back to the WCHISPTool installer
  - no Windows `WCHISPTool_CMD.exe` was found in the tested package

## Known Working Results

### WCHISPStudio Native USB ISP

WCHISPStudio V3.9 can flash the CH592 through the native USB Download mode.

Known working settings:

- Chip series: `CH59x`
- Chip model: `CH592`
- Download port: `USB`
- Download cfg pin: `PB22`
- RST as manual reset input pin: checked in the GUI during successful flashes
- No-key serial port download: checked in the GUI during successful flashes
- Run target program after download: checked in the GUI during successful flashes
- Erase / program / verify: passed

Known working files flashed by WCHISPStudio:

- `Firmware/build/bringup/northpole_ch592_bringup.hex`
- `Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex`

WCHISPStudio reports success with erase/program/verify.

### Earlier Working WCH-Link / wlink Path

Before the current failure state, WCH-LinkUtility V2.90 could flash:

- Core: `RISC-V`
- Series: `CH590/1/2`
- Address: `0x00000000`
- Clock speed: `Low`
- Active WCH-Link mode: `WCH-LinkRV`
- Operations: erase all, program, verify, reset and run

Earlier standalone `wlink` command also worked when the target was placed in Download mode first:

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" --chip CH59X --speed low status
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" flash --chip CH59X --speed low --erase "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex"
```

Earlier successful `wlink status` identified:

```text
Attached chip: CH59X [CH592] (ChipID: 0x92000000)
RISC-V ISA: RV32ACIMUX
RISC-V arch: WCH-V4C
```

## Resolved Root Cause

The board had been put into a protected CH592 state. WCHISPStudio V3.9 could still flash through
native USB Download mode, but WCH-LinkUtility, MounRiver, and `wlink` could not attach because
debug/read access was disabled.

The working recovery was:

```powershell
# Put CH592 into USB Download mode.
# Use Zadig to bind USB Module / 4348:55E0 to WinUSB.
# Do not change WCH-Link / 1A86:8010 interfaces.
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

After `config unprotect`, WCH-LinkUtility and MounRiver identified the chip again.

## Previously Broken State

### WCH-LinkE Debug Attach

WCH-LinkUtility and `wlink` can see the WCH-LinkE probe, but cannot attach to the target MCU.

Current WCH-LinkUtility symptoms:

```text
Connected RISC-V mode WCH-Link Cnt:1
Succeeded to connect with WCH-Link!
Begin to set chip type...
Failed, the chip type is not matched or status of chip is wrong!
```

Current `wlink` symptom:

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" --chip CH59X --speed low status
```

```text
Connected to WCH-Link v2.18(v38) (WCH-LinkE-CH32V305)
Error: Probe is not attached to an MCU, or debug is not enabled. (hint: use wchisp to enable debug)
```

### CH592 Config Readback

At one point, `wchisp config info` reported:

```text
USER_CFG: 0x4FFF0F4D
CFG_RESET_EN 0x1
  `- Enable
CFG_DEBUG_EN 0x0
  `- Disable
CFG_BOOT_EN 0x1
  `- Enable
CFG_ROM_READ 0x0
  `- Disable the programmer to read out, and keep the program secret
```

The earlier working development config was:

```text
USER_CFG: 0x4FFF0FD5
CFG_RESET_EN 0x0
  `- Disable
CFG_DEBUG_EN 0x1
  `- Enable
CFG_BOOT_EN 0x1
  `- Enable
CFG_ROM_READ 0x1
  `- Read enable
```

This suggests debug/read access may have been disabled by an ISP configuration/protection setting.

### `wchisp config set` Cannot Repair It

Attempted:

```powershell
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config set 4FFF0FD5
```

Result:

```text
[INFO] setting cfg value 4FFF0FD5
thread 'main' panicked at src/main.rs:277:21:
not implemented
```

The same `not implemented` panic was seen earlier for other config values. Therefore the tested `ch32-rs/wchisp` build cannot currently restore CH592 config words.

## Driver State Observations

WCHISPStudio and `ch32-rs/wchisp` use different Windows driver bindings for the CH592 native USB bootloader interface:

- WCHISPStudio V3.9 works with WCH's official CH375/WCHLink-style driver stack.
- `ch32-rs/wchisp` expects WinUSB/libusb/nusb access.

After using WCHISPStudio, `wchisp` may fail with:

```text
Failed to open USB device: Bus 004 Device 001: ID 4348:55e0
It's likely no WinUSB/LibUSB drivers installed.
```

Attempts to install Zadig on the CH592 bootloader interface later failed with:

```text
Driver installation failed
```

Do not install Zadig on the WCH-LinkE probe interfaces unless intentionally abandoning official WCH-Link/MounRiver tools.

## North Pole Firmware Observations

North Pole bring-up firmware builds successfully:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

The build now runs the KiCad audit and produces:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

After flashing North Pole bring-up firmware with WCHISPStudio, Windows reported:

```text
Unknown USB Device (Device Descriptor Request Failed)
```

This is currently interpreted as a likely custom USB CDC enumeration problem in the application firmware, not as a flashing failure.

BLE scan did not show `NorthPole BLE`. The next intended baseline test is to flash the unmodified WCH BLE `Peripheral` example and scan for:

```text
Simple Peripheral
```

## Experiments Performed

1. Confirmed dev board requires manual USB Download mode using PB22 Download button during reset/power-cycle.
2. Confirmed WCHISPStudio V3.9 can flash CH592 in USB Download mode.
3. Confirmed MounRiver LED/LCD probe project can build and previously ran on the dev board.
4. Confirmed WCH-LinkUtility V2.90 previously flashed the MounRiver LED probe through WCH-LinkE.
5. Confirmed standalone `wlink 0.1.1` previously attached/flashed when the board was in Download mode.
6. Confirmed `wlink 0.1.2` x86 behaved similarly to 0.1.1 in earlier tests.
7. Confirmed `wlink 0.1.2` x64 had USB driver incompatibility on this Windows setup.
8. Confirmed PlatformIO/wchisp native USB ISP could identify the CH592 with WinUSB installed, but flashing had verify mismatch / no-run problems before the official WCHISPStudio path was proven.
9. Installed WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10; WCH official GUI ISP works.
10. Reinstalled MounRiver; WCH-Link target attach still fails.
11. Tried `wchisp config set`; it panics as not implemented for this target/tool path.
12. Tried Zadig later for the target USB device; driver installation failed.

## Current Questions For External Help

1. How can `CFG_DEBUG_EN` be restored on CH592 when open-source `wchisp config set` is not implemented?
2. Does WCHISPStudio V3.9 have a reliable GUI path to restore CH592 config to development defaults?
3. Which WCHISPStudio checkboxes map to `CFG_DEBUG_EN`, `CFG_ROM_READ`, and `CFG_RESET_EN` on CH592?
4. Did enabling `Code and data protection mode` in WCHISPStudio likely change `USER_CFG` from `0x4FFF0FD5` to `0x4FFF0F4D`?
5. Is there a Windows version of `WCHISPTool_CMD` that can write CH592 config words through the official WCH driver?
6. Can WCH-LinkUtility recover a CH592 once `CFG_DEBUG_EN` is disabled, or must recovery be done through native USB ISP?
7. What is the safest sequence to return the board to:

```text
CFG_DEBUG_EN = Enable
CFG_ROM_READ = Read enable
WCH-LinkE attach works
WCHISPStudio still flashes
```

## Avoid Doing For Now

- Do not click `Disable Two-Line Interface` in WCH-LinkUtility.
- Do not install Zadig on WCH-LinkE interfaces.
- Do not enable code/data protection during bring-up.
- Do not rely on `wchisp config set` unless a fixed version is found.
- Do not treat North Pole USB CDC as validated; it likely needs separate debugging.
