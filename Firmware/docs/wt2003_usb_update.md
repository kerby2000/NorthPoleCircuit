# WT2003 USB Update Connector

Status: `BLOCKED`.

J4 uses the Tag-Connect footprint only as a convenient pogo connector for WT2003H4 USB/update access. It is not an ARM SWD/debug connector.

The current KiCad/PCB audit shows a functional problem: J4 pin 1 is unconnected. That means J4 currently provides USB D+, USB D-, and GND only; it does not provide the expected +5 V USB/update power path. Do not use J4 for WT2003 USB update until the schematic/PCB is fixed or a rework procedure defines how WT2003 USB VBUS is powered.

| J4 pin | Function |
|---:|---|
| 1 | BLOCKED: expected +5V WT2003 USB/update power, actual unconnected |
| 2 | D+ on `/DP2` |
| 3 | NC |
| 4 | D- on `/DM2` |
| 5 | GND |
| 6 | NC |

Verified PCB mapping from the KiCad audit:

| WT2003 pad | Net | J4 pin |
|---|---|---:|
| U6 pad 5 D+ | `/DP2` | 2 |
| U6 pad 4 D- | `/DM2` | 4 |

Use only a custom USB adapter/cable after the missing +5 V path is corrected.

Do not plug J4 into a WCH-LinkE or any ARM SWD adapter. The Tag-Connect footprint is reused only as a pogo connector for WT2003 USB storage/update.

Blocked workflow until fixed:

1. Do not connect a normal USB source to J4 and expect WT2003 update mode to work.
2. Define a schematic/PCB fix or a documented rework that supplies the required WT2003 USB/update +5 V path.
3. After the power path is fixed, connect J4 pin 2 to USB D+ and J4 pin 4 to USB D- using a custom adapter.
4. Copy the prepared audio files to the WT2003 drive.
5. Eject/unmount the drive on the computer.
6. Disconnect the WT2003 USB cable before trying UART playback.

The WT2003 may not respond to serial commands while it is connected to a computer as USB storage. Index playback depends on the order files are copied to the drive, not filename sorting. Use `audio_assets/pack_audio_assets.py` to prepare an ordered folder, and start hardware validation with `0001.mp3` only.
