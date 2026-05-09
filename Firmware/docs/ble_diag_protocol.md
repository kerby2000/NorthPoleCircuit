# BLE Diagnostic Protocol

The bring-up firmware exposes a diagnostic-only GATT service. It is for board validation and safe diagnostics, not production control.

## UUIDs

16-bit UUIDs are advertised/used in the Bluetooth base UUID form:

```text
0000xxxx-0000-1000-8000-00805f9b34fb
```

| Item | UUID | Access | Meaning |
|---|---:|---|---|
| North Pole diagnostic service | `0xFD90` | service | Diagnostic service |
| Firmware version | `0xFD91` | read | ASCII string, e.g. `0.1.0-bringup` |
| Board revision | `0xFD92` | read | ASCII string, e.g. `north-pole-ble-audio-current-pcb` |
| Status packet | `0xFD93` | read | Binary status, little-endian fields |
| Counters packet | `0xFD94` | read | Hall/touch counters, little-endian fields |
| Safe control | `0xFD95` | write | Restricted safe commands only |
| Build profile | `0xFD96` | read | ASCII string, e.g. `bringup` or `production` |

## Status Packet `0xFD93`

All multi-byte values are little-endian.

| Byte offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 4 | `uptime_ms` | Firmware uptime in milliseconds |
| 4 | 4 | `fault_mask` | Sticky fault bitmask from `fault_snapshot()` |
| 8 | 1 | `battery_status` | Placeholder, currently `0xFF` for UNKNOWN |
| 9 | 1 | `ip5209_i2c_present` | `1` if the conservative IP5209 probe ACKed |
| 10 | 1 | `ip5209_int_level` | Raw IP5209 `/INT` input level |
| 11 | 1 | `audio_hw_state` | `0` HW_NOT_TESTED, `1` HW_BLOCKED, `2` UART_READY, `3` ERROR |
| 12 | 1 | `motor_armed` | `1` only during an active motor arm window |
| 13 | 1 | `brightness` | Current capped RGB brightness |
| 14 | 1 | `settings_valid` | Settings CRC/range validity |
| 15 | 1 | `ble_state` | `0` idle, `1` advertising, `2` connected, `3` error |

Battery interpretation, IP5209 register meanings, and audio hardware state are placeholders until first-board validation.

## Counters Packet `0xFD94`

| Byte offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 4 | `hall1_edges` | Hall 1 edge count |
| 4 | 4 | `hall2_edges` | Hall 2 edge count |
| 8 | 1 | `hall1_level` | Raw Hall 1 input level |
| 9 | 1 | `hall2_level` | Raw Hall 2 input level |
| 10 | 1 | `touch_mask` | Bitmask of currently pressed touch pads |
| 11 | 1 | `touch_event_placeholder` | Currently `0xFF` until touch event reporting is validated |

## Safe Writes `0xFD95`

No raw motor commands are exposed over BLE. No settings writes are exposed over BLE.

| Command | Payload | Effect |
|---:|---|---|
| `0x01` | `[0x01, r, g, b]` | Set all RGB LEDs to one capped test color |
| `0x02` | `[0x02]` | Clear sticky faults |
| `0x03` | `[0x03]` | Enqueue WT2003 stop |
| `0x04` | `[0x04, file_lsb, file_msb]` | Enqueue WT2003 external-flash root index playback |
| `0x05` | `[0x05, volume]` | Enqueue WT2003 volume, `0..31` only |
| `0x06` | `[0x06]` | Enqueue WT2003 pause/resume |
| `0x07` | `[0x07]` | Enqueue WT2003 query status |

RGB writes use the normal RGB driver, so the configured global brightness cap still applies. Audio writes only enqueue UART protocol commands; they do not prove WT2003 hardware behavior. Use the USB CDC shell and logic analyzer first. BLE does not support MP3 upload, raw WT2003 frames, external flash format, settings writes, or motor commands.

Example clear-faults payload:

```text
02
```

Example low red RGB payload:

```text
01 10 00 00
```

Example audio play-index payload for file `7`:

```text
04 07 00
```

Example audio volume payload for low bring-up volume `5`:

```text
05 05
```
