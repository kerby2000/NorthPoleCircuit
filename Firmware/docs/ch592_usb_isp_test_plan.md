# CH592 USB ISP Test Plan

Purpose:

Test the CH592 native USB Download/ISP bootloader separately from WCH-LinkE. Use the same known-good MounRiver LED probe image for every tool so the test isolates the uploader.

Known-good inputs:

```text
Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.bin
```

Generate the BIN from the MounRiver ELF if needed:

```powershell
C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin\riscv-none-embed-objcopy.exe -O binary Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.elf Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.bin
```

Do not commit generated `.hex`, `.elf`, or `.bin` files.

## Required Tests

Put the board in CH592 Download mode for each test.

| Tool | HEX test | BIN test at `0x00000000` | Expected result |
|---|---|---|---|
| Official WCHISPStudio / WCH official downloader | `mounriver_ch592_led_probe.hex` | `mounriver_ch592_led_probe.bin` | PASS with WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10 |
| Official WCHISPTool_CMD for Windows | `mounriver_ch592_led_probe.hex` | `mounriver_ch592_led_probe.bin` | Not found in tested WCHISPTool_CMD Windows package |
| PlatformIO bundled `wchisp` | `mounriver_ch592_led_probe.hex` | `mounriver_ch592_led_probe.bin` | Fails verify when WinUSB/libusb driver is active; cannot open device when WCH driver is active |
| Standalone `wchisp 0.3.0` | `mounriver_ch592_led_probe.hex` | `mounriver_ch592_led_probe.bin` | Cannot open device when WCH driver is active |

## Rules

`wchisp -V` / no-verify is not acceptable for real flashing because an earlier no-verify
write did not produce a running application.

## CRITICAL: `wchisp config unprotect` Is The Recovery Tool

Even though `wchisp` is not a proven flashing path on this Windows setup, it is currently the known
working recovery tool for an accidentally protected CH592.

If WCH-LinkUtility/MounRiver/wlink stop attaching and the config shows debug/read protection
disabled, use:

```powershell
# Put CH592 into USB Download mode first.
# Use Zadig to bind only USB Module / 4348:55E0 to WinUSB.
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

Do not use `wchisp config set <hex>` for this; it panicked as not implemented in the tested build.
After unprotecting, WCH-LinkUtility and MounRiver attach worked again.

If official WCHISPTool succeeds:

```text
Treat PlatformIO bundled wchisp as incompatible with this CH592 bootloader/version until proven otherwise.
```

If official WCHISPTool fails:

```text
Debug USB boot mode, PB22 Download button timing, reset sequence, cable/port, and 4348:55E0 driver binding.
```

## Current Windows Driver Conclusion

The CH592 native USB Download device is `4348:55E0`.

On Windows, two driver stacks compete for this same interface:

```text
Official WCHISPStudio GUI: WCH CH375/WCHLink driver stack
ch32-rs/wchisp: WinUSB/libusb/nusb driver binding
```

Only one binding can be active at a time. After installing and using WCHISPStudio V3.9, both the
PlatformIO-packaged `wchisp` and standalone `wchisp 0.3.0` report that WinUSB/libusb is missing
and fail to open `4348:55E0`. This is expected while the official WCH driver is installed.

Do not switch the `4348:55E0` driver with Zadig unless deliberately testing `wchisp`; doing so can
break the known-good WCHISPStudio GUI path.

Exception: switching `4348:55E0` to WinUSB is appropriate for the explicit recovery operation
`wchisp config unprotect`. Do not change the WCH-LinkE probe interfaces `1A86:8010`.

The currently safe scripted path is WCH-LinkE through `wlink`, not native USB ISP.

## Possible Future Command-Line Work

If a native USB ISP command-line path is required on Windows, prefer building a small wrapper around
WCH's own ISP DLL/API instead of modifying `wchisp` first. Reason:

```text
WCHISPTool_CMD Linux/macOS package uses WCH55x ISP library APIs.
WCHISPStudio V3.9 already proves the WCH driver stack can program and verify CH592.
wchisp is designed for libusb/WinUSB and conflicts with the WCH driver binding on Windows.
```

The tested WCHISPTool_CMD archive did not include a Windows executable, even though its PDF claims
Windows support. The `Windows/README.md` only points back to the WCHISPTool installer.

## Observations To Record

For each attempt, record:

- Tool name and version.
- Whether board was visible as `4348:55E0`.
- Driver shown in Device Manager/Zadig.
- Exact file used: HEX or BIN.
- Program address for BIN.
- Whether erase/program/verify passed.
- Whether the LED/LCD probe runs after reset.
- Whether the board leaves Download mode normally.
