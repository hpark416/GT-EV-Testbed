#include <Servo.h>

// RC Pass-Through Test
// Passes CH2/D2 directly to Master/D9 and CH4/D3 directly to Slave/D10.

const int THROTTLE_IN_PIN = 2;
const int STEERING_IN_PIN = 3;

const int MASTER_ESC_PIN = 9;
const int SLAVE_ESC_PIN  = 10;

Servo masterESC;
Servo slaveESC;

volatile unsigned long throttleRise = 0;
volatile unsigned long steeringRise = 0;
volatile int throttlePulse = 1500;
volatile int steeringPulse = 1500;

void throttleISR() {
  if (digitalRead(THROTTLE_IN_PIN)) throttleRise = micros();
  else {
    unsigned long w = micros() - throttleRise;
    if (w >= 900 && w <= 2100) throttlePulse = w;
  }
}

void steeringISR() {
  if (digitalRead(STEERING_IN_PIN)) steeringRise = micros();
  else {
    unsigned long w = micros() - steeringRise;
    if (w >= 900 && w <= 2100) steeringPulse = w;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(THROTTLE_IN_PIN, INPUT);
  pinMode(STEERING_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_IN_PIN), throttleISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(STEERING_IN_PIN), steeringISR, CHANGE);

  masterESC.attach(MASTER_ESC_PIN);
  slaveESC.attach(SLAVE_ESC_PIN);
  masterESC.writeMicroseconds(1500);
  slaveESC.writeMicroseconds(1500);
  delay(3000);
  Serial.println("RC pass-through test");
}

void loop() {
  noInterrupts();
  int thr = throttlePulse;
  int str = steeringPulse;
  interrupts();

  masterESC.writeMicroseconds(thr);
  slaveESC.writeMicroseconds(str);

  Serial.print("THR CH2/D2: ");
  Serial.print(thr);
  Serial.print(" -> MASTER D9: ");
  Serial.print(thr);
  Serial.print(" | STR CH4/D3: ");
  Serial.print(str);
  Serial.print(" -> SLAVE D10: ");
  Serial.println(str);
  delay(50);
}
