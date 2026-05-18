# CH592 Possible Brick Recovery

This is for the worst case where:

- WCHISPStudio no longer sees the CH592 USB Download bootloader.
- MounRiver / WCH-LinkUtility / `wlink` cannot erase or program.
- The chip may have a bad/protected config word.

References:

- `ch32-rs/wlink` issue #69: confusion between WCH-Link, ISP, and debug modes.
- biemster `revive_ch59x.py` gist: raw USB recovery for CH59x devices that briefly enumerate
  the bootloader after reset.

The Linux command mentioned in that discussion:

```bash
udevadm monitor -k -s usb
```

is Linux-specific. On Windows, use the PowerShell watcher in this repo instead:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\ch592_recovery\watch_usb_bootloader.ps1 -Seconds 120 -IntervalMs 100
```

It watches for:

```text
VID_4348&PID_55E0  CH592 USB Download bootloader
VID_1A86&PID_8010  WCH-LinkE
```

## First: Do Not Keep Randomly Reprogramming

Stop changing WCHISPStudio protection settings. During bring-up:

```text
Code and data protection mode: unchecked
Do not click Disable Two-Line Interface
Do not use config set unless the exact config bytes are known
```

## Attempt 1: WCH-Link Unprotect

If WCH-Link can still attach even briefly, try this first.

Close MounRiver, WCHISPStudio, and WCH-LinkUtility. Power-cycle the target.

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" list
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" --chip CH59X --speed low status
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" unprotect --chip CH59X --speed low
```

Then try:

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" erase --chip CH59X --speed low --method default
```

If large images fail partway through flashing, first recover with a very small known-good HEX:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\ch592_recovery\try_wlink_flash_loop.ps1 -HexPath "Firmware\mounriver_ch592_led_probe\obj\mounriver_ch592_led_probe.hex" -Attempts 30
```

If the chip was already erased once, omit repeated erase. A small image has a much better chance
of flashing before another reset/glitch than the full WCH BLE `Peripheral.hex`.

If it still cannot attach, move to USB bootloader recovery.

## Attempt 2: Native USB Bootloader Unprotect

If the CH592 appears in USB Download mode as:

```text
VID:PID 4348:55E0
```

then use the already-proven `wchisp` recovery command:

```powershell
$wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
& $wchisp config unprotect
```

This requires the `4348:55E0` bootloader interface to be bound to WinUSB with Zadig. Do not change
the WCH-LinkE interfaces `1A86:8010`.

## Attempt 3: Catch The Brief Bootloader Window

The public CH59x recovery gist notes that some bad-config parts briefly enumerate the bootloader,
then reset too quickly for normal tools. The repo has a Windows-oriented polling script:

```text
Firmware/tools/ch592_recovery/revive_ch59x.py
```

Install Python dependencies:

```powershell
python -m pip install pyusb libusb-package
```

In another terminal, optionally watch whether Windows sees the bootloader at all:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\ch592_recovery\watch_usb_bootloader.ps1 -Seconds 120 -IntervalMs 100
```

Put the target into the best Download-mode sequence you can: hold Download/PB22 while power-cycling
or resetting. Then run the script before or during the reset attempts:

```powershell
python Firmware\tools\ch592_recovery\revive_ch59x.py --loop --max-seconds 120 --reboot
```

That read-only form only tries to catch the device, read config, and reboot.

If this prints `CH59x USB ISP device not caught` while the PowerShell watcher sees `4348:55E0`,
the likely reason is a Windows driver binding mismatch. PyUSB needs the bootloader interface bound
to WinUSB/libusb. WCHISPStudio uses the official WCH driver and will conflict with PyUSB.

If it catches the device and the config is bad, explicitly write the known development config:

```powershell
python Firmware\tools\ch592_recovery\revive_ch59x.py --loop --max-seconds 120 --write-dev-config --confirm WRITE_CONFIG --reboot
```

The script writes:

```text
USER_CFG = 0x4FFF0FD5
CFG_DEBUG_EN = Enable
CFG_ROM_READ = Read enable
CFG_RESET_EN = Disable
```

This is the previously observed working development config for this board.

## Observed 2026-05-17 Recovery

The bootloader was observed repeatedly appearing and disappearing on Windows:

```text
VID_4348&PID_55E0  USB Module
```

The Python recovery script initially caught the device and read a bad/suspicious config:

```text
config_read: ... A5 5A FF FF FF FF FF FF FF 3F FF 6F ...
```

Running the guarded config rewrite restored the known development config:

```powershell
python Firmware\tools\ch592_recovery\revive_ch59x.py --loop --max-seconds 60 --interval 0.03 --write-dev-config --confirm WRITE_CONFIG --reboot
```

Successful post-write evidence:

```text
config_read: ... FF FF FF FF FF FF FF FF D5 0F FF 4F ...
config_write_dev_0x4FFF0FD5: A8 00 02 00 00 00
reboot: A2 00 02 00 00 00
```

After that, WCH-Link attach worked again:

```text
Attached chip: CH59X [CH592] (ChipID: 0x92000000)
```

However, command-line `wlink flash` still failed during programming on this session, even for the
small LED probe image:

```text
Error: WCH-Link underlying protocol error: 0x55 [0x81, 0x55, 0x01, 0x02]
Error while fastprogram: [41, 01, 01, 05]
```

Treat this as: config/debug attach recovered, but CLI flash still needs either the exact manual
Download-mode sequence or the WCH GUI path.

## If None Of These Catch The Chip

If there is no USB bootloader enumeration and no WCH-Link attach, there may be no practical
software-only recovery path with the tools currently available. At that point:

- Try another USB cable/port only once.
- Try another CH592 dev board to confirm the toolchain.
- Keep this board for later low-level recovery experiments.
- Do not use it as the blocker for North Pole firmware bring-up.
