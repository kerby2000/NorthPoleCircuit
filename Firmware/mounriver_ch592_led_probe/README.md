# MounRiver CH592 LED Probe

This is a MounRiver/WCH-SDK-native LED probe for the CH592X-EVT-R1-LinkE development board.

It is copied from the WCH `LCD` example project structure and replaces only the application file with a simple LED test. It is separate from the North Pole firmware and is only for proving that MounRiver-built firmware runs on the dev board.

## Source

The project links to the WCH SDK at:

```text
C:\WCH\CH592EVT\EVT\EXAM\SRC
```

Keep the SDK in that location.

## Known-Good MounRiver Settings

This probe was successfully built and downloaded from MounRiver with these project settings stored in `mounriver_ch592_led_probe.wvproj`:

| Setting | Value |
|---|---|
| Vendor/toolchain | WCH / RISC-V |
| Series / MCU | CH59X / CH592X |
| Link setting | WCH-Link |
| SDK linked folders | `C:\WCH\CH592EVT\EVT\EXAM\SRC\Ld`, `RVMSIS`, `Startup`, `StdPeriphDriver` |
| Build output | `obj/mounriver_ch592_led_probe.hex` |
| Flash MCU type | `CH59x` |
| Flash address | `0x00000000` |
| Flash clock speed | Low |
| Flash operations | erase, program, verify, reset |
| Debug OpenOCD config | `${WCH:OpenOCD:default}/bin/wch-riscv.cfg` |

Keep the SDK installed at `C:\WCH\CH592EVT\`. The MounRiver project uses absolute links into that SDK tree.

## Programming Path

On this CH592X-EVT-R1-LinkE board, the working MounRiver path appears to be:

```text
PC -> WCH-LinkE USB interface -> WCH-LinkE controller -> CH592 debug/download interface
```

The project is configured as a WCH-Link project, not as a direct CH592 USB bootloader project.

The board still needs to be placed in Download mode because this board/firmware state does not yet support normal always-available debug attach from reset. Download mode gives the WCH tool a reliable short window to take control, erase/program/verify, then reset and run the flashed application.

This is different from PlatformIO `isp`, which uses the CH592 native USB bootloader device directly:

| Path | Physical interface | Needs Download mode | Uses WCH-LinkE |
|---|---|---:|---:|
| MounRiver working path | WCH-LinkE to target debug/download wiring | Yes on this board/setup | Yes |
| PlatformIO `isp` | PC USB to CH592 native USB bootloader | Yes | No |
| PlatformIO `wch-link` / `wlink` | WCH-LinkE to TCK/TIO debug wires | Normally no, if debug attach works | Yes |

## LED / LCD Mapping

From the CH592X-EVT-R1-LinkE schematic:

| LED | MCU pin | Notes |
|---|---|---|
| D4 / LED2 | Header net `LED2` | Not directly tied to PA4 unless jumpered |
| D3 / LED1 | Header net `LED1` | Not directly tied to PB23 unless jumpered |
| D1 power | 3.3 V rail | Always on, not firmware-controlled |

The current probe toggles PA4 and PB23 alternately every 250 ms. PA4 and PB23 are also LCD segment pins on this dev board, so without jumpers the visible result can be blinking LCD segments instead of LEDs. That still proves the downloaded program is running.

To blink the board LEDs, add jumper wires from the MCU header pins to the LED header pins:

| Jumper | Expected result |
|---|---|
| PA4 to `LED2` | D4 / LED2 blinks |
| PB23 to `LED1` | D3 / LED1 blinks, but this pin is also the Download/reset-side pin and may affect boot behavior |

## MounRiver Use

1. Open MounRiver Studio.
2. If this probe was already imported, remove it from the MounRiver workspace first. Do not delete project contents from disk.
3. Open `Firmware\mounriver_ch592_led_probe\mounriver_ch592_led_probe.wvproj`, or import the folder as an existing project.
4. Confirm the linked folders `Ld`, `RVMSIS`, `Startup`, and `StdPeriphDriver` are not marked invalid.
5. Build the `obj` configuration.
6. Flash the generated HEX using the MounRiver download path that already works on your machine.
7. After reset, D4 and D3 should alternate. If only D4 blinks, PB23 is being held or affected by Download wiring.

Expected generated file:

```text
Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex
```

If MounRiver still reports `CH59x_common.h` missing, check that this SDK file exists:

```text
C:\WCH\CH592EVT\EVT\EXAM\SRC\StdPeriphDriver\inc\CH59x_common.h
```

This project does not test BLE, USB CDC, audio, or the North Pole board.

Upload path status and command-line experiments are tracked in:

```text
Firmware/docs/ch592_upload_matrix.md
Firmware/docs/ch592_usb_isp_test_plan.md
Firmware/docs/wlink_attach_debug.md
```
