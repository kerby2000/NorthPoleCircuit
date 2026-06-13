# Rev-A MVP Rework State

This document records the physical state expected by the MVP demo firmware.

## Required Reworks

RGB LED path:

- WT2003 `BUSY` trace cut from the WT2003 chip.
- PA14/SPI0 MOSI jumpered to the WS2812 data input.
- Original PA15 `/LED` trace isolated from the LED data net.
- RGB backend must report the PA14/SPI0 MOSI path.

This rework is required because the original PA15 LED path pulled the data net
down in bench testing. After isolating PA15, all six RGB LEDs were controllable.

Hall sensors:

- Rev-A Hall sensor footprint/symbol mapping is wrong.
- Power/GND can be made usable, but the OUT pad is not practically accessible
  for normal assembly.
- Hall input is not a demo dependency unless a sensor is manually reworked.

Power:

- MVP operation is USB powered.
- IP5209 battery-only boost remains unresolved and is not part of the MVP demo.
- Do not treat battery-only operation as validated.

Audio:

- WT2003 USB mass-storage path works with the special USB cable.
- MP3 files can be copied to external flash.
- Playback works; volume range is `0..31`.
- Playback start can take several seconds depending on WT2003/storage state.

Motion:

- Sled motion is proven with the smooth motor path.
- Current best reported behavior:

  ```text
  motor wave-run-smooth 8000 1000 10000 all sleep1 fwd guard-fwd guard-duty 600
  ```

- Guard duty around `600` is quiet. Higher guard duty can create audible
  electrical/electromagnetic noise.
- Short tests showed USB current around `0.8 A`, below `1 A`; DRV8837 heating
  stayed moderate in the low `40 C` range.

## Rev-B Must Fix

- Correct Hall sensor symbol/footprint pinout.
- Move WS2812 data to a clean MCU pin or preserve the PA14/SPI0 MOSI route
  intentionally.
- Keep debug/I2C sharing on PB14/PB15 documented if retained.
- Revisit IP5209 battery boost start-up and VOUT regulation.
