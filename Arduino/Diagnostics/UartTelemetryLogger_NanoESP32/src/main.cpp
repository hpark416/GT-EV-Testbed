/*
 * UartTelemetryLogger_NanoESP32
 *
 * Read-only dual-UART FT85BD telemetry logger for Arduino Nano ESP32.
 * Writes CSV to Adafruit microSD SPI breakout for post-run analysis.
 *
 * Use while testing duty-cycle / speed / current control modes on the cart.
 * No RC input. No motor commands.
 *
 * UART (dual, same as UartSpeedController_NanoESP32):
 *   Serial1 master  D8 RX / D9 TX   ID 101
 *   Serial2 slave   D4 RX / D5 TX   ID 102
 *
 * microSD (Adafruit breakout, SPI):
 *   CS   -> D7
 *   MOSI -> D11
 *   MISO -> D12
 *   SCK  -> D13
 *   3V3, GND
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "FlipskyUart.h"

// =====================
// SD CARD (Adafruit microSD SPI breakout)
// =====================
static const int SD_CS_PIN = D7;
static const int SD_SCK_PIN = D13;
static const int SD_MISO_PIN = D12;
static const int SD_MOSI_PIN = D11;

// =====================
// FT85BD UART
// =====================
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
static const unsigned long STATUS_INTERVAL_MS = 5000;
static const unsigned long FLUSH_INTERVAL_MS = 3000;
static const uint16_t FLUSH_EVERY_ROWS = 32;

FlipskyUart masterEsc;
FlipskyUart slaveEsc;

File logFile;
char logPath[32] = "";
bool sdReady = false;
uint32_t rowsWritten = 0;
uint16_t rowsSinceFlush = 0;

unsigned long lastPollMs = 0;
unsigned long lastAliveMs = 0;
unsigned long lastStatusMs = 0;
unsigned long lastFlushMs = 0;
unsigned long sessionStartMs = 0;

// =====================
// HELPERS
// =====================
static void waitForUsbSerial() {
  const unsigned long deadline = millis() + 3000;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
}

static void pollEscPorts() {
  masterEsc.poll();
  slaveEsc.poll();
}

static bool pollEsc(FlipskyUart &esc) {
  esc.clearTelemetryFlag();
  esc.requestTelemetry(0);
  delay(RESPONSE_WAIT_MS);
  esc.poll();
  return esc.hasTelemetry();
}

static bool initSdCard() {
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    return false;
  }

  for (uint16_t n = 1; n < 1000; n++) {
    snprintf(logPath, sizeof(logPath), "/tel_%03u.csv", n);
    if (!SD.exists(logPath)) {
      return true;
    }
  }

  snprintf(logPath, sizeof(logPath), "/tel_ovf.csv");
  return true;
}

static bool openLogFile() {
  logFile = SD.open(logPath, FILE_WRITE);
  if (!logFile) {
    return false;
  }

  logFile.println(F("# GT-EV-Testbed UART telemetry log"));
  logFile.print(F("# session_start_ms,"));
  logFile.println(sessionStartMs);
  logFile.print(F("# master_id,"));
  logFile.print(MASTER_ESC_ID);
  logFile.print(F(",slave_id,"));
  logFile.println(SLAVE_ESC_ID);
  logFile.println(F("# poll_ms=125, dual_uart, read_only"));
  logFile.println(
      F("millis_ms,esc_id,vbat_v,rpm,motor_a,input_a,duty,fet_c,motor_c,fault"));
  logFile.flush();
  return true;
}

static void flushLogIfNeeded(bool force) {
  if (!sdReady || !logFile) {
    return;
  }

  const unsigned long now = millis();
  if (force || rowsSinceFlush >= FLUSH_EVERY_ROWS ||
      (now - lastFlushMs) >= FLUSH_INTERVAL_MS) {
    logFile.flush();
    rowsSinceFlush = 0;
    lastFlushMs = now;
  }
}

static void logSample(const FlipskyUart &esc) {
  if (!sdReady || !logFile) {
    return;
  }

  logFile.print(millis());
  logFile.print(',');
  logFile.print(esc.controllerId());
  logFile.print(',');
  logFile.print(esc.inputVoltage(), 3);
  logFile.print(',');
  logFile.print(esc.erpm());
  logFile.print(',');
  logFile.print(esc.motorCurrent(), 4);
  logFile.print(',');
  logFile.print(esc.inputCurrent(), 4);
  logFile.print(',');
  logFile.print(esc.dutyCycle(), 5);
  logFile.print(',');
  logFile.print(esc.tempFet(), 1);
  logFile.print(',');
  logFile.print(esc.tempMotor(), 1);
  logFile.print(',');
  logFile.println(esc.faultCode());

  rowsWritten++;
  rowsSinceFlush++;
  flushLogIfNeeded(false);
}

static void printSample(const __FlashStringHelper *label, const FlipskyUart &esc) {
  Serial.print(label);
  Serial.print(F(" V:"));
  Serial.print(esc.inputVoltage(), 2);
  Serial.print(F(" RPM:"));
  Serial.print(esc.erpm());
  Serial.print(F(" duty:"));
  Serial.print(esc.dutyCycle(), 4);
  Serial.print(F(" I:"));
  Serial.print(esc.motorCurrent(), 2);
  Serial.print(F(" fault:"));
  Serial.println(esc.faultCode());
}

static void probeEsc(FlipskyUart &esc, const __FlashStringHelper *label) {
  esc.sendFwVersion();
  delay(100);
  esc.poll();
  Serial.print(label);
  Serial.print(F(" FW: "));
  if (esc.hasFwVersion()) {
    Serial.print(esc.fwMajor());
    Serial.print('.');
    Serial.println(esc.fwMinor());
  } else {
    Serial.println(F("no reply"));
  }
}

static void pollAndLogBoth() {
  const bool gotMaster = pollEsc(masterEsc);
  const bool gotSlave = pollEsc(slaveEsc);

  if (gotMaster) {
    logSample(masterEsc);
    printSample(F("[M]"), masterEsc);
    masterEsc.clearTelemetryFlag();
  }
  if (gotSlave) {
    logSample(slaveEsc);
    printSample(F("[S]"), slaveEsc);
    slaveEsc.clearTelemetryFlag();
  }
}

// =====================
// ARDUINO
// =====================
void setup() {
  Serial.begin(115200);
  waitForUsbSerial();
  sessionStartMs = millis();

  masterEsc.begin(Serial1, ESC_BAUD, MASTER_RX_PIN, MASTER_TX_PIN);
  slaveEsc.begin(Serial2, ESC_BAUD, SLAVE_RX_PIN, SLAVE_TX_PIN);

  Serial.println();
  Serial.println(F("=== UART Telemetry SD Logger (Nano ESP32) ==="));
  Serial.println(F("Read-only | dual UART | Flipsky AA..DD protocol"));
  Serial.println(F("Master: D9->RX D8<-TX | Slave: D5->RX D4<-TX"));
  Serial.println(F("SD: CS=D7 MOSI=D11 MISO=D12 SCK=D13"));
  Serial.println();

  sdReady = initSdCard();
  if (!sdReady) {
    Serial.println(F("SD init FAILED — logging disabled"));
    Serial.println(F("Check wiring, FAT32 card, and 3V3 power to breakout"));
  } else if (!openLogFile()) {
    Serial.print(F("Could not open "));
    Serial.println(logPath);
    sdReady = false;
  } else {
    Serial.print(F("Logging to "));
    Serial.println(logPath);
  }

  probeEsc(masterEsc, F("Master"));
  probeEsc(slaveEsc, F("Slave"));
  Serial.println(F("Recording... (power cycle to start new file on next boot)"));
  Serial.println();
}

void loop() {
  pollEscPorts();

  const unsigned long now = millis();

  if (now - lastAliveMs >= ALIVE_INTERVAL_MS) {
    lastAliveMs = now;
    masterEsc.sendAlive();
    slaveEsc.sendAlive();
  }

  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    pollAndLogBoth();
  }

  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    Serial.print(F("rows="));
    Serial.print(rowsWritten);
    Serial.print(F("  RX master="));
    Serial.print(masterEsc.rxBytes());
    Serial.print(F(" slave="));
    Serial.println(slaveEsc.rxBytes());
    flushLogIfNeeded(true);
  }

  delay(2);
}
