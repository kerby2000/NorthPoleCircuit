# CH592 SPI0 MOSI Pinmux Test

Purpose: determine whether CH592 SPI0 can emit a WS2812-compatible MOSI stream
on PA14 without commandeering PA12/SCS, PA13/SCK, or PA15/MISO.

## Build Guard

This test is disabled in normal firmware. Build with:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_SPI0_MOSI_PINMUX_TEST=1','APP_MOTOR_PWM_BACKEND_ENABLE=1') }"
```

Do not use this build as production firmware.

## Scope Connections

```text
CH1 = PA12, currently G2, possible SPI0 SCS
CH2 = PA14, currently WT2003 BUSY, proposed SPI0 MOSI
CH3 = PA13, currently G1, possible SPI0 SCK
CH4 = PA15, current WS2812 LED pin, possible SPI0 MISO
GND = local board GND
```

Keep DRV8837 `/SLEEP` low for all tests.

Important: PA14 is currently connected to WT2003 BUSY on Rev-A. Do not run
commands that drive PA14 unless contention with the WT2003 BUSY output is
prevented or accepted for this experiment.

## Commands

Baseline:

```text
spi0test status
spi0test gpio-baseline
```

MOSI-only candidate:

```text
spi0test speed 2400000
spi0test init-mosi-only
spi0test send-pattern aa 200
spi0test send-ws2812-zeros 4
spi0test send-ws2812-ones 4
spi0test send-ws2812-pattern aa 4
```

Default SPI comparison:

```text
spi0test init-default-spi
spi0test send-pattern aa 200
```

Pin availability checks:

```text
spi0test pwm-pa12 500 3
spi0test hall-pa13-read 3
spi0test all-safe
```

## First Hardware Run Checklist

Flash:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

After flashing, first run only non-driving checks:

```text
spi0test status
spi0test gpio-baseline
```

Then decide whether PA14 can be driven for this experiment. On Rev-A, PA14 is
connected to WT2003 `BUSY`, which is an output from the audio chip. Active
MOSI tests intentionally drive PA14, so run them only if BUSY contention is
prevented or accepted as a short diagnostic risk.

For the first MOSI-only scope pass:

```text
spi0test speed 2400000
spi0test init-mosi-only
spi0test send-pattern aa 1000
spi0test send-ws2812-pattern aa 16
spi0test all-safe
```

Scope decision:

- PASS candidate: CH2/PA14 shows data; CH1/PA12, CH3/PA13, CH4/PA15 stay quiet.
- FAIL-A: CH3/PA13 toggles as SCK.
- FAIL-B: CH1/PA12 toggles as SCS.
- FAIL-C: CH4/PA15 toggles or is forced away from GPIO input.

Only after MOSI-only is captured, run default SPI for comparison:

```text
spi0test init-default-spi
spi0test send-pattern aa 1000
spi0test all-safe
```

Default SPI is expected to reveal what the stock SPI group does; it is not the
preferred Rev-B mapping unless PA12/PA13/PA15 remain usable.

## Acceptance Criteria

MOSI-only is acceptable for Rev-B only if scope proves:

- PA14 shows the intended SPI/WS2812 data waveform.
- PA12 remains quiet and usable.
- PA13 remains quiet and usable.
- PA15 remains quiet and usable.

If `init-default-spi` shows PA13/SCK toggling while `init-mosi-only` does not,
the experiment proves why the custom register init is required.

## Current Result

Firmware-side setup is complete.

Build command used:

```powershell
powershell -ExecutionPolicy Bypass -Command "& { & 'Firmware\tools\build.ps1' -Profile bringup -ExtraDefine @('APP_SPI0_MOSI_PINMUX_TEST=1','APP_MOTOR_PWM_BACKEND_ENABLE=1') }"
```

Build result:

```text
BUILD_OK Firmware\build\bringup\northpole_ch592_bringup.hex
FLASH used: 210372 B / 448 KB
RAM used: 25236 B / 26 KB
```

Notes:

- The RAM margin is intentionally tight in this diagnostic build because it keeps the motor PWM backend and adds SPI0 test commands.
- Normal builds keep `APP_SPI0_MOSI_PINMUX_TEST=0`, so `spi0test` is not exposed.
- First hardware pass was run on the Rev-A target board with the WT2003 `BUSY`
  trace cut, so PA14 could be driven without fighting the WT2003 output.

## Hardware Evidence

Scope wiring for this pass:

```text
CH1 = PA12 / SPI0 SCS candidate / current PWM_G2
CH2 = PA14 / SPI0 MOSI candidate / current WT2003 BUSY trace
CH3 = PA13 / SPI0 SCK candidate / current PWM_G1
CH4 = PA15 / current WS2812 LED pin
```

All active tests kept DRV8837 `/SLEEP` low.

### MOSI-Only SPI, 2.4 MHz

Command sequence:

```text
spi0test speed 2400000
spi0test init-mosi-only
spi0test send-pattern aa 65535
spi0test all-safe
```

Evidence:

```text
Firmware/docs/spi0_mosi_pinmux_evidence/20260606_114208_mosi_only_2p4m_aa.png
```

Result:

- CH2/PA14 toggled with the expected data waveform.
- CH1/PA12 stayed quiet.
- CH3/PA13 stayed quiet.
- CH4/PA15 stayed quiet.

Classification: `PASS_CANDIDATE_MOSI_ONLY`.

### MOSI-Only WS2812 Encoding, ~3.16 MHz

Command sequence:

```text
spi0test speed 3200000
spi0test init-mosi-only
spi0test send-ws2812-pattern aa 32
spi0test all-safe
```

The CH592 divider gave an effective SPI rate of about 3.16 MHz. This is close
to the intended 4-bit WS2812 encoding rate.

Evidence:

```text
Firmware/docs/spi0_mosi_pinmux_evidence/20260606_114429_mosi_only_3p2m_ws2812_aa.png
```

Result:

- CH2/PA14 produced the WS2812-style pulse stream.
- CH1/PA12 stayed quiet.
- CH3/PA13 stayed quiet.
- CH4/PA15 stayed quiet.

Classification: `PASS_CANDIDATE_WS2812_ON_PA14`.

### Default SPI Comparison

Command sequence:

```text
spi0test speed 2400000
spi0test init-default-spi
spi0test send-pattern aa 65535
spi0test all-safe
```

Evidence:

```text
Firmware/docs/spi0_mosi_pinmux_evidence/20260606_114446_default_spi_2p4m_aa.png
```

Observed result:

- CH2/PA14 toggled.
- CH1/PA12, CH3/PA13, and CH4/PA15 stayed quiet in this capture.

This was kept only as a comparison capture. The preferred Rev-B implementation
should still use explicit MOSI-only register setup so PA12/PA13/PA15 are not
intentionally enabled as SPI outputs.

### PA12 PWM Availability

The first automated PA12 PWM capture missed the active window, so it was not
counted. A timed capture then armed the scope first, started `spi0test pwm-pa12`
for 8 seconds, and stopped the scope while the command was still active.

Command sequence:

```text
spi0test all-safe
spi0test pwm-pa12 500 8
spi0test all-safe
```

Evidence:

```text
Firmware/docs/spi0_mosi_pinmux_evidence/20260606_120232_pa12_pwm_timed_ch1.png
```

Result:

- CH1/PA12 showed a clean 20 kHz PWM waveform at roughly 50% duty.
- Firmware returned to `spi0test all-safe`.
- DRV8837 `/SLEEP` remained low.

Classification: `PA12_PWM4_USABLE`.

### PA13 Input Availability

Command:

```text
spi0test hall-pa13-read 2
```

Observed output:

```text
spi0test pa13 level=1 t_ms=0
```

No external Hall sensor transition was applied during this quick pass, so this
only proves firmware can reconfigure/read PA13 as an input after the SPI test.
It should still be verified with the final Rev-B Hall footprint.

## Rev-B Recommendation

The SPI0 MOSI-only experiment supports the proposed Rev-B remap:

```text
PA14 -> WS2812 LED data through a level shifter
PA15 -> WT2003 BUSY input
PB6  -> PWM_G1
PA12 -> PWM_G2
PA13 -> HALL1 or other input
```

Rationale:

- PA14 can generate both raw SPI data and WS2812-style encoded pulses.
- PA12 remained quiet during MOSI-only transfers and was separately proven as
  usable PWM4.
- PA13 remained quiet during MOSI-only transfers and was separately proven as
  readable input firmware-side.
- PA15 remained quiet during MOSI-only transfers, so moving WT2003 `BUSY` there
  is reasonable if the schematic/footprint route is clean.

Production firmware should not copy this diagnostic initialization literally.
For the real WS2812 driver, configure only PA14/MOSI for SPI output and leave
PA12/PA13/PA15 in their normal application modes.

The consolidated Rev-B hardware checklist is maintained in
[rev_b_change_plan.md](rev_b_change_plan.md).
