// RC Channel Diagnostic
// Reads Radiolink RC CH2 on D2 and CH4 on D3.
// Does not drive ESCs.

const int CH2_PIN = 2;
const int CH4_PIN = 3;

volatile unsigned long ch2Rise = 0;
volatile unsigned long ch4Rise = 0;

volatile int ch2Pulse = 1500;
volatile int ch4Pulse = 1500;

void ch2ISR() {
  if (digitalRead(CH2_PIN)) ch2Rise = micros();
  else {
    unsigned long w = micros() - ch2Rise;
    if (w >= 900 && w <= 2100) ch2Pulse = w;
  }
}

void ch4ISR() {
  if (digitalRead(CH4_PIN)) ch4Rise = micros();
  else {
    unsigned long w = micros() - ch4Rise;
    if (w >= 900 && w <= 2100) ch4Pulse = w;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH4_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH4_PIN), ch4ISR, CHANGE);
  Serial.println("RC channel diagnostic only. ESC outputs disabled.");
}

void loop() {
  noInterrupts();
  int ch2 = ch2Pulse;
  int ch4 = ch4Pulse;
  interrupts();

  Serial.print("D2 / RC CH2 = ");
  Serial.print(ch2);
  Serial.print(" | D3 / RC CH4 = ");
  Serial.println(ch4);
  delay(200);
}
