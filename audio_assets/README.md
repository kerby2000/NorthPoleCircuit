# WT2003 Audio Assets

This folder tracks the intended WT2003 external-flash audio set. The files themselves are not committed by default.

Use short root-directory names such as `0001.mp3`. Keep the name before the extension to 8 characters or fewer. Filename playback uses the name without the extension, for example `audio play-name 0001`.

Index playback depends on copy order, not filename sort order. Use `pack_audio_assets.py` to create an ordered folder, then copy that folder's contents to the WT2003 USB drive through J4.

First hardware validation should use one low-volume file named `0001.mp3` only. Disconnect the WT2003 USB cable before UART playback; the chip may ignore serial commands while mounted as USB storage on a computer.
