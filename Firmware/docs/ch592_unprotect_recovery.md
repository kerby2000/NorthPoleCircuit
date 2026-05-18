# CH592 Protected-State Recovery

## CRITICAL RECOVERY NOTE

If WCH-LinkUtility, MounRiver, or `wlink` suddenly stop attaching to a CH592 that previously worked,
check for a protected target before changing wiring or reinstalling tools.

The observed failure was:

```text
WCH-LinkUtility:
Failed, the chip type is not matched or status of chip is wrong!

wlink:
Probe is not attached to an MCU, or debug is not enabled.
```

The root cause was a protected CH592 configuration:

```text
CFG_DEBUG_EN = Disable
CFG_ROM_READ = Disable
```

WCHISPStudio V3.9 could still flash the chip through native USB Download mode, but it did not
provide an obvious GUI path to unprotect/restore debug access. The fix was to temporarily use
`wchisp config unprotect` through a WinUSB driver binding.

## Recovery Recipe That Worked

1. Put CH592 into USB Download mode with the PB22 Download button/reset sequence.
2. In Zadig, select the **CH592 bootloader device only**:

   ```text
   USB Module
   VID:PID 4348:55E0
   ```

3. Replace/install the driver for that device as `WinUSB`.
4. Do **not** replace the WCH-LinkE probe interfaces:

   ```text
   WCH-Link (Interface 0)
   WCH-Link (Interface 1)
   VID:PID 1A86:8010
   ```

5. Run:

   ```powershell
   $wchisp = "$env:USERPROFILE\.platformio\packages\tool-wchisp\wchisp.exe"
   & $wchisp config unprotect
   ```

6. Power-cycle the board.
7. Try WCH-Link attach again:

   ```powershell
   & "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" --chip CH59X --speed low status
   ```

8. MounRiver and WCH-LinkUtility should again identify the chip.

## Important Tool Behavior

`wchisp config set <hex>` is exposed by the CLI but was not usable for this board/tool path:

```text
thread 'main' panicked at src/main.rs:277:21:
not implemented
```

`wchisp config unprotect` is different and did work as the recovery path.

WCHISPStudio V3.9 and `wchisp` need different Windows driver bindings for the same CH592 native
USB Download device:

```text
WCHISPStudio GUI: official WCH CH375/WCHLink driver stack
wchisp: WinUSB/libusb/nusb binding
```

Only one binding can be active for `4348:55E0` at a time. After using WinUSB for `wchisp`, the
WCHISPStudio GUI may stop seeing the CH592 bootloader until the official WCH driver is restored.
This does not affect WCH-LinkE debug attach, which uses the separate `1A86:8010` probe interface.

## Prevention

During bring-up:

- Keep `Code and data protection mode` unchecked in WCHISPStudio.
- Do not enable read/code protection for development boards.
- Do not click `Disable Two-Line Interface` in WCH-LinkUtility.
- Do not use config/protection commands unless the intended bit changes are understood.
- If WCH-Link attach suddenly fails after ISP experiments, run `wchisp config unprotect` before
  reinstalling MounRiver or changing hardware.

## Known Good Post-Recovery Check

After recovery, WCH-LinkUtility should again show a successful chip identification or memory read,
and MounRiver Download Settings should show a successful linked MCU query, for example:

```text
Linked MCU Type: CH591/2
Operation Result: Succeed
```

## If The Bootloader No Longer Starts

If the CH592 no longer stays in USB Download mode long enough for WCHISPStudio or `wchisp`, use the
escalation path in:

```text
docs/ch592_brick_recovery.md
Firmware/tools/ch592_recovery/revive_ch59x.py
```

That path is based on catching the brief `4348:55E0` bootloader window and writing the known-good
development config:

```text
USER_CFG = 0x4FFF0FD5
CFG_DEBUG_EN = Enable
CFG_ROM_READ = Read enable
CFG_RESET_EN = Disable
```
