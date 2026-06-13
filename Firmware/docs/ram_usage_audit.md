# RAM Usage Audit

This audit is for the current integrated MVP image:

```text
Firmware\build\mvp_demo\northpole_ch592_bringup.elf
```

The numbers below were captured from the MounRiver RISC-V GCC tools after a
`Firmware\tools\build_mvp_demo.ps1` build.

## Summary

CH592 RAM budget:

```text
RAM size: 26 KB = 26624 bytes
Current used: 25580 bytes
Free by linker accounting: 1044 bytes
Static/linker usage: 96.08%
```

Section breakdown:

| Section | Bytes | Notes |
|---|---:|---|
| `.highcode` | 9160 | Executable code placed in RAM by WCH SDK/linker |
| `.data` | 1808 | Initialized global/static data copied into RAM |
| `.bss` | 14100 | Zero-initialized global/static data |
| `.stack` | 512 | Linker stack reservation |
| Total RAM accounted | 25580 | Leaves only 1044 bytes |

Important: the 96% number is not just global variables. About 9 KB is
RAM-resident executable code. Buffer reductions help, but production margin also
depends on what the linker places in `.highcode`.

Refresh commands:

```powershell
$toolbin = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin"
$elf = "Firmware\build\mvp_demo\northpole_ch592_bringup.elf"
& "$toolbin\riscv-none-embed-size.exe" -A $elf
& "$toolbin\riscv-none-embed-nm.exe" -S --size-sort --radix=d $elf |
    Where-Object { $_ -match ' [bBdD] ' } |
    Select-Object -Last 80
```

## Largest RAM Data Symbols

Top current `.bss` / `.data` consumers:

| Symbol | Bytes | Area | Meaning / owner |
|---|---:|---|---|
| `MEM_BUF` | 6144 | `.bss` | WCH BLE heap, declared in `APP/peripheral_main.c` |
| `tx_ring` | 2048 | `.bss` | USB CDC shell TX ring |
| `motor_wave_dma_a1` | 1024 | `.bss` | Motor A1 DMA table |
| `motor_wave_dma_a2` | 1024 | `.bss` | Motor A2 DMA table |
| `buffer.2595` | 512 | `.bss` | Local static buffer emitted by compiler; inspect map before changing |
| `__global_locale` | 364 | `.data` | C library locale data |
| `devInfoAttrTbl` | 304 | `.data` | BLE Device Information service attributes |
| `gBleLlPara` | 296 | `.bss` | WCH BLE link-layer parameters |
| `simpleProfileAttrTbl` | 272 | `.data` | WCH example SimpleProfile GATT attributes |
| `rx_ring` | 256 | `.bss` | USB CDC shell RX ring |
| `queue` | 208 | `.bss` | WT2003 audio command queue |
| `northpoleAttrTbl` | 208 | `.data` | NorthPole custom GATT attributes |
| `report` | 168 | `.bss` | Shell/reporting buffer |
| `gapAttrTbl` | 144 | `.data` | GAP service attributes |
| `gapParameters` | 132 | `.bss` | BLE GAP parameters |
| `demo` | 132 | `.bss` | Demo scene state |
| `ep1_buf` | 128 | `.bss` | USB CDC endpoint buffer |
| `line.4477` | 96 | `.bss` | Shell line/input buffer |
| `motion` | 76 | `.bss` | Motion controller state |
| `ep0_buf`, `ep4_buf`, `registered`, `string_desc` | 64 each | `.bss` | USB/BLE support buffers |

These names are useful because they point to concrete reduction candidates.

## Largest RAM-Resident Code Symbols

The `.highcode` section is `9160` bytes. Largest visible symbols inside that
range:

| Symbol | Bytes | Notes |
|---|---:|---|
| `USB_IRQHandler` | 886 | USB interrupt path, only useful when USB CDC is enabled |
| `FLASH_EEPROM_CMD` | 874 | Flash/EEPROM command helper |
| `ll_get_connect_anchorPoint` | 752 | BLE controller |
| `LL_HopGetChannel2` | 618 | BLE controller |
| `TMOS_SystemProcess` | 518 | WCH TMOS/BLE scheduler |
| `SetSysClock` | 448 | Clock init |
| `lleIrqHandler` | 406 | BLE low-level interrupt |
| `tmos_proces_system_time` | 354 | TMOS timing |
| `ll_tx_wait_finish` | 348 | BLE controller |
| `ll_connect_send_data` | 306 | BLE controller |
| `BB_IRQLibHandler` | 262 | BLE/RF interrupt |
| `tmos_memory_free` | 264 | BLE/TMOS heap |
| `tmos_memory_allocate` | 242 | BLE/TMOS heap |
| `ll_connect_recv_data` | 224 | BLE controller |
| `ll_connect_event_continue` | 202 | BLE controller |
| `TMR3_IRQHandler` | 138 | Motor wave control update ISR |

Most `.highcode` is WCH BLE/USB/flash support, not NorthPole application code.
That means a production build with no USB CDC shell and fewer debug paths can
save both RAM variables and RAM-resident code.

## Reduction Plan

### 1. Split Profiles Harder

Keep the current MVP profile for bench work, but create a separate production
profile with fewer features compiled in.

Candidate profiles:

| Profile | Purpose | USB CDC shell | Diagnostic commands | BLE | Demo |
|---|---|---:|---:|---:|---:|
| `bringup` | hardware debug | on | full | on | optional |
| `mvp-demo` | Rev-A demo bench image | on | selected | on | on |
| `production` | final minimal app | off by default | off | selected | on |
| `factory-test` | manufacturing / board test | on or UART | limited | optional | off |

Expected production savings:

- USB CDC data buffers: about `64 + 128 + 64 + 256 + 2048 + 64 = 2624 B`.
- Shell parser/report/line and command state: several hundred bytes more.
- USB-related `.highcode`, especially `USB_IRQHandler`, if fully excluded.

This is the highest-value reduction if production can be operated by BLE, fixed
touch behavior, and safe defaults.

### 2. Reduce USB CDC TX Ring In MVP

Current:

```c
#define CDC_TX_RING_SIZE 2048u
```

Potential experiments:

```text
2048 -> 1024: save 1024 B
2048 -> 512:  save 1536 B
```

Risk:

- Long diagnostic prints may truncate/block more often.
- MVP debug usability may degrade.

Recommendation:

- Try `1024` first in an MVP RAM experiment.
- For production, compile USB CDC out instead of tuning the ring.

### 3. Shrink Or Rework Motor DMA Tables

Current:

```text
motor_wave_dma_a1 = 1024 B
motor_wave_dma_a2 = 1024 B
```

These are probably `256` entries each as `uint32_t`.

Potential reductions:

- Use `uint16_t` DMA entries if the DMA peripheral can safely write 16-bit timer
  compare values: save about `1024 B`.
- Reduce production table entries if 128 positions are acceptable: save up to
  `1024 B`, but this may hurt slow-speed smoothness.
- Generate/update a smaller rolling DMA buffer instead of full-cycle tables:
  more CPU complexity, less RAM.
- Compile out old/coarse diagnostic wave backends from production while keeping
  one proven engine.

Recommendation:

- Keep 256 phase positions for motion quality.
- First investigate whether A1/A2 compare DMA entries can be `uint16_t`.

### 4. Remove WCH Example GATT Service From Production

The image still contains WCH example SimpleProfile data:

```text
simpleProfileAttrTbl      272 B
simpleProfileChar* names   90 B total
simpleProfile values       small
```

If the NorthPole custom GATT service is enough, compile out SimpleProfile in the
production profile.

Potential saving:

- A few hundred RAM bytes.
- Some flash/code simplification.

Risk:

- Existing nRF Connect test expectations may need updating.

### 5. Tune BLE Heap Only After Feature Split

Current:

```text
MEM_BUF = 6144 B
```

This is the largest single RAM block. It is tempting to shrink first, but it is
owned by the WCH BLE stack and failures may be subtle.

Plan:

1. Build production without USB CDC and debug shell first.
2. Confirm BLE advertising, connection, and GATT writes.
3. Then test smaller heap values, for example:

```text
6144 -> 5632
6144 -> 5120
6144 -> 4608
```

Acceptance:

- Advertises after reset.
- nRF Connect can connect/read/write.
- Demo can run while connected.
- No memory allocation failures in repeated connect/disconnect cycles.

### 6. Move Constants To Flash

Some `.data` symbols are mutable only because they are not declared `const` or
because SDK APIs expect RAM pointers. Audit:

- GATT static strings and descriptors.
- Shell help text.
- RGB scene names.
- Demo status labels.

Use `const` where SDK/API semantics allow it. Do not force `const` into places
where the WCH stack writes into the buffer.

Expected saving:

- Dozens to a few hundred bytes.

### 7. Remove Diagnostic State From Production

Production should not need:

- IP5209 full dump decode structures.
- Shell line parser and command table.
- Scope diagnostic commands.
- RGB walk/strobe/test patterns except one internal self-test if desired.
- Raw I2C read/write.
- Raw motor commands.
- Audio format/raw commands.
- `hall watch`, `touch watch`.

Expected saving:

- Mostly flash/code, but also some buffers and parser state.

### 8. Stack Margin

The linker reserves only `512 B` for `.stack` in the current accounting. That
does not prove runtime stack safety.

Plan:

- Add a stack watermark/fill check in bring-up/MVP builds.
- Record worst observed stack use during:
  - BLE connected.
  - Demo running.
  - RGB active.
  - Audio command queued.
  - USB shell printing status.
- Production target should leave a static RAM margin plus measured stack margin.

Suggested production target:

```text
Static/linker RAM <= 80% before stack watermark confidence.
Absolute static/linker RAM <= 21 KB on a 26 KB part.
At least 2 KB practical headroom after measured worst-case stack.
```

## Proposed RAM Experiment Matrix

Create these as build profiles or temporary build wrappers:

| Experiment | Expected saving | Must still pass |
|---|---:|---|
| MVP with CDC TX ring 1024 | ~1024 B | Shell usable, demo starts |
| MVP with CDC TX ring 512 | ~1536 B | Short commands usable |
| MVP with USB CDC shell off | ~2.6 KB+ | BLE advertises, demo auto/touch works |
| Production without SimpleProfile | ~300-500 B | NorthPole GATT works |
| A-DMA tables as uint16_t | ~1024 B | Scope A1/A2 waveform unchanged |
| BLE heap 5120 after USB off | ~1024 B | Repeated BLE connect/disconnect |

Do these one at a time. Do not combine reductions until each individual
failure mode is understood.

## Current Priority

For the next milestone, prioritize:

1. Keep MVP stable for the physical demo.
2. Add runtime-loop/latency counters so we know whether RGB/touch/audio issues
   are CPU starvation or logic bugs.
3. Start a separate production profile with USB CDC and diagnostics compiled
   out.
4. Only then shrink BLE heap or motor DMA tables.
