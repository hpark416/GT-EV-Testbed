# UART Telemetry Diagnostic — Arduino Nano ESP32

Minimal **read-only** bring-up test for the **Flipsky FT85BD** dual ESC over COMM UART.

- Uses **Flipsky FT ESC UART** (`AA … DD` frames, Modbus CRC16) via `../../UartSpeedController/FlipskyUart.h`
- **Not** VESC packets (`0x02 … 0x03`) — older `VescUart.h` does not work on FT85BD
- No RC input. No motor commands (`SET_RPM` is never sent)

## Bring-up findings (verified on hardware)

| Topic | Finding |
|-------|---------|
| Protocol | FT85BD speaks **Flipsky UART**, not VESC. Telemetry cmd **0**, FW cmd **17**, alive cmd **25**. |
| Baud | **115200** — must be set in Flipsky ESC Tool per half, written to flash, then power-cycle. |
| Battery | Pack must be on for COMM to respond (USB tool live data also needs battery). |
| Signal type | Input must be **UART** (not PPM) on each half in the tool. |
| Controller IDs | Master **101**, slave **102** — configure **each half separately** in the tool. |
| Dual read | **Dual UART** (separate COMM harness per half) is verified and preferred when both wires exist. |
| CAN forward | Alternate mode: 3 wires to master only, slave polled via UART cmd **16** + target ID — depends on internal CAN between halves. |
| Logic level | Nano ESP32 is **3.3 V**; direct UART to FT85BD COMM is typical. |
| Parasitic power | ESC TX idle-high can dimly power the Nano LED when USB is unplugged — use common GND, avoid back-power surprises. |
| Raw frames | Healthy traffic starts with `AA`; example: `AA 01 19 … DD` (alive), `AA 0C 11 01 06 …` (FW 1.6), telemetry replies follow. |

### Verified dual-UART results

With both harnesses wired and firmware flashed:

- Master on D8/D9: **ID 101**, ~56.7 V pack, FW **1.6**, fault 0 at idle
- Slave on D4/D5: **ID 102**, matching pack voltage, independent RX byte counters
- Boot prints per-port FW probe and one-shot harness check before continuous polling

## Read modes

Set `READ_MODE` at the top of `src/main.cpp`:

| Mode | Wiring | When to use |
|------|--------|-------------|
| `DUAL_UART_PORTS` **(default)** | 6 signal wires + GND — Serial1 → master, Serial2 → slave | Separate COMM harness per half (verified) |
| `CAN_FORWARD_VIA_MASTER` | 3 wires + GND — Serial1 → master only | Single harness; slave via cmd 16 forward |

Poll interval **125 ms** (~8 Hz per ESC in dual mode). Alive ping every **1 s**. RX stats every **2 s**.

## Wiring (dual UART — default)

Cross TX/RX on **each** harness. One common GND is enough.

| Nano ESP32 | Master COMM (ID 101) | Slave COMM (ID 102) |
|------------|----------------------|---------------------|
| D9 (TX) | → RX | — |
| D8 (RX) | ← TX | — |
| D5 (TX) | — | → RX |
| D4 (RX) | — | ← TX |
| GND | GND | GND |

### Wiring (CAN-forward mode)

| Nano ESP32 | Master COMM only |
|------------|------------------|
| D9 (TX) | → RX |
| D8 (RX) | ← TX |
| GND | GND |

Wheels off the ground for any accidental spin.

## ESC prerequisites

1. **Flipsky ESC Tool** — connect each half via USB, set input signal **UART**, baud **115200**, write config, power-cycle.
2. Master ID **101** on the master half; slave ID **102** on the slave half.
3. Battery on before expecting UART replies.

## Build and upload

From this directory (Windows — use full `pio` path if not on PATH):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

Linux/macOS:

```bash
pio run -t upload
pio device monitor
```

Serial monitor: **115200 baud**.

Shared header path is set in `platformio.ini`: `-I ../../UartSpeedController`

### PlatformIO / upload notes

| Issue | Fix |
|-------|-----|
| `No module named 'intelhex'` | `pip install intelhex` in `%USERPROFILE%\.platformio\penv\Scripts\pip.exe` |
| DFU upload `LIBUSB_ERROR_NOT_FOUND` (2341:0070) | Install **Arduino ESP32 Boards** USB drivers via Arduino IDE 2.x, or set `upload_protocol = esptool` in `platformio.ini` |
| `Unknown board ID 'arduino_nano_esp32'` | Use espressif32 `develop` platform — see comment in `platformio.ini` |

## Expected serial output

Boot (dual UART):

```text
=== FT85BD UART Telemetry Diagnostic ===
Mode: dual UART (master + slave COMM ports)
Wiring (cross TX/RX, common GND):
  Master: D9->COMM RX   D8<-COMM TX
  Slave:  D5->COMM RX   D4<-COMM TX
  GND -> both ESC GND

Master FW: 1.6
Slave FW: 1.6

--- Harness check (one telemetry read each) ---
[Master 101]  Vbat: 56.70 V   RPM: 0   I: 0.00 A   Fault: 0   ID: 101
[Slave 102]   Vbat: 56.68 V   RPM: 0   I: 0.00 A   Fault: 0   ID: 102

Polling both ESCs...
```

Continuous loop:

```text
RX bytes  master=1200  slave=1150
Master raw RX (16): AA 01 19 8A 7E DD AA 0C 11 01 06 ...
Slave raw RX (16): AA 01 19 8A 7E DD ...
[Master 101]  Vbat: 56.69 V   RPM: 0   I: 0.00 A   Fault: 0   ID: 101
[Slave 102]   Vbat: 56.70 V   RPM: 0   I: 0.00 A   Fault: 0   ID: 102
```

## Troubleshooting

| Symptom | Check |
|---------|--------|
| `NO REPLY (RX bytes=0)` | TX/RX swapped on that harness? GND? Battery on? UART @ 115200 written per half? |
| `Master FW: no reply` / `Slave FW: no reply` | Same as above; try swapping RX/TX for that port only |
| `expected ID 101, got 102` (or vice versa) | Harness plugged into wrong ESC half — swap master/slave wires |
| Only one side in log, old format `[Master COMM]` / single `UART RX bytes:` | Re-flash this project — stale firmware before dual-UART build |
| Raw bytes never start with `AA` | Wrong protocol or baud; confirm Flipsky framing, not VESC |
| Flipsky ESC Tool works over USB but Nano does not | Battery required; confirm UART input type on ESC, not PPM |

## Protocol reference

Flipsky UART framing (implemented in `FlipskyUart.h`):

| Item | Value |
|------|-------|
| Frame | `0xAA` + len + payload + CRC16 + `0xDD` |
| CRC | Modbus CRC16 |
| Telemetry | cmd **0** (`FTESC_UART_OBTAIN_DATA_ONCE`) |
| Firmware version | cmd **17** |
| Keepalive | cmd **25** |
| CAN forward | cmd **16** + target CAN ID (single-UART slave poll) |

Source reference in repo tooling: `ESCtool/Flipsky ESC Tool/.../ftesc_v1.4_1.5_1.6_uart_handle.c`

## Next steps

After both halves read voltage and IDs correctly:

1. Flash `../../UartSpeedController_NanoESP32/` with `ENABLE_UART_RPM_CONTROL = false` and confirm boot harness check.
2. Enable RPM control only after telemetry is stable (wheels off ground, low RPM limits).
