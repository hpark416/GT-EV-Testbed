#include <Servo.h>

// =====================
// RC INPUTS
// =====================
const int THROTTLE_IN_PIN = 2;  // RC CH2 forward/back
const int STEERING_IN_PIN = 3;  // RC CH4 left/right

// =====================
// ESC OUTPUTS
// =====================
Servo masterESC;
Servo slaveESC;

const int MASTER_ESC_PIN = 9;
const int SLAVE_ESC_PIN  = 10;

// =====================
// BASIC PPM
// =====================
const int PPM_MID = 1500;

// Overall output limit. Tune this as your speed/current limit.
// 150 = very gentle, 220 = moderate, 350+ = aggressive.
const int SPEED_LIMIT_US = 220;

// =====================
// INPUT INVERSION
// =====================
// Leave true if pushing throttle stick up gives negative throttle in serial.
const bool INVERT_THROTTLE = true;
const bool INVERT_STEERING = false;

// =====================
// OUTPUT INVERSION
// =====================
// Prefer fixing motor direction in ESC Tool. Keep these false if both wheels
// spin correct direction with identical PPM commands.
const bool INVERT_MASTER = false;
const bool INVERT_SLAVE  = false;

// =====================
// PER-SIDE OUTPUT LIMITS
// =====================
// These set the max/min PPM pulse each side can receive.
// Keep symmetric for now unless one ESC needs calibrated compensation.
const int MASTER_FORWARD_MAX_US = PPM_MID + SPEED_LIMIT_US;
const int MASTER_REVERSE_MAX_US = PPM_MID - SPEED_LIMIT_US;

const int SLAVE_FORWARD_MAX_US  = PPM_MID + SPEED_LIMIT_US;
const int SLAVE_REVERSE_MAX_US  = PPM_MID - SPEED_LIMIT_US;

// Per-side gain compensation. Keep 1.00 for now.
const float MASTER_GAIN_FORWARD = 1.00;
const float MASTER_GAIN_REVERSE = 1.00;

const float SLAVE_GAIN_FORWARD  = 1.00;
const float SLAVE_GAIN_REVERSE  = 1.00;

// =====================
// STATIC FRICTION / BREAKAWAY TUNING
// =====================
// If a wheel hums but does not start at tiny commands, this forces any
// intentional nonzero command up to a minimum value.
// 0.10 = 10% of output range, 0.18 = 18%, 0.25 = stronger kick.
const float MIN_DRIVE_CMD = 0.18;

// Below this, commands are treated as true zero. Prevents creep/hum near center.
const float CMD_ZERO_THRESHOLD = 0.025;

// If true, minimum drive applies to both throttle and pivot steering.
// If false, it only applies when throttle is commanded.
const bool APPLY_MIN_TO_PIVOT_STEERING = true;

// =====================
// STEERING TUNING
// =====================
// Low-speed pivot steering. Keep modest.
const float LOW_SPEED_STEERING_GAIN = 0.35;

// At higher throttle, reduce steering so it is not twitchy.
const float HIGH_SPEED_STEERING_GAIN = 0.15;

// Minimum steering command for pivot turns. Set 0 to disable pivot assist.
const float MIN_PIVOT_STEER = 0.15;

// =====================
// INPUT TUNING
// =====================
const int RC_DEADBAND_US = 55;

const float THROTTLE_EXPO = 0.45;
const float STEERING_EXPO = 0.45;

// Ramping in normalized units per loop. 8/100 = 0.08 per 5 ms loop.
const float THROTTLE_RAMP_STEP = 8.0;
const float STEERING_RAMP_STEP = 8.0;

// =====================
// INTERRUPT STATE
// =====================
volatile unsigned long throttleRise = 0;
volatile unsigned long steeringRise = 0;

volatile int throttlePulse = 1500;
volatile int steeringPulse = 1500;

volatile unsigned long lastThrottleUpdate = 0;
volatile unsigned long lastSteeringUpdate = 0;

const unsigned long SIGNAL_TIMEOUT_MS = 100;

// =====================
// CONTROL STATE
// =====================
float throttleCurrent = 0;
float steeringCurrent = 0;

unsigned long lastPrint = 0;

// =====================
// INTERRUPTS
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

float applyMinDrive(float cmd) {
  if (abs(cmd) < CMD_ZERO_THRESHOLD) {
    return 0.0;
  }

  if (cmd > 0 && cmd < MIN_DRIVE_CMD) {
    return MIN_DRIVE_CMD;
  }

  if (cmd < 0 && cmd > -MIN_DRIVE_CMD) {
    return -MIN_DRIVE_CMD;
  }

  return cmd;
}

void writeNeutral() {
  masterESC.writeMicroseconds(PPM_MID);
  slaveESC.writeMicroseconds(PPM_MID);
  throttleCurrent = 0;
  steeringCurrent = 0;
}

// cmd is -1.0 to +1.0
int commandToPulse(float cmd, bool isMaster) {
  cmd = constrain(cmd, -1.0, 1.0);

  if (isMaster) {
    if (cmd >= 0) {
      cmd *= MASTER_GAIN_FORWARD;
      cmd = constrain(cmd, 0.0, 1.0);
      return PPM_MID + int(cmd * (MASTER_FORWARD_MAX_US - PPM_MID));
    } else {
      cmd *= MASTER_GAIN_REVERSE;
      cmd = constrain(cmd, -1.0, 0.0);
      return PPM_MID + int((-cmd) * (MASTER_REVERSE_MAX_US - PPM_MID));
    }
  } else {
    if (cmd >= 0) {
      cmd *= SLAVE_GAIN_FORWARD;
      cmd = constrain(cmd, 0.0, 1.0);
      return PPM_MID + int(cmd * (SLAVE_FORWARD_MAX_US - PPM_MID));
    } else {
      cmd *= SLAVE_GAIN_REVERSE;
      cmd = constrain(cmd, -1.0, 0.0);
      return PPM_MID + int((-cmd) * (SLAVE_REVERSE_MAX_US - PPM_MID));
    }
  }
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

  Serial.println("FT85BD PPM Arcade Mixer with Breakaway Assist");
  Serial.println("Throttle: CH2/D2 | Steering: CH4/D3");
  Serial.println("Keep wheels lifted for first test.");
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

  bool signalValid =
    thrAge < SIGNAL_TIMEOUT_MS &&
    strAge < SIGNAL_TIMEOUT_MS;

  if (!signalValid) {
    writeNeutral();
    Serial.println("RC SIGNAL LOST -> NEUTRAL");
    delay(10);
    return;
  }

  float throttle = pulseToNorm(thrUs, INVERT_THROTTLE);
  float steering = pulseToNorm(strUs, INVERT_STEERING);

  throttle = applyExpo(throttle, THROTTLE_EXPO);
  steering = applyExpo(steering, STEERING_EXPO);

  // True neutral bypasses ramps and breakaway assist.
  bool sticksCentered = (abs(throttle) < CMD_ZERO_THRESHOLD && abs(steering) < CMD_ZERO_THRESHOLD);
  if (sticksCentered) {
    writeNeutral();

    if (millis() - lastPrint > 300) {
      lastPrint = millis();
      Serial.print("ThrIn:");
      Serial.print(thrUs);
      Serial.print(" StrIn:");
      Serial.print(strUs);
      Serial.println(" | CENTERED -> NEUTRAL");
    }

    delay(5);
    return;
  }

  float throttleTarget = throttle;
  float steeringTarget = steering;

  throttleCurrent = rampToward(throttleCurrent, throttleTarget, THROTTLE_RAMP_STEP / 100.0);
  steeringCurrent = rampToward(steeringCurrent, steeringTarget, STEERING_RAMP_STEP / 100.0);

  // Dynamic steering gain:
  // More steering when stopped, less steering when moving fast.
  float throttleAbs = abs(throttleCurrent);
  float steeringGain =
    LOW_SPEED_STEERING_GAIN -
    throttleAbs * (LOW_SPEED_STEERING_GAIN - HIGH_SPEED_STEERING_GAIN);

  steeringGain = constrain(steeringGain, HIGH_SPEED_STEERING_GAIN, LOW_SPEED_STEERING_GAIN);

  float effectiveSteering = steeringCurrent * steeringGain;

  // Help pivot turn overcome deadband when throttle is near zero.
  if (abs(throttleCurrent) < 0.05 && abs(steeringCurrent) > 0.10) {
    if (effectiveSteering > 0 && effectiveSteering < MIN_PIVOT_STEER) {
      effectiveSteering = MIN_PIVOT_STEER;
    }
    if (effectiveSteering < 0 && effectiveSteering > -MIN_PIVOT_STEER) {
      effectiveSteering = -MIN_PIVOT_STEER;
    }
  }

  // Arcade mixing
  float masterCmd = throttleCurrent + effectiveSteering;
  float slaveCmd  = throttleCurrent - effectiveSteering;

  masterCmd = constrain(masterCmd, -1.0, 1.0);
  slaveCmd  = constrain(slaveCmd, -1.0, 1.0);

  if (INVERT_MASTER) masterCmd = -masterCmd;
  if (INVERT_SLAVE)  slaveCmd  = -slaveCmd;

  // Breakaway assist: prevents one wheel from humming at tiny nonzero commands.
  // This is especially helpful for unloaded bench tests and low-speed starts.
  if (APPLY_MIN_TO_PIVOT_STEERING || abs(throttleCurrent) > CMD_ZERO_THRESHOLD) {
    masterCmd = applyMinDrive(masterCmd);
    slaveCmd  = applyMinDrive(slaveCmd);
  }

  int masterPulse = commandToPulse(masterCmd, true);
  int slavePulse  = commandToPulse(slaveCmd, false);

  masterESC.writeMicroseconds(masterPulse);
  slaveESC.writeMicroseconds(slavePulse);

  if (millis() - lastPrint > 200) {
    lastPrint = millis();

    Serial.print("ThrIn:");
    Serial.print(thrUs);
    Serial.print(" StrIn:");
    Serial.print(strUs);

    Serial.print(" | Thr%:");
    Serial.print(throttleCurrent * 100.0, 1);
    Serial.print(" Str%:");
    Serial.print(steeringCurrent * 100.0, 1);

    Serial.print(" | SteerGain:");
    Serial.print(steeringGain, 2);

    Serial.print(" | MasterCmd%:");
    Serial.print(masterCmd * 100.0, 1);
    Serial.print(" SlaveCmd%:");
    Serial.print(slaveCmd * 100.0, 1);

    Serial.print(" | MasterPPM:");
    Serial.print(masterPulse);
    Serial.print(" SlavePPM:");
    Serial.println(slavePulse);
  }

  delay(5);
}
