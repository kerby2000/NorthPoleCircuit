# WCH-LinkE / wlink Attach Debug

Purpose:

Track WCH-LinkE debug attach separately from USB ISP flashing. The current MounRiver GUI download path works. PlatformIO `wlink` also works when the CH592 is manually placed in Download mode first.

## CRITICAL: PROTECTED CH592 LOOKS LIKE A BROKEN WCH-LINK

If the chip was accidentally protected, WCH-LinkE attach fails even though wiring, target power,
and the WCH-LinkE driver are fine.

Observed protected-state symptoms:

```text
WCH-LinkUtility:
Failed, the chip type is not matched or status of chip is wrong!

wlink:
Probe is not attached to an MCU, or debug is not enabled.
```

Observed bad config:

```text
CFG_DEBUG_EN = Disable
CFG_ROM_READ = Disable
```

Recovery that worked:

```powershell
# Put CH592 into USB Download mode.
# Bind only 4348:55E0 / USB Module to WinUSB with Zadig.
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

After this, WCH-LinkUtility and MounRiver could identify the chip again. See
`docs/ch592_unprotect_recovery.md`.

If no WCH-Link attach is possible and the USB bootloader only flickers briefly, use
`docs/ch592_brick_recovery.md`.

## Current Result

WCH-LinkE programming through WCH-LinkUtility V2.90 is confirmed working:

```text
Core: RISC-V
Series: CH590/1/2
Active WCH-Link Mode: WCH-LinkRV
Target: Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
Erase All + Program + Verify + Reset and Run: PASS
```

That result proves the WCH-LinkE hardware path is good enough for WCH's GUI programming flow.

PlatformIO bundled `wlink` sees the WCH-LinkE probe, but target attach fails while the CH592 is running normally:

```text
Probe is not attached to an MCU, or debug is not enabled.
```

Earlier PlatformIO OpenOCD `wch-link` result:

```text
WCH-Link failed to connect with riscvchip
Make sure the two-line debug interface has been opened.
Please check your physical link connection.
```

Summary of the upload investigation:

```text
PlatformIO wlink while not in Download mode: WCH-LinkE is detected, then AttachChip fails.
PlatformIO wlink in Download mode: WCH-LinkE is detected, CH592 attaches, flash succeeds.
PlatformIO wch-link/OpenOCD while not in Download mode: WCH-LinkE is detected, then riscvchip connect fails.
USB ISP with PlatformIO wchisp: verify mismatch with the known-good MounRiver HEX.
USB ISP with WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10: PASS through official WCH driver.
```

Architecture note:

```text
PlatformIO tool-wlink package version: 0.23.241116+sha.f44158e
Bundled executable version: wlink 0.1.1
Bundled executable PE machine: 0x014C, x86 / PE32
```

The downloaded upstream `wlink 0.1.2` x86 build also works in Download mode. The downloaded `wlink 0.1.2` x64 build can list the probe through `nusb`, but status/flash fail with:

```text
USB error: incompatible driver is installed for this interface
```

Confirmed working `wlink` condition:

```text
Put CH592 into Download mode first.
Run wlink with explicit chip CH59X and low speed.
```

Important nuance from the 2026-05-17 recovery session:

```text
WCH-Link status/attach can succeed while wlink flash still fails during fast programming.
```

Observed after restoring USER_CFG `0x4FFF0FD5` with the USB recovery script:

```text
wlink status: PASS, attached CH59X [CH592]
wlink unprotect: PASS, read protected false
wlink flash small LED probe: FAIL, protocol 0x55 / fastprogram error
```

Therefore, do not assume `status` success proves the command-line flash path is healthy. If flash
fails but attach works, retry the exact manual Download-mode sequence or use MounRiver/WCH-LinkUtility
GUI before changing config bits again.

Working command:

```powershell
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe flash --chip CH59X --speed low --erase --address 0x00000000 "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex"
```

Observed attach result in Download mode:

```text
Attached chip: CH59X [CH592] (ChipID: 0x92000000)
RISC-V ISA(misa): RV32ACIMUX
RISC-V arch(marchid): WCH-V4C
```

## Known Device State

| Item | Current note |
|---|---|
| WCH-LinkE VID/PID | `1A86:8010` when visible as WCH-Link |
| CH592 USB Download VID/PID | `4348:55E0` |
| WCH-LinkE mode | RV/RISC-V mode |
| Target power | Powered from board USB/switch |
| Target reference voltage | WCH-LinkE 3.3 V reference not required for this dev-board setup once target is powered |
| Debug wires | TIO/SWDIO, TCK/SWDCK, GND connected |
| Download mode | Required for current MounRiver GUI download flow and confirmed PlatformIO `wlink` upload flow |
| CH592 config | `CFG_DEBUG_EN = Enable`, `CFG_BOOT_EN = Enable`, `CFG_ROM_READ = Read enable` |
| WCH-LinkUtility GUI programming | PASS with `CH590/1/2`, address `0x00000000`, low clock |
| WCHISPStudio GUI native USB ISP | PASS with WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10 |

## Checks To Run

1. Confirm WCH-LinkE is in RV/RISC-V mode.
2. Confirm target is powered before attach.
3. Confirm common GND.
4. Confirm TIO/SWDIO and TCK/SWDCK continuity.
5. If attach suddenly stopped after ISP/config experiments, run `wchisp config unprotect`.
6. Try attach with target in normal run mode.
7. Try attach immediately after reset.
8. Try attach while target is held in Download mode.
9. Confirm MounRiver GUI still downloads successfully after any change.
10. Save `wlink status` or upload logs under `Firmware/docs/upload_logs/`.

## Interpretation

Because WCH-LinkUtility can erase, program, verify, reset, and then read back real code bytes from `0x00000000`, basic WCH-LinkE wiring and the HEX image are known good. The `wlink` path is also good when entered from Download mode.

They are not currently explained by:

```text
bad HEX file
missing target power
missing common ground
wrong TIO/TCK wiring
WCH-LinkE not being in RV mode
```

The likely remaining difference is board state: Download mode enables the WCH-LinkE attach/programming path on this dev board. Normal running firmware is not attachable by `wlink`/OpenOCD with the current setup.

Do not change the working WCH-LinkUtility driver binding just to satisfy `wlink` x64 unless we deliberately decide to risk the proven GUI flashing path. The `wlink` x86 path already reaches the WCHLinkDLL backend and works when the CH592 is in Download mode.

## Native USB ISP Driver Note

Native USB ISP is separate from WCH-LinkE. It uses the CH592 bootloader device:

```text
VID:PID 4348:55E0
```

On Windows, official WCHISPStudio and `ch32-rs/wchisp` expect different drivers for that same
device:

```text
WCHISPStudio V3.9: WCH CH375/WCHLink driver stack
wchisp / nusb / libusb: WinUSB/libusb-compatible binding
```

Only one binding can be active at a time. After the official WCH driver is active, `wchisp` reports
that no WinUSB/libusb driver is installed. This is a driver binding conflict, not a target wiring
problem.

## NorthPole Board Note

On the NorthPole board, PB14/PB15 are shared with IP5209 I2C through R15/R14 0 ohm links.

If debug attach is unreliable during board bring-up:

```text
Leave R14/R15 unpopulated temporarily, or remove them, so PB14/PB15 are dedicated to WCH-LinkE debug.
```

Normal firmware can use PB14/PB15 as IP5209 SDA/SCL only after debug/programming reliability is understood.
