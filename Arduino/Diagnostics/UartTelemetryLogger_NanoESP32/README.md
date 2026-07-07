# UART Telemetry SD Logger — Arduino Nano ESP32

Records **read-only** FT85BD telemetry from both ESC halves to a **CSV file** on an **Adafruit microSD SPI breakout**. Intended for bench and cart test sessions while comparing ESC control modes (duty cycle, speed, current).

Based on the dual-UART layout from `UartTelemetry_NanoESP32` and `UartSpeedController_NanoESP32`. Sends **no** motor commands.

## Why this exists

Use this logger to capture voltage, RPM, duty cycle, current, and temperature while evaluating low-speed drive behavior — especially after switching to **duty-cycle control** (~400 ERPM) where speed mode tended to buzz/hum and current mode was unstable for differential drive with a rider.

## Wiring

### FT85BD UART (dual — same as speed controller)

| Nano ESP32 | Master COMM (101) | Slave COMM (102) |
|------------|-------------------|-------------------|
| D9 (TX) | → RX | — |
| D8 (RX) | ← TX | — |
| D5 (TX) | — | → RX |
| D4 (RX) | — | ← TX |
| GND | GND | GND |

### Adafruit microSD SPI breakout

| Breakout | Nano ESP32 |
|----------|------------|
| 3V3 | 3V3 |
| GND | GND |
| CLK | D13 (SCK) |
| DO (MISO) | D12 |
| DI (MOSI) | D11 |
| CS | D7 |

Use a **FAT32** microSD card (≤32 GB recommended). Format on a PC if the card is blank.

## ESC prerequisites

1. Flipsky ESC Tool: **UART** input @ **115200** on each half (written to flash, power-cycle).
2. Master ID **101**, slave ID **102**.
3. Battery on for COMM replies.

## Build and upload

```bash
cd Arduino/Diagnostics/UartTelemetryLogger_NanoESP32
pio run -t upload
pio device monitor
```

## Output

Each boot creates the next free file: `/tel_001.csv`, `/tel_002.csv`, …

CSV columns:

```text
millis_ms,esc_id,vbat_v,rpm,motor_a,input_a,duty,fet_c,motor_c,fault
```

Header comment lines (`#`) record session metadata. Files flush every 32 rows and every 3 s.

Serial monitor prints a short live view plus `rows=` / RX byte counts every 5 s.

## Typical workflow

1. Confirm basic UART with `../UartTelemetry_NanoESP32/`.
2. Set ESC control mode in Flipsky tool (e.g. duty cycle).
3. Flash this logger, run the test maneuver, power off.
4. Remove SD card and analyze CSV in Excel / Python / `Docs/TestLogs/`.

## Troubleshooting

| Symptom | Check |
|---------|--------|
| `SD init FAILED` | CS=D7, SPI wiring, FAT32 card, 3V3 to breakout |
| No UART rows | Same as `UartTelemetry_NanoESP32` — battery, TX/RX swap |
| `duty` always 0 | ESC may not report duty in telemetry for all modes; RPM/current still valid |
| Empty file after run | Wait for flush (power off after status line) or call longer test |

## Related

- `../UartTelemetry_NanoESP32/` — serial-only bring-up
- `../../UartSpeedController_NanoESP32/` — vehicle controller (PPM / UART drive)
