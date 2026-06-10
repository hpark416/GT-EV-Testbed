# EV Cart Drive System

### Georgia Tech ME 8803 – Special Problems (EV & Grid)

A custom dual-motor electric vehicle control system built around repurposed Segway/Ninebot hoverboard motors, a FLIPSKY FT85BD dual ESC, and an Arduino Mega.

This project serves as both a proof-of-concept EV platform and an educational testbed for motor control, vehicle dynamics, telemetry, battery systems, and embedded software development.

---

# Project Objectives

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

# Current Hardware

## Powertrain

- 2x Segway/Ninebot Hoverboard Hub Motors
- FLIPSKY FT85BD Dual ESC
- Stock Segway 15S Lithium-Ion Battery Pack
- Emergency Stop Switch
- XT30 Power Distribution Board
- Main Power Breaker

## Control System

- Arduino Mega 2560
- Radiolink RC Receiver
- Radiolink RC Transmitter

## Planned Electronics

- Arduino UNO R4 WiFi
- Bluetooth Telemetry Interface
- Mobile Dashboard
- Data Logging
- Battery Monitoring System

---

# Vehicle Specifications

## Battery

| Parameter | Value |
|------------|---------|
| Chemistry | Lithium-Ion |
| Configuration | 15S |
| Nominal Voltage | 54.8V |
| Approximate Capacity | ~4.3Ah |
| Energy | ~236 Wh |

## Motors

| Parameter | Value |
|------------|---------|
| Type | Hoverboard Hub Motor |
| Quantity | 2 |
| Sensor Type | Hall Sensors |
| Pole Pairs | 7 |

## Controller

| Parameter | Value |
|------------|---------|
| ESC | FLIPSKY FT85BD |
| Configuration | Dual Motor |
| Communication | PPM (Current) |
| Planned Upgrade | UART Telemetry |

---

# Current Software Features

## RC Control

- Differential drive
- Arcade-style steering
- Adjustable speed limits
- Independent forward/reverse limits
- Steering sensitivity tuning
- Input deadband filtering
- Exponential response curves
- Ramp rate limiting

## Vehicle Modes

### Drive Mode

Arcade-style steering:

```text
Forward + Steering

Outside Wheel -> Faster
Inside Wheel  -> Slower
```

### Pivot Mode

Zero-radius turning:

```text
Left Wheel  -> Forward
Right Wheel -> Reverse
```

Allows the vehicle to rotate around its center without translating.

---

# ESC Configuration

Current controller configuration:

```text
Control Mode:
Speed Reverse

Input Type:
PPM

Pulse Range:
1000µs - 1500µs - 2000µs

Motor Startup:
Hall

Hall Sensors:
Enabled
```

---

# Repository Structure

```text
/
├── Arduino/
│   ├── SpeedReverseController/
│   ├── CurrentReverseController/
│   └── Experimental/
│
├── Configs/
│   ├── FT85BD_Master/
│   └── FT85BD_Slave/
│
├── Docs/
│   ├── Wiring/
│   ├── Schematics/
│   ├── Images/
│   └── Notes/
│
└── README.md
```

---

# Current Development Status

## Completed

- Battery integration
- FT85BD configuration
- Hall sensor motor identification
- RC receiver integration
- Arduino PPM controller
- Differential drive control
- Pivot steering mode
- Emergency stop integration

## In Progress

- Low-speed control tuning
- Speed-reverse optimization
- Differential steering refinement
- Wheel synchronization improvements

## Planned

- UART communication with FT85BD
- ESC telemetry
- Battery monitoring
- Bluetooth dashboard
- Data logging
- Mobile application
- Closed-loop wheel speed control

---

# Lessons Learned

- Hoverboard motors are surprisingly capable low-cost EV drive units.
- Hall sensor startup significantly improves low-speed performance.
- Current control provides intuitive driving feel.
- Speed control provides improved wheel synchronization.
- PPM is useful for initial development, but UART is preferred for advanced control and telemetry.

---

# Future Development Roadmap

## Phase 1 – Drive System

- Stable differential drive
- Reliable low-speed operation
- Steering optimization
- Vehicle integration

## Phase 2 – Telemetry

- UART communication
- Battery voltage monitoring
- Current monitoring
- Motor RPM feedback
- Temperature monitoring

## Phase 3 – Connectivity

- Bluetooth communication
- Mobile dashboard
- Remote diagnostics
- Data logging

## Phase 4 – Advanced Controls

- Closed-loop speed matching
- Odometry
- Autonomous navigation experiments
- Vehicle dynamics research

---

# License

This repository is intended for educational, research, and prototyping purposes.

---

# Author

Hyungjun Park  
Georgia Institute of Technology  
Mechanical Engineering / Biomedical Engineering  
ME 8803 – Special Problems (EV & Grid)
