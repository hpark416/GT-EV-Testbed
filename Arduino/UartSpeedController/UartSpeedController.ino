/*
 * UartSpeedController
 *
 * GT-EV-Testbed UART interface to the FLIPSKY FT85BD (VESC-protocol) dual ESC.
 *
 * - Telemetry: ERPM, battery voltage, motor current, fault code (master + slave)
 * - Optional RC-driven ERPM control (differential drive + pivot), mirroring
 *   SpeedReverseController mixing logic
 *
 * Hardware: Arduino Mega Serial1 (TX=18, RX=19) -> FT85BD COMM UART port
 * Master ESC ID 101 (direct UART), Slave ESC ID 102 (CAN forward via master)
 *
 * Set ENABLE_UART_RPM_CONTROL to false for telemetry-only bring-up.
 */

#include "VescUart.h"

// =====================
// FEATURE FLAGS
// =====================
const bool ENABLE_UART_RPM_CONTROL = false;  // Start false; enable after telemetry checks

// =====================
// ESC / UART
// =====================
VescUart vesc;

const unsigned long VESC_BAUD = 115200;
const uint8_t MASTER_ESC_ID = 101;
const uint8_t SLAVE_ESC_ID  = 102;

const int32_t MAX_FORWARD_ERPM = 50000;   // ~50% of configured max (100k ERPM)
const int32_t MAX_REVERSE_ERPM = 18000;   // ~18% reverse cap

// =====================
// RC INPUTS (same as SpeedReverseController)
// =====================
const int THROTTLE_IN_PIN = 2;
const int STEERING_IN_PIN = 3;

const bool INVERT_THROTTLE = true;
const bool INVERT_STEERING = false;
const bool INVERT_MASTER   = false;
const bool INVERT_SLAVE    = true;   // Slave ERPM sign often opposite of master

const int RC_DEADBAND_US = 60;
const float THROTTLE_EXPO = 0.45;
const float STEERING_EXPO = 0.45;
const float THROTTLE_RAMP_STEP = 0.025;
const float STEERING_RAMP_STEP = 0.025;
const float STEERING_GAIN = 0.40;
const float PIVOT_GAIN = 0.35;
const float PIVOT_EXPO = 0.75;
const float PIVOT_RAMP_STEP = 0.012;
const float MASTER_PIVOT_GAIN_TRIM = 1.00;
const float SLAVE_PIVOT_GAIN_TRIM  = 1.00;
const unsigned long SIGNAL_TIMEOUT_MS = 100;

// =====================
// TIMING
// =====================
const unsigned long TELEMETRY_INTERVAL_MS = 100;
const unsigned long ALIVE_INTERVAL_MS = 500;
const unsigned long PRINT_INTERVAL_MS = 250;

// =====================
// ISR STATE
// =====================
volatile unsigned long throttleRise = 0;
volatile unsigned long steeringRise = 0;
volatile int throttlePulse = 1500;
volatile int steeringPulse = 1500;
volatile unsigned long lastThrottleUpdate = 0;
volatile unsigned long lastSteeringUpdate = 0;

float throttleCurrent = 0;
float steeringCurrent = 0;
float pivotCurrent = 0;

unsigned long lastTelemetryMs = 0;
unsigned long lastAliveMs = 0;
unsigned long lastPrintMs = 0;
uint8_t telemetryTarget = 0;  // 0=master, 1=slave

// =====================
// ISR
// =====================
void throttleISR() {
  if (digitalRead(THROTTLE_IN_PIN)) {
    throttleRise = micros();
  } else {
    unsigned long width = micros() - throttleRise;
    if (width >= 900 && width <= 2100) {
      throttlePulse = width;
      lastThrottleUpdate = millis();
    }
  }
}

void steeringISR() {
  if (digitalRead(STEERING_IN_PIN)) {
    steeringRise = micros();
  } else {
    unsigned long width = micros() - steeringRise;
    if (width >= 900 && width <= 2100) {
      steeringPulse = width;
      lastSteeringUpdate = millis();
    }
  }
}

// =====================
// HELPERS
// =====================
float pulseToNorm(int pulse, bool invert) {
  int centered = pulse - 1500;
  if (abs(centered) < RC_DEADBAND_US) return 0.0;
  float x = constrain(centered / 500.0, -1.0, 1.0);
  if (invert) x = -x;
  return x;
}

float applyExpo(float x, float expo) {
  return (1.0 - expo) * x + expo * x * x * x;
}

float rampToward(float current, float target, float step) {
  if (current < target) return min(current + step, target);
  if (current > target) return max(current - step, target);
  return current;
}

int32_t limitErpm(float cmd) {
  float forwardLimit = MAX_FORWARD_ERPM;
  float reverseLimit = MAX_REVERSE_ERPM;
  float erpm = cmd * forwardLimit;
  if (cmd < 0) erpm = cmd * reverseLimit;
  return (int32_t)erpm;
}

void computeWheelCommands(float &masterCmd, float &slaveCmd) {
  if (abs(throttleCurrent) < 0.05) {
    float pivotTarget = applyExpo(steeringCurrent, PIVOT_EXPO) * PIVOT_GAIN;
    pivotCurrent = rampToward(pivotCurrent, pivotTarget, PIVOT_RAMP_STEP);
    masterCmd = pivotCurrent * MASTER_PIVOT_GAIN_TRIM;
    slaveCmd  = -pivotCurrent * SLAVE_PIVOT_GAIN_TRIM;
  } else {
    pivotCurrent = 0;
    float turn = steeringCurrent * STEERING_GAIN;
    masterCmd = throttleCurrent + turn;
    slaveCmd  = throttleCurrent - turn;
    float maxMag = max(abs(masterCmd), abs(slaveCmd));
    if (maxMag > 1.0) {
      masterCmd /= maxMag;
      slaveCmd  /= maxMag;
    }
  }

  if (INVERT_MASTER) masterCmd = -masterCmd;
  if (INVERT_SLAVE)  slaveCmd  = -slaveCmd;
}

void sendStop() {
  vesc.setRpm(0, 0);
  vesc.setRpm(0, SLAVE_ESC_ID);
  throttleCurrent = 0;
  steeringCurrent = 0;
  pivotCurrent = 0;
}

void requestNextTelemetry() {
  if (telemetryTarget == 0) {
    vesc.requestTelemetry(0);
  } else {
    vesc.requestTelemetry(SLAVE_ESC_ID);
  }
  telemetryTarget = 1 - telemetryTarget;
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* wait for USB serial */ }

  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_IN_PIN), throttleISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(STEERING_IN_PIN), steeringISR, CHANGE);

  vesc.begin(Serial1, VESC_BAUD);

  Serial.println(F("GT-EV-Testbed UartSpeedController"));
  Serial.println(F("Polling FT85BD over Serial1 (115200 baud)"));
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

// =====================
// LOOP
// =====================
void loop() {
  vesc.poll();

  unsigned long now = millis();

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

  noInterrupts();
  int thrUs = throttlePulse;
  int strUs = steeringPulse;
  unsigned long thrAge = now - lastThrottleUpdate;
  unsigned long strAge = now - lastSteeringUpdate;
  interrupts();

  if (thrAge > SIGNAL_TIMEOUT_MS || strAge > SIGNAL_TIMEOUT_MS) {
    sendStop();
    if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
      lastPrintMs = now;
      Serial.println(F("RC SIGNAL LOST -> ERPM 0"));
    }
    delay(10);
    return;
  }

  float throttle = applyExpo(pulseToNorm(thrUs, INVERT_THROTTLE), THROTTLE_EXPO);
  float steering = applyExpo(pulseToNorm(strUs, INVERT_STEERING), STEERING_EXPO);
  throttleCurrent = rampToward(throttleCurrent, throttle, THROTTLE_RAMP_STEP);
  steeringCurrent = rampToward(steeringCurrent, steering, STEERING_RAMP_STEP);

  float masterCmd = 0;
  float slaveCmd = 0;
  computeWheelCommands(masterCmd, slaveCmd);

  int32_t masterErpm = limitErpm(masterCmd);
  int32_t slaveErpm  = limitErpm(slaveCmd);

  vesc.setRpm(masterErpm, 0);
  vesc.setRpm(slaveErpm, SLAVE_ESC_ID);

  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    Serial.print(F("MasterERPM:"));
    Serial.print(masterErpm);
    Serial.print(F(" SlaveERPM:"));
    Serial.print(slaveErpm);
  }

  delay(5);
}
