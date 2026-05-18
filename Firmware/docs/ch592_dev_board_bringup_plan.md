# CH592 Dev Board Bring-Up Plan

This is the next working plan after confirming:

- WCH-LinkUtility can flash and verify through WCH-LinkE.
- The CH592 config was recovered to `USER_CFG = 0x4FFF0FD5`.
- WCH BLE `Broadcaster.hex` advertises and appears in nRF Connect as `abc`.

## Known-Good Dev-Board Baseline

Status on 2026-05-18: `PASS`.

Board:

```text
CH592X-EVT-R1-LinkE
WCH-LinkE firmware: v2.18(v38), RISC-V mode
Target chip: CH592, ChipID 0x92000000
```

Working flash path:

```powershell
& "$env:USERPROFILE\.platformio\packages\tool-wlink\wlink.exe" flash --chip CH59X --speed low --erase "Firmware\build\bringup\northpole_ch592_bringup.hex"
```

Known-good North Pole dev-board build:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_DEV_BOARD_BLE_SMOKE=1','APP_DEV_BOARD_BRINGUP_APP_SMOKE=1') }"
```

Known-good artifact:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
SHA256 E18CCA1B6B6AAEB0A15643D0F001D14B9DF90410CEE089EFF2884AEF2B44CB90
```

Validated behavior:

```text
BLE advertisement: NorthPole BLE
BLE address observed on PC: 70:19:88:8F:46:44
USB CDC shell: COM19, VID/PID 1A86:8040, serial NPCH5920001
Diagnostic GATT service: 0xFD90 readable
Reset recovery: COM19 disconnects/reconnects; BLE advertising resumes
```

Passing automated checks:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --profile dev-board --port COM19 --timeout 3 --reset-recovery --ble-name "NorthPole BLE"
python Firmware\tools\ble_diag_smoke_test.py --scan --timeout 10
```

Important notes:

- This is a dev-board smoke image, not target-board firmware.
- `APP_DEV_BOARD_BRINGUP_APP_SMOKE=1` keeps NorthPole target-only hardware access disabled.
- Do not use this build to validate DRV8837, WS2812, WT2003, IP5209, Hall, or touch behavior.
- A Windows CDC edge case was fixed during this baseline: exact 64-byte USB IN responses require a zero-length packet when no more TX data is pending.

## Rules For The Next Session

Do not change protection/config bits.

Do not use `wchisp config set`.

Do not switch USB drivers unless we deliberately choose a recovery path.

Use WCH-LinkUtility or MounRiver for normal flashing. Keep WCHISPStudio/native USB ISP as a fallback only.

Use `CLK Speed = Low`, `Erase All`, `Program`, `Verify`, and `Reset and Run`.

## Phase 1: Reconfirm Known Good State

1. Power-cycle the CH592 dev board normally.
2. Flash the WCH BLE `Broadcaster` example if needed:

```text
C:\WCH\CH592EVT\EVT\EXAM\BLE\Broadcaster\obj\Broadcaster.hex
```

3. Open nRF Connect.
4. Confirm device `abc` appears.

Expected result:

```text
BLE RF baseline PASS
```

If `abc` does not appear, stop and debug the dev board/tool state before touching North Pole firmware.

## Phase 2: Build North Pole Bring-Up Firmware

From repo root:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

Expected output:

```text
BUILD_OK ...\Firmware\build\bringup\northpole_ch592_bringup.hex
```

If the KiCad audit warns but the firmware still builds, record the warning and continue only if the HEX is produced.

## Phase 3: Flash North Pole Bring-Up Firmware

Use WCH-LinkUtility or MounRiver first.

Target file:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

WCH-LinkUtility settings:

| Setting | Value |
|---|---|
| Core | RISC-V |
| Series | CH590/1/2 |
| Address | `0x00000000` |
| CLK Speed | Low |
| Operations | Erase All, Program, Verify, Reset and Run |

Expected flash result:

```text
Operation is Successful
```

After flashing, power-cycle or reset normally. Do not hold Download.

## Phase 4: Scan For North Pole BLE

Phone first:

```text
nRF Connect -> scan for NorthPole BLE
```

PC fallback:

```powershell
python Firmware\tools\ble_diag_smoke_test.py --scan --timeout 20
```

Generic PC scan:

```powershell
@'
import asyncio
from bleak import BleakScanner

async def main():
    devices = await BleakScanner.discover(timeout=20, return_adv=True)
    for address, (device, adv) in devices.items():
        name = device.name or adv.local_name or "<no name>"
        print(f"{address} RSSI={adv.rssi:>4} name={name} uuids={adv.service_uuids}")

asyncio.run(main())
'@ | python -
```

Expected result:

```text
NorthPole BLE
```

If no North Pole device appears but the WCH `abc` broadcaster still works, compare our firmware against the WCH `Broadcaster` and `Peripheral` startup path.

## Phase 5: USB CDC Shell Check

If the North Pole firmware boots, check Windows Device Manager for a new COM port.

If a COM port appears, run:

```powershell
python Firmware\tools\usb_shell_smoke_test.py COMxx
```

Or manually open a serial terminal and run:

```text
version
status
pins verify
safe check
faults
```

Expected:

```text
version/status respond
safe check reports motor sleep low and all motor inputs low
```

If no USB CDC device appears, do not block BLE validation. USB CDC remains higher risk than BLE because the implementation is custom.

## Phase 6: If North Pole BLE Does Not Advertise

Use this order:

1. Reflash WCH `Broadcaster.hex`; confirm `abc`.
2. Build and flash the unmodified EVT Broadcaster through the repository command-line build path:

```powershell
& .\Firmware\tools\build.ps1 -Profile evt-broadcaster-baseline
```

Flash:

```text
Firmware\build\evt_broadcaster_unmodified\Broadcaster.hex
```

Expected scanner result:

```text
abc
```

The repository command-line build is expected to be bitwise identical to WCH's generated HEX:

```powershell
$wch = 'C:\WCH\CH592EVT\EVT\EXAM\BLE\Broadcaster\obj\Broadcaster.hex'
$repo = 'Firmware\build\evt_broadcaster_unmodified\Broadcaster.hex'
Get-FileHash $wch,$repo
cmd /c fc /b "$wch" "$repo"
```

Known-good result:

```text
SHA256 both files = 0B8F54FB1793830449032A8074F669681560CD3A0D3C56C8C7C38DCAA4BAF19D
fc /b: no differences encountered
```

If WCH's original `Broadcaster.hex` advertises but this bitwise-identical command-line-built `Broadcaster.hex` does not, stop and debug flashing/tool state before touching North Pole code.

3. Reflash North Pole bring-up; scan again.
4. Check whether North Pole firmware reaches `CH59x_BLEInit()`, `HAL_Init()`, `GAPRole_PeripheralInit()`, and `Peripheral_Init()`.
5. Temporarily disable high-risk app layers with build defines:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup -ExtraDefine APP_USB_CDC_SHELL_ENABLE=0
```

6. Build the dev-board BLE broadcaster smoke image. This intentionally uses the WCH Broadcaster role, which is the same role proven by the `abc` EVT example.

Important: dev-board smoke builds skip NorthPole safe-pin GPIO initialization. The CH592X-EVT-R1-LinkE pinout is not the NorthPole PCB pinout; for example PA11/TMR2 is part of the dev-board 32 kHz crystal circuit, while NorthPole uses the corresponding function as `PWM_A1`. Driving NorthPole motor-safe outputs on the dev board can stop BLE from starting.

```powershell
& .\Firmware\tools\build.ps1 -Profile bringup -ExtraDefine @('APP_DEV_BOARD_BLE_BROADCASTER_SMOKE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Flash:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

Expected scanner result:

```text
NorthPole BLE
```

The advertisement also includes short local name:

```text
NPB
```

If this broadcaster smoke image advertises, the North Pole build/link/startup path can run BLE and the next bug is specifically in the Peripheral/GATT path or in target-board-only bring-up layers. If this image does not advertise but WCH `Broadcaster.hex` still appears as `abc`, copy the EVT Broadcaster behavior into the NorthPole project one change at a time and keep NorthPole safe pins disabled for dev-board-only tests.

7. Build the dev-board BLE peripheral smoke image. This skips the North Pole bring-up init/poll layer and skips the custom diagnostic GATT service so the startup path stays close to the WCH Peripheral example. It also skips NorthPole safe-pin GPIO initialization for the same dev-board pin-conflict reason.

```powershell
& .\Firmware\tools\build.ps1 -Profile bringup -ExtraDefine @('APP_DEV_BOARD_BLE_SMOKE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Flash:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

Expected scanner result:

```text
NorthPole BLE
```

If the peripheral smoke image advertises but the normal bring-up image does not, the BLE stack and RF path are good; re-enable bring-up subsystems one at a time, starting with the shell disabled and USB CDC disabled. If broadcaster smoke works but peripheral smoke does not, focus on the WCH Peripheral role/GATT path. If neither North Pole smoke image advertises while WCH `Broadcaster.hex` does, compare startup/linker/build inputs against the WCH EVT `Broadcaster` project before changing hardware.

Do not tune motors, touch thresholds, WS2812 timing, WT2003 audio, or IP5209 I2C until BLE boot and safe shell behavior are understood.

## Phase 6A: Incremental Broadcaster Copy Ladder

If the NorthPole broadcaster smoke image still does not advertise, debug by changing one variable at a time from the bitwise-proven WCH Broadcaster baseline:

1. Baseline: bitwise WCH Broadcaster, advertises as `abc`.
2. Same WCH Broadcaster code, built from this repository, advertises as `abc`.
3. NorthPole main with WCH Broadcaster role, NorthPole safe pins disabled, advertises as `NorthPole BLE`.
4. Same image with only NorthPole advertising payload changed.
5. Same image with NorthPole board code linked but not initialized.
6. Same image with one NorthPole init layer enabled at a time: timebase, fault/log, board safe state, shell, USB CDC, settings.
7. Only after the broadcaster path survives, move to Peripheral/GATT.

The expected failure point identifies the first subsystem to inspect. On the dev board, do not enable NorthPole safe-pin GPIO mode unless the specific pin conflict has been handled.

The first two ladder builds are available now:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine LADDER_ADV_NORTHPOLE=1
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine LADDER_USE_NORTHPOLE_BROADCASTER=1
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','APP_USB_CDC_SHELL_ENABLE=0')
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','APP_USB_CDC_SHELL_ENABLE=0')
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','LADDER_INIT_NORTHPOLE_BOARD_SAFE=1','APP_USB_CDC_SHELL_ENABLE=0')
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','LADDER_INIT_NORTHPOLE_BOARD_SAFE=1','LADDER_LINK_FULL_NORTHPOLE_APP=1','APP_USB_CDC_SHELL_ENABLE=0')
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_MAIN=1','APP_DEV_BOARD_BLE_BROADCASTER_SMOKE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Flash these outputs separately:

```text
Firmware\build\broadcaster_ladder_abc\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_northpole\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_northpole_module\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_northpole_linked\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_init_core\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_init_board_safe\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_full_link\broadcaster_ladder.hex
Firmware\build\broadcaster_ladder_northpole_main\broadcaster_ladder.hex
```

Expected results:

| HEX | Expected scanner name | Meaning |
| --- | --- | --- |
| `broadcaster_ladder_abc\broadcaster_ladder.hex` | `abc` | Repo-owned Broadcaster clone works |
| `broadcaster_ladder_northpole\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | NorthPole advertising payload is valid |
| `broadcaster_ladder_northpole_module\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | Exact NorthPole broadcaster module works outside the full NorthPole project |
| `broadcaster_ladder_northpole_linked\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | NorthPole support modules can be linked without breaking BLE |
| `broadcaster_ladder_init_core\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | Low-risk NorthPole state init works |
| `broadcaster_ladder_init_board_safe\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB`, or expected dev-board pin conflict | NorthPole board safe-pin init is isolated |
| `broadcaster_ladder_full_link\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | Remaining NorthPole app/profile sources can be linked without breaking BLE |
| `broadcaster_ladder_northpole_main\broadcaster_ladder.hex` | `NorthPole BLE` or `NPB` | Exact NorthPole `peripheral_main.c` broadcaster-smoke flow works outside the full project profile |

Dev-board BLE smoke builds intentionally keep the WCH EVT startup sequence of initializing default UART1 and printing `VER_LIB` before `CH59x_BLEInit()`. This is isolated to `APP_DEV_BOARD_BLE_SMOKE` and `APP_DEV_BOARD_BLE_BROADCASTER_SMOKE`; normal target-board firmware still uses the NorthPole-safe debug/audio UART policy.

## Phase 7: First Successful North Pole Session

Once `NorthPole BLE` is visible:

1. Save a short note with:
   - HEX path.
   - Flash tool used.
   - WCH-LinkUtility/MounRiver settings.
   - Scanner used.
   - Observed BLE name/address/RSSI.
2. Try BLE diagnostic reads only.
3. If USB CDC also works, run:

```text
version
status
pins verify
safe check
```

4. Stop there unless all safe checks look clean.

## Stop Conditions

Stop and document before proceeding if:

- WCH-Link attach fails again.
- Any tool reports protected/read-protected/debug-disabled state.
- Flash verify fails.
- WCH `Broadcaster` no longer advertises.
- North Pole firmware resets repeatedly.
- USB appears as an unknown device after North Pole firmware.
- Safe pin state cannot be confirmed.
