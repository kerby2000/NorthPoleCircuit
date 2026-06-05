# CH592 Target Board USB CDC / Bring-Up Regression Problem Statement

Date: 2026-05-26

## Short Summary

North Pole CH592 target hardware can run BLE and USB CDC with an older dev-board-smoke bring-up image, but current target-oriented bring-up images either break USB enumeration or enumerate USB CDC without any shell response.

We need help isolating why the target-mode firmware stops responding, and whether the safest route is to restart from the last known-good firmware configuration and add target-board behavior in small increments.

## Hardware

- MCU: WCH CH592X BLE MCU
- Board: North Pole BLE/Audio final JLCPCB target board
- Programming/debug: WCH-LinkE / WCH-LinkUtility / MounRiver path
- USB: CH592 native USB CDC shell on target board USB-C
- BLE: CH592 BLE advertising verified with nRF Connect
- Relevant target nets:
  - USB D+ = U2 pad 15 `/DP`
  - USB D- = U2 pad 16 `/DN`
  - DRV8837 global `/SLEEP` = U2 pad 3 PB0
  - RGB WS2812 data = U2 pad 28 PA15 `/LED`
  - Motor A IN2 = U2 pad 1 PA10/TMR1 `/PWM_A2`

## Known-Good Firmware Artifact

This HEX works on the target board:

```text
Firmware/build/bringup_dev_ble_diag_usb_cdc_smoke/northpole_ch592_bringup.hex
```

Observed behavior:

- Windows enumerates USB CDC as COM19.
- USB shell responds to commands.
- BLE advertises as `NorthPole BLE`.
- nRF Connect can see/read custom diagnostic service `0xFD90`.
- RGB LEDs stay off.
- Example shell responses observed:

```text
version=0.1.0-bringup profile=bringup board=north-pole-ble-audio-current-pcb ...
fault_mask=0x00000000
settings valid=1 version=1 volume=6 brightness=8 scene=0 motor_limit=25 demo=0 crc=0x325a flash=0
audio validation=HARDWARE_VALIDATION_PENDING hw=HW_NOT_TESTED ...
```

This image was built as a dev-board / BLE diagnostic smoke profile, so many target-board hardware reads were intentionally disabled:

```text
hall input reads disabled in dev-board smoke build
touch reads disabled in dev-board smoke build
i2c commands disabled in dev-board smoke build
ip5209 commands disabled in dev-board smoke build
```

## Known-Bad Firmware Artifact

This HEX is bad on the target board:

```text
Firmware/build/bringup/northpole_ch592_bringup.hex
```

Observed behavior before recent pin fix:

- Windows shows USB descriptor failure / device not recognized.
- All 6 WS2812 LEDs emit white.
- Reflashing the known-good smoke image restores USB CDC and LEDs off.

## Important Bug Already Found

The firmware had a serious pin mapping bug:

- U2 pad 1 `/PWM_A2` is `PA10/TMR1`, not `PB10/TMR1_`.
- `PB10` is U2 pad 16 and is USB D- `/DN`.

Old firmware code incorrectly mapped pad 1/TMR1 to PB10 and could configure or drive USB D- as a motor output. This plausibly explains the original USB descriptor failures.

Fix applied in:

```text
Firmware/northpole_ch592_bringup/APP/northpole/ch592_board_port.c
Firmware/tools/extract_kicad_pinmap.py
Firmware/docs/hardware_pin_audit.md
```

Key fixed behavior:

```c
case 1: hw->port = HW_PORT_A; hw->mask = bTMR1; break; /* /PWM_A2 */
GPIOPinRemap(DISABLE, RB_PIN_TMR1); /* Keep TMR1 on PA10; PB10 is USB D-. */
```

After this fix, the smaller ladder images no longer obviously kill USB enumeration, but the USB shell still does not respond.

## Current Failure After Pin Fix

Target ladder images were built to isolate the failure:

```text
Firmware/build/target_ladder/
```

Important images:

```text
00_dev_smoke_reference
01_core_no_target_gpio
02aa_early_safe_inputs_only
02ab_early_safe_sleep_only
02ac_early_safe_rgb_only
02a_early_safe_sleep_rgb_only
02b_board_safe_sleep_rgb_only
02_safe_pins_only
03_power_i2c_only
...
```

The user flashed at least two of the new `02*` ladder images after the pin fix and still saw:

- USB CDC COM port appears.
- Sending `version\r\n` produces no response.
- Automated smoke test can open COM19 but all commands timeout.

Automated test command:

```powershell
python Firmware\tools\usb_shell_smoke_test.py --port COM19 --profile target --timeout 2
```

Observed output:

```text
>>> version
<no response>
>>> status
<no response>
...
USB CDC smoke test failed: no response for version, status, pins verify, safe check, faults, settings show, rgb off, motor off, audio status, ip5209 status
```

Follow-up split on 2026-05-26:

- `00_dev_smoke_reference` still works on the target board.
- `01_core_no_target_gpio` already fails on the target board.

Therefore the failure is not caused by target GPIO safe states or target peripheral drivers. It is caused by a startup/build-mode difference between dev-smoke mode and target-core mode.

## Root Cause Identified: WCH DEBUG/PRINT UART1 Path

The build script previously compiled all profiles with:

```text
-DDEBUG=1
```

In the WCH SDK, `PRINT()` is active whenever `DEBUG` is defined, and `_write()` routes `printf()` to the selected debug UART:

```c
#ifdef DEBUG
int _write(int fd, char *buf, int size)
{
    ...
#if DEBUG == Debug_UART1
        while(R8_UART1_TFC == UART_FIFO_SIZE);
        R8_UART1_THR = *buf++;
#endif
}
#endif
```

The working dev-smoke startup initializes UART1 before BLE init:

```c
GPIOA_SetBits(bTXD1);
GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
UART1_DefInit();
PRINT("%s\n", VER_LIB);
```

The failing target-core startup does not initialize UART1 because UART1 is routed to the WT2003 audio chip on the target board. However, `peripheral.c` still contains many WCH `PRINT()` calls, including BLE state callbacks:

```c
PRINT("Initialized..\n");
PRINT("Advertising..\n");
PRINT("Connected..\n");
```

This is the confirmed target-core failure path: target-core firmware left WCH `PRINT()` enabled but removed the WCH UART1 debug startup, so WCH BLE state logging could block or disturb startup before the USB shell responded.

Two new diagnostic ladder images were added:

```text
Firmware/build/target_ladder/01a_core_no_target_gpio_no_wch_debug/northpole_ch592_bringup.hex
Firmware/build/target_ladder/01b_core_no_target_gpio_evt_uart_startup/northpole_ch592_bringup.hex
```

Observed interpretation:

- `01a_core_no_target_gpio_no_wch_debug` works.
- `01b_core_no_target_gpio_evt_uart_startup` works.
- Therefore the correct target-board fix is to disable WCH `DEBUG`/`PRINT()` for normal target builds.
- The EVT UART startup path is only acceptable for dev-board/debug-isolation builds because target UART1 is routed to WT2003 audio.

`Firmware/tools/build.ps1` was updated so normal `bringup` and `production` target builds no longer define WCH `DEBUG` by default. WCH `DEBUG` remains enabled automatically for WCH EVT baseline and dev-board smoke builds, and can be explicitly enabled with `-WchDebugPrint` for one-off experiments.

## Key Code Paths

Target startup:

```text
Firmware/northpole_ch592_bringup/APP/peripheral_main.c
```

Target mode does:

```c
SetSysClock(CLK_SOURCE_PLL_60MHz);
northpole_ch592_early_safe_pins();        // if enabled
CH59x_BLEInit();
HAL_Init();
GAPRole_PeripheralInit();
Peripheral_Init();
app_bringup_init();
Main_Circulation();
```

Main loop:

```c
while (1) {
    TMOS_SystemProcess();
    app_bringup_poll();
}
```

USB shell init/poll:

```text
Firmware/northpole_ch592_bringup/APP/northpole/usb_cdc_shell.c
Firmware/northpole_ch592_bringup/APP/northpole/shell.c
Firmware/northpole_ch592_bringup/APP/northpole/app_bringup.c
```

`app_bringup_init()` initializes:

```c
timebase_init();
log_init(LOG_LEVEL_INFO);
fault_init();
board_init_safe_pins();       // target mode if enabled
usb_cdc_shell_init();
LOG_INFO("bring-up profile active\r\n");
settings_init();
...
ble_service_init();
shell_init();
```

`app_bringup_poll()` does:

```c
shell_poll();
usb_cdc_shell_poll();
...
ble_service_poll();
```

## Dev Smoke Path Difference

The known-good smoke path defines one or both of:

```text
APP_DEV_BOARD_BLE_SMOKE=1
APP_DEV_BOARD_BRINGUP_APP_SMOKE=1
```

This causes:

- NorthPole target safe pins are skipped:

```c
#define APP_DEV_BOARD_SKIP_NORTHPOLE_SAFE_PINS \
    (APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BLE_BROADCASTER_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE)
```

- WCH EVT-style UART startup remains present:

```c
#if APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BLE_BROADCASTER_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    PRINT("%s\n", VER_LIB);
#endif
```

- Target peripherals are skipped in `app_bringup_init()`.

The target mode removes the WCH EVT-style debug UART init and applies target pin initialization.

## Current Hypotheses

1. The original full-target USB failure was caused by the PA10/PB10 pin mapping bug. That part is likely fixed.
2. The current "COM port but no shell response" is probably not a Windows driver issue, because the known-good HEX responds on the same board and COM port.
3. The new failure likely happens after USB enumerates but before or inside the main loop/shell polling path.
4. Possible causes:
   - `app_bringup_init()` is blocking or faulting before `shell_init()`.
   - `LOG_INFO()` via USB CDC blocks or behaves differently when USB is configured early.
   - Target-mode BLE/GATT/service path or TMOS startup differs from the proven dev smoke path.
   - Early safe GPIO still touches a pin that affects CH592 USB/BLE/runtime, even after the PB10 fix.
   - Stack/RAM pressure or link/order difference changed behavior.
   - Interrupt priority / USB IRQ / BLE interaction differs between target mode and smoke mode.

## Suggested Next Debug Strategy

Restart from the known-good behavior and move forward one small step at a time.

### Step 1: Prove the current source can still reproduce known-good behavior

Flash this current-source rebuild:

```text
Firmware/build/target_ladder/00_dev_smoke_reference/northpole_ch592_bringup.hex
```

Expected:

- USB shell responds.
- BLE advertises.
- LEDs stay off.

If this fails, current source no longer reproduces the known-good image and we should diff current source against the source state that produced:

```text
Firmware/build/bringup_dev_ble_diag_usb_cdc_smoke/northpole_ch592_bringup.hex
```

### Step 2: Prove target mode with all target GPIO/peripherals disabled

Flash:

```text
Firmware/build/target_ladder/01_core_no_target_gpio/northpole_ch592_bringup.hex
```

Expected:

- USB shell responds.
- BLE advertises.
- No target GPIO safe init.

If `00` works but `01` fails, the bug is not a hardware pin side effect. It is in target-mode startup, compile-time mode differences, BLE/GATT, or app init ordering.

### Step 3: Add one target safe action at a time

Only if `01` works:

```text
Firmware/build/target_ladder/02aa_early_safe_inputs_only/northpole_ch592_bringup.hex
Firmware/build/target_ladder/02ab_early_safe_sleep_only/northpole_ch592_bringup.hex
Firmware/build/target_ladder/02ac_early_safe_rgb_only/northpole_ch592_bringup.hex
```

Expected:

- USB shell still responds.
- `/SLEEP` low when enabled.
- RGB data low when enabled.

If one of these breaks the shell, inspect that exact GPIO config.

### Step 4: Only then add target peripherals

Do not proceed to I2C, touch, Hall, RGB writes, audio, or motor until the shell responds reliably.

## Questions For Review

1. Is there any CH592 USB requirement that can be violated by configuring PA9, PA14, PB12, PB13, PB0, or PA15 before `CH59x_BLEInit()` / `HAL_Init()`?
2. Can `LOG_INFO()` after `usb_cdc_shell_init()` deadlock or starve the main loop if USB is configured but host is not draining IN packets?
3. Does WCH CH592 require a specific order for `SetSysClock`, USB device init, BLE init, and HAL init?
4. Is the custom USB CDC implementation safe to use together with BLE/TMOS, or does it miss a WCH-required interrupt/DMA detail?
5. Could the dev smoke image work only because `APP_DEV_BOARD_BRINGUP_APP_SMOKE` skips target GATT/control paths or target subsystem references?
6. Is RAM usage close enough to the CH592 limit that small build-mode differences can corrupt USB/BLE state?

## Files To Inspect

```text
Firmware/northpole_ch592_bringup/APP/peripheral_main.c
Firmware/northpole_ch592_bringup/APP/peripheral.c
Firmware/northpole_ch592_bringup/APP/northpole/app_bringup.c
Firmware/northpole_ch592_bringup/APP/northpole/usb_cdc_shell.c
Firmware/northpole_ch592_bringup/APP/northpole/shell.c
Firmware/northpole_ch592_bringup/APP/northpole/ch592_board_port.c
Firmware/northpole_ch592_bringup/APP/include/app_config.h
Firmware/northpole_ch592_bringup/APP/include/board_pins_autogen_notes.h
Firmware/tools/build.ps1
Firmware/tools/build_target_ladder.ps1
Firmware/docs/hardware_pin_audit.md
```

## Current Practical Recommendation

Do not keep trying the full target image.

Use the old known-good HEX as the behavioral baseline, then make the source reproduce that baseline. Once current source can reliably produce a working `00_dev_smoke_reference`, add target-mode behavior one step at a time and stop at the first image that loses shell response.
