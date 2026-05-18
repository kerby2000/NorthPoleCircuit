# wlink Attach Result

Status: PASS IN DOWNLOAD MODE

Observed result while not in Download mode:

```text
Connected to WCH-Link v2.18(v38) (WCH-LinkE-CH32V305)
Probe is not attached to an MCU, or debug is not enabled.
```

Observed result after manually placing CH592 into Download mode:

```text
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe --chip CH59X --speed low status
Connected to WCH-Link v2.18(v38) (WCH-LinkE-CH32V305)
Attached chip: CH59X [CH592] (ChipID: 0x92000000)
RISC-V ISA(misa): RV32ACIMUX
RISC-V arch(marchid): WCH-V4C
allhalted: true
```

Confirmed flash command:

```powershell
C:\Users\lukin\.platformio\packages\tool-wlink\wlink.exe flash --chip CH59X --speed low --erase --address 0x00000000 "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex"
```

Confirmed flash result:

```text
Read Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex as IntelHex format
Flashing 2604 bytes to 0x00000000
Flash done
Now reset...
```

Latest direct `wlink` low-speed result:

```text
wlink without Download mode can detect WCH-LinkE but cannot attach CH592.
```

Upstream `wlink` 0.1.2 test:

```text
win-x64:
  Probe is visible through nusb.
  Fails with: USB error: incompatible driver is installed for this interface.

win-x86:
  Probe is visible through WCHLinkDLL/CH375Driver.
  WCH-Link v2.18(v38) is detected.
  Without Download mode, AttachChip returns protocol error 0x55 [0x81, 0x55, 0x01, 0x01].
  In Download mode, status and flash work like wlink 0.1.1.
```

Result:

```text
wlink 0.1.2 x86 works in Download mode.
wlink 0.1.2 x64 currently fails on the installed driver binding.
```

Notes:

```text
The WCH-LinkE probe is visible.
Target attach through PlatformIO wlink requires CH592 Download mode.
MounRiver GUI download still works, so this is not proof of a bad HEX.
WCH-LinkUtility V2.90 can erase, program, verify, reset, and read back real program bytes through the same WCH-LinkE.
```

WCH-LinkUtility confirmation:

```text
Core: RISC-V
Series: CH590/1/2
Address: 0x00000000
Target: Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
Clock: Low
Operation: Erase All + Program + Verify + Reset and Run
Result: PASS
```

Readback after WCH-LinkUtility programming:

```text
0x00000000: 6F 00 C0 78 ...
```

This means the previous PlatformIO `wlink` failure was caused by target state. The CH592X-EVT-R1-LinkE board must be manually placed in Download mode before `wlink` attach/flash.

Next debug steps are tracked in:

```text
Firmware/docs/wlink_attach_debug.md
```
