# MounRiver GUI Download PASS

Date: 2026-05-16

Board:

```text
CH592X-EVT-R1-LinkE
```

Tool path:

```text
MounRiver Studio GUI download
WCH-LinkE
```

Second confirmed GUI tool path:

```text
WCH-LinkUtility V2.90
WCH-LinkE
```

Input:

```text
Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
```

Observed result:

```text
Download succeeds.
Verify succeeds in MounRiver.
Application runs after reset.
PA4/PB23 activity is visible on LCD segments and, with jumpers added, on the LED header nets.
```

Conclusion:

```text
MounRiver-generated HEX is valid.
MounRiver GUI + WCH-LinkE is the golden upload path.
WCH-LinkUtility V2.90 + WCH-LinkE is also a confirmed working GUI flash path.
```

## WCH-LinkUtility V2.90 Confirmation

Settings used:

| Setting | Value |
|---|---|
| Core | RISC-V |
| Series | CH590/1/2 |
| Address | `0x00000000` |
| Target file | `Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex` |
| Operations | Erase All, Program, Verify, Reset and Run |
| CLK speed | Low |
| Connected WCH-Link | `RISC-V Link [#1]` |
| Active WCH-Link mode | `WCH-LinkRV` |

Observed utility log:

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

Flash readback observations:

```text
Before reflashing, a read showed suspicious repeated A9 BD F9 F3 data near the end of the selected read window.
After programming and verifying the known-good HEX, readback at 0x00000000 began with 6F 00 C0 78.
```

The first bytes `6F 00 C0 78` match the first data bytes in the known-good MounRiver HEX image, so WCH-LinkUtility readback confirms the programmed image is present at flash address `0x00000000`.
