# CH592X PlatformIO Probe

This is a small PlatformIO probe project for the CH592X-EVT-R1-LinkE development board. It is only for isolating upload paths from VS Code. It is not the source of truth for the North Pole firmware.

The active North Pole firmware remains the WCH EVT/MounRiver project in:

```text
Firmware/northpole_ch592_bringup/
```

The known-good LED probe is:

```text
Firmware/mounriver_ch592_led_probe/
```

## Status

| Path | Status | Notes |
|---|---|---|
| MounRiver GUI download through WCH-LinkE | PASS | Golden reference. Same HEX runs on the board. |
| WCHISPStudio native USB ISP GUI | PASS | WCHISPStudio V3.9 / WCHISPTool_CH57x-59x V3.10 works with the official WCH driver. |
| PlatformIO `ch592x_wlink_download_mode` | PASS | Requires manually putting CH592 into Download mode first. Uses `wlink --chip CH59X --speed low --erase`. |
| PlatformIO `ch592x_wlink_mounriver_hex_download_mode` | PASS | Same `wlink` path, but flashes the known-good MounRiver HEX. |
| PlatformIO `ch592x_isp_mounriver_hex` | EXPERIMENTAL | Keeps one USB ISP target for future bootloader experiments. Current result was verify mismatch or WinUSB/WCH-driver conflict. |

Do not treat any no-verify `wchisp` result as a successful flash on this board.

## Build Only

The default environment is intentionally build-only:

```powershell
pio run -d Firmware\platformio_ch592_probe
```

If you accidentally run upload against `ch592x_build_only`, it fails with an explanatory message.

## WCH-LinkE Download-Mode Upload

This is the first confirmed working PlatformIO-controlled WCH-LinkE path.

Required sequence:

1. Power the CH592X-EVT-R1-LinkE board.
2. Put the CH592 into Download mode.
3. Run one of the commands below before Download mode times out.

Build and upload the PlatformIO probe firmware:

```powershell
pio run -d Firmware\platformio_ch592_probe -e ch592x_wlink_download_mode -t upload
```

Upload the known-good MounRiver HEX through the same `wlink` path:

```powershell
pio run -d Firmware\platformio_ch592_probe -e ch592x_wlink_mounriver_hex_download_mode -t upload
```

Before using the MounRiver HEX upload target, build the MounRiver probe so this generated file exists:

```text
Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex
```

The custom upload command shape is:

```powershell
wlink flash --chip CH59X --speed low --erase <firmware.hex>
```

Known direct command that passed:

```powershell
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe flash --chip CH59X --speed low --erase --address 0x00000000 "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex"
```

`wlink` warns that `--address` is ignored for Intel HEX and ELF inputs, so the PlatformIO wrapper omits it and relies on the HEX load addresses.

PlatformIO's packaged `tool-wlink @ 0.23.241116+sha.f44158e` contains a 32-bit `wlink.exe`:

```text
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe
PE machine: 0x014C, x86 / PE32
wlink --version: 0.1.1
```

The downloaded upstream `wlink 0.1.2` x86 build works with the same Download-mode sequence. The x64 build can list the probe through `nusb`, but currently fails on status/flash with:

```text
USB error: incompatible driver is installed for this interface
```

Do not replace PlatformIO's managed `tool-wlink` package by hand. `wlink 0.1.2` was tested and is not retained in this repository because PlatformIO's packaged `wlink 0.1.1` already works in Download mode.

## Experimental USB ISP

This environment uses the CH592 native USB Download/ISP bootloader and PlatformIO's bundled `wchisp`. It does not use WCH-LinkE.

```powershell
pio run -d Firmware\platformio_ch592_probe -e ch592x_isp_mounriver_hex -t upload
```

Known result on this board:

| Test | Result |
|---|---|
| `ch592x_isp_mounriver_hex` with WinUSB/libusb driver | FAIL, verify mismatch |
| `ch592x_isp_mounriver_hex` with official WCH driver | FAIL, `wchisp` cannot open `4348:55E0` |
| Standalone `wchisp 0.3.0` with official WCH driver | FAIL, `wchisp` cannot open `4348:55E0` |
| WCHISPStudio V3.9 native USB ISP GUI | PASS |

This target is retained only for future USB bootloader experiments. The current recommended CLI path is `ch592x_wlink_download_mode`.

Driver note:

```text
CH592 USB Download VID:PID: 4348:55E0
Official WCHISPStudio expects the WCH CH375/WCHLink driver stack.
ch32-rs/wchisp expects WinUSB/libusb/nusb.
Windows can bind only one driver to that USB interface at a time.
```

Do not switch the working WCH driver binding with Zadig unless deliberately testing `wchisp`.
