/*
 * UartSpeedController — Arduino Nano ESP32 (PlatformIO)
 *
 * GT-EV-Testbed UART interface to the FLIPSKY FT85BD (VESC-protocol) dual ESC.
 * Logic matches Arduino/UartSpeedController (Mega); pins adapted for Nano ESP32.
 *
 * USB Serial  -> debug monitor
 * Serial1     -> FT85BD COMM UART (D8 RX, D9 TX)
 * D2 / D3     -> RC throttle / steering (CH2 / CH4)
 *
 * Set ENABLE_UART_RPM_CONTROL to false for telemetry-only bring-up.
 */

#include <Arduino.h>
#include "VescUart.h"

// =====================
// FEATURE FLAGS
// =====================
static const bool ENABLE_UART_RPM_CONTROL = false;

// =====================
// PINS (Arduino D-labels — work with either pin numbering mode)
// =====================
static const int THROTTLE_IN_PIN = D2;
static const int STEERING_IN_PIN = D3;
static const int VESC_RX_PIN = D8;   // connect to FT85BD TX
static const int VESC_TX_PIN = D9;   // connect to FT85BD RX

// =====================
// ESC / UART
// =====================
VescUart vesc;

static const unsigned long VESC_BAUD = 115200;
static const uint8_t MASTER_ESC_ID = 101;
static const uint8_t SLAVE_ESC_ID = 102;

static const int32_t MAX_FORWARD_ERPM = 50000;
static const int32_t MAX_REVERSE_ERPM = 18000;

// =====================
// RC TUNING
// =====================
static const bool INVERT_THROTTLE = true;
static const bool INVERT_STEERING = false;
static const bool INVERT_MASTER = false;
static const bool INVERT_SLAVE = true;

static const int RC_DEADBAND_US = 60;
static const float THROTTLE_EXPO = 0.45f;
static const float STEERING_EXPO = 0.45f;
static const float THROTTLE_RAMP_STEP = 0.025f;
static const float STEERING_RAMP_STEP = 0.025f;
static const float STEERING_GAIN = 0.40f;
static const float PIVOT_GAIN = 0.35f;
static const float PIVOT_EXPO = 0.75f;
static const float PIVOT_RAMP_STEP = 0.012f;
static const float MASTER_PIVOT_GAIN_TRIM = 1.00f;
static const float SLAVE_PIVOT_GAIN_TRIM = 1.00f;
static const unsigned long SIGNAL_TIMEOUT_MS = 100;

// =====================
// TIMING
// =====================
static const unsigned long TELEMETRY_INTERVAL_MS = 100;
static const unsigned long ALIVE_INTERVAL_MS = 500;
static const unsigned long PRINT_INTERVAL_MS = 250;

// =====================
// ISR STATE
// =====================
static portMUX_TYPE rcMux = portMUX_INITIALIZER_UNLOCKED;

static volatile unsigned long throttleRise = 0;
static volatile unsigned long steeringRise = 0;
static volatile int throttlePulse = 1500;
static volatile int steeringPulse = 1500;
static volatile unsigned long lastThrottleUpdate = 0;
static volatile unsigned long lastSteeringUpdate = 0;

static float throttleCurrent = 0;
static float steeringCurrent = 0;
static float pivotCurrent = 0;

static unsigned long lastTelemetryMs = 0;
static unsigned long lastAliveMs = 0;
static unsigned long lastPrintMs = 0;
static uint8_t telemetryTarget = 0;

// =====================
// ISR
// =====================
void IRAM_ATTR throttleISR() {
  if (digitalRead(THROTTLE_IN_PIN)) {
    throttleRise = micros();
  } else {
    const unsigned long width = micros() - throttleRise;
    if (width >= 900 && width <= 2100) {
      throttlePulse = (int)width;
      lastThrottleUpdate = millis();
    }
  }
}

void IRAM_ATTR steeringISR() {
  if (digitalRead(STEERING_IN_PIN)) {
    steeringRise = micros();
  } else {
    const unsigned long width = micros() - steeringRise;
    if (width >= 900 && width <= 2100) {
      steeringPulse = (int)width;
      lastSteeringUpdate = millis();
    }
  }
}

// =====================
// HELPERS
// =====================
static float pulseToNorm(int pulse, bool invert) {
  const int centered = pulse - 1500;
  if (abs(centered) < RC_DEADBAND_US) {
    return 0.0f;
  }
  float x = constrain(centered / 500.0f, -1.0f, 1.0f);
  if (invert) {
    x = -x;
  }
  return x;
}

static float applyExpo(float x, float expo) {
  return (1.0f - expo) * x + expo * x * x * x;
}

static float rampToward(float current, float target, float step) {
  if (current < target) {
    return min(current + step, target);
  }
  if (current > target) {
    return max(current - step, target);
  }
  return current;
}

static int32_t limitErpm(float cmd) {
  const float forwardLimit = (float)MAX_FORWARD_ERPM;
  const float reverseLimit = (float)MAX_REVERSE_ERPM;
  float erpm = cmd * forwardLimit;
  if (cmd < 0) {
    erpm = cmd * reverseLimit;
  }
  return (int32_t)erpm;
}

static void computeWheelCommands(float &masterCmd, float &slaveCmd) {
  if (abs(throttleCurrent) < 0.05f) {
    const float pivotTarget = applyExpo(steeringCurrent, PIVOT_EXPO) * PIVOT_GAIN;
    pivotCurrent = rampToward(pivotCurrent, pivotTarget, PIVOT_RAMP_STEP);
    masterCmd = pivotCurrent * MASTER_PIVOT_GAIN_TRIM;
    slaveCmd = -pivotCurrent * SLAVE_PIVOT_GAIN_TRIM;
  } else {
    pivotCurrent = 0;
    const float turn = steeringCurrent * STEERING_GAIN;
    masterCmd = throttleCurrent + turn;
    slaveCmd = throttleCurrent - turn;
    const float maxMag = max(abs(masterCmd), abs(slaveCmd));
    if (maxMag > 1.0f) {
      masterCmd /= maxMag;
      slaveCmd /= maxMag;
    }
  }

  if (INVERT_MASTER) {
    masterCmd = -masterCmd;
  }
  if (INVERT_SLAVE) {
    slaveCmd = -slaveCmd;
  }
}

static void sendStop() {
  vesc.setRpm(0, 0);
  vesc.setRpm(0, SLAVE_ESC_ID);
  throttleCurrent = 0;
  steeringCurrent = 0;
  pivotCurrent = 0;
}

static void requestNextTelemetry() {
  if (telemetryTarget == 0) {
    vesc.requestTelemetry(0);
  } else {
    vesc.requestTelemetry(SLAVE_ESC_ID);
  }
  telemetryTarget = 1 - telemetryTarget;
}

static void readRcSnapshot(int &thrUs, int &strUs, unsigned long &thrAge, unsigned long &strAge) {
  portENTER_CRITICAL(&rcMux);
  thrUs = throttlePulse;
  strUs = steeringPulse;
  const unsigned long now = millis();
  thrAge = now - lastThrottleUpdate;
  strAge = now - lastSteeringUpdate;
  portEXIT_CRITICAL(&rcMux);
}

// =====================
// ARDUINO
// =====================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_IN_PIN), throttleISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(STEERING_IN_PIN), steeringISR, CHANGE);

  vesc.begin(Serial1, VESC_BAUD, VESC_RX_PIN, VESC_TX_PIN);

  Serial.println(F("GT-EV-Testbed UartSpeedController (Nano ESP32)"));
  Serial.println(F("FT85BD UART on Serial1: D8=RX, D9=TX @ 115200"));
  Serial.print(F("RPM control: "));
  Serial.println(ENABLE_UART_RPM_CONTROL ? F("ENABLED") : F("DISABLED (telemetry only)"));

  delay(500);
  vesc.sendFwVersion();
  delay(100);

  if (vesc.hasFwVersion()) {
    Serial.print(F("FW version: "));
    Serial.print(vesc.fwMajor());
    Serial.print('.');
    Serial.println(vesc.fwMinor());
  } else {
    Serial.println(F("No FW response yet — check wiring and baud rate."));
  }

  Serial.println(F("Ready."));
}

void loop() {
  vesc.poll();

  const unsigned long now = millis();

  if (now - lastAliveMs >= ALIVE_INTERVAL_MS) {
    lastAliveMs = now;
    vesc.sendAlive();
  }

  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    requestNextTelemetry();
  }

  if (!ENABLE_UART_RPM_CONTROL) {
    if (now - lastPrintMs >= PRINT_INTERVAL_MS && vesc.hasTelemetry()) {
      lastPrintMs = now;
      vesc.clearTelemetryFlag();
      Serial.print(F("ERPM:"));
      Serial.print(vesc.erpm());
      Serial.print(F(" V:"));
      Serial.print(vesc.inputVoltage(), 1);
      Serial.print(F(" I:"));
      Serial.print(vesc.motorCurrent(), 2);
      Serial.print(F(" Fault:"));
      Serial.print(vesc.faultCode());
      Serial.print(F(" ID:"));
      Serial.println(vesc.controllerId());
    }
    delay(5);
    return;
  }

  int thrUs = 0;
  int strUs = 0;
  unsigned long thrAge = 0;
  unsigned long strAge = 0;
  readRcSnapshot(thrUs, strUs, thrAge, strAge);

  if (thrAge > SIGNAL_TIMEOUT_MS || strAge > SIGNAL_TIMEOUT_MS) {
    sendStop();
    if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
      lastPrintMs = now;
      Serial.println(F("RC SIGNAL LOST -> ERPM 0"));
    }
    delay(10);
    return;
  }

  const float throttle = applyExpo(pulseToNorm(thrUs, INVERT_THROTTLE), THROTTLE_EXPO);
  const float steering = applyExpo(pulseToNorm(strUs, INVERT_STEERING), STEERING_EXPO);
  throttleCurrent = rampToward(throttleCurrent, throttle, THROTTLE_RAMP_STEP);
  steeringCurrent = rampToward(steeringCurrent, steering, STEERING_RAMP_STEP);

  float masterCmd = 0;
  float slaveCmd = 0;
  computeWheelCommands(masterCmd, slaveCmd);

  const int32_t masterErpm = limitErpm(masterCmd);
  const int32_t slaveErpm = limitErpm(slaveCmd);

  vesc.setRpm(masterErpm, 0);
  vesc.setRpm(slaveErpm, SLAVE_ESC_ID);

  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    Serial.print(F("MasterERPM:"));
    Serial.print(masterErpm);
    Serial.print(F(" SlaveERPM:"));
    Serial.println(slaveErpm);
  }

  delay(5);
}
