#include <Servo.h>

// Current Reverse controller placeholder.
// Current Reverse commands torque/current rather than speed.
// Kept as fallback / comparison firmware.

Servo masterESC;
Servo slaveESC;

const int MASTER_ESC_PIN = 9;
const int SLAVE_ESC_PIN  = 10;
const int PPM_MID = 1500;

void setup() {
  masterESC.attach(MASTER_ESC_PIN);
  slaveESC.attach(SLAVE_ESC_PIN);

  masterESC.writeMicroseconds(PPM_MID);
  slaveESC.writeMicroseconds(PPM_MID);
}

void loop() {
  masterESC.writeMicroseconds(PPM_MID);
  slaveESC.writeMicroseconds(PPM_MID);
  delay(20);
}
