const int SWITCH_PIN = 2;  // ATmega pin 4 (PD2)

int score = 0;
bool lastState = HIGH;

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  bool currentState = digitalRead(SWITCH_PIN);

  // Switch just closed (pressed)
  if (currentState == LOW && lastState == HIGH) {
    score++;
    Serial.print("Switch triggered! Score: ");
    Serial.println(score);
    delay(50); // debounce
  }

  lastState = currentState;
}