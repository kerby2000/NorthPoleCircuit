# RGB WS2812 PA14 Bring-Up Evidence

This folder stores local oscilloscope evidence for the Rev-A MVP rework where
MCU PA14 (`/BUSY`) is jumpered to the WS2812 `/LED` data net after the WT2003
BUSY trace is cut.

## Current Result

Firmware image under test:

```text
Firmware/build/bringup/northpole_ch592_bringup.hex
RGB backend: pa14-rework
```

Confirmed over USB CDC:

```text
rgb brightness 24
rgb off
rgb all 255 0 0
rgb all 0 255 0
rgb all 0 0 255
rgb order test
rgb chase 24
```

2026-06-06 Rev-A hardware result:

- WT2003 `BUSY` trace is cut.
- MCU-side PA14/BUSY is jumpered to the WS2812 data input.
- The old PA15 `/LED` trace was also cut/isolated.
- After isolating PA15, the LED data net returned to about 3.3 V and PA14 can
  control all six LEDs.

The PA15 result matters: before the cut, something on the old PA15 path was
pulling the LED data net down. It may have been firmware pin initialization or
another load on that route, but the Rev-A conclusion is simple: do not leave
PA15 and PA14 both connected to the WS2812 data net.

Remaining issue:

- LED0 can stay green after otherwise valid `rgb` commands.
- Scope capture of `rgb off` showed a suspicious leading high/meander before
  the first normal LED frame.
- That points to a first-bit/startup artifact, not to a dead LED chain.

The PA14 GPIO bit-bang experiment was tried as an alternative, but bench
captures showed invalid-looking WS2812 frames and all LEDs could latch white.
The current reference returns to the SPI0-MOSI PA14 backend. The driver now
keeps SPI ownership of PA14 and sends zero-byte reset/latch windows before and
after every WS2812 frame, so any SPI enable artifact is reset away before LED0
data begins.

## Evidence Files

- `20260606_164539_rgb_pa14_green_resetlow_fix.*`: first capture after the
  reset-low fix. Shows the long HIGH before the frame was removed.
- `20260606_164717_rgb_pa14_green_resetlow_fix_overview_100usdiv.*`: frame
  overview.
- `20260606_172424_rgb_pa14_square_1khz_ch2.*`: post-PC-restart 1 kHz PA14 GPIO
  square-wave attempt on scope CH2.
- `20260606_173506_rgb_pa14_square_2hz_ch2.png`: slow PA14 GPIO square-wave
  attempt on scope CH2.

## Interpretation

The expected WS2812 message construction is:

```text
color order: GRB
bit order: MSB first
encoding: 0 = 1000, 1 = 1110
SPI clock: about 3.2 MHz
WS2812 bit cell: about 1.25 us
6 LEDs frame length: about 180 us
reset/latch low: 240 us
```

If LED0 remains green with the SPI0-MOSI PA14 backend, capture the first 50 us
of `rgb off` on the LED data net. The useful pass/fail question is whether the
pre-frame reset window is low all the way until the first normal WS2812 `0` bit,
or whether there is still an extra high pulse before the real frame.

## Required Next Test

Flash:

```text
Firmware/build/bringup/northpole_ch592_bringup.hex
```

Then run:

```text
rgb backend
rgb brightness 24
rgb off
rgb all 255 0 0
rgb all 0 255 0
rgb all 0 0 255
```

Expected:

```text
backend reports spi0-mosi-pa14
LED0 no longer stuck green after rgb off
all six LEDs follow red/green/blue commands
```
