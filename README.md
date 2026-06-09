# North Pole BLE Audio Card

This is a work-in-progress derivative of the original
[Janky Jingle Crew NorthPoleCircuit](https://github.com/Janky-Jingle-Crew/NorthPoleCircuit).

The goal is to turn the original moving magnetic Christmas PCB card into a BLE-enabled audio postcard / mini-diorama with rechargeable battery power, real audio playback, capacitive touch controls, Hall checkpoints, and addressable RGB lighting.

![North Pole BLE Audio Card PCB render](./Media/north_pole_ble_audio_card_render.png)

## Status

Current state: hardware revision in progress.

- KiCad 10 PCB project is included under `PCB/`.
- Draft JLCPCB manufacturing files are included under `PCB/production/`.
- Firmware bring-up scaffold lives under `Firmware/northpole_ch592_bringup/` and is based on the WCH CH592 EVT BLE Peripheral project.
- The board has not yet been validated on real hardware.
- The audio playback IC sourcing is still a risk and should be checked before ordering.

Do not treat the current production files as a proven release. Review ERC/DRC, BOM sourcing, assembly rotations, antenna keepout, LiPo safety, and audio availability before manufacturing.

## Repository Layout

```text
PCB/                     KiCad 10 hardware project and draft manufacturing files
Firmware/                CH592X BLE/audio firmware bring-up project and docs
audio_assets/            WT2003 external-flash manifest and copy-order packer
Code/                    Original upstream firmware, kept for reference only
CAD/                     Original upstream mechanical assets
Media/                   Original media plus new PCB renders
CHANGES_FROM_ORIGINAL.md Summary of the redesign
```

## Hardware Overview

This redesign keeps the magnetic track idea from the original project, but changes most of the control electronics.

Main blocks in the current KiCad design:

- CH592X BLE MCU
- 3 x DRV8837 H-bridge motor drivers
- WT2003H4-24SS audio playback IC
- PY25Q64HA SPI NOR flash for audio assets
- IP5209 1S LiPo charge / boost PMIC
- ME6211 3.3 V regulator
- 2 x DRV5032 Hall sensors
- 4 capacitive touch buttons
- 6 x 1010 addressable RGB LEDs, currently JLCPCB `C41347988`
- USB-C input
- 32 MHz crystal
- WCH-style PCB BLE antenna footprint
- J3 WCH-LinkE/debug Tag-Connect footprint
- J4 WT2003 USB update Tag-Connect footprint
- thin speaker and 1S LiPo battery placement helpers

## Opening The PCB

Use KiCad 10.

Open:

```text
PCB/NorthPoleCircuit_PCB.kicad_pro
```

The local custom libraries are kept in the repo:

- `PCB/cust_sym.kicad_sym`
- `PCB/cust_fp/`
- `PCB/footprints.pretty/`

## Manufacturing Files

Draft manufacturing outputs are in:

```text
PCB/production/
```

Included files:

- `NorthPoleCircuit_PCB14.zip` - Gerber/drill archive
- `bom.csv` - JLCPCB BOM
- `positions.csv` - JLCPCB pick-and-place
- `designators.csv` - Fabrication Toolkit designator output

Before ordering, re-export these from KiCad/Fabrication Toolkit and compare against the files here.

## Firmware

Firmware for this revision is under [Firmware/](./Firmware/). The initial target is `Firmware/northpole_ch592_bringup/`, copied from a working WCH CH592 EVT BLE Peripheral example and then modified at the application layer.

Important hardware notes:

- J3 is WCH-LinkE/debug: 1=3.3V target reference, 2=TIO/SWDIO/PB14, 4=TCK/SWDCK/PB15, 5=GND, 3/6=NC.
- PB14/PB15 are shared with IP5209 I2C through R15/R14 0 ohm links. Do not expect IP5209 I2C access during active WCH-LinkE debugging.
- J4 is intended as WT2003 USB update only, not ARM SWD, but the current PCB revision is blocked for update use: pin 1 is unconnected, so J4 has D+/D-/GND only. Do not use it for WT2003 USB update until +5 V is routed or a rework power path is documented.
- PB0 controls global DRV8837 `~SLEEP`; idle/fault state is `/SLEEP` low and all bridge IN pins low.

The existing `Code/` directory is from the original upstream NorthPoleCircuit project and is not firmware for the CH592X BLE/audio redesign.

## Major Changes

See [CHANGES_FROM_ORIGINAL.md](./CHANGES_FROM_ORIGINAL.md).

Short version:

- single BLE-capable CH592X control architecture
- rechargeable LiPo power path
- audio playback IC and speaker path
- capacitive touch buttons
- Hall checkpoint sensing
- addressable RGB LEDs
- revised KiCad 10 PCB with custom artwork and JLCPCB metadata

## Attribution

Original project:

- [Janky Jingle Crew - NorthPoleCircuit](https://github.com/Janky-Jingle-Crew/NorthPoleCircuit)

Related inspiration:

- [Jeff McBride - Gauss Speedway](https://jeffmcbride.net/gauss-speedway/)

This repository is a derivative redesign. The original concept, mechanical idea, and much of the visual/magnetic-track inspiration belong to the original authors.

## License

TODO.

Before publishing a release or manufacturing publicly, verify the license/permission status of the upstream NorthPoleCircuit project. If the upstream project has no explicit open-source license, only the new original work in this redesign can be licensed without additional permission.
