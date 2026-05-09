# Pre-Hardware TODOs

These are not commit blockers. Keep them visible for first-board and post-bring-up work.

| Area | TODO |
|---|---|
| WT2003 response matching | `handle_frame()` currently clears the active command for any valid WT2003 frame. After hardware testing, match `frame->command` against `active_command.wt_command`; treat unrelated valid frames as unsolicited. |
| USB CDC | USB CDC remains `NEEDS_HARDWARE_TEST`. Validate enumeration on Windows and Linux before relying on it as the only diagnostic path. |
| WS2812 timing | Bit-bang timing is clock-sensitive and assumes 60 MHz. Verify T0H/T1H/period/reset timing with a logic analyzer, including BLE activity. |
| Reset audio stop | The reset command attempts `audio_wt2003_stop()`, but reset follows immediately. Treat motor/RGB safe state as the guaranteed behavior; do not rely on graceful audio shutdown until hardware proves it. |
