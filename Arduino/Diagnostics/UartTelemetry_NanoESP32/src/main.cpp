/*
 * UartTelemetry_NanoESP32
 *
 * Minimal FT85BD UART diagnostic for Arduino Nano ESP32.
 * Uses Flipsky FT ESC UART protocol (AA ... DD), not VESC packets.
 *
 * Dual-read modes (set READ_MODE below):
 *   CAN_FORWARD_VIA_MASTER — 3 wires to master COMM; slave polled via cmd 16
 *   DUAL_UART_PORTS      — Serial1->master, Serial2->slave (6 wires + GND)
 *
 * No RC inputs. No motor commands. Telemetry read only.
 */

#include <Arduino.h>
#include "FlipskyUart.h"

static const int MASTER_RX_PIN = D8;
static const int MASTER_TX_PIN = D9;
static const int SLAVE_RX_PIN = D4;
static const int SLAVE_TX_PIN = D5;
static const unsigned long ESC_BAUD = 115200;

static const uint8_t MASTER_ESC_ID = 101;
static const uint8_t SLAVE_ESC_ID = 102;

static const unsigned long POLL_INTERVAL_MS = 125;
static const unsigned long RESPONSE_WAIT_MS = 25;
static const unsigned long ALIVE_INTERVAL_MS = 1000;
static const unsigned long STATUS_INTERVAL_MS = 2000;

enum class ReadMode : uint8_t {
  // Wire D8/D9 to master COMM only. Alternate local + CAN-forward slave.
  CAN_FORWARD_VIA_MASTER,
  // Wire Serial1 to master COMM and Serial2 to slave COMM.
  DUAL_UART_PORTS,
};

// --- pick dual-read strategy ---
static const ReadMode READ_MODE = ReadMode::DUAL_UART_PORTS;

FlipskyUart masterEsc;
FlipskyUart slaveEsc;

unsigned long lastPollMs = 0;
unsigned long lastAliveMs = 0;
unsigned long lastStatusMs = 0;
bool pollSlaveNext = false;

void waitForUsbSerial() {
  const unsigned long deadline = millis() + 3000;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
}

void printPinMap() {
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    Serial.println(F("Wiring (cross TX/RX, common GND):"));
    Serial.println(F("  Master: D9->COMM RX   D8<-COMM TX"));
    Serial.println(F("  Slave:  D5->COMM RX   D4<-COMM TX"));
    Serial.println(F("  GND -> both ESC GND"));
    return;
  }

  Serial.print(F("Master UART: D"));
  Serial.print(MASTER_RX_PIN);
  Serial.print(F("/D"));
  Serial.println(MASTER_TX_PIN);
}

void printPollMode() {
  Serial.println(F("Protocol: Flipsky FT ESC (AA ... DD)"));
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    Serial.println(F("Mode: dual UART (master + slave COMM ports)"));
  } else {
    Serial.println(F("Mode: master UART + CAN-forward to slave 102"));
  }
}

void printTelemetry(const __FlashStringHelper *label, const FlipskyUart &esc) {
  Serial.print(label);
  Serial.print(F("  Vbat: "));
  Serial.print(esc.inputVoltage(), 2);
  Serial.print(F(" V   RPM: "));
  Serial.print(esc.erpm());
  Serial.print(F("   I: "));
  Serial.print(esc.motorCurrent(), 2);
  Serial.print(F(" A   Fault: "));
  Serial.print(esc.faultCode());
  Serial.print(F("   ID: "));
  Serial.println(esc.controllerId());
}

void printLastRawBytes(const FlipskyUart &esc, const __FlashStringHelper *tag) {
  const uint8_t n = esc.lastRawLen();
  if (n == 0) {
    return;
  }

  Serial.print(tag);
  Serial.print(F(" raw RX ("));
  Serial.print(n);
  Serial.print(F("): "));
  for (uint8_t i = 0; i < n; i++) {
    if (esc.lastRaw()[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(esc.lastRaw()[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void probeEsc(FlipskyUart &esc) {
  esc.clearLastRaw();
  esc.sendFwVersion();
  delay(100);
  esc.poll();
}

bool pollEsc(FlipskyUart &esc, uint8_t targetId) {
  esc.clearTelemetryFlag();
  esc.requestTelemetry(targetId);
  delay(RESPONSE_WAIT_MS);
  esc.poll();
  return esc.hasTelemetry();
}

void verifyHarness(FlipskyUart &esc, const __FlashStringHelper *label,
                   uint8_t expectedId) {
  Serial.print(label);
  Serial.print(F(" harness: "));

  if (!pollEsc(esc, 0)) {
    Serial.print(F("NO REPLY (RX bytes="));
    Serial.print(esc.rxBytes());
    Serial.println(F(") — check TX/RX swap, GND, battery, UART@115200"));
    return;
  }

  printTelemetry(label, esc);

  if (esc.controllerId() != expectedId) {
    Serial.print(F("  *** expected ID "));
    Serial.print(expectedId);
    Serial.print(F(", got "));
    Serial.print(esc.controllerId());
    Serial.println(F(" — wrong COMM port or ESC ID in tool ***"));
  }

  esc.clearTelemetryFlag();
}

void pollBothViaCanForward() {
  const uint8_t targetId = pollSlaveNext ? SLAVE_ESC_ID : 0;
  pollSlaveNext = !pollSlaveNext;

  if (pollEsc(masterEsc, targetId)) {
    printTelemetry(pollSlaveNext ? F("[Slave 102 CAN]") : F("[Master 101]"),
                  masterEsc);
    masterEsc.clearTelemetryFlag();
  }
}

void pollDualUart() {
  const bool gotMaster = pollEsc(masterEsc, 0);
  const bool gotSlave = pollEsc(slaveEsc, 0);

  if (gotMaster) {
    printTelemetry(F("[Master 101]"), masterEsc);
    masterEsc.clearTelemetryFlag();
  }
  if (gotSlave) {
    printTelemetry(F("[Slave 102]"), slaveEsc);
    slaveEsc.clearTelemetryFlag();
  }
}

void setup() {
  Serial.begin(115200);
  waitForUsbSerial();

  masterEsc.begin(Serial1, ESC_BAUD, MASTER_RX_PIN, MASTER_TX_PIN);
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    slaveEsc.begin(Serial2, ESC_BAUD, SLAVE_RX_PIN, SLAVE_TX_PIN);
  }

  Serial.println();
  Serial.println(F("=== FT85BD UART Telemetry Diagnostic ==="));
  Serial.println(F("Read-only — no motor commands sent"));
  printPollMode();
  printPinMap();
  Serial.println(F("@ 115200 baud"));
  Serial.println();

  probeEsc(masterEsc);
  Serial.print(F("Master FW: "));
  if (masterEsc.hasFwVersion()) {
    Serial.print(masterEsc.fwMajor());
    Serial.print('.');
    Serial.println(masterEsc.fwMinor());
  } else {
    Serial.println(F("no reply"));
  }

  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    probeEsc(slaveEsc);
    Serial.print(F("Slave FW: "));
    if (slaveEsc.hasFwVersion()) {
      Serial.print(slaveEsc.fwMajor());
      Serial.print('.');
      Serial.println(slaveEsc.fwMinor());
    } else {
      Serial.println(F("no reply"));
    }

    Serial.println();
    Serial.println(F("--- Harness check (one telemetry read each) ---"));
    verifyHarness(masterEsc, F("[Master 101]"), MASTER_ESC_ID);
    verifyHarness(slaveEsc, F("[Slave 102]"), SLAVE_ESC_ID);
  }

  Serial.println();
  Serial.println(F("Polling both ESCs..."));
  Serial.println();
}

void loop() {
  masterEsc.poll();
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    slaveEsc.poll();
  }

  const unsigned long now = millis();

  if (now - lastAliveMs >= ALIVE_INTERVAL_MS) {
    lastAliveMs = now;
    masterEsc.sendAlive();
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      slaveEsc.sendAlive();
    }
  }

  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    Serial.print(F("RX bytes  master="));
    Serial.print(masterEsc.rxBytes());
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      Serial.print(F("  slave="));
      Serial.print(slaveEsc.rxBytes());
    }
    Serial.println();
    printLastRawBytes(masterEsc, F("Master"));
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      printLastRawBytes(slaveEsc, F("Slave"));
    }
    Serial.println();
  }

  if (now - lastPollMs < POLL_INTERVAL_MS) {
    return;
  }
  lastPollMs = now;

  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    pollDualUart();
  } else {
    pollBothViaCanForward();
  }
}
