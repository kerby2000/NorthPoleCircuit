# WCH-LinkE Debug

J3 is the WCH-LinkE/debug connector.

| J3 pin | Function |
|---:|---|
| 1 | 3.3V target reference |
| 2 | TIO / SWDIO / PB14 / SDA-side MCU net |
| 3 | NC unless reset is later wired |
| 4 | TCK / SWDCK / PB15 / SCL-side MCU net |
| 5 | GND |
| 6 | NC |

PB14/PB15 are shared with IP5209 I2C through 0 ohm links:

| Link | Debug-side net | IP5209-side net |
|---|---|---|
| R15 | `/SWDIO` / PB14 / SDA | `/SDA` |
| R14 | `/SWDCK` / PB15 / SCL | `/SCL` |

Normal firmware uses PB14/PB15 as SDA/SCL. WCH-LinkE uses them as TIO/TCK. Do not expect IP5209 I2C access during active WCH-Link debugging.

If debug communication is unreliable, remove R14/R15 or leave them unpopulated temporarily so the IP5209 bus cannot load the debug pins.
