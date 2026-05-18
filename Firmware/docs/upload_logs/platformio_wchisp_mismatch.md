# PlatformIO wchisp Mismatch

Date: 2026-05-16

Board:

```text
CH592X-EVT-R1-LinkE
```

Input:

```text
Firmware/mounriver_ch592_led_probe/obj/mounriver_ch592_led_probe.hex
```

Command:

```powershell
pio run -d Firmware\platformio_ch592_probe -e ch592x_isp_mounriver_hex -t upload
```

Tool versions observed during the investigation:

```text
PlatformIO package: tool-wchisp @ 0.23.240914
Standalone latest test: wchisp 0.3.0 win-x64
```

Important observed log lines:

```text
Read .../mounriver_ch592_led_probe.hex as IntelHex format
Firmware size: 3072
Erased 8 code flash sectors
Code flash 3072 bytes written
Verifying...
Error: Verify failed, mismatch
```

Config command result:

```text
wchisp config info
USER_CFG: 0x4FFF0FD5
CFG_DEBUG_EN = Enable

wchisp config set 4FFF0FC5
thread 'main' panicked at src/main.rs:277:21:
not implemented
```

So `wchisp config set` is exposed in the CLI, but is not implemented for this CH592/CH59x path in
the tested build. It cannot currently be used to toggle `CFG_DEBUG_EN`.

Important later finding:

```text
wchisp config unprotect
```

is different from `config set` and did work as the recovery path after the chip entered a protected
state. The target had to be put in USB Download mode and the CH592 bootloader device `4348:55E0`
had to be bound to WinUSB with Zadig. After `config unprotect`, WCH-LinkUtility and MounRiver could
identify the chip again.

Driver conflict after installing official WCHISPStudio V3.9:

```text
wchisp 0.3.0 info
Opening USB device #0
Failed to open USB device: Bus 004 Device 001: ID 4348:55e0
It's likely no WinUSB/LibUSB drivers installed.
Error: Failed to open USB device on Windows
```

This is expected when `4348:55E0` is bound to the official WCH driver used by WCHISPStudio instead
of WinUSB/libusb.

Conclusion:

```text
PlatformIO bundled wchisp is not a valid CH592 upload path on this board/tool/driver combination,
but `wchisp config unprotect` is a useful recovery command for accidental protection.
No-verify experiments were removed from the cleaned PlatformIO project because a no-verify write did not produce a running application.

Do not switch the working WCH driver binding just to make wchisp open the device unless deliberately
testing native USB ISP or running the protected-state recovery. WCHISPStudio V3.9 now proves the
official GUI USB ISP path works.
```
