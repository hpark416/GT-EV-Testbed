# Experimental Arduino Sketches

This folder is for **unstable or one-off test sketches**. Code here should not be considered stable vehicle firmware.

## Guidelines

- Do **not** flash experimental code to the cart without reviewing safety interlocks (e-stop, power sequencing, and RC failsafe behavior).
- Prefer descriptive filenames or subfolders (e.g. `uart-telemetry-probe/`, `ppm-calibration/`).
- Document what each experiment tests and any hardware prerequisites in a short comment at the top of the source file.

When an experiment matures into a reliable control strategy, promote it to `SpeedReverseController/`, `CurrentReverseController/`, or a new top-level sketch folder.

## Current files

| File | Purpose |
|------|---------|
| `Updated_FT85BD_PPM_PivotSteering.cpp` | Pivot steering tuning experiment |
| `Updated_FT85BD_PPM_Breakaway_Assist.cpp` | Low-speed breakaway / startup assist experiment |
