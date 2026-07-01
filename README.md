# GT-EV-Testbed

### Georgia Tech ME 8803 – Special Problems (EV & Grid)

A custom dual-motor electric vehicle control system built around repurposed Segway/Ninebot hoverboard motors, a FLIPSKY FT85BD dual ESC, and Arduino-based vehicle control.

This project serves as both a proof-of-concept EV platform and an educational testbed for motor control, vehicle dynamics, telemetry, battery systems, and embedded software development.

---

## Project Objectives

The goal of this project is to develop a low-cost, modular electric vehicle platform capable of:

- Differential drive control
- Zero-radius turning
- Radio control operation
- Telemetry collection
- Battery and power monitoring
- Expandable software architecture
- Future BLE/mobile dashboard integration
- Educational demonstrations and experimentation

---

## Current Hardware

### Powertrain

- 2x Segway/Ninebot hoverboard hub motors
- FLIPSKY FT85BD dual ESC
- Stock Segway 15S lithium-ion battery pack
- Emergency stop switch
- Power distribution board
- Main power breaker

### Control System

- Arduino Mega 2560
- Radiolink RC receiver
- Radiolink RC transmitter
- PPM control signal to FT85BD

### Planned Electronics

- Arduino UNO R4 WiFi
- UART telemetry interface
- Bluetooth/mobile dashboard
- Data logging
- Battery monitoring

---

## Vehicle Specifications

### Battery

| Parameter | Value |
|---|---:|
| Chemistry | Lithium-Ion |
| Configuration | 15S |
| Nominal Voltage | ~54.8V |
| Approximate Energy | ~236 Wh |

### Motors

| Parameter | Value |
|---|---:|
| Type | Hoverboard hub motor |
| Quantity | 2 |
| Sensor Type | Hall sensors |
| Pole Pairs | 7 |

### Controller

| Parameter | Value |
|---|---:|
| ESC | FLIPSKY FT85BD |
| Configuration | Dual motor |
| Current Control Method | PPM (primary) |
| UART Interface | Serial telemetry + optional ERPM control (`UartSpeedController`) |

---

## Current Software Features

- Radiolink RC input
- Interrupt-based RC pulse capture
- PPM output to FT85BD
- Differential drive
- Arcade-style steering
- Zero-radius pivot mode
- Adjustable forward speed limit
- Separately capped reverse speed
- Steering sensitivity tuning
- Input deadband filtering
- Exponential response shaping
- Ramp-rate limiting
- UART telemetry from FT85BD (`UartSpeedController`, optional ERPM drive)

---

## ESC Configuration

Current controller configuration target:

```text
Control Mode: Speed Reverse
Input Type: PPM
Pulse Range: 1000 us - 1500 us - 2000 us
Motor Startup: Hall
Hall Sensors: Enabled
Master ESC ID: 101
Slave ESC ID: 102
```

---

## Repository Structure

```text
/
├── Arduino/
│   ├── SpeedReverseController/
│   ├── CurrentReverseController/
│   ├── UartSpeedController/
│   ├── UartSpeedController_NanoESP32/   (PlatformIO, Arduino Nano ESP32)
│   ├── Diagnostics/
│   └── Experimental/
├── Configs/
│   ├── FT85BD_Master/
│   ├── FT85BD_Slave/
│   └── Control_Setups/
├── Docs/
│   ├── Wiring/
│   ├── Images/
│   ├── Notes/
│   └── TestLogs/
└── README.md
```

---

## UART Setup (FT85BD ↔ Arduino Mega)

The `Arduino/UartSpeedController/` sketch talks to the FT85BD over the VESC UART protocol at **115200 baud**. It polls telemetry from master ESC **101** (direct UART) and slave ESC **102** (CAN forward through the master).

### 1. Hardware wiring

Use **Arduino Mega Serial1** so USB `Serial` remains free for debug output:

```text
Arduino Mega TX1 (D18)  ->  FT85BD COMM RX
Arduino Mega RX1 (D19)  ->  FT85BD COMM TX
Arduino GND             ->  FT85BD COMM GND
```

Important:

- **Common ground** is required between the Mega and FT85BD.
- FT85BD UART is **3.3 V logic**. The Mega TX line is 5 V — use a **bidirectional level shifter** (recommended) or a resistor divider on the Mega TX → FT85BD RX path. Connect Mega RX to FT85BD TX through the level shifter as well.
- Do **not** connect the Mega to a powered ESC UART port until wiring is verified with a multimeter.
- Keep the **e-stop** and main breaker accessible during all UART bring-up tests.

See `Docs/Wiring/README.md` for the full pin map including PPM and RC wiring.

### 2. FT85BD / VESC Tool configuration

1. Connect the FT85BD to a PC via USB and open **VESC Tool** (or the FLIPSKY configuration app).
2. Confirm controller IDs:
   - Master (left): **101**
   - Slave (right): **102**
3. Under **App Settings → General**, set the app to **UART** (or UART + another input only if you understand the priority rules). For UART-only testing, disconnect PPM control wires to avoid conflicting inputs.
4. Under **App Settings → UART**, confirm baud rate **115200** (default).
5. Under motor settings, confirm **Speed** control mode matches your intent (Speed Reverse for this sketch).
6. Load and write the known-good XML configs from `Configs/FT85BD_Master/`, `Configs/FT85BD_Slave/`, and `Configs/Control_Setups/` as needed.
7. Power-cycle the ESC after writing configuration.

### 3. Upload firmware

**Arduino Mega (Arduino IDE):** open `Arduino/UartSpeedController/UartSpeedController.ino`.

**Arduino Nano ESP32 (PlatformIO):** open `Arduino/UartSpeedController_NanoESP32/` and run `pio run -t upload` (see that folder's `README.md`). Uses D8/D9 for FT85BD UART; 3.3 V logic — no level shifter needed.

For either board:

1. Leave `ENABLE_UART_RPM_CONTROL` set to **`false`** for the first test (telemetry only).
2. Upload the sketch.

### 4. Verify telemetry

1. Open the Serial Monitor at **115200 baud**.
2. With the ESC powered (wheels off the ground), you should see:
   - `FW version: x.xx` after startup (if the master responds)
   - Periodic lines: `ERPM`, `V` (input voltage), `I` (motor current), `Fault`, `ID`
3. Spin a motor by hand or apply a light command — ERPM and current should change.
4. Confirm both IDs **101** and **102** appear as telemetry alternates between master and slave.

If there is no response:

- Swap TX/RX if needed (they must cross).
- Recheck ground and level shifting.
- Confirm only one device drives the UART lines.
- Verify baud rate and that the UART app is enabled on the ESC.

### 5. Enable RC + ERPM control (optional)

Only after telemetry is stable:

1. Set `ENABLE_UART_RPM_CONTROL` to **`true`** and re-upload.
2. Confirm RC receiver wiring (CH2 → D2, CH4 → D3) matches `SpeedReverseController`.
3. Test with wheels **off the ground** and low ERPM limits (`MAX_FORWARD_ERPM`, `MAX_REVERSE_ERPM` in the sketch).
4. Tune `INVERT_MASTER` / `INVERT_SLAVE` so both wheels turn in the expected direction.
5. Verify RC failsafe: releasing signal should command **0 ERPM** on both motors.

### 6. Coexistence with PPM control

`SpeedReverseController` (PPM on D9/D10) and `UartSpeedController` (Serial1) can share the same RC input pins but should **not** drive the ESC simultaneously. Use one control method at a time:

| Mode | ESC input | Arduino sketch |
|------|-----------|----------------|
| PPM drive | PPM pins | `SpeedReverseController` |
| UART telemetry / drive | COMM UART | `UartSpeedController` |

---

## Development Status

### Completed

- Battery integration
- FT85BD bring-up
- Hall sensor motor identification
- RC receiver integration
- Arduino PPM controller
- Differential drive control
- Pivot steering mode
- Initial ESC configuration workflow
- UART telemetry sketch (`UartSpeedController`)

### In Progress

- UART ERPM drive tuning and validation
- Low-speed control tuning
- Speed Reverse optimization
- Differential steering refinement
- Wheel synchronization improvements
- Configuration management for master/slave ESC settings

### Planned

- Full UART replacement of PPM for vehicle drive
- ESC telemetry logging to SD / BLE bridge
- Battery voltage monitoring
- Current monitoring
- Motor RPM feedback
- Temperature monitoring
- Bluetooth dashboard
- Data logging
- Closed-loop wheel speed control

---

## Lessons Learned

- Hoverboard motors are capable low-cost EV drive units.
- Hall sensor startup significantly improves low-speed performance.
- Current Reverse mode provides intuitive torque-based behavior but can struggle with unloaded low-speed startup.
- Speed Reverse mode improves wheel synchronization but requires careful ERPM and PPM range tuning.
- PPM is useful for initial bring-up, but UART is preferred for advanced control and telemetry.
- Master/slave ESC IDs and XML configuration files must be carefully version-controlled.

---

## Author

Hyungjun Park  
Georgia Institute of Technology  
ME 8803 – Special Problems (EV & Grid)
