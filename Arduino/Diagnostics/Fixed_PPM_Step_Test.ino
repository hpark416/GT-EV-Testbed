#include <Servo.h>

// Fixed PPM Step Test
// Sends identical pulses to both ESCs to isolate Arduino mixer from ESC behavior.

Servo masterESC;
Servo slaveESC;

const int MASTER_ESC_PIN = 9;
const int SLAVE_ESC_PIN  = 10;

void setup() {
  masterESC.attach(MASTER_ESC_PIN);
  slaveESC.attach(SLAVE_ESC_PIN);
  masterESC.writeMicroseconds(1500);
  slaveESC.writeMicroseconds(1500);
  delay(5000);
}

void loop() {
  masterESC.writeMicroseconds(1700);
  slaveESC.writeMicroseconds(1700);
  delay(3000);

  masterESC.writeMicroseconds(1500);
  slaveESC.writeMicroseconds(1500);
  delay(3000);

  masterESC.writeMicroseconds(1300);
  slaveESC.writeMicroseconds(1300);
  delay(3000);

  masterESC.writeMicroseconds(1500);
  slaveESC.writeMicroseconds(1500);
  delay(3000);
}
