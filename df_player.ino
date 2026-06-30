#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mySoftwareSerial(10, 11);  // RX, TX
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  pinMode(13, OUTPUT);

  // Confirm chip is running
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);

  mySoftwareSerial.begin(9600);
  myDFPlayer.begin(mySoftwareSerial);  // no check, just initialize

  myDFPlayer.volume(30);
  delay(500);

  myDFPlayer.play(1);
  delay(2000);  // adjust to match length of track 1

  myDFPlayer.play(2);
  delay(2000);  // adjust to match length of track 2

  myDFPlayer.play(3);
  delay(2000);  // adjust to match length of track 3
}

void loop() {
  // keep blinking continuously so you know the chip is alive
  digitalWrite(13, HIGH);
  delay(300);
  digitalWrite(13, LOW);
  delay(300);
}