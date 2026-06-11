# Diagnostics

This folder contains sketches used to isolate and verify hardware behavior.

Recommended diagnostic order:

1. RC input pulse reader
2. PPM pass-through
3. Identical PPM output to both ESCs
4. Fixed pulse step test
5. One-motor ESC test
6. UART telemetry test (`../UartSpeedController/` — telemetry-only mode first)
