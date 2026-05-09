# Host Tests

These tests exercise firmware-equivalent logic that does not need CH592 hardware.

Run from the repository root:

```powershell
python -m unittest discover Firmware\host_tests
```

The tests intentionally model pure logic only: shell tokenization, settings CRC/defaults/recovery, motor command validation, RGB brightness limiting, and audio timeout behavior.
