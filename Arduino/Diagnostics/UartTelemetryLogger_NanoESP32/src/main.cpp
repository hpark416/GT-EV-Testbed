/*
 * UartTelemetryLogger_NanoESP32
 *
 * Dual-UART FT85BD telemetry logger + RC drive for Arduino Nano ESP32.
 * Writes CSV to Adafruit microSD SPI breakout while driving the cart so
 * stutter / low-speed behavior can be captured and analyzed.
 *
 * UART (dual, same as UartSpeedController_NanoESP32):
 *   Serial1 master  D8 RX / D9 TX   ID 101
 *   Serial2 slave   D4 RX / D5 TX   ID 102
 *
 * RC PWM in:
 *   Throttle D2, Steering D3
 *
 * PPM drive out (default — proven on meka cart):
 *   Master D6 -> COMM PPM, Slave D10 -> COMM PPM
 *   ESC tool: Input Signal Type = PPM
 *
 * microSD (Adafruit breakout, SPI):
 *   CS   -> D7
 *   MOSI -> D11
 *   MISO -> D12
 *   SCK  -> D13
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <driver/gpio.h>
#include "FlipskyUart.h"

static const char FIRMWARE_VERSION[] = "2.3.0";

// =====================
// DRIVE OUTPUT MODE
// =====================
enum class DriveMode : uint8_t {
  PPM,
  UART_CMD32,
  UART_CURRENT,
};

static const bool ENABLE_DRIVE = true;
static const DriveMode DRIVE_MODE = DriveMode::PPM;

// =====================
// SD CARD (Adafruit microSD SPI breakout)
// =====================
static const int SD_CS_PIN = D7;
static const int SD_SCK_PIN = D13;
static const int SD_MISO_PIN = D12;
static const int SD_MOSI_PIN = D11;

// =====================
// PINS
// =====================
static const int THROTTLE_IN_PIN = D2;
static const int STEERING_IN_PIN = D3;
static const int MASTER_RX_PIN = D8;
static const int MASTER_TX_PIN = D9;
static const int SLAVE_RX_PIN = D4;
static const int SLAVE_TX_PIN = D5;
static const int MASTER_PPM_PIN = D6;
static const int SLAVE_PPM_PIN = D10;

static const unsigned long ESC_BAUD = 115200;
static const uint8_t MASTER_ESC_ID = 101;
static const uint8_t SLAVE_ESC_ID = 102;

static const int PPM_MID_US = 1500;
static const int PPM_FORWARD_RANGE_US = 400;
static const int PPM_REVERSE_RANGE_US = 144;
static const int PPM_FRAME_US = 20000;
static const int PPM_INTER_FRAME_GAP_US = 400;

static const float MAX_FORWARD_CURRENT_A = 40.0f;
static const float MAX_REVERSE_CURRENT_A = 15.0f;
static const float CURRENT_TO_MICROA = 1000000.0f;

static const bool INVERT_THROTTLE = true;
static const bool INVERT_STEERING = false;
static const bool INVERT_MASTER = false;
static const bool INVERT_SLAVE = false;

static const int RC_DEADBAND_US = 60;
static const float CMD_ZERO_THRESHOLD = 0.03f;
static const float THROTTLE_EXPO = 0.60f;
static const float STEERING_EXPO = 0.50f;
static const float THROTTLE_RAMP_STEP = 0.015f;
static const float THROTTLE_BRAKE_RAMP_STEP = 0.06f;
static const float STEERING_RAMP_STEP = 0.020f;
static const float SPEED_STEERING_GAIN = 0.30f;
static const float MAX_TURN_REDUCTION = 0.75f;
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

static const unsigned long POLL_INTERVAL_MS = 125;
static const unsigned long RESPONSE_WAIT_MS = 25;
static const unsigned long ALIVE_INTERVAL_MS = 1000;
static const unsigned long STATUS_INTERVAL_MS = 5000;
static const unsigned long FLUSH_INTERVAL_MS = 3000;
static const uint16_t FLUSH_EVERY_ROWS = 32;

// Logged control_state values (see CSV header comments).
static const uint8_t CTRL_STOP_NO_THROTTLE = 0;
static const uint8_t CTRL_NORMAL_DRIVE = 1;
static const uint8_t CTRL_STEERING_TIMEOUT = 2;
static const uint8_t CTRL_FAILSAFE_STOP = 3;

static const int32_t RC_AGE_NEVER_SEEN = -1;

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

enum class TelState : uint8_t { Idle, WaitMaster, WaitSlave };

struct EscSample {
  bool valid = false;
  int32_t rpm = 0;
  float duty = 0.0f;
  float motorCurrent = 0.0f;
  float inputCurrent = 0.0f;
  float vIn = 0.0f;
  uint8_t faultCode = 0;
};

FlipskyUart masterEsc;
FlipskyUart slaveEsc;
GpioPpmOut masterPpm;
GpioPpmOut slavePpm;

EscSample pendingMaster;
EscSample pendingSlave;

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
unsigned long telWaitStartMs = 0;

TelState telState = TelState::Idle;

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

static int logThrUs = 1500;
static int logStrUs = 1500;
static float logThrottleNorm = 0.0f;
static float logSteeringNorm = 0.0f;
static float logLeftMix = 0.0f;
static float logRightMix = 0.0f;
static int logMstPpmUs = PPM_MID_US;
static int logSlvPpmUs = PPM_MID_US;
static int32_t logThrottleAgeMs = RC_AGE_NEVER_SEEN;
static int32_t logSteeringAgeMs = RC_AGE_NEVER_SEEN;
static uint8_t logThrottleSignalValid = 0;
static uint8_t logSteeringSignalValid = 0;
static uint8_t logThrottleEverSeen = 0;
static uint8_t logSteeringEverSeen = 0;
static uint8_t logStopCommanded = 0;
static uint8_t logControlState = CTRL_STOP_NO_THROTTLE;

static unsigned long lastEsc1TelemetryMs = 0;
static unsigned long lastEsc2TelemetryMs = 0;
static unsigned long lastLogRowMs = 0;

// =====================
// HELPERS
// =====================
static void waitForUsbSerial() {
  const unsigned long deadline = millis() + 3000;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
}

static const __FlashStringHelper *driveModeLabel() {
  switch (DRIVE_MODE) {
    case DriveMode::PPM:
      return F("PPM D6/D10");
    case DriveMode::UART_CMD32:
      return F("UART cmd32");
    case DriveMode::UART_CURRENT:
      return F("UART current");
  }
  return F("unknown");
}

static const char *faultName(uint8_t code) {
  switch (code) {
    case 0:
      return "NONE";
    case 1:
      return "OVER_VOLTAGE";
    case 2:
      return "UNDER_VOLTAGE";
    case 3:
      return "DRV";
    case 4:
      return "ABS_OVER_CURRENT";
    case 5:
      return "OVER_TEMP_FET";
    case 6:
      return "OVER_TEMP_MOTOR";
    case 7:
      return "GATE_DRIVER_OVER_VOLTAGE";
    case 8:
      return "GATE_DRIVER_UNDER_VOLTAGE";
    case 9:
      return "MCU_UNDER_VOLTAGE";
    case 10:
      return "BOOTING_FROM_WATCHDOG_RESET";
    case 11:
      return "ENCODER_SPI";
    case 12:
      return "ENCODER_SINCOS_BELOW_MIN_AMPLITUDE";
    case 13:
      return "ENCODER_SINCOS_ABOVE_MAX_AMPLITUDE";
    case 14:
      return "FLASH_CORRUPTION";
    case 15:
      return "HIGH_OFFSET_CURRENT_SENSOR_1";
    case 16:
      return "HIGH_OFFSET_CURRENT_SENSOR_2";
    case 17:
      return "HIGH_OFFSET_CURRENT_SENSOR_3";
    case 18:
      return "UNBALANCED_CURRENTS";
    case 19:
      return "BRK";
    case 20:
      return "RESOLVER_LOT";
    case 21:
      return "RESOLVER_DOS";
    case 22:
      return "RESOLVER_LOS";
    case 23:
      return "FLASH_CORRUPTION_APP_CFG";
    case 24:
      return "FLASH_CORRUPTION_MC_CFG";
    case 25:
      return "ENCODER_NO_MAGNET";
    case 26:
      return "ENCODER_MAGNET_TOO_STRONG";
    case 27:
      return "PHASE_FILTER";
    case 28:
      return "ENCODER_FAULT";
    case 29:
      return "LV_OUTPUT_FAULT";
    case 30:
      return "ENCODER_SLIP";
    case 31:
      return "OVERSPEED";
    case 32:
      return "UNDERSPEED";
    case 33:
      return "ABS_OVERSPEED";
    default:
      return "UNKNOWN";
  }
}

static void captureEscSample(const FlipskyUart &esc, EscSample &out,
                             unsigned long &lastUpdateMs) {
  out.valid = true;
  out.rpm = esc.erpm();
  out.duty = esc.dutyCycle();
  out.motorCurrent = esc.motorCurrent();
  out.inputCurrent = esc.inputCurrent();
  out.vIn = esc.inputVoltage();
  out.faultCode = esc.faultCode();
  lastUpdateMs = millis();
}

static void printCsvFloat(bool valid, float value, uint8_t decimals) {
  if (!valid) {
    return;
  }
  logFile.print(value, decimals);
}

static void printCsvInt(bool valid, int32_t value) {
  if (!valid) {
    return;
  }
  logFile.print(value);
}

static void printCsvEscFields(const EscSample &sample) {
  printCsvInt(sample.valid, sample.rpm);
  logFile.print(',');
  printCsvFloat(sample.valid, sample.duty, 5);
  logFile.print(',');
  printCsvFloat(sample.valid, sample.motorCurrent, 4);
  logFile.print(',');
  printCsvFloat(sample.valid, sample.inputCurrent, 4);
  logFile.print(',');
  printCsvFloat(sample.valid, sample.vIn, 3);
  logFile.print(',');
  if (sample.valid) {
    logFile.print(sample.faultCode);
    logFile.print(',');
    logFile.print(faultName(sample.faultCode));
  } else {
    logFile.print(',');
  }
}

static unsigned long escAgeMs(unsigned long lastUpdateMs) {
  if (lastUpdateMs == 0) {
    return 0;
  }
  return millis() - lastUpdateMs;
}

static void pollEscPorts() {
  masterEsc.poll();
  slaveEsc.poll();
}

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

static void updateCommandLogSnapshot(float masterCmd, float slaveCmd) {
  logLeftMix = masterCmd;
  logRightMix = slaveCmd;
  if (DRIVE_MODE == DriveMode::PPM) {
    logMstPpmUs = cmdToPpmUs(masterCmd);
    logSlvPpmUs = cmdToPpmUs(slaveCmd);
  }
}

static void sendStop() {
  if (DRIVE_MODE == DriveMode::PPM) {
    masterPpm.writeMicroseconds(PPM_MID_US);
    slavePpm.writeMicroseconds(PPM_MID_US);
  } else if (DRIVE_MODE == DriveMode::UART_CMD32) {
    masterEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0);
    slaveEsc.setCurrentGearAndObtain(FTESC_GEAR_NEUTRAL, 0);
  } else {
    masterEsc.setCurrent(0);
    slaveEsc.setCurrent(0);
  }
  throttleCurrent = 0;
  steeringCurrent = 0;
  pivotCurrent = 0;
  logLeftMix = 0.0f;
  logRightMix = 0.0f;
  logMstPpmUs = PPM_MID_US;
  logSlvPpmUs = PPM_MID_US;
}

static void sendWheelDrive(float masterCmd, float slaveCmd) {
  updateCommandLogSnapshot(masterCmd, slaveCmd);

  if (DRIVE_MODE == DriveMode::PPM) {
    masterPpm.writeMicroseconds(logMstPpmUs);
    slavePpm.writeMicroseconds(logSlvPpmUs);
    return;
  }

  logMstPpmUs = PPM_MID_US;
  logSlvPpmUs = PPM_MID_US;

  if (DRIVE_MODE == DriveMode::UART_CMD32) {
    masterEsc.setCurrentGearAndObtain(gearForCommand(masterCmd),
                                      magnitudeMicroA(masterCmd));
    slaveEsc.setCurrentGearAndObtain(gearForCommand(slaveCmd),
                                     magnitudeMicroA(slaveCmd));
    return;
  }

  masterEsc.setCurrent(limitCurrentMicroA(masterCmd));
  slaveEsc.setCurrent(limitCurrentMicroA(slaveCmd));
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

  logFile.println(F("# GT-EV-Testbed UART telemetry + drive log"));
  logFile.print(F("# session_start_ms,"));
  logFile.println(sessionStartMs);
  logFile.print(F("# master_id,"));
  logFile.print(MASTER_ESC_ID);
  logFile.print(F(" (esc1),slave_id,"));
  logFile.print(SLAVE_ESC_ID);
  logFile.println(F(" (esc2)"));
  logFile.print(F("# firmware,"));
  logFile.println(FIRMWARE_VERSION);
  logFile.print(F("# poll_ms="));
  logFile.print(POLL_INTERVAL_MS);
  logFile.print(F(",drive="));
  logFile.println(driveModeLabel());
  logFile.println(
      F("# control_state: 0=STOP_NO_THROTTLE 1=NORMAL_DRIVE "
        "2=STEERING_TIMEOUT 3=FAILSAFE_STOP"));
  logFile.println(
      F("# throttle_age_ms,steering_age_ms: -1 if channel never seen"));
  logFile.println(
      F("# stop_commanded: 1=firmware neutral due to invalid throttle only"));
  logFile.println(
      F("time_ms,loop_dt_ms,"
        "throttle_us,steering_us,throttle_norm,steering_norm,left_mix,right_mix,"
        "master_ppm_us,slave_ppm_us,"
        "throttle_age_ms,steering_age_ms,"
        "throttle_signal_valid,steering_signal_valid,"
        "throttle_ever_seen,steering_ever_seen,"
        "stop_commanded,control_state,"
        "esc1_last_update_age_ms,esc2_last_update_age_ms,"
        "esc1_rpm,esc1_duty,esc1_motor_current,esc1_input_current,esc1_vin,"
        "esc1_fault_code,esc1_fault_name,"
        "esc2_rpm,esc2_duty,esc2_motor_current,esc2_input_current,esc2_vin,"
        "esc2_fault_code,esc2_fault_name"));
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

static void logCombinedRow(const EscSample &esc1, const EscSample &esc2) {
  if (!sdReady || !logFile) {
    return;
  }

  const unsigned long now = millis();
  const unsigned long loopDtMs =
      (lastLogRowMs == 0) ? 0 : (now - lastLogRowMs);
  lastLogRowMs = now;

  logFile.print(now);
  logFile.print(',');
  logFile.print(loopDtMs);
  logFile.print(',');
  logFile.print(logThrUs);
  logFile.print(',');
  logFile.print(logStrUs);
  logFile.print(',');
  logFile.print(logThrottleNorm, 4);
  logFile.print(',');
  logFile.print(logSteeringNorm, 4);
  logFile.print(',');
  logFile.print(logLeftMix, 4);
  logFile.print(',');
  logFile.print(logRightMix, 4);
  logFile.print(',');
  logFile.print(logMstPpmUs);
  logFile.print(',');
  logFile.print(logSlvPpmUs);
  logFile.print(',');
  logFile.print(logThrottleAgeMs);
  logFile.print(',');
  logFile.print(logSteeringAgeMs);
  logFile.print(',');
  logFile.print(logThrottleSignalValid);
  logFile.print(',');
  logFile.print(logSteeringSignalValid);
  logFile.print(',');
  logFile.print(logThrottleEverSeen);
  logFile.print(',');
  logFile.print(logSteeringEverSeen);
  logFile.print(',');
  logFile.print(logStopCommanded);
  logFile.print(',');
  logFile.print(logControlState);
  logFile.print(',');
  logFile.print(escAgeMs(lastEsc1TelemetryMs));
  logFile.print(',');
  logFile.print(escAgeMs(lastEsc2TelemetryMs));
  logFile.print(',');
  printCsvEscFields(esc1);
  logFile.print(',');
  printCsvEscFields(esc2);
  logFile.println();

  rowsWritten++;
  rowsSinceFlush++;
  flushLogIfNeeded(false);
}

static void finishTelemetryCycle() {
  logCombinedRow(pendingMaster, pendingSlave);
  pendingMaster = EscSample{};
  pendingSlave = EscSample{};
  telState = TelState::Idle;
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

static void startTelemetryCycle() {
  pendingMaster = EscSample{};
  pendingSlave = EscSample{};
  masterEsc.clearTelemetryFlag();
  masterEsc.requestTelemetry(0);
  telWaitStartMs = millis();
  telState = TelState::WaitMaster;
}

static void serviceTelemetryLogging() {
  const unsigned long now = millis();

  if (telState == TelState::Idle) {
    if (now - lastPollMs >= POLL_INTERVAL_MS) {
      lastPollMs = now;
      startTelemetryCycle();
    }
    return;
  }

  if (telState == TelState::WaitMaster) {
    if (masterEsc.hasTelemetry()) {
      captureEscSample(masterEsc, pendingMaster, lastEsc1TelemetryMs);
      masterEsc.clearTelemetryFlag();
      slaveEsc.clearTelemetryFlag();
      slaveEsc.requestTelemetry(0);
      telWaitStartMs = now;
      telState = TelState::WaitSlave;
      return;
    }
    if (now - telWaitStartMs >= RESPONSE_WAIT_MS) {
      slaveEsc.clearTelemetryFlag();
      slaveEsc.requestTelemetry(0);
      telWaitStartMs = now;
      telState = TelState::WaitSlave;
    }
    return;
  }

  if (telState == TelState::WaitSlave) {
    if (slaveEsc.hasTelemetry()) {
      captureEscSample(slaveEsc, pendingSlave, lastEsc2TelemetryMs);
      slaveEsc.clearTelemetryFlag();
      finishTelemetryCycle();
      return;
    }
    if (now - telWaitStartMs >= RESPONSE_WAIT_MS) {
      finishTelemetryCycle();
    }
  }
}

static void updateDrive() {
  const unsigned long nowUs = micros();
  const unsigned long thrAge =
      throttleEver ? (nowUs - lastThrottleUpdateUs) / 1000UL : ULONG_MAX;
  const unsigned long strAge =
      steeringEver ? (nowUs - lastSteeringUpdateUs) / 1000UL : ULONG_MAX;

  logThrUs = throttlePulse;
  logStrUs = steeringPulse;

  const bool throttleOk = throttleEver && thrAge <= SIGNAL_TIMEOUT_MS;
  const bool steeringOk = steeringEver && strAge <= SIGNAL_TIMEOUT_MS;

  logThrottleSignalValid = throttleOk ? 1 : 0;
  logSteeringSignalValid = steeringOk ? 1 : 0;
  logThrottleEverSeen = throttleEver ? 1 : 0;
  logSteeringEverSeen = steeringEver ? 1 : 0;
  logThrottleAgeMs = throttleEver ? (int32_t)thrAge : RC_AGE_NEVER_SEEN;
  logSteeringAgeMs = steeringEver ? (int32_t)strAge : RC_AGE_NEVER_SEEN;
  logStopCommanded = (ENABLE_DRIVE && !throttleOk) ? 1 : 0;

  if (!throttleOk) {
    logControlState =
        throttleEver ? CTRL_FAILSAFE_STOP : CTRL_STOP_NO_THROTTLE;
  } else if (!steeringOk) {
    logControlState = CTRL_STEERING_TIMEOUT;
  } else {
    logControlState = CTRL_NORMAL_DRIVE;
  }

  const float throttleRaw = pulseToNorm(throttlePulse, INVERT_THROTTLE);
  const float steeringRaw =
      steeringOk ? pulseToNorm(steeringPulse, INVERT_STEERING) : 0.0f;
  logThrottleNorm = throttleRaw;
  logSteeringNorm = steeringRaw;

  if (!ENABLE_DRIVE) {
    return;
  }

  if (!throttleOk) {
    sendStop();
    return;
  }

  const float throttle =
      applyExpo(throttleRaw, THROTTLE_EXPO);
  float steering = 0.0f;
  if (steeringOk) {
    steering = applyExpo(steeringRaw, STEERING_EXPO);
  }

  const bool sticksCentered = fabsf(throttle) < CMD_ZERO_THRESHOLD &&
                              fabsf(steering) < CMD_ZERO_THRESHOLD;

  float masterCmd = 0;
  float slaveCmd = 0;

  if (sticksCentered) {
    sendStop();
    return;
  }

  throttleCurrent = rampThrottle(throttleCurrent, throttle);
  if (steeringOk) {
    steeringCurrent = rampToward(steeringCurrent, steering, STEERING_RAMP_STEP);
  } else {
    steeringCurrent = rampToward(steeringCurrent, 0.0f, STEERING_RAMP_STEP);
  }

  computeWheelCommands(masterCmd, slaveCmd);
  sendWheelDrive(masterCmd, slaveCmd);
}

// =====================
// ARDUINO
// =====================
void setup() {
  Serial.begin(115200);
  waitForUsbSerial();
  sessionStartMs = millis();

  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);

  masterEsc.begin(Serial1, ESC_BAUD, MASTER_RX_PIN, MASTER_TX_PIN);
  slaveEsc.begin(Serial2, ESC_BAUD, SLAVE_RX_PIN, SLAVE_TX_PIN);

  Serial.println();
  Serial.print(F("Firmware v"));
  Serial.println(FIRMWARE_VERSION);
  Serial.println(F("=== UART Telemetry SD Logger + Drive (Nano ESP32) ==="));
  Serial.print(F("Drive: "));
  Serial.println(ENABLE_DRIVE ? driveModeLabel() : F("disabled"));
  Serial.println(F("UART: master D9->RX D8<-TX | slave D5->RX D4<-TX"));
  if (ENABLE_DRIVE && DRIVE_MODE == DriveMode::PPM) {
    Serial.println(F("PPM: D6 -> master COMM PPM | D10 -> slave COMM PPM"));
    Serial.println(F("ESC tool: Input Signal Type = PPM"));
  } else if (ENABLE_DRIVE) {
    Serial.println(F("ESC tool: Input Signal Type = UART"));
  }
  Serial.println(F("RC: throttle D2, steering D3"));
  Serial.println(F("SD: CS=D7 MOSI=D11 MISO=D12 SCK=D13"));
  Serial.println();

  if (ENABLE_DRIVE && DRIVE_MODE == DriveMode::PPM) {
    const bool masterOk = masterPpm.begin(MASTER_PPM_PIN);
    const bool slaveOk = slavePpm.begin(SLAVE_PPM_PIN);
    Serial.print(F("PPM outputs: "));
    Serial.println(masterOk && slaveOk ? F("OK") : F("FAIL"));
    masterPpm.writeMicroseconds(PPM_MID_US);
    slavePpm.writeMicroseconds(PPM_MID_US);
  }

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
  Serial.println(F("Drive + record... (power cycle for new CSV file)"));
  Serial.println();
}

void loop() {
  static unsigned long lastPpmFrameUs = 0;

  pollEscPorts();
  pollOneRcChannel();
  updateDrive();
  serviceTelemetryLogging();

  const unsigned long now = millis();
  const unsigned long nowUs = micros();

  if (ENABLE_DRIVE && DRIVE_MODE == DriveMode::PPM) {
    if (nowUs - lastPpmFrameUs >= (unsigned long)PPM_FRAME_US) {
      lastPpmFrameUs = nowUs;
      servicePpmOutputs();
    }
  } else {
    delay(2);
  }

  if (now - lastAliveMs >= ALIVE_INTERVAL_MS) {
    lastAliveMs = now;
    masterEsc.sendAlive();
    slaveEsc.sendAlive();
  }

  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    Serial.print(F("rows="));
    Serial.print(rowsWritten);
    Serial.print(F(" thr="));
    Serial.print(logThrUs);
    Serial.print(F(" str="));
    Serial.print(logStrUs);
    Serial.print(F(" ppm M/S="));
    Serial.print(logMstPpmUs);
    Serial.print('/');
    Serial.print(logSlvPpmUs);
    Serial.print(F(" RX M="));
    Serial.print(masterEsc.rxBytes());
    Serial.print(F(" S="));
    Serial.println(slaveEsc.rxBytes());
    flushLogIfNeeded(true);
  }
}
