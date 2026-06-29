// ═══════════════════════════════════════════════════════════════════
// Pitt BOP IT! — v5  (dual OLED, two I2C buses, both at 0x3C)
// Target : ATmega1284P bare metal  (MightyCore "Standard" pinout)
// Team   : Maddie Dimarco · Quentin Wilson · Jack Wasco · Jasmine Horton
//
// ── PIN MAP ────────────────────────────────────────────────────────
//  Component                  Arduino  Port   Physical
//  ──────────────────────────────────────────────────────
//  Stepper IN1                0        PB0    1
//  Stepper IN2                1        PB1    2
//  Stepper IN3                2        PB2    3
//  Stepper IN4                3        PB3    4
//  Encoder CLK                4        PB4    5
//  Encoder DT                 5        PB5    6
//  Encoder SW (button)        6        PB6    7
//  Serial1 RX1 ← DFPlayer TX  10       PD2    16
//  Serial1 TX1 → 1kΩ → DFPlayer RX  11  PD3  17
//  Switch (INPUT_PULLUP)      12       PD4    18
//  RGB Red   (220Ω to GND)    13       PD5    19
//  RGB Green (220Ω to GND)    14       PD6    20
//  RGB Blue  (220Ω to GND)    15       PD7    21
//  OLED1 SCL  hw I2C bus 1    16       PC0    22   ← challenge display
//  OLED1 SDA  hw I2C bus 1    17       PC1    23   ← challenge display
//  OLED2 SCL  sw I2C bus 2    18       PC2    24   ← score display
//  OLED2 SDA  sw I2C bus 2    19       PC3    25   ← score display
//  Keypad col C1              24       PA0    40
//  Keypad col C2              25       PA1    39
//  Keypad col C3              26       PA2    38
//  Keypad col C4              27       PA3    37
//  Keypad row R1              28       PA4    36
//  Keypad row R2              29       PA5    35
//  Keypad row R3              30       PA6    34
//  Keypad row R4              31       PA7    33
//
// ── OLED WIRING ────────────────────────────────────────────────────
//  Both OLEDs are address 0x3C — they run on separate I2C buses
//  so there is no address conflict.
//
//  OLED 1 (challenge display):
//    SDA → PC1  physical pin 23   (hardware I2C, Wire)
//    SCL → PC0  physical pin 22   (hardware I2C, Wire)
//
//  OLED 2 (score display):
//    SDA → PC3  physical pin 25   (software I2C, Wire1)
//    SCL → PC2  physical pin 24   (software I2C, Wire1)
//
//  Both: VCC → 5V, GND → GND
//  Keep jumpers as-is on both boards — no changes needed.
//
// ── LIBRARY NEEDED ─────────────────────────────────────────────────
//  Install "SlowSoftWire" by Peter Fleury via Arduino Library Manager
//  It provides a Wire-compatible software I2C on any two GPIO pins.
//
// ── DFPlayer SD card  (/mp3/ folder, FAT32) ───────────────────────
//  0001.mp3  "Hack it!"
//  0002.mp3  "Pull it!"
//  0003.mp3  "Crack it!"
//  0004.mp3  police siren  (fail)
//  0005.mp3  cash / win    (score == 99)
//
// ── DFPlayer wiring ────────────────────────────────────────────────
//  VCC → 5 V,  GND → GND,  100 µF cap across VCC/GND recommended
//  TX  → 1284P pin 16 (RX1) direct
//  RX  ← 1 kΩ ← 1284P pin 17 (TX1)
//  SPK1/SPK2 → 8 Ω speaker
//
// ── RGB LED (common cathode — HIGH = on) ──────────────────────────
//  Fail  → alternates RED / BLUE  (police siren)
//  Pass  → brief GREEN flash then off
//  Win   → solid GREEN (score == 99)
// ═══════════════════════════════════════════════════════════════════

#include <Wire.h>              // hardware I2C — bus 1 (challenge OLED)
#include <SlowSoftWire.h>      // software I2C — bus 2 (score OLED)
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <DFRobotDFPlayerMini.h>

// ── Software I2C bus for score display ───────────────────────────
// SlowSoftWire(SDA_pin, SCL_pin, enable_internal_pullups)
// PC2 = Arduino 18 = physical 24 (SCL2)
// PC3 = Arduino 19 = physical 25 (SDA2)
SlowSoftWire Wire1(19, 18, true);   // SDA=19, SCL=18

// ── Two OLED displays ─────────────────────────────────────────────
// dispChallenge : Wire  (hardware I2C, PC0/PC1, pins 22/23)
// dispScore     : Wire1 (software I2C, PC2/PC3, pins 24/25)
// Both at address 0x3C — no conflict since they're on different buses
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_ADDR     0x3C

Adafruit_SSD1306 dispChallenge(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire,  -1);
Adafruit_SSD1306 dispScore    (SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

// ── Keypad ───────────────────────────────────────────────────────
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {28, 29, 30, 31};  // PA4–PA7
byte colPins[COLS] = {24, 25, 26, 27};  // PA0–PA3
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── DFPlayer ─────────────────────────────────────────────────────
DFRobotDFPlayerMini dfPlayer;
#define AUDIO_HACK  1
#define AUDIO_PULL  2
#define AUDIO_CRACK 3
#define AUDIO_FAIL  4
#define AUDIO_WIN   5

// ── Rotary encoder ───────────────────────────────────────────────
#define ENC_CLK 4
#define ENC_DT  5
#define ENC_SW  6

// ── Switch ───────────────────────────────────────────────────────
#define SWITCH_PIN 12

// ── RGB LED (common cathode) ─────────────────────────────────────
#define RGB_R 13
#define RGB_G 14
#define RGB_B 15

// ── Stepper ──────────────────────────────────────────────────────
#define IN1 0
#define IN2 1
#define IN3 2
#define IN4 3
const int halfStep[8][4] = {
  {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
  {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
};
const int STEPS_PER_REV = 4096;
const int STEP_DELAY_US = 2000;
int stepperPos = 0;

// ── Secret keypad code ───────────────────────────────────────────
const char* SECRET_CODE = "1683";

// ── Game state ───────────────────────────────────────────────────
enum GameState {
  IDLE,
  WAITING_KEYPAD,
  WAITING_SWITCH,
  WAITING_ENCODER,
  GAME_OVER,
  GAME_WIN
};
GameState state = IDLE;

int           score          = 0;
unsigned long actionDeadline = 0;
unsigned long ACTION_TIMEOUT = 5000;

// Keypad vars
char keypadEntry[17] = "";
byte keypadLen       = 0;

// Switch vars
bool switchLastState = HIGH;

// Encoder vars
int  encTargetCW  = 0;
int  encTargetCCW = 0;
int  encCountCW   = 0;
int  encCountCCW  = 0;
bool encPhaseCW   = true;
bool encLastCLK   = HIGH;

// ═══════════════════════════════════════════════════════════════════
//  RGB helpers
// ═══════════════════════════════════════════════════════════════════
void rgbOff() {
  digitalWrite(RGB_R, LOW);
  digitalWrite(RGB_G, LOW);
  digitalWrite(RGB_B, LOW);
}

void rgbSet(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r);
  digitalWrite(RGB_G, g);
  digitalWrite(RGB_B, b);
}

void flashPassLED() {
  rgbSet(0,1,0); delay(300);
  rgbOff();      delay(100);
  rgbSet(0,1,0); delay(300);
  rgbOff();
}

void flashFailLED(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    rgbSet(1,0,0); delay(120);
    rgbSet(0,0,1); delay(120);
  }
  rgbOff();
}

void setWinLED() { rgbSet(0,1,0); }

// ═══════════════════════════════════════════════════════════════════
//  Stepper helpers
// ═══════════════════════════════════════════════════════════════════
void stepOnce(int dir) {
  stepperPos = (stepperPos + dir + 8) % 8;
  digitalWrite(IN1, halfStep[stepperPos][0]);
  digitalWrite(IN2, halfStep[stepperPos][1]);
  digitalWrite(IN3, halfStep[stepperPos][2]);
  digitalWrite(IN4, halfStep[stepperPos][3]);
  delayMicroseconds(STEP_DELAY_US);
}

void releaseCoils() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void unlockSafe() {
  for (int i = 0; i < STEPS_PER_REV; i++) stepOnce(1);
  releaseCoils();
}

// ═══════════════════════════════════════════════════════════════════
//  Score display  (dispScore — Wire1, software I2C, PC2/PC3)
// ═══════════════════════════════════════════════════════════════════
void scoreShowIdle() {
  dispScore.clearDisplay();
  dispScore.setTextColor(SSD1306_WHITE);
  dispScore.setTextSize(1);
  dispScore.setCursor(0,  0); dispScore.println("BOP IT!");
  dispScore.setCursor(0, 16); dispScore.println("Press * to");
  dispScore.setCursor(0, 28); dispScore.println("start");
  dispScore.display();
}

void scoreShowScore() {
  dispScore.clearDisplay();
  dispScore.setTextColor(SSD1306_WHITE);
  dispScore.setTextSize(1);
  dispScore.setCursor(0,  0); dispScore.println("Score:");
  dispScore.setTextSize(4);
  dispScore.setCursor(0, 16); dispScore.println(score);
  dispScore.display();
}

void scoreShowGameOver() {
  dispScore.clearDisplay();
  dispScore.setTextColor(SSD1306_WHITE);
  dispScore.setTextSize(2);
  dispScore.setCursor(0,  0); dispScore.println("GAME");
  dispScore.setCursor(0, 20); dispScore.println("OVER");
  dispScore.setTextSize(1);
  dispScore.setCursor(0, 46); dispScore.print("Score: ");
  dispScore.println(score);
  dispScore.display();
}

void scoreShowWin() {
  dispScore.clearDisplay();
  dispScore.setTextColor(SSD1306_WHITE);
  dispScore.setTextSize(2);
  dispScore.setCursor(0,  0); dispScore.println("YOU");
  dispScore.setCursor(0, 20); dispScore.println("WIN!");
  dispScore.setTextSize(1);
  dispScore.setCursor(0, 46); dispScore.println("Score: 99");
  dispScore.display();
}

// ═══════════════════════════════════════════════════════════════════
//  Challenge display  (dispChallenge — Wire, hardware I2C, PC0/PC1)
// ═══════════════════════════════════════════════════════════════════
void challengeShowIdle() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 20); dispChallenge.println("Waiting...");
  dispChallenge.display();
}

void challengeShowHack() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2);
  dispChallenge.setCursor(0,  0); dispChallenge.println("HACK IT!");
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 20); dispChallenge.println("Enter code + #");
  dispChallenge.setCursor(0, 32); dispChallenge.println("* to clear");
  dispChallenge.setTextSize(2);
  dispChallenge.setCursor(0, 46); dispChallenge.println(keypadEntry);
  dispChallenge.display();
}

void challengeShowPull() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2);
  dispChallenge.setCursor(0,  0); dispChallenge.println("PULL IT!");
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 24); dispChallenge.println("Hit the switch!");
  dispChallenge.display();
}

void challengeShowCrack() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2);
  dispChallenge.setCursor(0, 0); dispChallenge.println("CRACK IT!");
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 18);
  dispChallenge.print("CW:"); dispChallenge.print(encTargetCW);
  dispChallenge.print("  CCW:"); dispChallenge.println(encTargetCCW);
  dispChallenge.setCursor(0, 30);
  if (encPhaseCW) {
    dispChallenge.print(">>> CW: ");
    dispChallenge.print(encCountCW);
    dispChallenge.print("/");
    dispChallenge.println(encTargetCW);
  } else {
    dispChallenge.print(">>> CCW: ");
    dispChallenge.print(encCountCCW);
    dispChallenge.print("/");
    dispChallenge.println(encTargetCCW);
  }
  int total = encTargetCW + encTargetCCW;
  int done  = encPhaseCW ? encCountCW : (encTargetCW + encCountCCW);
  int barW  = (int)(done * 124.0 / total);
  dispChallenge.drawRect(2, 50, 124, 10, SSD1306_WHITE);
  if (barW > 0) dispChallenge.fillRect(2, 50, barW, 10, SSD1306_WHITE);
  dispChallenge.display();
}

void challengeShowGameOver() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 20); dispChallenge.println("Press * to");
  dispChallenge.setCursor(0, 32); dispChallenge.println("play again");
  dispChallenge.display();
}

void challengeShowWin() {
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1);
  dispChallenge.setCursor(0, 16); dispChallenge.println("Safe unlocked!");
  dispChallenge.setCursor(0, 32); dispChallenge.println("Press * to");
  dispChallenge.setCursor(0, 44); dispChallenge.println("play again");
  dispChallenge.display();
}

// ═══════════════════════════════════════════════════════════════════
//  Encoder direction reader  (+1 CW, -1 CCW, 0 nothing)
// ═══════════════════════════════════════════════════════════════════
int readEncoder() {
  bool clk = digitalRead(ENC_CLK);
  if (clk == encLastCLK) return 0;
  encLastCLK = clk;
  if (clk == LOW) {
    return (digitalRead(ENC_DT) == HIGH) ? +1 : -1;
  }
  return 0;
}

// ═══════════════════════════════════════════════════════════════════
//  Challenge launchers
// ═══════════════════════════════════════════════════════════════════
void startKeypadChallenge() {
  keypadLen = 0; keypadEntry[0] = '\0';
  state          = WAITING_KEYPAD;
  actionDeadline = millis() + ACTION_TIMEOUT;
  dfPlayer.play(AUDIO_HACK);
  challengeShowHack();
}

void startSwitchChallenge() {
  switchLastState = digitalRead(SWITCH_PIN);
  state           = WAITING_SWITCH;
  actionDeadline  = millis() + ACTION_TIMEOUT;
  dfPlayer.play(AUDIO_PULL);
  challengeShowPull();
}

void startEncoderChallenge() {
  encTargetCW  = random(2, 8);
  encTargetCCW = random(2, 8);
  encCountCW   = 0;
  encCountCCW  = 0;
  encPhaseCW   = true;
  encLastCLK   = digitalRead(ENC_CLK);
  state          = WAITING_ENCODER;
  actionDeadline = millis() + ACTION_TIMEOUT;
  dfPlayer.play(AUDIO_CRACK);
  challengeShowCrack();
}

void pickNextChallenge() {
  switch (random(3)) {
    case 0: startKeypadChallenge();  break;
    case 1: startSwitchChallenge();  break;
    case 2: startEncoderChallenge(); break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  Pass / Fail
// ═══════════════════════════════════════════════════════════════════
void onPass() {
  score++;

  if (score >= 99) {
    state = GAME_WIN;
    dfPlayer.play(AUDIO_WIN);
    setWinLED();
    scoreShowWin();
    challengeShowWin();
    delay(500);
    unlockSafe();
    return;
  }

  flashPassLED();

  if (score % 5 == 0 && ACTION_TIMEOUT > 2000)
    ACTION_TIMEOUT -= 500;

  scoreShowScore();
  delay(500);
  pickNextChallenge();
}

void onFail() {
  state = GAME_OVER;
  dfPlayer.play(AUDIO_FAIL);
  scoreShowGameOver();
  challengeShowGameOver();
  flashFailLED(4000);
}

// ═══════════════════════════════════════════════════════════════════
//  Setup
// ═══════════════════════════════════════════════════════════════════
void setup() {
  // Stepper
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  releaseCoils();

  // Encoder
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  encLastCLK = digitalRead(ENC_CLK);

  // Switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // RGB LED
  pinMode(RGB_R, OUTPUT); pinMode(RGB_G, OUTPUT); pinMode(RGB_B, OUTPUT);
  rgbOff();

  // DFPlayer on Serial1
  Serial1.begin(9600);
  if (!dfPlayer.begin(Serial1)) {
    for (int i = 0; i < 10; i++) {
      rgbSet(1,0,0); delay(150);
      rgbOff();      delay(150);
    }
    for (;;);
  }
  dfPlayer.volume(25);

  // Hardware I2C bus (Wire) — challenge display
  Wire.begin();

  // Software I2C bus (Wire1 via SlowSoftWire) — score display
  Wire1.begin();

  // Init both OLEDs — same address 0x3C, different buses
  if (!dispChallenge.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) for (;;);
  if (!dispScore.begin(SSD1306_SWITCHCAPVCC,     OLED_ADDR)) for (;;);

  randomSeed(analogRead(0));

  scoreShowIdle();
  challengeShowIdle();
  state = IDLE;
}

// ═══════════════════════════════════════════════════════════════════
//  Main loop
// ═══════════════════════════════════════════════════════════════════
void loop() {
  char key = keypad.getKey();

  // ── IDLE ────────────────────────────────────────────────────
  if (state == IDLE) {
    if (key == '*') {
      score          = 0;
      ACTION_TIMEOUT = 5000;
      rgbOff();
      scoreShowScore();
      challengeShowIdle();
      delay(500);
      pickNextChallenge();
    }
    return;
  }

  // ── GAME WIN ────────────────────────────────────────────────
  if (state == GAME_WIN) {
    if (key == '*') {
      state = IDLE;
      rgbOff();
      scoreShowIdle();
      challengeShowIdle();
    }
    return;
  }

  // ── GAME OVER ───────────────────────────────────────────────
  if (state == GAME_OVER) {
    if (key == '*') {
      state = IDLE;
      rgbOff();
      scoreShowIdle();
      challengeShowIdle();
    }
    return;
  }

  // ── Timeout ─────────────────────────────────────────────────
  if (millis() > actionDeadline) {
    onFail();
    return;
  }

  // ── WAITING_KEYPAD ──────────────────────────────────────────
  if (state == WAITING_KEYPAD) {
    if (key) {
      if (key == '*') {
        keypadLen = 0; keypadEntry[0] = '\0';
      } else if (key == '#') {
        if (strcmp(keypadEntry, SECRET_CODE) == 0) onPass();
        else                                        onFail();
        return;
      } else if (keypadLen < 16) {
        keypadEntry[keypadLen++] = key;
        keypadEntry[keypadLen]   = '\0';
      }
      challengeShowHack();
    }
  }

  // ── WAITING_SWITCH ──────────────────────────────────────────
  if (state == WAITING_SWITCH) {
    bool cur = digitalRead(SWITCH_PIN);
    if (cur == LOW && switchLastState == HIGH) {
      delay(50);
      onPass();
      return;
    }
    switchLastState = cur;
  }

  // ── WAITING_ENCODER ─────────────────────────────────────────
  if (state == WAITING_ENCODER) {
    int dir = readEncoder();

    if (encPhaseCW) {
      if (dir == +1) {
        encCountCW++;
        challengeShowCrack();
        if (encCountCW >= encTargetCW) {
          encPhaseCW = false;
          challengeShowCrack();
        }
      } else if (dir == -1) {
        encCountCW = 0;
        challengeShowCrack();
      }
    } else {
      if (dir == -1) {
        encCountCCW++;
        challengeShowCrack();
        if (encCountCCW >= encTargetCCW) {
          onPass();
          return;
        }
      } else if (dir == +1) {
        encCountCCW = 0;
        challengeShowCrack();
      }
    }

    if (digitalRead(ENC_SW) == LOW) {
      delay(50);
      if (!encPhaseCW && encCountCCW >= encTargetCCW) onPass();
      while (digitalRead(ENC_SW) == LOW);
    }
  }
}
