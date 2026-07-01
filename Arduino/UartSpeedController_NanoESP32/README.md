# UartSpeedController — Arduino Nano ESP32 (PlatformIO)

PlatformIO port of `../UartSpeedController/` for the **Arduino Nano ESP32**. Shares `VescUart.h` from the Mega sketch folder.

## Wiring (Nano ESP32)

| Signal | Nano pin | Connect to |
|--------|----------|------------|
| RC throttle (CH2) | D2 | Radiolink CH2 |
| RC steering (CH4) | D3 | Radiolink CH4 |
| FT85BD UART TX | D9 | FT85BD COMM **RX** |
| FT85BD UART RX | D8 | FT85BD COMM **TX** |
| Ground | GND | FT85BD COMM GND, RC receiver GND |

Nano ESP32 GPIO is **3.3 V** — same as FT85BD UART; a level shifter is usually **not** required (unlike the 5 V Mega).

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB cable to the Nano ESP32

## Build and upload

From this directory:

```bash
pio run
pio run -t upload
pio device monitor
```

Or open `Arduino/UartSpeedController_NanoESP32` as the PlatformIO project folder in VS Code/Cursor and use the **Upload** and **Monitor** tasks.

## Configuration

Edit `src/main.cpp`:

| Constant | Default | Purpose |
|----------|---------|---------|
| `ENABLE_UART_RPM_CONTROL` | `false` | `true` enables RC → ERPM drive |
| `MAX_FORWARD_ERPM` | `50000` | Forward speed cap |
| `MAX_REVERSE_ERPM` | `18000` | Reverse speed cap |
| `VESC_RX_PIN` / `VESC_TX_PIN` | D8 / D9 | UART pins (change if needed) |

FT85BD setup (UART app, baud 115200, IDs 101/102) is the same as documented in the root `README.md` UART section.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `Unknown board ID 'arduino_nano_esp32'` | In `platformio.ini`, set `platform = https://github.com/platformio/platform-espressif32.git#develop` |
| No serial monitor output | Select the Nano ESP32 USB port; monitor baud is **115200** |
| No FT85BD response | Swap D8/D9 if RX/TX reversed; confirm UART app enabled on ESC |
| Upload fails | Hold **BOOT** if needed; try lower `upload_speed` in `platformio.ini` |
