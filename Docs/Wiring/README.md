# Wiring Notes

## Arduino Mega

Current intended wiring:

```text
Radiolink CH2 signal -> Arduino Mega D2
Radiolink CH4 signal -> Arduino Mega D3

Arduino Mega D9  -> FT85BD Master PPM
Arduino Mega D10 -> FT85BD Slave PPM

Arduino Mega TX1 (D18) -> FT85BD COMM RX  (use 3.3 V level shifter)
Arduino Mega RX1 (D19) -> FT85BD COMM TX  (use 3.3 V level shifter)

All grounds common.
```

### Arduino Nano ESP32 (UartSpeedController_NanoESP32)

```text
Radiolink CH2 -> D2
Radiolink CH4 -> D3

Master UART: D9->COMM RX  D8<-COMM TX
Slave UART:  D5->COMM RX  D4<-COMM TX
GND -> both ESC COMM GND

PPM (optional): D6 master, D10 slave
```

### Arduino Nano ESP32 + Adafruit microSD (UartTelemetryLogger_NanoESP32)

Same dual UART as above, plus SPI microSD breakout:

```text
SD CS   -> D7
SD MOSI -> D11
SD MISO -> D12
SD SCK  -> D13
SD 3V3  -> 3V3
SD GND  -> GND
```

Nano ESP32 UART is 3.3 V (direct connection to FT85BD COMM is typical). Built and uploaded via PlatformIO.

## FT85BD UART (COMM port)

```text
Mega TX1 (D18) -> FT85BD COMM RX
Mega RX1 (D19) -> FT85BD COMM TX
GND            -> GND

Baud rate: 115200
Protocol:  Flipsky UART (AA ... DD) — not VESC packets
Master ID: 101
Slave ID:  102
```

Use a bidirectional 3.3 V / 5 V level shifter between the Mega and FT85BD UART pins. Do not tie PPM and UART active control together at the same time — see README UART setup section.

## RC Receiver

Current intended assignment:

```text
CH2 -> throttle
CH4 -> steering
```

Add battery, ESC, E-stop, and power distribution wiring notes here.
