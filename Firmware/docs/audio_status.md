# Audio Status

Status: `HARDWARE_VALIDATION_PENDING`.

The current KiCad audit does not block the WT2003 UART:

- U2 pad 13 `TXD1__13` reaches WT2003 pad 11 `RXD_11` through R12.
- U2 pad 14 `RXD1__14` reaches WT2003 pad 23 `TXD_23` through R13.
- WT2003 pad 20 `BUSY` reaches CH592 PA14 `/BUSY`.

Therefore `BOARD_AUTOGEN_AUDIO_UART_CONNECTED` is currently `1` and `BOARD_AUTOGEN_AUDIO_HW_BLOCKED` is currently `0`.

The firmware still does not fake a working audio device. `audio_wt2003.c` implements the WT2003HX V2.00 frame encoder/parser, a nonblocking command queue, timeout/error counters, and raw TX/RX diagnostics, but UART response behavior and BUSY timing require target-board validation.

Current bring-up behavior:

- UART1 is remapped to PB12/PB13 only when an audio command path opens the WT2003 backend.
- UART1 is configured for WT2003 default `9600 8N1`; USB CDC remains the host shell.
- The driver waits `APP_AUDIO_POWER_ON_DELAY_MS` before accepting commands.
- Commands are spaced by `APP_AUDIO_COMMAND_SPACING_MS`, currently 250 ms per datasheet guidance.
- `audio status` reports hardware validation state, UART-ready state, BUSY pin, last TX/RX frames, last command/result, timeout/error counters, software version if queried, peripheral status if queried, and external flash count if queried.
- BLE exposes only safe audio controls: play index, stop, volume, pause/resume, and query status. It does not upload MP3 files and exposes no motor commands.
- External flash formatting is disabled unless `APP_AUDIO_ALLOW_FORMAT_COMMAND=1` and `audio format-ext-flash CONFIRM` are both used.

Important caveats:

- WT2003 serial commands may not respond while the chip is mounted to a computer as USB storage through J4.
- Copy order controls index playback; filename order is not reliable for index playback.
- Filename playback uses the name without extension and names should be 8 bytes or fewer.
- Do not claim audio works until the first-board sequence in `wt2003_protocol.md` passes.

If a future board revision removes the R12/R13 path or changes the nets incorrectly, rerunning `tools/extract_kicad_pinmap.py` will regenerate the header and can mark audio as hardware blocked.
