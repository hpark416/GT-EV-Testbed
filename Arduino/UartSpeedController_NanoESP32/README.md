# UartSpeedController — Arduino Nano ESP32 (PlatformIO)

UART speed controller for the **FLIPSKY FT85BD** on the **GT-EV meka cart**, using the **Flipsky FT ESC UART protocol** (`AA … DD`). Replaces PPM with signed **current** commands (cmd 4) over dual COMM ports.

**Firmware version:** `1.1.0` (printed at boot on USB serial).

**Not** VESC packets — uses `../UartSpeedController/FlipskyUart.h`.

## What this program does

The Nano ESP32 sits between the RC receiver and two FT85BD halves (master **101**, slave **102**):

1. **Reads RC PWM** on D2 (throttle) and D3 (steering) via `pulseInLong` polling (reliable on ESP32-S3).
2. **Mixes steering** for a two-motor cart: at speed, slows the inside wheel only (does not boost the outside wheel — reduces spin-out).
3. **Sends UART cmd 4** (`SET_CURRENT`) with signed target current to each ESC every control loop (ESC mode: **Current Bidirectional**).
4. **Brakes at neutral** — when both sticks are centered, commands `0` A to both wheels.
5. **Polls telemetry** (voltage, RPM, current, fault) once per second on USB serial.
6. **Sends keepalive** (cmd 25) once per second to each ESC.

### Drive behavior (current firmware)

| Stick | Behavior |
|-------|----------|
| Forward | Both wheels drive forward (subject to steering mix) |
| Back | Both wheels reverse (capped lower than forward) |
| Neutral (throttle + steering centered) | **Brake** — `setCurrent(0)` on both ESCs |
| Steering while moving | Inside wheel slows; outside wheel keeps throttle |
| Steering at zero throttle | **No pivot** by default (`ENABLE_PIVOT_STEERING = false`) — pivot was disabled because it caused flip risk on the cart |
| RC signal lost | Immediate stop (`0` A) |

### Why UART instead of PPM

- Finer control (signed current commands, not ~400 µs PPM range).
- No minimum-drive “breakaway kick” that made low-speed PPM choppy.
- Software ramps and expo shape low-speed response smoothly.
- Neutral can command active zero-speed hold instead of ESC center-coast.

## Bring-up summary (from diagnostics)

| Topic | Setting |
|-------|---------|
| Protocol | Flipsky UART (`AA` + len + payload + CRC16 + `DD`) |
| Baud | **115200** per half, written in Flipsky ESC Tool, power-cycle |
| Input type | **UART** (not PPM) on each ESC half |
| IDs | Master **101**, slave **102** |
| Preferred wiring | **Dual UART** — separate COMM harness per half (verified) |
| Logic level | Nano ESP32 **3.3 V** — direct UART to FT85BD typical |

## Wiring (dual UART — default)

Cross TX/RX on each harness. One common GND.

| Nano ESP32 | Master COMM (101) | Slave COMM (102) |
|------------|-------------------|-------------------|
| D9 (TX) | → RX | — |
| D8 (RX) | ← TX | — |
| D5 (TX) | — | → RX |
| D4 (RX) | — | ← TX |
| D2 | RC throttle (CH2) | |
| D3 | RC steering (CH4) | |
| GND | GND | GND |

### Single-harness mode (`CAN_FORWARD_VIA_MASTER`)

Only D8/D9 to master COMM. Telemetry alternates local master + slave via cmd **16**. RPM control on slave uses CAN-forwarded cmd **39** — **dual UART is recommended for drive**.

## Flipsky ESC Tool — Control Setup

Configure **each ESC separately** (connect to master, set ID **101**, write; then slave, ID **102**, write). Battery on. Power-cycle after writing.

### Required settings

| Setting | Master (101) | Slave (102) | Notes |
|---------|--------------|-------------|-------|
| **Input Signal Type** | UART | UART | Must match firmware |
| **Control Mode** | **Current Bidirectional** | **Current Bidirectional** | Firmware sends cmd **4** (`SET_CURRENT`), not cmd 39 |
| **ESC ID** | **101** | **102** | Must match harness / firmware |
| **Baud rate** | 115200 | 115200 | Set on connection screen |

**Do not use** `Current Gear Reverse Brake` or other gear/throttle modes for this project — those expect pedal-like input patterns. This firmware already sends explicit RPM targets.

### Recommended settings (stability)

| Setting | Suggested | Why |
|---------|-----------|-----|
| **Throttle Response Level** | level 1 or 2 | Gentler ESC response; firmware already ramps |
| **Signal Ramp Time Pos / Neg** | 0.20 – 0.30 s | Light ESC-side smoothing; avoid stacking with very slow ramps |
| **Reverse Delay Time** | **0** (or minimal) | Firmware handles forward/reverse instantly |
| **Assisted Turnaround Mode** | **None** | Firmware handles steering; pivot disabled for flip safety |
| **Brake-Parking Mode** | OFF to start | Try ON only if neutral still coasts after `setSpeed(0)` |
| **Slow-Release Motor** | **OFF** | Prevents creeping/coasting at zero command |
| **Minimal Current at Zero Pedal** | defaults / low | Avoid holding motor current when command is zero |
| **Multi-ESC Sync Control** | **OFF** | Each half has its own UART; Nano mixes left/right |
| **Traction Control System** | OFF to start | Optional later — see tuning guide |

### Assisted Turnaround ESC ID Setup

Leave at defaults or ignore — not used. This cart uses **two** ESCs (101/102) on separate UART ports, not the four-corner assisted-turnaround layout.

### Motor direction (both ESCs)

Set rotation direction in **Flipsky ESC Tool → Motor Setup** on each half so that **positive RPM drives the wheel forward** on the cart. Keep `INVERT_MASTER` and `INVERT_SLAVE` **false** in firmware — then straight throttle shows matching signs, e.g. `MasterRPM:22000 SlaveRPM:22000`.

Only use firmware inversion if you cannot change direction in the tool.

### After writing settings

1. **Motor Setup** — invert motor direction on whichever half spins backward when given positive RPM (wheels-up test).
2. Power-cycle both ESCs.
3. Re-flash this firmware if needed.
4. Boot serial should show RC PWM probe + harness check with correct IDs and voltage.

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code / Cursor extension or CLI)
- Flipsky ESC Tool V1.6 (or compatible)
- RC receiver outputting **PWM** (not PPM/SBUS) on the wired channels

## Build and upload

```powershell
cd Arduino/UartSpeedController_NanoESP32
pio run -t upload
pio device monitor
```

Windows if `pio` not on PATH:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

Monitor at **115200** baud. Boot prints firmware version first:

```text
Firmware v1.1.0
=== GT-EV-Testbed UartSpeedController (Nano ESP32) ===
Drive: UART cmd 4 SET_CURRENT (ESC: Current Bidirectional)
```

Telemetry prints once per second:

```text
RC  Thr:1520us OK  Str:1500us OK  pwm Thr:412 Str:411
[Master 101]  Vbat:56.57V  RPM:0  I:0.00A  Fault:0  ID:101
[Slave 102]  Vbat:56.67V  RPM:0  I:0.00A  Fault:0  ID:102
Cmd Master:12.5A Slave:12.5A
```

## Firmware configuration (`src/main.cpp`)

### Feature flags

| Constant | Default | Purpose |
|----------|---------|---------|
| `ENABLE_UART_RPM_CONTROL` | `true` | `false` = telemetry only; `true` = RC drives motors |
| `READ_MODE` | `DUAL_UART_PORTS` | Dual COMM vs single UART + CAN forward |

### Speed / current limits

| Constant | Default | Purpose |
|----------|---------|---------|
| `MAX_FORWARD_CURRENT_A` | `40.0` | Forward current cap (amps) |
| `MAX_REVERSE_CURRENT_A` | `15.0` | Reverse current cap (amps) |

### Direction / inversion

| Constant | Default | Purpose |
|----------|---------|---------|
| `INVERT_THROTTLE` | `false` | `true` if forward stick commands reverse |
| `INVERT_STEERING` | `false` | Flip if steering is mirrored |
| `INVERT_MASTER` | `false` | Last resort — prefer motor direction in ESC Tool |
| `INVERT_SLAVE` | `false` | Last resort — prefer motor direction in ESC Tool |

### Throttle feel

| Constant | Default | Purpose |
|----------|---------|---------|
| `THROTTLE_EXPO` | `0.60` | Higher = softer low speed (0.45 aggressive, 0.70 very gentle) |
| `THROTTLE_RAMP_STEP` | `0.015` | Accel smoothing per loop (~5 ms) |
| `THROTTLE_BRAKE_RAMP_STEP` | `0.06` | Faster decel when backing off throttle |
| `RC_DEADBAND_US` | `60` | Stick center deadband (µs) |
| `CMD_ZERO_THRESHOLD` | `0.03` | Normalized threshold for neutral brake |

### Steering / stability

| Constant | Default | Purpose |
|----------|---------|---------|
| `SPEED_STEERING_GAIN` | `0.30` | Turn strength at speed (lower = less spin risk) |
| `MAX_TURN_REDUCTION` | `0.75` | Max inside-wheel slowdown (fraction of throttle) |
| `STEERING_EXPO` | `0.50` | Steering curve shaping |
| `STEERING_RAMP_STEP` | `0.020` | Steering input smoothing |
| `ENABLE_PIVOT_STEERING` | `false` | In-place pivot when throttle near zero — **flip risk** |
| `PIVOT_STEERING_GAIN` | `0.18` | Only if pivot enabled — keep low |

### Timing / telemetry

| Constant | Default | Purpose |
|----------|---------|---------|
| `TELEMETRY_INTERVAL_MS` | `1000` | USB serial report rate |
| `ALIVE_INTERVAL_MS` | `1000` | ESC keepalive (cmd 25) |
| `SIGNAL_TIMEOUT_MS` | `100` | RC lost → stop if no PWM update |

## Tuning guide

### Test order

1. **ESC Tool** — UART, Current Bidirectional, IDs 101/102, power-cycle.
2. **Telemetry only** — `ENABLE_UART_RPM_CONTROL = false`, confirm voltage + IDs on boot.
3. **Wheels up** — enable RPM control, verify forward / reverse / neutral brake.
4. **Wheels down, low limits** — keep `MAX_FORWARD_RPM` at 22000 or lower.
5. **Increase speed** — bump forward cap in steps (e.g. 22000 → 28000 → 35000) only when stable.

### If forward / reverse is wrong

- Stick reversed → toggle `INVERT_THROTTLE`.
- One wheel wrong direction → fix in **ESC Tool Motor Setup** first; only then toggle `INVERT_MASTER` / `INVERT_SLAVE`.

### If low speed is still jerky

- Raise `THROTTLE_EXPO` (try 0.65 – 0.70).
- Lower `THROTTLE_RAMP_STEP` (try 0.010).
- In ESC Tool, lower **Throttle Response Level** or reduce **Signal Ramp** slightly.
- Confirm **Slow-Release Motor** is OFF.

### If it spins out or feels tippy

- Lower `SPEED_STEERING_GAIN` (try 0.20 – 0.25).
- Lower `MAX_FORWARD_RPM`.
- Keep `ENABLE_PIVOT_STEERING = false`.
- Optionally enable **Traction Control** in ESC Tool with conservative `TC Erpm Diff` values.

### If neutral coasts instead of braking

- Firmware already sends `setSpeed(0)` — confirm sticks are truly centered (watch `RC` line in serial).
- In ESC Tool try **Brake-Parking Mode ON**.
- Increase **Brake Light Level** / brake current if the tool exposes it on your firmware version.

### If RC shows STALE / never

- Receiver must output **PWM** (~1000–2000 µs, ~50 Hz), not PPM/SBUS.
- Common GND between Nano and receiver.
- Boot **RC PWM probe** should show `10/10 PWM frames` per channel — move sticks during boot.
- Try swapping a signal wire to confirm receiver channel mapping (CH2 throttle, CH4 steering).

## Bring-up order

1. Flash and run `../Diagnostics/UartTelemetry_NanoESP32/` first — confirm voltage + IDs.
2. Configure both ESCs in Flipsky ESC Tool (see above).
3. Flash this project with `ENABLE_UART_RPM_CONTROL = false` — boot harness check should match.
4. Wheels off ground → `ENABLE_UART_RPM_CONTROL = true` → test direction and neutral brake.
5. Low-speed ground test → tune per guide → raise RPM caps gradually.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `Unknown board ID 'arduino_nano_esp32'` | Use espressif32 `develop` platform — see `platformio.ini` |
| `NO REPLY` on harness check | Swap RX/TX for that port; battery on; UART input in tool |
| Wrong ID on harness | Master/slave harness swapped; fix ESC ID in tool |
| No motor response with RPM enabled | Input Signal Type = UART; Control Mode = Current Bidirectional |
| Motor runs wrong direction | `INVERT_THROTTLE` / `INVERT_MASTER` / `INVERT_SLAVE` |
| RC STALE / never | PWM wiring, GND, channel mapping — see tuning guide |
| Stale VESC-based build | Rebuild — this project uses `FlipskyUart.h` only |

## Protocol reference

| Command | ID | Use |
|---------|-----|-----|
| Obtain data | 0 | Telemetry (voltage, RPM, current) |
| CAN forward | 16 | Poll slave on single UART |
| FW version | 17 | Boot probe |
| Keepalive | 25 | Sent every 1 s |
| Set speed | 39 | Not used by this firmware (use cmd 4) |
| Set current | 4 | Signed target current (RC drive) |

See `../../ESCtool/Flipsky ESC Tool/.../ftesc_v1.4_1.5_1.6_uart_handle.c` and `../Diagnostics/UartTelemetry_NanoESP32/README.md` for details.

## Development notes

| Date / phase | Work |
|--------------|------|
| v1.0.x | Dual UART telemetry + cmd 39 RPM drive |
| v1.2.0 | PPM drive on D6/D10 (proven path); optional UART cmd 32 gear+current |
| v1.1.0 | cmd 4 SET_CURRENT for Current Bidirectional ESC mode; firmware version in boot log |
| Initial | Dual UART telemetry + RPM control from `UartTelemetry_NanoESP32` bring-up |
| Telemetry | 1 Hz USB report; RC throttle/steering in each line |
| RC input | ESP32-S3: switched from ISR to `pulseInLong` polling; boot PWM probe |
| Drive tuning | Meka cart profile: brake-at-neutral, slow-inside steering, pivot off, lower RPM caps, throttle expo/ramps |
