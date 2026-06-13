# IP5209 LX Scope Capture

These tools automate HANMATEK DOS1104 / OWON-compatible SCPI captures through the local InstrumentKit driver used by the Magnetic Levitation Chess hardware project.

## Safety

Use a short ground spring or very short ground lead if possible.
Scope ground is earth-referenced; connect only to board GND.
Do not connect scope ground to LX, VBAT, VOUT, or any non-ground node.
Start with 1x probes.

Expected signals:

- CH1 CSIN: DC around battery voltage, about 3.7-4.1 V.
- CH2 LX: switching node, may swing from near 0 V to around 5 V with ringing.
- CH3 optional: +5V/VOUT.
- CH4 optional: KEY or VREG.

## Connections

Minimum setup:

```text
CH1 = L2 CSIN / battery side of boost inductor
CH2 = L2 LX side / IP5209 LX switching node
Scope GND = local board GND near U7/IP5209
```

Optional 4-channel setup:

```text
CH3 = +5V/VOUT
CH4 = VREG
```

The workflow now assumes this 4-channel setup. CH4 can still be used for KEY with
`--ch4 key` when explicitly debugging button timing.

## Quick Scope Check

```powershell
python Firmware\tools\scope\ip5209_lx_capture.py --check-scope
```

The default InstrumentKit path is:

```text
C:\Users\lukin\Documents\VS code projects\InstrumentKit\src
```

Override it with `--instrumentkit-src` if needed.

## Capture Commands

```powershell
python Firmware\tools\scope\ip5209_lx_capture.py --mode idle
python Firmware\tools\scope\ip5209_lx_capture.py --mode wake-single --battery-voltage 3.9 --usb-state disconnected
python Firmware\tools\scope\ip5209_lx_capture.py --mode steady-switching --ch3 vout --ch4 vreg
python Firmware\tools\scope\ip5209_lx_capture.py --mode load-test --load-ohms 220 --ch3 vout --ch4 vreg
```

Add optional DMM references when the CSV voltage scale looks suspicious:

```powershell
python Firmware\tools\scope\ip5209_lx_capture.py --rewrite-report-only --ch2-dmm-v 3.87
python Firmware\tools\scope\ip5209_lx_capture.py --mode wake-single --ch3 vout --ch4 vreg --ch2-dmm-v 3.87 --ch3-dmm-v 2.8 --ch4-dmm-v 3.1
```

The report will flag `CSV_SCALE_SUSPECT` when raw waveform means disagree with
DMM references. The analyzer does not silently correct voltages; it reports the
ratio so the scope/driver scaling can be debugged.

Or run the guided workflow:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\scope\ip5209_capture_workflow.ps1 -BatteryVoltage 3.9 -UsbState disconnected
```

## Outputs

Evidence is written under:

```text
Firmware/docs/ip5209_scope_evidence/
```

The latest markdown report is written to:

```text
Firmware/docs/ip5209_lx_scope_report.md
```

The report is rebuilt from a local capture manifest so captures from separate `idle`, `wake-single`, `steady-switching`, and `load-test` commands remain visible together.

Generated CSV and image captures are ignored by Git by default. Keep selected evidence intentionally only when it is small and useful for a hardware decision.

## Motor Bridge PWM Capture

The current target-board `bringup` firmware enables the timer PWM backend by
default:

```text
APP_MOTOR_PWM_BACKEND_ENABLE=1
```

Build and flash the normal bring-up image before motor captures:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup
```

If the shell reports `motor wave-status ... backend=0`, the flashed HEX was
built with the old safe default and cannot generate wave motion.

Probe setup for bridge B:

```text
CH1 = B2 / /PWM_B2
CH2 = B1 / /PWM_B1
Scope GND = board GND
```

Use the same CH1=IN2, CH2=IN1 convention for the other bridges:

```text
Bridge A: CH1 = A2 / /PWM_A2, CH2 = A1 / /PWM_A1
Bridge G: CH1 = G2 / /PWM_G2, CH2 = G1 / /PWM_G1
```

Scope-only setup, leaving the scope running:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --mode setup-only
```

Capture a short B forward command through the USB CDC shell:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-forward --duty-permille 50 --duration-ms 300
```

The default acquisition mode is `run-stop`: the script starts the scope, starts
the motor command, stops the scope while the waveform is still active, then saves
the screenshot and waveform. This is more reliable on the DOS1104 than depending
on a single-trigger capture during early bring-up.

To capture and print CH592 PWMX register state while the command is active:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-forward --duty-permille 50 --duration-ms 1000 --pwm-debug --verbose-shell
```

Expected with PWM backend enabled:

- `single-forward`: CH2/B1 pulses at about 20 kHz, CH1/B2 low.
- `single-reverse`: CH1/B2 pulses at about 20 kHz, CH2/B1 low.

If you specifically want to debug the scope trigger path, force single-trigger
mode:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode single-forward --acquire-mode single-trigger --trigger-sweep SINGle --trigger-channel 2 --trigger-level 1.5 --duration-ms 1000
```

If the command is accepted but both traces remain flat, run the static input
diagnostic before debugging PWM routing:

```text
motor diag-inputs B forward 5000
motor diag-inputs B reverse 5000
motor diag-inputs B brake 5000
```

This command drives the DRV8837 input nets as static GPIO while the bridge is
wake-enabled. For bridge B, `forward` should make B1 high and B2 low; `reverse`
should make B2 high and B1 low; `brake` should make both high. Use this only
for deliberate bridge-output checks.

The same checks can be automated through the scope helper:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-forward --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-reverse --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode diag-brake --duration-ms 5000
```

To capture both directions in one longer sweep:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sequence-forward-reverse --duty-permille 50 --duration-ms 300
```

To generate a reference plot for the pseudo-sine diagnostic envelope:

```powershell
python Firmware\tools\scope\motor_sine_reference_plot.py --speed-hz 1 --amplitude-permille 50
```

This writes:

```text
Firmware/docs/motor_sine_reference/motor_sine_reference.png
Firmware/docs/motor_sine_reference/motor_sine_reference.md
```

For the full A/B phase check, use a 4-channel scope setup:

```text
CH1 = A1 / /PWM_A1 / DRV A IN1
CH2 = A2 / /PWM_A2 / DRV A IN2
CH3 = B1 / /PWM_B1 / DRV B IN1
CH4 = B2 / /PWM_B2 / DRV B IN2
Scope GND = board GND only
```

If the probe placement is easier around the PCB, this alternate physical order
is also supported by the script:

```text
CH1 = B2 / /PWM_B2 / DRV B IN2
CH2 = A2 / /PWM_A2 / DRV A IN2
CH3 = B1 / /PWM_B1 / DRV B IN1
CH4 = A1 / /PWM_A1 / DRV A IN1
```

Use `--channel-map ab-physical` for that order.

Do not connect the scope ground to a bridge output. If bridge outputs need to be
checked, probe them relative to board GND in a separate test.

For the current target-board physical hookup:

```text
CH1 = B2
CH2 = A2
CH3 = B1
CH4 = A1
```

Use the paired-overlay mode for reference-style screenshots:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-plot --sine-target AB --channel-map ab-physical --speed-hz 1 --amplitude-permille 1000 --duration-ms 1200 --acquire-mode single-trigger --trigger-channel 3 --trigger-slope rising --trigger-level 1.5 --trigger-sweep SINGle --scope-arm-mode single --scope-horizontal-offset-div 6 --scope-acquire-mode sample --scope-memory-depth 5000 --trigger-holdoff-ns 100 --pair-overlay --no-waveform --verbose-shell
```

The pair overlay intentionally places channels in two rows:

```text
CH1/CH3 B row = -3.5 div
CH2/CH4 A row = +0.5 div
```

This makes the scope screen look like the reference logic plot: one B row and
one A row, instead of four separate traces.

For continuous clock-divider style diagnostic captures, use
`sine-scope-clkdiv`. The logical shape is the same as `sine-scope-plot`, but it
is repeated for the full command duration so the scope can be stopped while the
waveform is still live:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 1024 --amplitude-permille 1000 --duration-ms 5000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --verbose-shell
```

In this diagnostic, `fast_clkdiv` maps directly to the per-sample slot width in
microseconds. One electrical cycle is always 32 slots. A useful sweep is:

```text
fast_clkdiv  slot_us  approx electrical frequency
1024         1024     30.5 Hz
512          512      61.0 Hz
256          256      122 Hz
128          128      244 Hz
64           64       488 Hz
32           32       977 Hz
16           16       1.95 kHz
8            8        3.91 kHz
4            4        7.81 kHz
2            2        15.6 kHz
1            1        31.25 kHz requested, but software GPIO overhead dominates
```

Values below about 4 us are diagnostic stress tests. They are useful for finding
the practical limit of the current bit-banged scope visualization, but they are
not yet a production timing guarantee.

## Hardware-Timed Motor Wave

After the GPIO visualization is proven, build and flash the hardware-timed
motor-wave image:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup -ExtraDefine APP_MOTOR_PWM_BACKEND_ENABLE=1
```

Flash:

```text
Firmware\build\bringup\northpole_ch592_bringup.hex
```

The new shell commands are:

```text
motor wave-start <electrical_hz_x1000> <amplitude_permille> [AB|A|B|G|all] [sleep0|sleep1]
motor wave-clkdiv <slot_us> <amplitude_permille> [AB|A|B|G|all] [sleep0|sleep1]
motor wave-dma-a <slot_us> <amplitude_permille> [sleep0|sleep1]
motor wave-dma-hybrid <slot_us> <amplitude_permille> [AB|A|all] [sleep0|sleep1]
motor wave-run <electrical_hz_x1000> <amplitude_permille> <ms> [AB|A|B|G|all] [sleep0|sleep1]
motor wave-status
motor wave-stop
```

This path uses hardware PWM for the bridge inputs and TMR3 only as a duty-update
scheduler:

```text
A1/A2 = TMR2/TMR1 PWM
B1/B2 = PWM9/PWM7 PWM
G1/G2 = PWM5/PWM4 PWM
TMR3  = sine-table sample scheduler
```

Do not confuse the two rates:

```text
20 kHz = PWM carrier frequency
electrical frequency = full A/B sine-table cycle frequency
sample update rate = electrical frequency * 32 samples
```

For example, `wave-clkdiv 256` means one sine-table sample every `256 us`, or
one full electrical cycle every `8.192 ms` (`122 Hz`). It is not a 20 kHz sine
wave. The 20 kHz signal is the carrier inside each duty slot.

Keep `/SLEEP` low for the first scope work:

```text
motor wave-clkdiv 10000 1000 AB sleep0
motor wave-status
motor wave-stop
```

Then compress the sample slot while watching the same physical probe hookup
(`CH1=B2`, `CH2=A2`, `CH3=B1`, `CH4=A1`):

```text
motor wave-clkdiv 1024 1000 AB sleep0
motor wave-stop
motor wave-clkdiv 256 1000 AB sleep0
motor wave-stop
```

At long timebase the duty envelope should match the 32-sample reference shape.
At short timebase each active input should be a 20 kHz PWM carrier, with duty
updated by TMR3. `wave-status` should report `running=1`, `sleep=0`, and an
increasing `ticks` count.

On 2026-06-05, `wave-clkdiv 64 1000 AB sleep0` still produced PWM activity on
the scope but starved the USB CDC shell before `wave-stop` could be acknowledged.
Treat values below `128 us` as stress tests only; the Python wrapper now rejects
them unless `--allow-wave-starvation-risk` is explicitly provided.

That starvation is not caused by continuous debug printing. At `64 us` slots,
TMR3 interrupts at about `15.6 kHz` and updates multiple PWM channels, leaving
too little service time for USB CDC, BLE/TMOS, and the main loop. DMA/offload
would help only if it removes those high-rate duty updates from the CPU. The
current CH59x SDK shows TMR1/TMR2 DMA support, which is promising for A1/A2, but
we have not yet proven an equivalent DMA path for the PWMX channels used by B/G.
No board rework is needed for the next firmware experiments.

First DMA experiment:

```text
motor wave-dma-a 400 1000 sleep0
motor wave-status
motor wave-stop
```

This uses TMR2 DMA for A1 and TMR1 DMA for A2. The firmware expands each
32-sample sine-table duty value into repeated 20 kHz PWM carrier entries, then
loops those buffers in timer DMA. `wave-status` should report `dma=1`,
`targets=A`, nonzero `entries`, and nonzero `repeat`. This intentionally does
not drive B/G yet because the local SDK has not shown a PWMX DMA feeder for
those channels. The full USB/BLE bring-up image keeps the first DMA buffer
small enough to fit CH592 RAM, so start with a 400 us slot. At the current 20 kHz
carrier this uses 8 repeats per sample and 256 DMA entries total.

B/G DMA mapping result:

```text
TMR1/TMR2 expose DMA registers and WCH SDK APIs.
PWMX channels PWM4/PWM5/PWM7/PWM9 do not expose DMA registers or SDK APIs on CH592.
```

So the current B/G mapping experiment is intentionally hybrid:

```text
motor wave-dma-hybrid 400 1000 AB sleep0
motor wave-status
motor wave-stop
```

Expected status:

```text
running=1 targets=AB sleep=0 ... dma=1 dma_supported=A dma_error=0
```

This means A1/A2 are fed by TMR2/TMR1 DMA, while B1/B2 and G1/G2 are still
updated by the sample scheduler because their PWMX block has no documented DMA
feeder. B/G-only targets are rejected by `wave-dma-hybrid`; use `wave-clkdiv`
for B/G-only scheduler tests.

Current hardware-timed scope capture examples:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode wave-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 1024 --amplitude-permille 1000 --duration-ms 1000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --pwm-debug --verbose-shell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode wave-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 256 --amplitude-permille 1000 --duration-ms 1000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --pwm-debug --verbose-shell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode wave-clkdiv --sine-target AB --channel-map ab-physical --display-channels 3 --fast-clkdiv 1024 --amplitude-permille 1000 --duration-ms 300 --stop-during-active-s 0.005 --timebase 20e-6 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --no-waveform --pwm-debug --verbose-shell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode wave-dma-a --channel-map ab-physical --display-channels 2,4 --fast-clkdiv 400 --amplitude-permille 1000 --duration-ms 1000 --wave-sleep sleep0 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --pwm-debug --verbose-shell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode wave-dma-hybrid --sine-target AB --channel-map ab-physical --fast-clkdiv 400 --amplitude-permille 1000 --duration-ms 1000 --wave-sleep sleep0 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --scope-memory-depth 5000 --pair-overlay --no-waveform --pwm-debug --verbose-shell
```

2026-06-05 first DMA test results:

- `motor wave-dma-a 400 1000 sleep0` passed with `rc=0`, `dma=1`,
  `entries=256`, and `repeat=8`.
- Overview screenshot:
  `Firmware/docs/motor_pwm_scope_evidence/20260605_151945_wave-dma-a_ab_physical_AB.png`.
- A1 carrier zoom screenshot:
  `Firmware/docs/motor_pwm_scope_evidence/20260605_152104_wave-dma-a_ab_physical_AB.png`.
- `motor wave-dma-a 800 1000 sleep0` rejected cleanly with `rc=-3`,
  `dma_error=3`, and `running=0`; this is expected with the RAM-safe 256-entry
  DMA buffers.
- `motor wave-dma-hybrid 400 1000 AB sleep0` passed with `rc=0`, `targets=AB`,
  `dma=1`, `dma_supported=A`, and `dma_error=0`. A1/A2 were DMA-driven while
  B1/B2 were scheduler-updated PWMX.
- `motor wave-dma-hybrid 400 1000 all sleep0` passed with `rc=0`,
  `targets=ABG`, `dma=1`, `dma_supported=A`, and `dma_error=0`. `pwm-debug`
  showed nonzero PWMX data on B/G channels during the run, proving the hybrid
  scheduler path exercises B and G while A remains DMA-driven.

The older `motor_sine_clkdiv_sweep.ps1` workflow below is for the software
scope-visualization commands, not for the hardware-timed `wave-clkdiv` engine.
To run that full divider progression as separate screenshots:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\scope\motor_sine_clkdiv_sweep.ps1 -Port COM19 -Dividers "1024,512,256,128,64,32,16,8,4,2,1"
```

The wrapper first probes the USB CDC shell with a short
`motor sine-scope-clkdiv 1024 1000 20 AB` command. If the board is still running
older firmware, it stops before configuring the scope and prints:

```text
Target firmware does not support 'motor sine-scope-clkdiv'.
Flash Firmware\build\bringup\northpole_ch592_bringup.hex and rerun this sweep.
```

Use `-SkipPreflight` only when you have already verified the target command by
hand and want to avoid opening the serial port before the scope capture.

For high-rate single-channel captures, reduce the displayed channels. For
example, CH3 is B1 in the current physical hookup and is the default trigger
reference:

```powershell
powershell -ExecutionPolicy Bypass -File Firmware\tools\scope\motor_sine_clkdiv_sweep.ps1 -Port COM19 -Dividers "16,8,4,2,1" -DisplayChannels "3"
```

The DOS1104 sample rate depends on timebase, memory depth, and displayed channel
count. Use the four-channel overview for phase/order. For high-rate timing, use
fewer channels:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-scope-clkdiv --sine-target AB --channel-map ab-physical --fast-clkdiv 8 --amplitude-permille 1000 --duration-ms 5000 --timebase 50e-6 --display-channels 3 --scope-memory-depth 20000 --acquire-mode run-stop --scope-horizontal-offset-div 0 --scope-acquire-mode sample --pair-overlay --no-waveform --verbose-shell
```

Do not infer pass/fail from the color of the sample-rate text on the scope.
Treat trigger status, visible waveform, numeric sample rate, timebase, and memory
depth as the evidence. Metadata readback is opt-in because deep-memory metadata
queries can wedge this DOS1104 USB protocol if used at the wrong time:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py ... --read-waveform-metadata --waveform-metadata-source screen
```

Use `--waveform-metadata-source deep` or `--waveform-source deep` only after a
basic screenshot capture is working and the scope USB link is healthy.

Manual shell command for the 4-channel A/B check:

```text
motor sine-demo 1 50 5000 AB
```

Expected behavior:

- A positive half-cycle: A1 has PWM duty, A2 is low.
- A negative half-cycle: A2 has PWM duty, A1 is low.
- B is the same split waveform shifted by 90 degrees, so B1/B2 transition a quarter electrical cycle from A1/A2.
- At `1 Hz`, the 32-sample table holds each step for about 31 ms.
- The waveform is not an analog sine. It is a 20 kHz PWM carrier with a stepped sine duty envelope.

For a non-energizing logic-only phase check, use:

```text
motor sine-diag-inputs 1 5000 AB
```

This drives the same A/B sign sequence as `sine-demo`, but keeps DRV8837
`/SLEEP` low. The bridge outputs should stay disabled. Expected scope behavior
with the 4-channel A/B hookup:

- A positive half-cycle: A1 high, A2 low.
- A negative half-cycle: A2 high, A1 low.
- B is the same binary sign waveform shifted by 90 degrees.
- At `1 Hz`, each table step is about 31 ms, and each half-cycle is about 500 ms.

For a 2-channel automated capture of one bridge or target pair, flash a PWM-enabled image and run:

```powershell
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-demo --driver A --sine-target A --speed-hz 1 --amplitude-permille 50 --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-demo --driver B --sine-target B --speed-hz 1 --amplitude-permille 50 --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-demo --sine-target AB --channel-map ab-physical --speed-hz 1 --amplitude-permille 50 --duration-ms 5000
python Firmware\tools\scope\motor_bridge_pwm_capture.py --port COM19 --mode sine-diag-inputs --sine-target AB --channel-map ab-physical --speed-hz 1 --duration-ms 5000
```

The `driver` channel map records CH1/CH2 only. The `ab-reference` and
`ab-physical` channel maps record CH1..CH4. Use the fixed `single-forward` and
`single-reverse` modes when you need to inspect individual 20 kHz PWM pulses.

Generated evidence is written under:

```text
Firmware/docs/motor_pwm_scope_evidence/
```
