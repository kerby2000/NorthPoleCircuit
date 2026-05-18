# CH592 Broadcaster Ladder

This is a dev-board-only BLE isolation project.

It starts from the WCH `EVT/EXAM/BLE/Broadcaster` structure that is already proven on the CH592X-EVT-R1-LinkE board. The goal is to change one thing at a time until the first failing change is found.

Build from repo root:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder
```

Expected scanner result:

```text
abc
```

Build with only the advertising payload changed:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine LADDER_ADV_NORTHPOLE=1
```

Expected scanner result:

```text
NorthPole BLE
```

or short name:

```text
NPB
```

Build with the exact NorthPole broadcaster module:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine LADDER_USE_NORTHPOLE_BROADCASTER=1
```

Expected scanner result:

```text
NorthPole BLE
```

Build with the NorthPole support modules linked but not initialized:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Expected scanner result:

```text
NorthPole BLE
```

Build with low-risk NorthPole state initialization:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Expected scanner result:

```text
NorthPole BLE
```

Build with NorthPole board safe-pin initialization:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','LADDER_INIT_NORTHPOLE_BOARD_SAFE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

This is expected to be risky on CH592X-EVT-R1-LinkE because the dev-board pinout is not the NorthPole PCB pinout.

Build with the rest of the NorthPole app/profile sources linked but not initialized:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_BROADCASTER=1','LADDER_LINK_NORTHPOLE_MODULES=1','LADDER_INIT_NORTHPOLE_CORE=1','LADDER_INIT_NORTHPOLE_BOARD_SAFE=1','LADDER_LINK_FULL_NORTHPOLE_APP=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Expected scanner result:

```text
NorthPole BLE
```

Build with the exact NorthPole `peripheral_main.c` as `main`, using broadcaster-smoke mode:

```powershell
& .\Firmware\tools\build.ps1 -Profile broadcaster-ladder -ExtraDefine @('LADDER_USE_NORTHPOLE_MAIN=1','APP_DEV_BOARD_BLE_BROADCASTER_SMOKE=1','APP_USB_CDC_SHELL_ENABLE=0')
```

Expected scanner result:

```text
NorthPole BLE
```

Flash output:

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

Do not use this project as production firmware. It exists only to isolate CH592 BLE bring-up behavior before the NorthPole target board arrives.
