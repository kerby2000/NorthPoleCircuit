# PCB

This folder contains the KiCad 10 hardware project for the BLE/audio redesign.

Open `NorthPoleCircuit_PCB.kicad_pro` in KiCad 10.

Local project assets:

- `cust_sym.kicad_sym` - custom symbols
- `cust_fp/` - custom footprints used by the board
- `footprints.pretty/` - local STEP models referenced by the PCB
- `production/` - draft manufacturing outputs

The production files are draft outputs for review. Re-export them from KiCad/Fabrication Toolkit before ordering boards.

## Firmware And Bring-Up

The detailed CH592X firmware implementation and first-board bring-up plan is maintained in `../Firmware/README.md`.

Use the bring-up/test firmware profile first. It should keep all high-power outputs disabled at boot and expose explicit diagnostic hooks for power, touch, RGB LEDs, Hall sensors, audio, BLE, and DRV8837 motor testing before running the production firmware.
