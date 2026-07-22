// ═══════════════════════════════════════════════════════════
// 28BYJ-48 Stepper Motor — Standalone Test
// ATmega328P bare + HW-095 (L298N)
// Continuously rotates +90° then -90° with a 2 second pause
// ═══════════════════════════════════════════════════════════

// ── Pin assignments ──────────────────────────────────────────
const int IN1 = 8;   // ATmega PB0 pin 14
const int IN2 = 9;   // ATmega PB1 pin 15
const int IN3 = 10;  // ATmega PB2 pin 16
const int IN4 = 11;  // ATmega PB3 pin 17

// ── Bipolar half-step sequence for L298N ─────────────────────
const int halfStep[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

const int STEPS_PER_90DEG = 1024;
const int STEP_DELAY_US   = 2000;  // 1ms per step = ~1 sec for 90°
int currentStep           = 0;

// ── Step the motor one position ───────────────────────────────
void stepMotor(int index) {
  digitalWrite(IN1, halfStep[index][0]);
  digitalWrite(IN2, halfStep[index][1]);
  digitalWrite(IN3, halfStep[index][2]);
  digitalWrite(IN4, halfStep[index][3]);
  delayMicroseconds(STEP_DELAY_US);
}

// ── De-energize all coils ─────────────────────────────────────
void releaseCoils() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ── Rotate +90° (clockwise) ───────────────────────────────────
void rotatePlus90() {
  for (int i = 0; i < STEPS_PER_90DEG; i++) {
    currentStep = (currentStep + 1) % 8;
    stepMotor(currentStep);
  }
  releaseCoils();
}

// ── Rotate -90° (counter-clockwise) ──────────────────────────
void rotateMinus90() {
  for (int i = 0; i < STEPS_PER_90DEG; i++) {
    currentStep = (currentStep + 7) % 8;
    stepMotor(currentStep);
  }
  releaseCoils();
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

// ── Loop: continuously cycles +90 then -90 ───────────────────
void loop() {
  rotatePlus90();
  delay(2000);
  rotateMinus90();
  delay(2000);
}