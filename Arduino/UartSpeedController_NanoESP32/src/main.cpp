/*
 * UartSpeedController — Arduino Nano ESP32 (PlatformIO)
 *
 * FT85BD UART speed controller using Flipsky FT ESC protocol (AA ... DD).
 * Based on bring-up findings from Diagnostics/UartTelemetry_NanoESP32.
 *
 * USB Serial  -> debug monitor
 * Serial1     -> master COMM (D8 RX, D9 TX)  ID 101
 * Serial2     -> slave COMM  (D4 RX, D5 TX)  ID 102   [DUAL_UART_PORTS]
 *
 * Set ENABLE_UART_RPM_CONTROL to false for telemetry-only bring-up.
 *
 * Drive modes (see DRIVE_MODE):
 *   PPM — proven on this cart; ESC input = PPM, wires on D6/D10 -> COMM PPM
 *   UART_CMD32 — VX-remote style cmd 32 (experimental, no extra wires)
 *   UART_CURRENT — cmd 4 SET_CURRENT (experimental)
 */

#include <Arduino.h>
#include <driver/gpio.h>
#include "FlipskyUart.h"

// =====================
// FIRMWARE VERSION
// =====================
static const char FIRMWARE_VERSION[] = "1.2.4";

// =====================
// DRIVE OUTPUT MODE
// =====================
enum class DriveMode : uint8_t {
  PPM,            // ESC Input Signal Type = PPM (proven on meka cart)
  UART_CMD32,     // cmd 32 gear+current (VX-remote style guess)
  UART_CURRENT,   // cmd 4 signed current
};

// PPM is the known-good path on this cart. Try UART_CMD32 if you cannot rewire yet.
static const DriveMode DRIVE_MODE = DriveMode::PPM;

// =====================
// FEATURE FLAGS
// =====================
static const bool ENABLE_UART_RPM_CONTROL = true;

// =====================
// UART READ MODE
// =====================
enum class ReadMode : uint8_t {
  CAN_FORWARD_VIA_MASTER,
  DUAL_UART_PORTS,
};

static const ReadMode READ_MODE = ReadMode::DUAL_UART_PORTS;

// =====================
// PINS
// =====================
static const int THROTTLE_IN_PIN = D2;
static const int STEERING_IN_PIN = D3;
static const int MASTER_RX_PIN = D8;
static const int MASTER_TX_PIN = D9;
static const int SLAVE_RX_PIN = D4;
static const int SLAVE_TX_PIN = D5;

// PPM outputs (separate from UART TX pins). Wire to COMM **PPM** on each half.
static const int MASTER_PPM_PIN = D6;
static const int SLAVE_PPM_PIN = D10;

static const int PPM_MID_US = 1500;
static const int PPM_FORWARD_RANGE_US = 400;  // 50% of stick -> 1900 us
static const int PPM_REVERSE_RANGE_US = 144;  // 18% reverse cap -> 1356 us

// GPIO bit-bang PPM @ 50 Hz (LEDC is unreliable on Nano ESP32).
static const int PPM_FRAME_US = 20000;
static const int PPM_INTER_FRAME_GAP_US = 400;

class GpioPpmOut {
public:
  bool begin(int arduinoPin) {
    arduinoPin_ = arduinoPin;
    gpio_ = digitalPinToGPIONumber(arduinoPin);
    if (gpio_ < 0) {
      return false;
    }
    pinMode(arduinoPin, OUTPUT);
    gpio_set_level((gpio_num_t)gpio_, 0);
    pulseUs_ = PPM_MID_US;
    return true;
  }

  int gpio() const { return gpio_; }
  int arduinoPin() const { return arduinoPin_; }

  void writeMicroseconds(int pulseUs) {
    pulseUs_ = constrain(pulseUs, 500, 2500);
  }

  int pulseUs() const { return pulseUs_; }

  void emitPulse() const {
    if (gpio_ < 0) {
      return;
    }
    gpio_set_level((gpio_num_t)gpio_, 1);
    delayMicroseconds(pulseUs_);
    gpio_set_level((gpio_num_t)gpio_, 0);
  }

private:
  int arduinoPin_ = -1;
  int gpio_ = -1;
  int pulseUs_ = PPM_MID_US;
};

GpioPpmOut masterPpm;
GpioPpmOut slavePpm;

static void servicePpmOutputs() {
  const unsigned long frameStartUs = micros();
  masterPpm.emitPulse();
  delayMicroseconds(PPM_INTER_FRAME_GAP_US);
  slavePpm.emitPulse();
  const unsigned long elapsedUs = micros() - frameStartUs;
  if (elapsedUs < (unsigned long)PPM_FRAME_US) {
    delayMicroseconds((unsigned int)(PPM_FRAME_US - elapsedUs));
  }
}

// =====================
// ESC
// =====================
FlipskyUart masterEsc;
FlipskyUart slaveEsc;

static const unsigned long ESC_BAUD = 115200;
static const uint8_t MASTER_ESC_ID = 101;
static const uint8_t SLAVE_ESC_ID = 102;

static const float MAX_FORWARD_CURRENT_A = 40.0f;
static const float MAX_REVERSE_CURRENT_A = 15.0f;
static const float CURRENT_TO_MICROA = 1000000.0f;

// =====================
// RC TUNING — meka cart (stable, brake-at-neutral)
// =====================
// Forward stick = forward. Flip if your radio is reversed.
static const bool INVERT_THROTTLE = true;
static const bool INVERT_STEERING = false;
static const bool INVERT_MASTER = false;
static const bool INVERT_SLAVE = false;  // set motor direction in Flipsky ESC Tool instead

static const int RC_DEADBAND_US = 60;
static const float CMD_ZERO_THRESHOLD = 0.03f;

// Higher expo = softer low-speed response (less choppy than PPM breakaway kick).
static const float THROTTLE_EXPO = 0.60f;
static const float STEERING_EXPO = 0.50f;
static const float THROTTLE_RAMP_STEP = 0.015f;
static const float THROTTLE_BRAKE_RAMP_STEP = 0.06f;
static const float STEERING_RAMP_STEP = 0.020f;

// At speed: slow the inside wheel only (does not boost outside wheel — safer).
static const float SPEED_STEERING_GAIN = 0.30f;
static const float MAX_TURN_REDUCTION = 0.75f;

// Pivot in place is easy to flip the cart — off by default.
static const bool ENABLE_PIVOT_STEERING = false;
static const float PIVOT_THROTTLE_THRESHOLD = 0.06f;
static const float PIVOT_STEERING_GAIN = 0.18f;
static const float PIVOT_EXPO = 0.75f;
static const float PIVOT_RAMP_STEP = 0.010f;
static const float MASTER_PIVOT_GAIN_TRIM = 1.00f;
static const float SLAVE_PIVOT_GAIN_TRIM = 1.00f;
static const unsigned long SIGNAL_TIMEOUT_MS = 300;
static const int RC_PWM_MIN_US = 850;
static const int RC_PWM_MAX_US = 2150;

// =====================
// TIMING (from UartTelemetry_NanoESP32 bring-up)
// =====================
static const unsigned long TELEMETRY_INTERVAL_MS = 1000;
static const unsigned long RESPONSE_WAIT_MS = 25;
static const unsigned long ALIVE_INTERVAL_MS = 1000;

// =====================
// RC STATE (pulseIn polling — reliable on ESP32-S3)
// =====================
static int throttlePulse = 1500;
static int steeringPulse = 1500;
static unsigned long lastThrottleUpdateUs = 0;
static unsigned long lastSteeringUpdateUs = 0;
static uint32_t throttleValidCount = 0;
static uint32_t steeringValidCount = 0;
static bool throttleEver = false;
static bool steeringEver = false;
static uint8_t rcPollNext = 0;

static float throttleCurrent = 0;
static float steeringCurrent = 0;
static float pivotCurrent = 0;

static unsigned long lastTelemetryMs = 0;
static unsigned long lastAliveMs = 0;
static bool pollSlaveNext = false;

// =====================
// RC POLLING
// =====================
static void pollOneRcChannel() {
  const int pin = (rcPollNext == 0) ? THROTTLE_IN_PIN : STEERING_IN_PIN;
  rcPollNext ^= 1;

  const unsigned long width = pulseInLong(pin, HIGH, 25000);
  if (width < (unsigned long)RC_PWM_MIN_US ||
      width > (unsigned long)RC_PWM_MAX_US) {
    return;
  }

  const unsigned long nowUs = micros();
  if (pin == THROTTLE_IN_PIN) {
    throttlePulse = (int)width;
    lastThrottleUpdateUs = nowUs;
    throttleEver = true;
    throttleValidCount++;
  } else {
    steeringPulse = (int)width;
    lastSteeringUpdateUs = nowUs;
    steeringEver = true;
    steeringValidCount++;
  }
}

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
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    slaveEsc.poll();
  }
}

static bool pollEsc(FlipskyUart &esc, uint8_t targetId) {
  esc.clearTelemetryFlag();
  esc.requestTelemetry(targetId);
  delay(RESPONSE_WAIT_MS);
  esc.poll();
  return esc.hasTelemetry();
}

static void printPinMap() {
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    Serial.println(F("Wiring: master D9->RX D8<-TX | slave D5->RX D4<-TX | common GND"));
    return;
  }
  Serial.println(F("Wiring: master D9->RX D8<-TX | slave via CAN-forward cmd 16"));
}

static void printRcLine(int thrUs, int strUs, unsigned long thrAge,
                        unsigned long strAge, bool thrEver, bool strEver,
                        uint32_t thrCount, uint32_t strCount) {
  const bool thrOk = thrEver && thrAge <= SIGNAL_TIMEOUT_MS;
  const bool strOk = strEver && strAge <= SIGNAL_TIMEOUT_MS;

  Serial.print(F("RC  Thr:"));
  Serial.print(thrUs);
  Serial.print(thrOk ? F("us OK  ") : F("us STALE "));
  if (!thrOk) {
    Serial.print(thrEver ? thrAge : 0);
    Serial.print(thrEver ? F("ms  ") : F("ms (never)  "));
  }
  Serial.print(F("Str:"));
  Serial.print(strUs);
  Serial.print(strOk ? F("us OK  ") : F("us STALE "));
  if (!strOk) {
    Serial.print(strEver ? strAge : 0);
    Serial.print(strEver ? F("ms  ") : F("ms (never)  "));
  }
  Serial.print(F("pwm Thr:"));
  Serial.print(thrCount);
  Serial.print(F(" Str:"));
  Serial.println(strCount);
}

static void printTelemetryLine(const __FlashStringHelper *label, const FlipskyUart &esc) {
  Serial.print(label);
  Serial.print(F("  Vbat:"));
  Serial.print(esc.inputVoltage(), 2);
  Serial.print(F("V  RPM:"));
  Serial.print(esc.erpm());
  Serial.print(F("  I:"));
  Serial.print(esc.motorCurrent(), 2);
  Serial.print(F("A  Fault:"));
  Serial.print(esc.faultCode());
  Serial.print(F("  ID:"));
  Serial.println(esc.controllerId());
}

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

static float rampThrottle(float current, float target) {
  const bool braking =
      fabsf(target) < fabsf(current) ||
      (fabsf(target) < CMD_ZERO_THRESHOLD && fabsf(current) > CMD_ZERO_THRESHOLD);
  return rampToward(current, target,
                    braking ? THROTTLE_BRAKE_RAMP_STEP : THROTTLE_RAMP_STEP);
}

static int32_t limitCurrentMicroA(float cmd) {
  float amps = cmd * MAX_FORWARD_CURRENT_A;
  if (cmd < 0) {
    amps = cmd * MAX_REVERSE_CURRENT_A;
  }
  return (int32_t)(amps * CURRENT_TO_MICROA);
}

static float microAToAmps(int32_t microA) {
  return (float)microA / CURRENT_TO_MICROA;
}

static uint8_t gearForCommand(float cmd) {
  const float absCmd = fabsf(cmd);
  if (absCmd < CMD_ZERO_THRESHOLD) {
    return FTESC_GEAR_NEUTRAL;
  }
  if (cmd < 0) {
    return FTESC_GEAR_REVERSE;
  }
  if (absCmd < 0.34f) {
    return FTESC_GEAR_FORWARD_LOW;
  }
  if (absCmd < 0.67f) {
    return FTESC_GEAR_FORWARD_MED;
  }
  return FTESC_GEAR_FORWARD_HIGH;
}

static int32_t magnitudeMicroA(float cmd) {
  float amps = fabsf(cmd) * MAX_FORWARD_CURRENT_A;
  if (cmd < 0) {
    amps = fabsf(cmd) * MAX_REVERSE_CURRENT_A;
  }
  return (int32_t)(amps * CURRENT_TO_MICROA);
}

static int cmdToPpmUs(float cmd) {
  if (fabsf(cmd) < CMD_ZERO_THRESHOLD) {
    return PPM_MID_US;
  }
  if (cmd > 0) {
    return PPM_MID_US + (int)(cmd * PPM_FORWARD_RANGE_US);
  }
  return PPM_MID_US + (int)(cmd * PPM_REVERSE_RANGE_US);
}

static const __FlashStringHelper *driveModeLabel() {
  switch (DRIVE_MODE) {
    case DriveMode::PPM:
      return F("PPM on D6/D10 -> COMM PPM");
    case DriveMode::UART_CMD32:
      return F("UART cmd 32 gear+current");
    case DriveMode::UART_CURRENT:
      return F("UART cmd 4 SET_CURRENT");
  }
  return F("unknown");
}

static void computeWheelCommands(float &masterCmd, float &slaveCmd) {
  const float throttleAbs = fabsf(throttleCurrent);
  const float steeringAbs = fabsf(steeringCurrent);
  const float steer = steeringCurrent;

  if (ENABLE_PIVOT_STEERING && throttleAbs < PIVOT_THROTTLE_THRESHOLD &&
      steeringAbs > 0.08f) {
    const float pivotTarget =
        applyExpo(steer, PIVOT_EXPO) * PIVOT_STEERING_GAIN;
    pivotCurrent = rampToward(pivotCurrent, pivotTarget, PIVOT_RAMP_STEP);
    masterCmd = pivotCurrent * MASTER_PIVOT_GAIN_TRIM;
    slaveCmd = -pivotCurrent * SLAVE_PIVOT_GAIN_TRIM;
  } else {
    pivotCurrent = rampToward(pivotCurrent, 0.0f, PIVOT_RAMP_STEP);

    float turn = steeringAbs * SPEED_STEERING_GAIN;
    turn = constrain(turn, 0.0f, MAX_TURN_REDUCTION);

    const float outsideCmd = throttleCurrent;
    const float insideCmd = throttleCurrent * (1.0f - turn);

    if (steer > 0.05f) {
      masterCmd = outsideCmd;
      slaveCmd = insideCmd;
    } else if (steer < -0.05f) {
      masterCmd = insideCmd;
      slaveCmd = outsideCmd;
    } else {
      masterCmd = throttleCurrent;
      slaveCmd = throttleCurrent;
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
  if (DRIVE_MODE == DriveMode::PPM) {
    masterPpm.writeMicroseconds(PPM_MID_US);
    slavePpm.writeMicroseconds(PPM_MID_US);
  } else if (DRIVE_MODE == DriveMode::UART_CMD32) {
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      masterEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0);
      slaveEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0);
    } else {
      masterEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0);
      masterEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0, SLAVE_ESC_ID);
    }
  } else {
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      masterEsc.setCurrent(0);
      slaveEsc.setCurrent(0);
    } else {
      masterEsc.setCurrent(0);
      masterEsc.setCurrent(0, SLAVE_ESC_ID);
    }
  }
  throttleCurrent = 0;
  steeringCurrent = 0;
  pivotCurrent = 0;
}

static void sendWheelDrive(float masterCmd, float slaveCmd) {
  if (DRIVE_MODE == DriveMode::PPM) {
    masterPpm.writeMicroseconds(cmdToPpmUs(masterCmd));
    slavePpm.writeMicroseconds(cmdToPpmUs(slaveCmd));
    return;
  }

  if (DRIVE_MODE == DriveMode::UART_CMD32) {
    const uint8_t masterGear = gearForCommand(masterCmd);
    const uint8_t slaveGear = gearForCommand(slaveCmd);
    const int32_t masterMicroA = magnitudeMicroA(masterCmd);
    const int32_t slaveMicroA = magnitudeMicroA(slaveCmd);

    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      masterEsc.setCurrentGearAndObtain(masterGear, masterMicroA);
      slaveEsc.setCurrentGearAndObtain(slaveGear, slaveMicroA);
    } else {
      masterEsc.setCurrentGearAndObtain(masterGear, masterMicroA);
      masterEsc.setCurrentGearAndObtain(slaveGear, slaveMicroA, SLAVE_ESC_ID);
    }
    return;
  }

  const int32_t masterMicroA = limitCurrentMicroA(masterCmd);
  const int32_t slaveMicroA = limitCurrentMicroA(slaveCmd);
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    masterEsc.setCurrent(masterMicroA);
    slaveEsc.setCurrent(slaveMicroA);
    return;
  }

  masterEsc.setCurrent(masterMicroA);
  masterEsc.setCurrent(slaveMicroA, SLAVE_ESC_ID);
}

static void readRcSnapshot(int &thrUs, int &strUs, unsigned long &thrAge,
                           unsigned long &strAge, uint32_t &thrCount,
                           uint32_t &strCount, bool &thrEver, bool &strEver) {
  thrUs = throttlePulse;
  strUs = steeringPulse;
  thrCount = throttleValidCount;
  strCount = steeringValidCount;
  thrEver = throttleEver;
  strEver = steeringEver;
  const unsigned long nowUs = micros();
  thrAge = thrEver ? (nowUs - lastThrottleUpdateUs) / 1000UL : ULONG_MAX;
  strAge = strEver ? (nowUs - lastSteeringUpdateUs) / 1000UL : ULONG_MAX;
}

static void probeRcPwm(int pin, const __FlashStringHelper *label) {
  int lastGood = -1;
  int validSamples = 0;

  for (int i = 0; i < 10; i++) {
    const unsigned long width = pulseInLong(pin, HIGH, 25000);
    if (width >= (unsigned long)RC_PWM_MIN_US &&
        width <= (unsigned long)RC_PWM_MAX_US) {
      lastGood = (int)width;
      validSamples++;
    }
    delay(20);
  }

  Serial.print(label);
  Serial.print(F(" D"));
  Serial.print(pin);
  Serial.print(F(" (GPIO"));
  Serial.print(digitalPinToGPIONumber(pin));
  Serial.print(F("): "));
  if (validSamples > 0) {
    Serial.print(lastGood);
    Serial.print(F("us, "));
    Serial.print(validSamples);
    Serial.println(F("/10 PWM frames"));
  } else {
    Serial.println(F("no PWM detected"));
  }
}

static void probeEsc(FlipskyUart &esc) {
  esc.clearLastRaw();
  esc.sendFwVersion();
  delay(100);
  esc.poll();
}

static void verifyHarness(FlipskyUart &esc, const __FlashStringHelper *label,
                          uint8_t expectedId) {
  Serial.print(label);
  Serial.print(F(" harness: "));

  if (!pollEsc(esc, 0)) {
    Serial.print(F("NO REPLY (RX bytes="));
    Serial.print(esc.rxBytes());
    Serial.println(F(")"));
    return;
  }

  printTelemetryLine(label, esc);

  if (esc.controllerId() != expectedId) {
    Serial.print(F("  *** expected ID "));
    Serial.print(expectedId);
    Serial.print(F(", got "));
    Serial.println(esc.controllerId());
  }

  esc.clearTelemetryFlag();
}

static void pollTelemetryDual() {
  const bool gotMaster = pollEsc(masterEsc, 0);
  const bool gotSlave = pollEsc(slaveEsc, 0);

  if (gotMaster) {
    printTelemetryLine(F("[Master 101]"), masterEsc);
    masterEsc.clearTelemetryFlag();
  }
  if (gotSlave) {
    printTelemetryLine(F("[Slave 102]"), slaveEsc);
    slaveEsc.clearTelemetryFlag();
  }
}

static void pollTelemetryCanForward() {
  const uint8_t targetId = pollSlaveNext ? SLAVE_ESC_ID : 0;
  pollSlaveNext = !pollSlaveNext;

  if (pollEsc(masterEsc, targetId)) {
    printTelemetryLine(pollSlaveNext ? F("[Slave 102 CAN]") : F("[Master 101]"),
                       masterEsc);
    masterEsc.clearTelemetryFlag();
  }
}

static void reportTelemetry(int thrUs, int strUs, unsigned long thrAge,
                            unsigned long strAge, bool thrEver, bool strEver,
                            uint32_t thrCount, uint32_t strCount,
                            bool throttleOk, bool steeringOk,
                            float masterCmd, float slaveCmd) {
  printRcLine(thrUs, strUs, thrAge, strAge, thrEver, strEver, thrCount,
              strCount);

  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    pollTelemetryDual();
  } else {
    pollTelemetryCanForward();
  }

  if (ENABLE_UART_RPM_CONTROL) {
    Serial.print(F("Cmd Master:"));
    if (DRIVE_MODE == DriveMode::PPM) {
      Serial.print(cmdToPpmUs(masterCmd));
      Serial.print(F("us Slave:"));
      Serial.print(cmdToPpmUs(slaveCmd));
      Serial.print(F("us"));
    } else if (DRIVE_MODE == DriveMode::UART_CMD32) {
      Serial.print(microAToAmps(magnitudeMicroA(masterCmd)), 1);
      Serial.print(F("A g"));
      Serial.print(gearForCommand(masterCmd));
      Serial.print(F(" Slave:"));
      Serial.print(microAToAmps(magnitudeMicroA(slaveCmd)), 1);
      Serial.print(F("A g"));
      Serial.print(gearForCommand(slaveCmd));
    } else {
      Serial.print(microAToAmps(limitCurrentMicroA(masterCmd)), 1);
      Serial.print(F("A Slave:"));
      Serial.print(microAToAmps(limitCurrentMicroA(slaveCmd)), 1);
      Serial.print(F("A"));
    }
    if (!throttleOk) {
      Serial.print(F("  STOP: throttle lost"));
    } else if (!steeringOk) {
      Serial.print(F("  (steering stale -> straight)"));
    }
    if (masterEsc.faultCode() != 0 || slaveEsc.faultCode() != 0) {
      Serial.print(F("  Fault M:"));
      Serial.print(masterEsc.faultCode());
      Serial.print(F(" S:"));
      Serial.print(slaveEsc.faultCode());
    }
    Serial.println();
  }
}

// =====================
// ARDUINO
// =====================
void setup() {
  Serial.begin(115200);
  waitForUsbSerial();

  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);

  Serial.println();
  Serial.print(F("Firmware v"));
  Serial.println(FIRMWARE_VERSION);
  Serial.println(F("=== GT-EV-Testbed UartSpeedController (Nano ESP32) ==="));
  Serial.println(F("Protocol: Flipsky FT ESC UART (AA ... DD)"));
  Serial.print(F("Drive: "));
  Serial.println(driveModeLabel());
  if (DRIVE_MODE == DriveMode::PPM) {
    Serial.println(F("PPM wiring: D6 -> master COMM PPM | D10 -> slave COMM PPM"));
    Serial.println(F("ESC tool: Input Signal Type = PPM, Control Mode = Speed Reverse"));
    Serial.println(F("(UART wires stay on D8/D9 and D4/D5 for telemetry if supported)"));
  } else {
    Serial.println(F("ESC tool: Input Signal Type = UART, Control Mode = Current Bidirectional"));
  }
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    Serial.println(F("Mode: dual UART (master + slave COMM ports)"));
  } else {
    Serial.println(F("Mode: master UART + CAN-forward telemetry"));
  }
  printPinMap();
  Serial.print(F("Motor control: "));
  Serial.println(ENABLE_UART_RPM_CONTROL ? F("ENABLED") : F("DISABLED (telemetry only)"));
  if (ENABLE_UART_RPM_CONTROL && READ_MODE != ReadMode::DUAL_UART_PORTS) {
    Serial.println(F("Note: slave drive uses CAN-forward — prefer DUAL_UART_PORTS"));
  }
  Serial.println();
  Serial.println(F("--- RC PWM probe (before interrupts) ---"));
  probeRcPwm(THROTTLE_IN_PIN, F("Throttle"));
  probeRcPwm(STEERING_IN_PIN, F("Steering"));
  Serial.println(F("Move sticks if a channel shows 'no PWM detected'."));
  Serial.println(F("RC read mode: pulseIn polling (no interrupts)"));
  Serial.println();

  masterEsc.begin(Serial1, ESC_BAUD, MASTER_RX_PIN, MASTER_TX_PIN);
  if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
    slaveEsc.begin(Serial2, ESC_BAUD, SLAVE_RX_PIN, SLAVE_TX_PIN);
  }

  if (ENABLE_UART_RPM_CONTROL && DRIVE_MODE == DriveMode::PPM) {
    const bool masterOk = masterPpm.begin(MASTER_PPM_PIN);
    const bool slaveOk = slavePpm.begin(SLAVE_PPM_PIN);
    Serial.print(F("PPM GPIO bit-bang D"));
    Serial.print(MASTER_PPM_PIN);
    Serial.print(F("(GPIO"));
    Serial.print(masterPpm.gpio());
    Serial.print(F(") D"));
    Serial.print(SLAVE_PPM_PIN);
    Serial.print(F("(GPIO"));
    Serial.print(slavePpm.gpio());
    Serial.println(masterOk && slaveOk ? F(") OK") : F(") FAIL"));
    masterPpm.writeMicroseconds(PPM_MID_US);
    slavePpm.writeMicroseconds(PPM_MID_US);
  }

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
    Serial.println(F("--- Harness check ---"));
    verifyHarness(masterEsc, F("[Master 101]"), MASTER_ESC_ID);
    verifyHarness(slaveEsc, F("[Slave 102]"), SLAVE_ESC_ID);
  }

  Serial.println();
  Serial.println(F("Ready."));
}

void loop() {
  static unsigned long lastPpmFrameUs = 0;

  pollEscPorts();
  pollOneRcChannel();

  const unsigned long now = millis();
  const unsigned long nowUs = micros();

  if (ENABLE_UART_RPM_CONTROL && DRIVE_MODE == DriveMode::PPM) {
    if (nowUs - lastPpmFrameUs >= (unsigned long)PPM_FRAME_US) {
      lastPpmFrameUs = nowUs;
      servicePpmOutputs();
    }
  } else {
    delay(5);
  }

  if (now - lastAliveMs >= ALIVE_INTERVAL_MS) {
    lastAliveMs = now;
    masterEsc.sendAlive();
    if (READ_MODE == ReadMode::DUAL_UART_PORTS) {
      slaveEsc.sendAlive();
    }
  }

  int thrUs = 0;
  int strUs = 0;
  unsigned long thrAge = 0;
  unsigned long strAge = 0;
  uint32_t thrCount = 0;
  uint32_t strCount = 0;
  bool thrEver = false;
  bool strEver = false;
  readRcSnapshot(thrUs, strUs, thrAge, strAge, thrCount, strCount, thrEver,
                 strEver);

  const bool throttleOk = thrEver && thrAge <= SIGNAL_TIMEOUT_MS;
  const bool steeringOk = strEver && strAge <= SIGNAL_TIMEOUT_MS;

  float masterCmd = 0;
  float slaveCmd = 0;

  if (ENABLE_UART_RPM_CONTROL) {
    if (!throttleOk) {
      sendStop();
    } else {
      const float throttle =
          applyExpo(pulseToNorm(thrUs, INVERT_THROTTLE), THROTTLE_EXPO);
      float steering = 0.0f;
      if (steeringOk) {
        steering =
            applyExpo(pulseToNorm(strUs, INVERT_STEERING), STEERING_EXPO);
      }

      const bool sticksCentered = fabsf(throttle) < CMD_ZERO_THRESHOLD &&
                                  fabsf(steering) < CMD_ZERO_THRESHOLD;

      if (sticksCentered) {
        throttleCurrent = 0.0f;
        steeringCurrent = 0.0f;
        pivotCurrent = 0.0f;
        masterCmd = 0;
        slaveCmd = 0;
        sendWheelDrive(0, 0);
      } else {
        throttleCurrent = rampThrottle(throttleCurrent, throttle);
        if (steeringOk) {
          steeringCurrent =
              rampToward(steeringCurrent, steering, STEERING_RAMP_STEP);
        } else {
          steeringCurrent =
              rampToward(steeringCurrent, 0.0f, STEERING_RAMP_STEP);
        }

        computeWheelCommands(masterCmd, slaveCmd);
        sendWheelDrive(masterCmd, slaveCmd);
      }
    }
  }

  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    reportTelemetry(thrUs, strUs, thrAge, strAge, thrEver, strEver, thrCount,
                    strCount, throttleOk, steeringOk, masterCmd, slaveCmd);
  }
}
