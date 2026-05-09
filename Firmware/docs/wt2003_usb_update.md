# WT2003 USB Update Connector

J4 uses the Tag-Connect footprint only as a convenient pogo connector for WT2003H4 USB/update access. It is not an ARM SWD/debug connector.

| J4 pin | Function |
|---:|---|
| 1 | +5V WT2003 USB/update power |
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

Use only a custom USB adapter/cable wired for this pinout.

Do not plug J4 into a WCH-LinkE or any ARM SWD adapter. The Tag-Connect footprint is reused only as a pogo connector for WT2003 USB storage/update.

Workflow:

1. Power the WT2003 USB/update path through J4 pin 1 and GND.
2. Connect J4 pin 2 to USB D+ and J4 pin 4 to USB D- using a custom adapter.
3. Copy the prepared audio files to the WT2003 drive.
4. Eject/unmount the drive on the computer.
5. Disconnect the WT2003 USB cable before trying UART playback.

The WT2003 may not respond to serial commands while it is connected to a computer as USB storage. Index playback depends on the order files are copied to the drive, not filename sorting. Use `audio_assets/pack_audio_assets.py` to prepare an ordered folder, and start hardware validation with `0001.mp3` only.
