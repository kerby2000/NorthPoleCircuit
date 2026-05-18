# Toolchain

Initial firmware base is the WCH CH592 EVT SDK in MounRiver Studio style.

Verified local paths:

```text
MounRiver Studio: C:\MounRiver\MounRiver_Studio2
CH592 EVT SDK:    C:\WCH\CH592EVT
EVT baseline:     C:\WCH\CH592EVT\EVT\EXAM\BLE\Peripheral
EVT broadcaster:  C:\WCH\CH592EVT\EVT\EXAM\BLE\Broadcaster
Compiler:         RISC-V Embedded GCC, riscv-none-embed-gcc
```

The selected baseline is the WCH BLE `Peripheral` example. It is smaller than `BLE_UART` while still preserving the CH59x BLE library linkage, WCH startup file, linker script, HAL initialization, `CH59x_BLEInit()`, `HAL_Init()`, `GAPRole_PeripheralInit()`, and the standard peripheral profile scaffold.

## Baseline Verification

The unmodified EVT `Peripheral` example was built locally with the MounRiver-bundled compiler and the project settings from its `.cproject`.

Command:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile evt-baseline
```

Expected output:

```text
Firmware\build\evt_peripheral_unmodified\Peripheral.hex
```

Observed size from the local build:

```text
text=155052 data=1196 bss=8204 dec=164452
FLASH used: 156248 B / 448 KB
```

The unmodified EVT `Broadcaster` example is the current RF sanity baseline because it was observed in nRF Connect as `abc`.

Command:

```powershell
& .\Firmware\tools\build.ps1 -Profile evt-broadcaster-baseline
```

Expected output:

```text
Firmware\build\evt_broadcaster_unmodified\Broadcaster.hex
```

Observed size from the local command-line build:

```text
text=125880 data=424 bss=8136 dec=134440
FLASH used: 126304 B / 448 KB
```

The command-line build was bitwise compared against the WCH/MounRiver-generated HEX:

```powershell
$wch = 'C:\WCH\CH592EVT\EVT\EXAM\BLE\Broadcaster\obj\Broadcaster.hex'
$repo = 'Firmware\build\evt_broadcaster_unmodified\Broadcaster.hex'
Get-FileHash $wch,$repo
cmd /c fc /b "$wch" "$repo"
```

Observed result:

```text
SHA256: 0B8F54FB1793830449032A8074F669681560CD3A0D3C56C8C7C38DCAA4BAF19D
SIZE:   355299 bytes
fc /b:  no differences encountered
```

## North Pole Bring-Up Build

The active bring-up project is:

```text
Firmware\northpole_ch592_bringup
```

Build:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

Expected output:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
Firmware\build\bringup\northpole_ch592_bringup.elf
```

Observed size from the local build:

```text
text=165696 data=1572 bss=8412 dec=175680
FLASH used: 167268 B / 448 KB
```

Production profile currently builds the same MounRiver project with `FIRMWARE_PROFILE_PRODUCTION` defined. The production application behavior is still intentionally pending; the first hardware target is bring-up.

## MounRiver Import

Open/import `Firmware\northpole_ch592_bringup` in MounRiver Studio.

The repo project uses linked SDK resources pointing at `C:\WCH\CH592EVT\...` so the SDK startup, linker script, HAL, BLE library, RVMSIS, and StdPeriphDriver are not vendored into this repository.

The MounRiver `.project` file intentionally contains hardcoded linked-resource paths under `C:\WCH\CH592EVT\...`. Keep the CH592 EVT SDK at that path, or update both the project links and build scripts together. The project files `.project` and `.cproject` are source-of-truth project metadata; generated `.settings/` workspace state is not committed.

## Flashing

Headless flashing has not been proven on this board from the repo scripts yet.

Use MounRiver Studio first:

1. Import/open `Firmware\northpole_ch592_bringup`.
2. Build the `obj` configuration.
3. Use MounRiver's Download action with the connected WCH-supported programming path.
4. Verify reset and then measure motor/RGB/audio-safe pins before running any peripheral test.

`Firmware\tools\flash.ps1 -Method openocd` contains an initial WCH OpenOCD path for WCH-Link style programming, but it still needs hardware validation.
