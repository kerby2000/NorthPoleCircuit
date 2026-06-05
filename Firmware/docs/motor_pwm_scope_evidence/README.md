# Motor PWM Scope Evidence

Local captures from `Firmware/tools/scope/motor_bridge_pwm_capture.py` are written here.

Generated screenshots, CSV waveform dumps, and JSON-like evidence files are ignored by Git.
Keep only small selected evidence intentionally if it is needed for a hardware decision.

Probe setup used for bridge B validation:

```text
CH1 = B2 / /PWM_B2
CH2 = B1 / /PWM_B1
Scope GND = board GND
```

Use a short ground lead where practical. Start with low duty and short command windows.
