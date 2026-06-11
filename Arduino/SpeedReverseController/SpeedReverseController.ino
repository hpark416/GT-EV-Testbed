#include <Servo.h>

// =====================
// RC INPUTS
// =====================
const int THROTTLE_IN_PIN = 2;  // RC CH2
const int STEERING_IN_PIN = 3;  // RC CH4

// =====================
// ESC OUTPUTS
// =====================
Servo masterESC;
Servo slaveESC;

const int MASTER_ESC_PIN = 9;
const int SLAVE_ESC_PIN  = 10;

// =====================
// PPM SETTINGS
// =====================
const int PPM_MID = 1500;
const int PPM_RANGE_US = 400;   // 1500 ± 400 = 1100–1900 us

// =====================
// SPEED LIMITS
// =====================
// Main forward speed limit as percent of full PPM range.
const float MAX_FORWARD_SPEED_PERCENT = 50.0;

// Reverse is separately capped for safety.
const float MAX_REVERSE_SPEED_PERCENT = 18.0;

// Steering while moving.
const float STEERING_GAIN = 0.40;

// Pivot / zero-radius turn tuning.
const float PIVOT_GAIN = 0.35;
const float PIVOT_EXPO = 0.75;
const float PIVOT_RAMP_STEP = 0.012;

// Balance left/right pivot speed.
// If master is faster, lower MASTER_PIVOT_GAIN_TRIM.
// If slave is faster, lower SLAVE_PIVOT_GAIN_TRIM.
const float MASTER_PIVOT_GAIN_TRIM = 1.00;
const float SLAVE_PIVOT_GAIN_TRIM  = 1.00;

// =====================
// INVERSIONS
// =====================
const bool INVERT_THROTTLE = true;
const bool INVERT_STEERING = false;
const bool INVERT_MASTER   = false;
const bool INVERT_SLAVE    = false;

// =====================
// INPUT TUNING
// =====================
const int RC_DEADBAND_US = 60;

const float THROTTLE_EXPO = 0.45;
const float STEERING_EXPO = 0.45;

// Ramping
const float THROTTLE_RAMP_STEP = 0.025;
const float STEERING_RAMP_STEP = 0.025;

// Signal timeout
const unsigned long SIGNAL_TIMEOUT_MS = 100;

// =====================
// INTERRUPT STATE
// =====================
volatile unsigned long throttleRise = 0;
volatile unsigned long steeringRise = 0;

volatile int throttlePulse = 1500;
volatile int steeringPulse = 1500;

volatile unsigned long lastThrottleUpdate = 0;
volatile unsigned long lastSteeringUpdate = 0;

// =====================
// CONTROL STATE
// =====================
float throttleCurrent = 0;
float steeringCurrent = 0;
float pivotCurrent = 0;

unsigned long lastPrint = 0;

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

  float x = centered / 500.0;
  x = constrain(x, -1.0, 1.0);

  if (invert) x = -x;
  return x;
}

float applyExpo(float x, float expo) {
  return (1.0 - expo) * x + expo * x * x * x;
}

float rampToward(float current, float target, float step) {
  if (current < target) {
    current += step;
    if (current > target) current = target;
  } else if (current > target) {
    current -= step;
    if (current < target) current = target;
  }
  return current;
}

float limitForwardReverse(float cmd) {
  float forwardLimit = MAX_FORWARD_SPEED_PERCENT / 100.0;
  float reverseLimit = MAX_REVERSE_SPEED_PERCENT / 100.0;

  if (cmd > forwardLimit) cmd = forwardLimit;
  if (cmd < -reverseLimit) cmd = -reverseLimit;

  return cmd;
}

void writeNeutral() {
  masterESC.writeMicroseconds(PPM_MID);
  slaveESC.writeMicroseconds(PPM_MID);
  throttleCurrent = 0;
  steeringCurrent = 0;
  pivotCurrent = 0;
}

int cmdToPPM(float cmd) {
  cmd = constrain(cmd, -1.0, 1.0);
  return PPM_MID + int(cmd * PPM_RANGE_US);
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);

  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(THROTTLE_IN_PIN), throttleISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(STEERING_IN_PIN), steeringISR, CHANGE);

  masterESC.attach(MASTER_ESC_PIN);
  slaveESC.attach(SLAVE_ESC_PIN);

  writeNeutral();

  Serial.println("FT85BD Speed Reverse Controller");
  Serial.println("Forward speed percent limit + capped reverse + pivot trim");
  delay(3000);
  Serial.println("Ready.");
}

// =====================
// LOOP
// =====================
void loop() {
  noInterrupts();
  int thrUs = throttlePulse;
  int strUs = steeringPulse;
  unsigned long thrAge = millis() - lastThrottleUpdate;
  unsigned long strAge = millis() - lastSteeringUpdate;
  interrupts();

  if (thrAge > SIGNAL_TIMEOUT_MS || strAge > SIGNAL_TIMEOUT_MS) {
    writeNeutral();
    Serial.println("RC SIGNAL LOST -> NEUTRAL");
    delay(10);
    return;
  }

  float throttle = pulseToNorm(thrUs, INVERT_THROTTLE);
  float steering = pulseToNorm(strUs, INVERT_STEERING);

  throttle = applyExpo(throttle, THROTTLE_EXPO);
  steering = applyExpo(steering, STEERING_EXPO);

  throttleCurrent = rampToward(throttleCurrent, throttle, THROTTLE_RAMP_STEP);
  steeringCurrent = rampToward(steeringCurrent, steering, STEERING_RAMP_STEP);

  float masterCmd = 0;
  float slaveCmd = 0;

  if (abs(throttleCurrent) < 0.05) {
    // True zero-radius pivot mode:
    // wheels move equal/opposite, with separate trim.
    float pivotTarget = applyExpo(steeringCurrent, PIVOT_EXPO) * PIVOT_GAIN;
    pivotCurrent = rampToward(pivotCurrent, pivotTarget, PIVOT_RAMP_STEP);

    masterCmd = pivotCurrent * MASTER_PIVOT_GAIN_TRIM;
    slaveCmd  = -pivotCurrent * SLAVE_PIVOT_GAIN_TRIM;
  } else {
    pivotCurrent = 0;

    // Option B arcade mixing:
    // outside wheel speeds up, inside wheel slows down.
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

  // Apply forward/reverse speed caps AFTER mixing.
  masterCmd = limitForwardReverse(masterCmd);
  slaveCmd  = limitForwardReverse(slaveCmd);

  int masterPPM = cmdToPPM(masterCmd);
  int slavePPM  = cmdToPPM(slaveCmd);

  masterESC.writeMicroseconds(masterPPM);
  slaveESC.writeMicroseconds(slavePPM);

  if (millis() - lastPrint > 200) {
    lastPrint = millis();

    Serial.print("Thr:");
    Serial.print(throttleCurrent * 100.0, 1);
    Serial.print("% Str:");
    Serial.print(steeringCurrent * 100.0, 1);

    Serial.print("% Pivot:");
    Serial.print(pivotCurrent * 100.0, 1);

    Serial.print("% | MasterCmd:");
    Serial.print(masterCmd * 100.0, 1);
    Serial.print("% SlaveCmd:");
    Serial.print(slaveCmd * 100.0, 1);

    Serial.print("% | MasterPPM:");
    Serial.print(masterPPM);
    Serial.print(" SlavePPM:");
    Serial.println(slavePPM);
  }

  delay(5);
}
