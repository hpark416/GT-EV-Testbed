# FT85BD Configuration Files

This folder stores **FLIPSKY FT85BD XML motor and control setup files** exported from the VESC Tool–compatible configuration workflow.

## Layout

| Folder | Contents |
|--------|----------|
| `FT85BD_Master/` | Master (left) motor setup XML |
| `FT85BD_Slave/` | Slave (right) motor setup XML |
| `Control_Setups/` | Shared control/input setup XML (PPM mode, calibration, ramp, reverse timing) |

The FT85BD dual ESC uses separate master/slave identities. Do not overwrite one side with the other.

## Controller IDs

Each ESC must have a **unique CAN/controller ID**. Do not duplicate IDs across master and slave.

| ESC | ID |
|-----|----|
| Master | **101** |
| Slave | **102** |

Always verify the connected ESC ID before writing setup.

## Suggested naming convention

```text
Master_MotorSetup_YYYYMMDD.xml
Slave_MotorSetup_YYYYMMDD.xml
Master_ControlSetup_YYYYMMDD.xml
Slave_ControlSetup_YYYYMMDD.xml
```

## File types

- **Motor setup** — motor identification values, current limits, power limits, ERPM limits, and battery settings.
- **Control setup** — input type, PPM mode, PPM calibration, deadband, ramp times, reverse timing, and ESC ID.

## Usage

1. Identify motors and complete hall-sensor detection on each ESC individually.
2. Export XML from the configured unit into the appropriate folder here.
3. Always save the working configuration before experimenting.
4. Commit configs when they represent a known-good, tested setup.
5. Document significant changes in `Docs/Notes/` and `Docs/TestLogs/`.
