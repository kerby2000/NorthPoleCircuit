# Changes From Original NorthPoleCircuit

This fork started from the original Janky Jingle Crew NorthPoleCircuit and explores a BLE/audio redesign.

## System Changes

| Area | Original project | BLE audio card redesign |
|---|---|---|
| Controller | CH32V003-based original control architecture | CH592X BLE MCU |
| Wireless | Not a primary feature | BLE control/configuration target |
| Power | USB-C powered card | USB-C plus 1S LiPo charge/boost path |
| Audio | Original buzzer/music approach | Dedicated audio playback IC, flash, and speaker path |
| Motor drive | Original magnetic propulsion driver design | 3 x DRV8837 H-bridge phase drive |
| Position feedback | Original track concept | 2 Hall checkpoint sensors |
| User input | Original buttons | 4 capacitive touch buttons |
| Lighting | Original decorative lighting | 6 addressable 1010 RGB LEDs |
| PCB | Original North Pole Circuit artwork/layout | Revised KiCad 10 PCB postcard layout |

## Hardware Changes

- Added CH592X BLE MCU.
- Added WCH-style PCB antenna footprint and RF keepout considerations.
- Added IP5209 LiPo charge/boost power path.
- Added ME6211 3.3 V logic regulator.
- Added WT2003H4-24SS audio playback IC and SPI NOR flash.
- Added speaker body/wire-pad helper footprints.
- Added 2 Hall sensors for sled position checkpoints.
- Added 4 capacitive touch electrodes.
- Added 6 addressable RGB LEDs.
- Updated RGB LED sourcing to JLCPCB `C41347988` / `TZ-H1010-RGB/A-BU08UF-TA1305NA/W109`.
- Added explicit JLCPCB/LCSC/MPN metadata to the KiCad project.
- Added Fabrication Toolkit rotation-offset properties for IC placement.

## PCB And Manufacturing Changes

- Migrated the active hardware project to KiCad 10.
- Preserved the magnetic-track concept while adding new electronics around it.
- Added local custom symbols and footprints needed by the new layout.
- Added draft JLCPCB production outputs under `PCB/production/`.
- Added local STEP models for selected custom footprints.

## Firmware Status

The new firmware is not written yet.

The original upstream `Code/` directory is kept as reference material. New firmware for this BLE/audio version should live under `Firmware/`.

## Open Items Before A Public Release

- Validate the PCB on real hardware.
- Re-run and review ERC/DRC before ordering.
- Re-export manufacturing files from the exact final KiCad state.
- Confirm JLCPCB stock and assembly status for every part.
- Resolve WT2003H4/audio IC sourcing.
- Verify BLE antenna stackup and keepout.
- Verify LiPo charge current and battery safety.
- Write and test the CH592X firmware.
- Add final photos/video after first board bring-up.
