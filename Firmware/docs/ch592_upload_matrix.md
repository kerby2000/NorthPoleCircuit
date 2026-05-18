# CH592 Upload Matrix

Board under test:

```text
CH592X-EVT-R1-LinkE
```

Known-good programmer paths:

```text
MounRiver Studio GUI download through WCH-LinkE
WCH-LinkUtility V2.90 GUI through WCH-LinkE
WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10 GUI through CH592 native USB Download mode
PlatformIO custom wlink upload through WCH-LinkE after manual Download-mode entry
```

## CRITICAL: IF WCH-LINK STOPS WORKING, CHECK PROTECTION FIRST

If WCH-LinkUtility, MounRiver, or `wlink` report that the chip type/status is wrong or debug is
not enabled, the CH592 may have been put into a protected state.

The recovery that worked was:

```powershell
# Put CH592 into USB Download mode first.
# Bind only the CH592 bootloader device 4348:55E0 to WinUSB with Zadig.
# Do not change the WCH-LinkE 1A86:8010 driver.
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

Then power-cycle and retry WCH-Link/MounRiver. See `docs/ch592_unprotect_recovery.md`.

Known-good project:

```text
Firmware/mounriver_ch592_led_probe/
```

Known-good generated output:

```text
Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
```

Do not commit generated `obj/`, `.elf`, `.hex`, `.bin`, or `.map` files unless they are explicitly placed in a documented `reference_artifacts/` folder for a specific reproducibility reason.

## Golden MounRiver Settings

From `Firmware/mounriver_ch592_led_probe/mounriver_ch592_led_probe.wvproj`:

| Setting | Value |
|---|---|
| MCU | CH592X |
| Series | CH59X |
| Link | WCH-Link |
| Flash MCU type | CH59x |
| Flash address | `0x00000000` |
| Output HEX | `obj/mounriver_ch592_led_probe.hex` |
| Flash operations | erase, program, verify, reset |
| Flash clock speed | Low |
| OpenOCD config | `${WCH:OpenOCD:default}/bin/wch-riscv.cfg` |
| SDK linked folders | `C:\WCH\CH592EVT\EVT\EXAM\SRC\Ld`, `RVMSIS`, `Startup`, `StdPeriphDriver` |

## Path Status

| Path | Physical route | Tool | Status | Production-safe |
|---|---|---|---|---|
| MounRiver GUI WCH-LinkE | PC USB -> WCH-LinkE -> CH592 debug/download path | MounRiver | PASS | Yes, current primary |
| WCH-LinkUtility GUI WCH-LinkE | PC USB -> WCH-LinkE -> CH592 debug/download path | WCH-LinkUtility V2.90 | PASS | Yes, current fallback |
| PlatformIO custom `wlink` Download-mode upload | PC USB -> WCH-LinkE -> CH592 debug/download path | `wlink flash --chip CH59X --speed low --erase` | PASS | Yes, for dev-board flashing after manual Download-mode entry |
| Official WCH USB ISP GUI | PC USB -> CH592 native USB bootloader | WCHISPStudio V3.9, internal WCHISPTool_CH57x-59x V3.10 | PASS | Yes, GUI only |
| Official WCH USB ISP CMD | PC USB -> CH592 native USB bootloader | WCHISPTool_CMD | NOT AVAILABLE on Windows package tested | No |
| PlatformIO `ch592x_isp_mounriver_hex` | PC USB -> CH592 native USB bootloader | `wchisp` from PlatformIO package | FAIL / driver-conflicted for flashing, but `config unprotect` recovered debug access | No for flashing; yes for recovery |
| Standalone `wchisp` 0.3.0 | PC USB -> CH592 native USB bootloader | `wchisp.exe` from ch32-rs release | DRIVER CONFLICT with WCH driver | No for normal flashing |

## Current Recommendation

Daily development/debug:

```text
MounRiver Studio, WCH-LinkUtility, or PlatformIO custom wlink Download-mode upload
```

Emergency or field flashing:

```text
Official WCHISPStudio GUI native USB ISP, or WCH-LinkE wlink if the debug connector is available
```

PlatformIO:

```text
The custom wlink Download-mode upload path is usable on the dev board.
Keep USB ISP through wchisp experimental.
```

## Native USB ISP Driver Split On Windows

The CH592 native USB Download bootloader appears as:

```text
VID:PID 4348:55E0
```

Two incompatible Windows driver paths were observed:

| Tool family | Driver expectation | Result |
|---|---|---|
| Official WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10 | WCH CH375/WCHLink driver stack, observed as WCH-supplied driver names such as `CH375_A64` / `WCHLink_A64` depending on interface | PASS |
| `ch32-rs/wchisp` from PlatformIO and standalone `wchisp 0.3.0` | WinUSB/libusb/nusb binding for `4348:55E0` | Cannot open device while WCH driver is installed |

Windows binds one driver to a USB interface at a time. Therefore WCHISPStudio and `wchisp`
cannot both use the CH592 native USB bootloader at the same time unless the driver binding is
swapped with Zadig or Device Manager. Do not keep switching drivers during normal work; it makes
the proven WCHISPStudio path fragile.

Observed after installing and using WCHISPStudio V3.9:

```text
wchisp 0.3.0 info
Opening USB device #0
Failed to open USB device: Bus 004 Device 001: ID 4348:55e0
It's likely no WinUSB/LibUSB drivers installed.
```

This is expected when the device is bound to the official WCH driver.

The WCHISPTool_CMD package downloaded from WCH contains Linux/macOS binaries and source plus a
Windows README, but no Windows command-line executable was found in the tested archive. The
Windows README redirects to the WCHISPTool installer, which installed GUI tools only.

## Protected-State Recovery

WCHISPStudio V3.9 can flash a protected CH592 through native USB Download mode, but the tested GUI
did not expose a clear way to unprotect the device. When `CFG_DEBUG_EN`/read access were disabled,
WCH-LinkUtility and MounRiver could no longer attach even though WCHISPStudio could still flash.

The working recovery path was:

```powershell
# 1. Put CH592 in USB Download mode.
# 2. Use Zadig on USB Module 4348:55E0 only and install WinUSB.
# 3. Run:
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

Do not use `wchisp config set <hex>` for this recovery; that command panicked as not implemented
in the tested `wchisp` build. `config unprotect` is the command that worked.

After unprotecting, WCH-LinkUtility and MounRiver identified the target again.

If the bootloader no longer remains visible long enough for WCHISPStudio or `wchisp`, use the
brick-recovery escalation notes in `docs/ch592_brick_recovery.md`.

2026-05-17 recovery note:

```text
revive_ch59x.py caught the flickering 4348:55E0 bootloader and rewrote USER_CFG 0x4FFF0FD5.
WCH-Link status recovered afterward.
CLI wlink flash still failed during fast programming in that same session.
```

Use that result to separate two states:

| State | Meaning |
|---|---|
| `wlink status` works | Debug/config is no longer fully locked out |
| `wlink flash` succeeds | Actual command-line programming path is proven for that session |
| `wlink status` works but `wlink flash` fails | Use GUI MounRiver/WCH-LinkUtility or repeat exact Download-mode entry; do not change protection bits again |

## Confirmed PlatformIO `wlink` Flow

The earlier `wlink` failures occurred because the CH592 was not in Download mode. Once the board was manually placed in Download mode, standalone `wlink` attached and reported:

```text
Connected to WCH-Link v2.18(v38) (WCH-LinkE-CH32V305)
Attached chip: CH59X [CH592] (ChipID: 0x92000000)
RISC-V ISA(misa): RV32ACIMUX
RISC-V arch(marchid): WCH-V4C
```

The known-good MounRiver HEX was then flashed successfully with:

```powershell
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe flash --chip CH59X --speed low --erase --address 0x00000000 "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex"
```

Observed result:

```text
Read Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex as IntelHex format
Flashing 2604 bytes to 0x00000000
Flash done
Now reset...
```

`wlink` warns that `--address` is ignored for Intel HEX and ELF inputs, so the PlatformIO wrapper omits `--address` and uses the image's load addresses.

The PlatformIO probe project now has two explicit Download-mode environments:

```powershell
pio run -d Firmware\platformio_ch592_probe -e ch592x_wlink_download_mode -t upload
pio run -d Firmware\platformio_ch592_probe -e ch592x_wlink_mounriver_hex_download_mode -t upload
```

In both cases, put the CH592 into Download mode before starting upload.

`wlink 0.1.2` x86 was tested and behaved the same as PlatformIO's bundled `wlink 0.1.1` in Download mode. It is not retained in this repository because the packaged tool already works.

## WCH-LinkUtility GUI Result

WCH-LinkUtility V2.90 was also able to program and verify the known-good MounRiver HEX through WCH-LinkE.

Observed working settings:

| Setting | Value |
|---|---|
| Tool | WCH-LinkUtility V2.90 |
| Core | RISC-V |
| Series | CH590/1/2 |
| Address | `0x00000000` |
| Target file | `Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex` |
| Operations | Erase All, Program, Verify, Reset and Run |
| CLK speed | Low |
| Connected WCH-Link | `RISC-V Link [#1]` |
| Active WCH-Link mode | `WCH-LinkRV` |

Observed operation log:

```text
Begin to set chip type...
Succeed
Begin to Erase...
Succeed
Begin to Program and Verify...
Succeed
Begin to Reset...
Succeed
Operation is Successful
```

Readback note:

```text
Before reflashing, one read near the end of the selected window showed repeated A9 BD F9 F3 data.
After reflashing, readback at 0x00000000 showed real program data beginning with 6F 00 C0 78.
That matches the first bytes of the known-good MounRiver HEX image.
```

This proves that the WCH-LinkE hardware path, target power, chip selection, and HEX image are valid. The previous PlatformIO attach failures were not evidence of a bad board connection or invalid HEX.

## BLE RF Baseline

The WCH BLE `Broadcaster` example was flashed and confirmed visible in nRF Connect as:

```text
abc
```

Example under test:

```text
C:\WCH\CH592EVT\EVT\EXAM\BLE\Broadcaster\obj\Broadcaster.hex
```

This proves the dev board can boot WCH BLE firmware and transmit BLE advertisements. Use this as
the RF baseline before debugging North Pole firmware advertising.

## Important Distinction

MounRiver WCH-LinkE programming and PlatformIO `isp` are different physical paths:

| Name | Uses WCH-LinkE | Needs CH592 Download mode |
|---|---:|---:|
| MounRiver GUI download | Yes | Yes on this board/setup |
| PlatformIO `isp` / `wchisp` | No | Yes |
| PlatformIO custom `wlink` | Yes | Yes on this board/setup |

The PlatformIO `isp` failure does not invalidate the MounRiver HEX. The same HEX runs when flashed through the MounRiver WCH-LinkE path.
