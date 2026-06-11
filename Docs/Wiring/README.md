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

## FT85BD UART (COMM port)

```text
Mega TX1 (D18) -> FT85BD COMM RX
Mega RX1 (D19) -> FT85BD COMM TX
GND            -> GND

Baud rate: 115200 (default)
Protocol:  VESC UART packets
Master ID: 101 (direct serial)
Slave ID:  102 (CAN forward via master)
```

Use a bidirectional 3.3 V / 5 V level shifter between the Mega and FT85BD UART pins. Do not tie PPM and UART active control together at the same time — see README UART setup section.

## RC Receiver

Current intended assignment:

```text
CH2 -> throttle
CH4 -> steering
```

Add battery, ESC, E-stop, and power distribution wiring notes here.
