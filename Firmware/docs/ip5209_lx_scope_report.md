# IP5209 LX Scope Report

- Generated UTC: `2026-05-31T22:44:36+00:00`
- Board revision: `north-pole-ble-audio-current-pcb`
- Firmware image: `Firmware\build\bringup\northpole_ch592_bringup.hex`
- Battery voltage: `3.9`
- USB state: `disconnected`
- Scope IDN: `HANMATEK,DOS1104,24050102,V1.2.0->`

## Probe Connections

- CH1: L2 CSIN / battery side of boost inductor
- CH2: L2 LX side / IP5209 LX switching node
- CH3: +5V/VOUT when enabled
- CH4: VREG or KEY when enabled
- Scope ground: local board GND near U7/IP5209

Safety notes: use a short ground spring or very short ground lead if possible. Scope ground is earth-referenced; connect only to board GND. Do not connect scope ground to LX, VBAT, VOUT, or any non-ground node. Start with 1x probes.

## Voltage Scale Sanity

Earlier CH1/CH2 CSV files reported CH2 near 7.8 V while the scope screen and DMM indicated about 3.9 V on the LX node. Treat raw CSV voltages as suspect until the per-capture scale checks below agree with DMM references. The analyzer now flags `CSV_SCALE_SUSPECT` instead of silently treating doubled CSV values as real hardware voltage.

## Captures

### 20260531_225335_idle

- Captured UTC: `2026-05-31T20:53:43+00:00`
- Mode: `idle`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_225335_idle.png](ip5209_scope_evidence/20260531_225335_idle.png)
- Waveform CSV: [20260531_225335_idle_ch1_ch2.csv](ip5209_scope_evidence/20260531_225335_idle_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=0.1447 V, ratio=26.7`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 0.16 | 0.1299 | 0.32 | 0 | 0 |  |
| CH2 | LX | 1520 | -0.16 | 0.32 | 0.1447 | 0.48 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 0.1447 | 26.74 | `CSV_SCALE_SUSPECT` |

### 20260531_233153_idle

- Captured UTC: `2026-05-31T21:32:01+00:00`
- Mode: `idle`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_233153_idle.png](ip5209_scope_evidence/20260531_233153_idle.png)
- Waveform CSV: [20260531_233153_idle_ch1_ch2.csv](ip5209_scope_evidence/20260531_233153_idle_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.783 V, ratio=0.497`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 1.76 | 2.4 | 2.027 | 0.64 | 64 | 0 | 210 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.783 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.783 | 0.4972 | `CSV_SCALE_SUSPECT` |

### 20260531_233203_wake_single

- Captured UTC: `2026-05-31T21:32:50+00:00`
- Mode: `wake-single`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `STOP->`
- Screenshot: [20260531_233203_wake_single.png](ip5209_scope_evidence/20260531_233203_wake_single.png)
- Waveform CSV: [20260531_233203_wake_single_ch1_ch2.csv](ip5209_scope_evidence/20260531_233203_wake_single_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.774 V, ratio=0.498`
- Note: Falling-edge trigger did not show LX activity. Try a rising-edge capture at 4.5 V if needed.

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.613 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.774 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.774 | 0.4978 | `CSV_SCALE_SUSPECT` |

### 20260531_233345_steady_500nsdiv

- Captured UTC: `2026-05-31T21:34:15+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_233345_steady_500nsdiv.png](ip5209_scope_evidence/20260531_233345_steady_500nsdiv.png)
- Waveform CSV: [20260531_233345_steady_500nsdiv_ch1_ch2.csv](ip5209_scope_evidence/20260531_233345_steady_500nsdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.794 V, ratio=0.497`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 760 | 7.52 | 7.68 | 7.666 | 0.16 | 0 | 0 |  |
| CH2 | LX | 760 | 7.68 | 7.84 | 7.794 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.794 | 0.4965 | `CSV_SCALE_SUSPECT` |

### 20260531_233352_steady_1usdiv

- Captured UTC: `2026-05-31T21:34:15+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_233352_steady_1usdiv.png](ip5209_scope_evidence/20260531_233352_steady_1usdiv.png)
- Waveform CSV: [20260531_233352_steady_1usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260531_233352_steady_1usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.809 V, ratio=0.496`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 760 | 7.52 | 7.68 | 7.657 | 0.16 | 0 | 0 |  |
| CH2 | LX | 760 | 7.68 | 7.84 | 7.809 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.809 | 0.4956 | `CSV_SCALE_SUSPECT` |

### 20260531_233400_steady_2usdiv

- Captured UTC: `2026-05-31T21:34:15+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_233400_steady_2usdiv.png](ip5209_scope_evidence/20260531_233400_steady_2usdiv.png)
- Waveform CSV: [20260531_233400_steady_2usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260531_233400_steady_2usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.814 V, ratio=0.495`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 760 | 7.52 | 7.68 | 7.657 | 0.16 | 0 | 0 |  |
| CH2 | LX | 760 | 7.68 | 7.84 | 7.814 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.814 | 0.4953 | `CSV_SCALE_SUSPECT` |

### 20260531_233407_steady_5usdiv

- Captured UTC: `2026-05-31T21:34:15+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260531_233407_steady_5usdiv.png](ip5209_scope_evidence/20260531_233407_steady_5usdiv.png)
- Waveform CSV: [20260531_233407_steady_5usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260531_233407_steady_5usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.808 V, ratio=0.496`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 760 | 7.52 | 7.68 | 7.654 | 0.16 | 0 | 0 |  |
| CH2 | LX | 760 | 7.68 | 7.84 | 7.808 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.808 | 0.4956 | `CSV_SCALE_SUSPECT` |

### 20260601_000021_idle

- Captured UTC: `2026-05-31T22:00:29+00:00`
- Mode: `idle`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000021_idle.png](ip5209_scope_evidence/20260601_000021_idle.png)
- Waveform CSV: [20260601_000021_idle_ch1_ch2.csv](ip5209_scope_evidence/20260601_000021_idle_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000031_wake_single

- Captured UTC: `2026-05-31T22:00:57+00:00`
- Mode: `wake-single`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `READy->`
- Screenshot: [20260601_000031_wake_single.png](ip5209_scope_evidence/20260601_000031_wake_single.png)
- Waveform CSV: [20260601_000031_wake_single_ch1_ch2.csv](ip5209_scope_evidence/20260601_000031_wake_single_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`
- Note: Falling-edge trigger did not show LX activity. Try a rising-edge capture at 4.5 V if needed.

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000125_steady_500nsdiv

- Captured UTC: `2026-05-31T22:01:57+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000125_steady_500nsdiv.png](ip5209_scope_evidence/20260601_000125_steady_500nsdiv.png)
- Waveform CSV: [20260601_000125_steady_500nsdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000125_steady_500nsdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000133_steady_1usdiv

- Captured UTC: `2026-05-31T22:01:57+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000133_steady_1usdiv.png](ip5209_scope_evidence/20260601_000133_steady_1usdiv.png)
- Waveform CSV: [20260601_000133_steady_1usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000133_steady_1usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000141_steady_2usdiv

- Captured UTC: `2026-05-31T22:01:57+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000141_steady_2usdiv.png](ip5209_scope_evidence/20260601_000141_steady_2usdiv.png)
- Waveform CSV: [20260601_000141_steady_2usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000141_steady_2usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000149_steady_5usdiv

- Captured UTC: `2026-05-31T22:01:57+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000149_steady_5usdiv.png](ip5209_scope_evidence/20260601_000149_steady_5usdiv.png)
- Waveform CSV: [20260601_000149_steady_5usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000149_steady_5usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.77 V, ratio=0.498`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | 7.52 | 7.68 | 7.608 | 0.16 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.77 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.77 | 0.498 | `CSV_SCALE_SUSPECT` |

### 20260601_000228_idle

- Captured UTC: `2026-05-31T22:02:36+00:00`
- Mode: `idle`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000228_idle.png](ip5209_scope_evidence/20260601_000228_idle.png)
- Waveform CSV: [20260601_000228_idle_ch1_ch2.csv](ip5209_scope_evidence/20260601_000228_idle_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000238_wake_single

- Captured UTC: `2026-05-31T22:03:03+00:00`
- Mode: `wake-single`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `READy->`
- Screenshot: [20260601_000238_wake_single.png](ip5209_scope_evidence/20260601_000238_wake_single.png)
- Waveform CSV: [20260601_000238_wake_single_ch1_ch2.csv](ip5209_scope_evidence/20260601_000238_wake_single_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`
- Note: Falling-edge trigger did not show LX activity. Try a rising-edge capture at 4.5 V if needed.

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000325_steady_500nsdiv

- Captured UTC: `2026-05-31T22:03:56+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000325_steady_500nsdiv.png](ip5209_scope_evidence/20260601_000325_steady_500nsdiv.png)
- Waveform CSV: [20260601_000325_steady_500nsdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000325_steady_500nsdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000332_steady_1usdiv

- Captured UTC: `2026-05-31T22:03:56+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000332_steady_1usdiv.png](ip5209_scope_evidence/20260601_000332_steady_1usdiv.png)
- Waveform CSV: [20260601_000332_steady_1usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000332_steady_1usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000340_steady_2usdiv

- Captured UTC: `2026-05-31T22:03:56+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000340_steady_2usdiv.png](ip5209_scope_evidence/20260601_000340_steady_2usdiv.png)
- Waveform CSV: [20260601_000340_steady_2usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000340_steady_2usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000348_steady_5usdiv

- Captured UTC: `2026-05-31T22:03:56+00:00`
- Mode: `steady-switching`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `unknown`
- Screenshot: [20260601_000348_steady_5usdiv.png](ip5209_scope_evidence/20260601_000348_steady_5usdiv.png)
- Waveform CSV: [20260601_000348_steady_5usdiv_ch1_ch2.csv](ip5209_scope_evidence/20260601_000348_steady_5usdiv_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.751 V, ratio=0.499`

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 7.68 | 4.149 | 7.84 | 4 | 2 | 1.25e+07 |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.751 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.751 | 0.4993 | `CSV_SCALE_SUSPECT` |

### 20260601_000420_load_220R_wake_single

- Captured UTC: `2026-05-31T22:05:38+00:00`
- Mode: `wake-single`
- Classification: `NO_LX_SWITCHING_DETECTED`
- Trigger status: `AUTO->`
- Screenshot: [20260601_000420_load_220R_wake_single.png](ip5209_scope_evidence/20260601_000420_load_220R_wake_single.png)
- Waveform CSV: [20260601_000420_load_220R_wake_single_ch1_ch2.csv](ip5209_scope_evidence/20260601_000420_load_220R_wake_single_ch1_ch2.csv)
- Warning: `CSV_SCALE_SUSPECT CH2 LX: DMM=3.87 V, raw_mean=7.76 V, ratio=0.499`
- Note: Falling-edge trigger did not show LX activity. Try a rising-edge capture at 4.5 V if needed.

| Channel | Signal | Samples | Min V | Max V | Mean V | P-P V | Edges | Pulses | Est. Hz |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| CH1 | CSIN | 1520 | -0.16 | 0.16 | 0.1366 | 0.32 | 0 | 0 |  |
| CH2 | LX | 1520 | 7.68 | 7.84 | 7.76 | 0.16 | 0 | 0 |  |

| Scale Check | DMM V | Raw Mean V | DMM/CSV Ratio | Status |
|---|---:|---:|---:|---|
| CH2 LX | 3.87 | 7.76 | 0.4987 | `CSV_SCALE_SUSPECT` |


## Conclusion

LX did not switch after SW3 wake. IP5209 wakes VREG but does not enable boost switching. Focus next on KEY, NTC, RSET, LIGHT/VSET straps, IP5209 soldering, wrong/damaged chip, and register diagnostics under USB power. One or more captures have `CSV_SCALE_SUSPECT`; use the screenshot and DMM reference for absolute voltage until the scope CSV scaling is calibrated.
