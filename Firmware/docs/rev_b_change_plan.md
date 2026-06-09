# Rev-B Hardware Change Plan

Purpose: collect the first-board bring-up findings that should become Rev-B
schematic, PCB, and firmware changes. This is the dedicated checklist for the
next board revision.

## Confirmed Rev-A Issues

### Hall Sensor Footprint / Symbol

The Hall sensor pinout is wrong on the current PCB. During bring-up, +3.3 V was
found to be connected correctly, but the footprint tied the package `NC` pad to
GND instead of giving the IC its intended GND/OUT routing. With a manual ground
workaround, the sensor reacted to a magnet at about 4 mm and pulled the output
low, so the part itself is usable.

Rev-B action:

- Rebuild the Hall symbol and footprint from the exact sensor datasheet.
- Verify package top-view pinout before layout: VCC, GND, NC, OUT must match
  the real part, not the earlier custom symbol.
- Enlarge or expose the OUT node enough for bring-up probing.
- Re-run `hall read` with a magnet after Rev-B assembly.

Current Rev-A limitation: Hall output is not practical to rework by hand because
the OUT pad is too small.

### WS2812 LED Data / WT2003 BUSY Pin Conflict

Rev-A routes WS2812 LED data to PA15 and WT2003 `BUSY` to PA14. The PA15
bit-bang LED path was unreliable during bring-up. A dedicated SPI0 MOSI-only
experiment proved that PA14 can generate WS2812-compatible encoded pulses while
PA12, PA13, and PA15 remain quiet.

The final Rev-A MVP rework was:

```text
cut WT2003 BUSY trace
jumper MCU-side PA14/BUSY to WS2812 LED data
cut/isolate the old PA15 -> /LED trace
```

After the PA15 trace was isolated, the LED data net returned to about 3.3 V and
the six WS2812 LEDs became controllable from PA14. Before that cut, the old PA15
path was pulling the LED data net down. Do not copy the Rev-A PA15 LED route
into Rev-B.

Rev-B action:

```text
PA14 -> WS2812 LED data through the level shifter
PA15 -> WT2003 BUSY input
PB6  -> PWM_G1
PA12 -> PWM_G2
PA13 -> HALL1 or other input
```

Firmware action:

- Use PA14 as the default WS2812 data pin.
- Prefer the SPI0 MOSI-only WS2812 backend on PA14. The pinmux experiment
  proved it emits WS2812-compatible pulses, and the driver now emits explicit
  reset-low zero bytes before and after each frame to avoid first-LED startup
  artifacts.
- Keep the PA14 GPIO bit-bang backend only as a diagnostic fallback; the Rev-A
  bench attempt produced invalid-looking frames and all LEDs could latch white.
- Keep PA12/PA13/PA15 in their normal application modes while SPI0 emits only
  MOSI on PA14.
- Keep the Rev-A PA14 LED jumper backend as a bring-up-only build option.

Evidence: [spi0_mosi_pinmux_test_report.md](spi0_mosi_pinmux_test_report.md).

### WT2003 USB Update Connector J4

The hardware audit found that J4 is not a complete USB update connector:

```text
J4 pin 1: unconnected, expected +5 V
J4 pin 2: /DP2
J4 pin 4: /DM2
J4 pin 5: GND
```

Rev-B action:

- Add +5 V/VBUS to the WT2003 USB update connector if standalone WT2003 USB
  update mode is required.
- Re-run the KiCad pin audit after schematic update.

### IP5209 Battery-Only Boost

Battery-only operation is not solved on Rev-A. Measurements showed:

- Battery around 3.9 V.
- SW3 can wake IP5209 VREG to about 3.1 V.
- VOUT/+5 V stayed around 2.7-2.8 V instead of rising to 5.0-5.2 V.
- A 220 ohm load dropped VOUT further.

Rev-B action:

- Continue IP5209 power-stage investigation before copying the exact circuit.
- Review KEY, NTC, RSET, LIGHT/VSET straps, inductor/current sense path, and
  output capacitor placement.
- Keep scope evidence and register dumps with the power-design notes.

## Rev-B Firmware Defaults To Revisit

- Board revision string once the schematic is final.
- Pin audit expected nets and generated pin map.
- RGB backend default: SPI0 MOSI on PA14.
- Hall input pin assignments after the corrected footprint is routed.
- Motor G pin assignments after PA12/PB6 remap.

## Rev-A MVP Rework

For the MVP demo on the current board, the WT2003 `BUSY` trace is cut and the
MCU-side PA14 signal can be jumpered to the WS2812 LED data input. Build firmware
with:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_RGB_WS2812_USE_SPI0_MOSI_PA14=1','APP_MOTOR_PWM_BACKEND_ENABLE=1') }"
```

This is not a production pin map. It is a controlled Rev-A rework test for the
six LEDs.

The current preferred Rev-A LED build uses PA14 GPIO bit-bang instead of SPI0
MOSI:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_RGB_WS2812_USE_PA14_BITBANG=1','APP_MOTOR_PWM_BACKEND_ENABLE=1') }"
```
