# IP5209 Battery Boost Bring-Up Problem Statement

Date: 2026-05-27

## Board

- Project: North Pole BLE/Audio Card
- MCU: WCH CH592X
- PMIC: IP5209T
- 3.3 V regulator: ME6211C33M5
- Battery: 1-cell LiPo, measured about 3.9 V
- Firmware: `Firmware/build/bringup/northpole_ch592_bringup.hex`

## Relevant Power Path

From the KiCad PCB:

- Battery positive net: `Net-(U7-VBAT)`
- IP5209:
  - U7 pin 9 `VBAT` -> `Net-(U7-VBAT)`
  - U7 pin 10 `CSIN` -> `Net-(U7-CSIN)`
  - U7 pin 11 `CSIN_S` -> `Net-(U7-CSIN)`
  - U7 pins 13/14/15 `LX`
  - U7 pins 16/17 `VOUT` -> `/VOUT`
  - U7 pin 3 `VREG` -> `/VREG`
  - U7 pin 8 `KEY` -> `Net-(U7-KEY)`
- R9: 10 milliohm current sense resistor between `Net-(U7-CSIN)` and `Net-(U7-VBAT)`.
- L2: 1 uH boost inductor between `Net-(U7-CSIN)` and `Net-(C16-Pad1)` / IP5209 LX-side switching node.
- L1: 10 uH inductor for the CH592 internal DC/DC path, not the IP5209 boost inductor.
- ME6211 input is fed from IP5209 `/VOUT` / nominal +5 V rail.
- ME6211 output is board `+3.3V`.

## Known Good USB-Powered Behavior

With USB connected and battery absent:

- Board boots.
- CH592 USB CDC shell works.
- BLE advertises as `NorthPole BLE`.
- After `i2c release-debug`, IP5209 ACKs at 7-bit address `0x75`.
- `i2c scan` reports `0x75`.
- `ip5209 probe` returns `rc=0`.
- `ip5209 dump` works.

Example:

```text
i2c release-debug
i2c scan
i2c found=1 0x75
ip5209 probe
ip5209 probe addr=0x75 rc=0
```

## Battery-First Behavior

With battery connected first, USB disconnected:

Initial measurements before pressing SW3:

```text
BAT+ to GND          ~3.9 V
U7 VBAT to GND      ~3.9 V
U7 CSIN to GND      ~3.9 V
U7 CSIN_S to GND    ~3.9 V
U7 VREG to GND      ~0.2 V
+5V/VOUT to GND     ~1.1 V
+3.3V to GND        ~1.1 V
SCL to GND          ~0.2 V
SDA to GND          ~0.2 V
/INT to GND         ~0.0 V
KEY to GND          ~0.45 V
```

This indicates the battery voltage reaches U7, but IP5209 `VREG` does not start by itself.

After pressing SW3 briefly:

```text
U7 VREG to GND      ~3.1 V
U7 VOUT pins 16/17  ~2.8 V
+5V rail            ~2.8 V
ME6211 input        ~2.8 V
ME6211 output/+3.3V ~2.8 V
```

So SW3 wakes the IP5209 internal LDO, but the boost output does not reach 5 V.

With a temporary 220 ohm load from +5 V to GND, rated at least 0.25 W:

```text
+5V rail after SW3/load  ~2.4 V
```

This argues against a simple light-load auto-shutdown explanation. The rail collapses further under a very small load, so the boost stage is either not really regulating, is current limiting/protecting, or the VOUT/LX power path has an assembly or component issue.

## Resistance Checks With Power Removed

Measured resistance to GND:

```text
+5V rail to GND              ~440-470 kOhm
+3.3V rail to GND            ~460-490 kOhm
IP5209 VOUT pins 16/17 to GND ~450 kOhm
IP5209 boost inductor area    ~240-270 kOhm
```

These values do not look like a hard short on +5 V or +3.3 V.

## Important Clarification About Inductors

The IP5209 boost inductor is board reference `L2`, value `1uH`.

Board reference `L1` is a separate 10 uH inductor associated with the CH592 internal DC/DC path. It is not the IP5209 boost inductor.

The expected IP5209 boost behavior after a valid short press on SW3 is:

```text
U7 VREG     about 3.1 V
U7 VOUT     about 5.0-5.2 V
+5V rail    about 5.0-5.2 V
+3.3V rail  about 3.3 V
```

Actual behavior:

```text
U7 VREG     about 3.1 V
U7 VOUT     about 2.8 V
+5V rail    about 2.8 V
+3.3V rail  about 2.8 V
```

## I2C Observations

When IP5209 is awake from USB-first sequencing, I2C works:

```text
i2c found=1 0x75
ip5209 probe addr=0x75 rc=0
```

When battery-first and before a valid power-up, IP5209 does not ACK:

```text
i2c found=0
ip5209 probe addr=0x75 rc=-2
```

In this firmware, `rc=-2` means address NACK. It is not a bus stuck-low condition.

## SW3 / KEY

SW3 is connected from IP5209 `KEY` to GND. A short press should wake/open the SOC indicator LEDs and the step-up converter according to the IP5209 datasheet.

Observed:

- SW3 short press does wake `VREG` to about 3.1 V.
- SW3 short press does not produce a valid 5 V boost output; VOUT remains around 2.8 V.

## Suspicious Measurement

R7 is part of the NTC divider:

```text
VREG -> R7 2M -> NTC node -> R8 1M -> GND
```

Therefore neither side of R7 should be at battery voltage. Expected after `VREG` is up:

```text
R7 VREG side  about 3.1 V
R7 NTC side   about 1.0 V
```

An observed value of about 3.9 V on one side of R7 would be abnormal and should be rechecked against the physical component reference/orientation.

## NTC And SYS_CTL5

The local IP5209/IP5109/IP5207/IP5108 register PDF shows `SYS_CTL5` at register `0x07`.

Relevant bits for this bring-up:

```text
0x07 bit 6 = NTC function disable
    0: NTC enabled
    1: NTC disabled

0x07 bit 1 = WLED/flashlight key mode
    0: long press 2 s
    1: double short press

0x07 bit 0 = shutdown key mode
    0: double short press
    1: long press 2 s
```

The reset value for bit 6 is `0`, so NTC is enabled by default. With the current schematic divider:

```text
VREG -> R7 2M -> NTC node -> R8 1M -> GND
```

the expected NTC node is about one third of VREG, roughly 1.0 V when VREG is 3.1 V. That sits inside the datasheet's normal NTC window described by the register document. NTC should still be measured, but it is not the strongest explanation for a boost rail stuck around 2.4-2.8 V.

The firmware status field named `boost` was renamed to `boost_cfg` in the diagnostic output. It is decoded from register `0x01` bit 2 and means "IP5209 boost enable/config bit is set". It is not a measurement of VOUT and does not prove that the physical +5 V rail is regulating.

## Reference Designs Checked

- M5Stack PowerC HAT documentation identifies an IP5209 power-management chip, 5 V boost output, and an I2C address of `0x75`. This matches the address seen on this board.
- PiSugar 2/Pro software documentation uses an IP5209-style power manager over I2C and reinforces that software can read power/status registers, but the board must still have a valid PMIC power path before the host can run from battery.
- The upstream Linux IP5xxx power-supply driver patch names register `0x01` bit 2 as the boost-enable bit. This supports the firmware interpretation that `boost_cfg=1` is a register state, not a VOUT measurement.

Reference links:

```text
https://docs.m5stack.com/en/hat/hat-powerc
https://github.com/PiSugar/PiSugar/wiki/PiSugar-Power-Manager-(Software)
https://github.com/PiSugar/PiSugar/wiki/PiSugar-2-(Pro)-I2C-Manual
https://lkml.indiana.edu/2411.1/02609.html
```

## Current Hypotheses

Most likely:

1. IP5209 boost converter is not enabling or is immediately entering protection.
2. VOUT power path is failing under even a small load; the 220 ohm test dropped the rail to about 2.4 V.
3. There is an assembly issue around IP5209 LX pins, L2, VOUT capacitors, or VOUT routing.
4. IP5209 is affected by an invalid strap or sense pin, especially NTC/RSET/KEY/LIGHT/VSET.
5. IP5209 part is damaged or wrong variant.

Less likely:

- A pure light-load shutdown, because a 220 ohm load made the rail lower instead of stabilizing at 5 V.
- A hard short on +5 V or +3.3 V, because resistance checks are hundreds of kOhms.
- A firmware I2C bug, because the failure exists before the MCU has valid power from battery.

## Proposed Next Experiments

1. Measure +5 V immediately after SW3 press and keep watching for 40 seconds.
   - If it briefly rises to 5 V then falls, suspect light-load shutdown or protection.
   - If it never rises above 2.8 V, suspect boost-stage hardware or PMIC protection.

2. Capture the expanded firmware diagnostic output while USB powered:
   - `i2c release-debug`
   - `ip5209 status`
   - `ip5209 dump`
   - Confirm `boost_cfg`, `ntc_disabled`, `light_load_shutdown`, `vin_pullout_boost`, and raw `reg00/sys01/sys07/read70/read71/read72/read77`.

3. Use the read-modify-write diagnostic helpers only for controlled experiments:
   - `ip5209 boost on|off` modifies only `SYS_CTL0[0x01]` bit 2.
   - `ip5209 light-load enable|disable` modifies only `SYS_CTL1[0x02]` bit 1.
   - `ip5209 ntc enable|disable` modifies only `SYS_CTL5[0x07]` bit 6, where `0` means NTC enabled and `1` means NTC disabled.
   - Each command prints old value, new value, write result, and readback value.

4. Probe IP5209 boost inductor `L2` with a scope if available:
   - One side should be battery/CSIN.
   - The other side should be the LX switching node.
   - If LX never switches, IP5209 is not enabling boost or is in protection.

5. Recheck NTC divider:
   - U7 pin 6 / NTC should be around 1.0 V when VREG is 3.1 V.
   - Datasheet thresholds indicate NTC below 0.5 V or above 1.5 V is outside normal charge temperature range.

6. Recheck KEY:
   - KEY should not be stuck low.
   - Pressing SW3 should pull KEY to GND.

7. Inspect/reflow:
   - U7 IP5209 pins 13/14/15 LX.
   - U7 pins 16/17 VOUT.
   - L2 1 uH inductor pads.
   - VOUT capacitors.
   - R9 10 milliohm current sense resistor.

## Main Question For Review

Given VBAT/CSIN are present at about 3.9 V and SW3 wakes VREG to about 3.1 V, why would IP5209 VOUT remain around 2.8 V instead of boosting to 5 V, with no apparent hard short on the +5 V rail?
