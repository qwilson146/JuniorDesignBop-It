#define newLED 13

void setup() {
  pinMode(newLED, OUTPUT);
}

void loop() {
  digitalWrite(newLED, HIGH);
  delay(1000); 
  digitalWrite(newLED, LOW);
  delay(1000);

}
