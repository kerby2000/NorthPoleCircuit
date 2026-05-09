# WT2003HX Protocol

Status: `HARDWARE_VALIDATION_PENDING`.

This project implements the WT2003HX UART protocol from the V2.00 datasheet, but the target board has not been tested yet.

## UART

| Setting | Value |
|---|---|
| Baud | 9600 default |
| Data | 8 bits |
| Parity | none |
| Stop | 1 bit |
| Level | 3.3 V TTL |

Wait 500 ms to 1 s after WT2003 power-up before sending commands. The firmware uses a conservative 1000 ms startup delay.

## Frame Format

```text
7E LEN CMD PARAM... CHECKSUM EF
```

The V2.00 examples define `LEN` as the number of bytes from `CMD` through the final `EF`, so:

```text
LEN = 1 command + N params + 1 checksum + 1 end
CHECKSUM = low byte of LEN + CMD + all params
```

Examples implemented in host tests:

| Operation | Frame |
|---|---|
| Stop | `7E 03 AB AE EF` |
| Pause | `7E 03 AA AD EF` |
| External flash index 1 | `7E 05 A0 00 01 A6 EF` |
| External flash filename `0001` | `7E 07 A1 30 30 30 31 69 EF` |
| Query version | `7E 03 C0 C3 EF` |
| Query volume | `7E 03 C1 C4 EF` |
| Query status | `7E 03 C2 C5 EF` |
| Volume 31 | `7E 04 AE 1F D1 EF` |

## Command Subset

The bring-up firmware exposes the postcard-relevant subset: query version, query volume, query status, query external flash count, query peripheral status, external-flash root index/name playback, stop, pause/resume, next, previous, volume 0-31, playback mode, output SPK/DAC, idle/deep sleep, and raw-frame send for controlled bring-up.

`DF` external-flash format is implemented only behind `APP_AUDIO_ALLOW_FORMAT_COMMAND=1` and shell confirmation:

```text
audio format-ext-flash CONFIRM
```

Do not enable that flag on production builds.

## Responses

The parser resynchronizes on `0x7E`, validates `0xEF`, validates checksum, rejects impossible lengths, and times out partial frames. It stores the last raw TX frame, last raw RX frame, command, first result/status byte, and error counters.

Basic write-command result codes:

| Code | Meaning |
|---:|---|
| `00` | command executed successfully |
| `01` | ambiguous datasheet wording; record raw context |
| `02` | file not found / specified file absent |
| `05` | device offline |

Query commands reuse the first parameter as data, not always as an error code. The firmware therefore preserves raw command and raw result/status bytes in `audio status`.

## USB Storage Caveat

When the WT2003 is connected to a computer as USB storage through J4, serial commands may not respond. After copying files, disconnect the WT2003 USB cable before UART playback.

Index playback depends on copy order, not filename sort order. Filename playback uses the name without extension, and names should be 8 bytes or fewer before `.mp3` or `.wav`.

## Hardware Checklist

1. Copy one low-volume file named `0001.mp3` to WT2003 external flash via J4.
2. Disconnect the WT2003 USB cable.
3. Power-cycle the board.
4. Wait at least 1 s after WT2003 power-up.
5. Run `audio version`.
6. Run `audio qperiph`.
7. Run `audio qcount-ext`.
8. Run `audio volume 5`.
9. Run `audio play-index 1`.
10. Check BUSY goes high while playing.
11. Run `audio stop`.
12. Check BUSY returns low.

Do not send serial commands while the WT2003 is mounted as PC USB storage, and do not assume file index order equals filename order.
