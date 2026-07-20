#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>

// ── Display 1: CODE (hardware I2C, physical pins 22/23) ──────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 dispCode(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Display 2: SCORE (bit-bang I2C, physical pins 24/25) ─────────
#define BB_SCL 18
#define BB_SDA 19
#define BB_DELAY 10

// ── Limit switch (physical pin 18 = PD4) ─────────────────────────
#define LIMIT_SWITCH_PIN 12

// ── Rotary encoder (physical pin 5 = PB4, physical pin 6 = PB5) ──
#define ENCODER_CLK 4
#define ENCODER_DT 5

#define DETENTS_PER_REV 20
#define COUNTS_PER_REV (DETENTS_PER_REV * 2)
#define TOLERANCE 4

volatile int encoderPos = 0;
volatile int lastCLK = HIGH;
unsigned long actionStartTime = 0;
unsigned long ACTION_TIMEOUT = 8000;
const unsigned long MIN_TIMEOUT = 2000;
const unsigned long TIMEOUT_STEP = 300;

ISR(PCINT1_vect) {
  int clk = digitalRead(ENCODER_CLK);
  int dt = digitalRead(ENCODER_DT);
  if (clk != lastCLK) {
    encoderPos += (dt != clk) ? 1 : -1;
    lastCLK = clk;
  }
}

// ── DFPlayer (hardware UART1, physical pins 16/17 = PD2/PD3) ─────
DFRobotDFPlayerMini myDFPlayer;

#define TRACK_HACK 1
#define TRACK_CRACK 2
#define TRACK_PULL 3
#define TRACK_LOSE 4
#define TRACK_WIN 5
#define TRACK_BEEP 6

unsigned long lastBeepTime = 0;

// ── Win thresholds / demo mode ───────────────────────────────────
#define WIN_NORMAL 99
#define WIN_DEMO   3
#define DEMO_CODE  "AAAA"
int winThreshold = WIN_NORMAL;
bool demoMode = false;   // tracks current mode so AAAA can toggle it

// ── RGB LED ───────────────────────────────────────────────────────
#define LED_RED 13
#define LED_GREEN 14
#define LED_BLUE 15

void ledOff() {
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
}

void flashWinLED() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_GREEN, HIGH);
    delay(200);
    digitalWrite(LED_GREEN, LOW);
    delay(200);
  }
  ledOff();
}

void flashLoseLED() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    delay(200);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, HIGH);
    delay(200);
  }
  ledOff();
}

// ── Stepper motor (physical pins 1-4 = PB0-PB3) ──────────────────
#define IN1 0
#define IN2 1
#define IN3 2
#define IN4 3

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
const int STEP_DELAY_US = 2000;
int currentStep = 0;

void stepMotor(int index) {
  digitalWrite(IN1, halfStep[index][0]);
  digitalWrite(IN2, halfStep[index][1]);
  digitalWrite(IN3, halfStep[index][2]);
  digitalWrite(IN4, halfStep[index][3]);
  delayMicroseconds(STEP_DELAY_US);
}

void releaseCoils() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void rotatePlus90() {
  for (int i = 0; i < STEPS_PER_90DEG; i++) {
    currentStep = (currentStep + 1) % 8;
    stepMotor(currentStep);
  }
  releaseCoils();
}

void rotateMinus90() {
  for (int i = 0; i < STEPS_PER_90DEG; i++) {
    currentStep = (currentStep + 7) % 8;
    stepMotor(currentStep);
  }
  releaseCoils();
}

// ── EEPROM state ──────────────────────────────────────────────────
#define EEPROM_ADDR 0
#define STATE_NORMAL 0x00
#define STATE_WIN 0x01

// ── Keypad (physical pins 33-40 = PA0-PA7) ───────────────────────
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {31, 24, 25, 27};
byte colPins[COLS] = {26, 28, 29, 30};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── Game state ────────────────────────────────────────────────────
enum Challenge { CHALLENGE_KEYPAD, CHALLENGE_ENCODER, CHALLENGE_SWITCH };
Challenge currentChallenge;

char entry[17] = "";
byte entryLen = 0;
int score = 0;
bool lastSwitchState = HIGH;
int lastEncoderPos = 0;

char secretCode[5] = "0000";

void generateSecretCode() {
  for (int i = 0; i < 4; i++) {
    secretCode[i] = '0' + random(0, 10);
  }
}

// ── Crack it sequence: Right 90, Left 180, Right 270 ─────────────
struct CrackStep { int degrees; bool isLeft; };
CrackStep crackSeq[3] = {
  { 90,  false },  // Right
  { 180, true  },  // Left
  { 270, false }   // Right
};
int crackIndex  = 0;
int crackTarget = 0;

void loadCrackStep(int idx) {
  encoderPos = 0;
  lastEncoderPos = 0;
  int counts = (int)round((crackSeq[idx].degrees / 360.0) * COUNTS_PER_REV);
  crackTarget = crackSeq[idx].isLeft ? -counts : counts;
}

// ── Bit-bang I2C ─────────────────────────────────────────────────
void bb_sclHigh() { pinMode(BB_SCL, INPUT); }
void bb_sclLow()  { pinMode(BB_SCL, OUTPUT); digitalWrite(BB_SCL, LOW); }
void bb_sdaHigh() { pinMode(BB_SDA, INPUT); }
void bb_sdaLow()  { pinMode(BB_SDA, OUTPUT); digitalWrite(BB_SDA, LOW); }

void bb_start() {
  bb_sdaHigh(); delayMicroseconds(BB_DELAY);
  bb_sclHigh(); delayMicroseconds(BB_DELAY);
  bb_sdaLow();  delayMicroseconds(BB_DELAY);
  bb_sclLow();  delayMicroseconds(BB_DELAY);
}
void bb_stop() {
  bb_sdaLow();  delayMicroseconds(BB_DELAY);
  bb_sclHigh(); delayMicroseconds(BB_DELAY);
  bb_sdaHigh(); delayMicroseconds(BB_DELAY);
}
bool bb_writeByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    if (data & (1 << i)) bb_sdaHigh(); else bb_sdaLow();
    delayMicroseconds(BB_DELAY);
    bb_sclHigh(); delayMicroseconds(BB_DELAY);
    bb_sclLow();  delayMicroseconds(BB_DELAY);
  }
  bb_sdaHigh();
  delayMicroseconds(BB_DELAY);
  bb_sclHigh(); delayMicroseconds(BB_DELAY);
  bool ack = !digitalRead(BB_SDA);
  bb_sclLow();  delayMicroseconds(BB_DELAY);
  return ack;
}
void bb_sendCmd(uint8_t addr, uint8_t cmd) {
  bb_start();
  bb_writeByte(addr << 1);
  bb_writeByte(0x00);
  bb_writeByte(cmd);
  bb_stop();
}

GFXcanvas1 scoreCanvas(128, 64);

void scoreInit() {
  uint8_t addr = 0x3C;
  uint8_t initCmds[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
  };
  for (uint8_t i = 0; i < sizeof(initCmds); i++)
    bb_sendCmd(addr, initCmds[i]);
}

void scoreFlush() {
  uint8_t addr = 0x3C;
  bb_sendCmd(addr, 0x21); bb_sendCmd(addr, 0); bb_sendCmd(addr, 127);
  bb_sendCmd(addr, 0x22); bb_sendCmd(addr, 0); bb_sendCmd(addr, 7);

  uint8_t* cbuf = scoreCanvas.getBuffer();
  for (uint8_t page = 0; page < 8; page++) {
    bb_start();
    bb_writeByte(addr << 1);
    bb_writeByte(0x40);
    for (uint8_t x = 0; x < 128; x++) {
      uint8_t colByte = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        uint16_t y = page * 8 + bit;
        uint8_t src = cbuf[(x >> 3) + y * 16];
        if (src & (0x80 >> (x & 7))) colByte |= (1 << bit);
      }
      bb_writeByte(colByte);
    }
    bb_stop();
  }
}

void updateScoreDisplay() {
  scoreCanvas.fillScreen(0);
  scoreCanvas.setTextColor(1);
  scoreCanvas.setTextSize(2);
  scoreCanvas.setCursor(0, 0);
  scoreCanvas.println("SCORE:");
  scoreCanvas.println(score);
  scoreCanvas.setTextSize(1);
  scoreCanvas.setCursor(0, 52);
  scoreCanvas.println("Select # to Restart");
  scoreFlush();
}

void showRestartPrompt() {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(2);
  dispCode.setCursor(0, 24);
  dispCode.println("Restart?");
  dispCode.display();

  while (true) {
    char key = keypad.getKey();
    if (key == '#') break;
    delay(30);
  }
}

// ── Demo mode select at boot ─────────────────────────────────────
void drawDemoScreen(const char* buf) {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(1);
  dispCode.setCursor(0, 0);
  dispCode.println("Demo code + #");
  dispCode.println("or # to play");
  dispCode.setTextSize(2);
  dispCode.setCursor(0, 36);
  dispCode.println(buf);
  dispCode.display();
}

void checkDemoMode() {
  char buf[5] = "";
  byte len = 0;
  drawDemoScreen(buf);

  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        break;
      } else if (key == '*') {
        len = 0; buf[0] = '\0';
      } else if (len < 4) {
        buf[len++] = key;
        buf[len] = '\0';
      }
      drawDemoScreen(buf);
    }
    delay(30);
  }

  if (strcmp(buf, DEMO_CODE) == 0) {
    demoMode = true;
    winThreshold = WIN_DEMO;
  } else {
    demoMode = false;
    winThreshold = WIN_NORMAL;
  }
}

// ── In-game demo/real mode toggle (D key) ─────────────────────────
void drawTogglePrompt(const char* buf) {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(1);
  dispCode.setCursor(0, 0);
  dispCode.println("Enter code + #");
  dispCode.println("to toggle mode");
  dispCode.setCursor(0, 20);
  dispCode.print("Currently: ");
  dispCode.println(demoMode ? "DEMO" : "REAL");
  dispCode.setTextSize(2);
  dispCode.setCursor(0, 40);
  dispCode.println(buf);
  dispCode.display();
}

void toggleDemoPrompt() {
  char buf[5] = "";
  byte len = 0;
  drawTogglePrompt(buf);

  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        break;
      } else if (key == '*') {
        len = 0; buf[0] = '\0';
      } else if (len < 4) {
        buf[len++] = key;
        buf[len] = '\0';
      }
      drawTogglePrompt(buf);
    }
    delay(30);
  }

  // AAAA flips whichever mode we're currently in; anything else = no change
  if (strcmp(buf, DEMO_CODE) == 0) {
    demoMode = !demoMode;
    winThreshold = demoMode ? WIN_DEMO : WIN_NORMAL;
  }

  restartGame();
}

void showCurrentChallenge() {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);

  if (currentChallenge == CHALLENGE_KEYPAD) {
    dispCode.setTextSize(2);
    dispCode.setCursor(0, 0);
    dispCode.println("Hack it!");
    dispCode.setCursor(0, 20);
    dispCode.print("Code:");
    dispCode.println(secretCode);
    dispCode.setCursor(0, 44);
    dispCode.println(entry);

  } else if (currentChallenge == CHALLENGE_ENCODER) {
    dispCode.setTextSize(2);
    dispCode.setCursor(0, 0);
    dispCode.println("Crack it!");
    dispCode.setTextSize(1);
    dispCode.setCursor(0, 22);
    dispCode.print("Step ");
    dispCode.print(crackIndex + 1);
    dispCode.println("/3");
    dispCode.setTextSize(2);
    dispCode.setCursor(0, 34);
    dispCode.print(crackSeq[crackIndex].isLeft ? "L " : "R ");
    dispCode.print(crackSeq[crackIndex].degrees);

  } else if (currentChallenge == CHALLENGE_SWITCH) {
    dispCode.setTextSize(2);
    dispCode.setCursor(0, 0);
    dispCode.println("PULL");
    dispCode.println("IT!");
  }

  dispCode.display();
}

void showSuccess() {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(2);
  dispCode.setCursor(16, 24);
  dispCode.println("NICE!");
  dispCode.display();
  delay(800);
}

void showFail() {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(2);
  dispCode.setCursor(16, 24);
  dispCode.println("FAIL!");
  dispCode.display();
  delay(1000);
}

void showWin() {
  dispCode.clearDisplay();
  dispCode.setTextColor(SSD1306_WHITE);
  dispCode.setTextSize(2);
  dispCode.setCursor(0, 24);
  dispCode.println("YOU WIN!");
  dispCode.display();
  delay(2000);
}

void pickChallenge() {
  currentChallenge = (Challenge)random(3);
  entry[0] = '\0';
  entryLen = 0;
  encoderPos = 0;
  lastEncoderPos = 0;

  if (currentChallenge == CHALLENGE_KEYPAD) {
    generateSecretCode();
    myDFPlayer.play(TRACK_HACK);
  } else if (currentChallenge == CHALLENGE_ENCODER) {
    crackIndex = 0;
    loadCrackStep(0);
    myDFPlayer.play(TRACK_CRACK);
  } else if (currentChallenge == CHALLENGE_SWITCH) {
    myDFPlayer.play(TRACK_PULL);
  }

  actionStartTime = millis();
  lastBeepTime = millis();
}

void restartGame() {
  score = 0;
  ACTION_TIMEOUT = 8000;
  updateScoreDisplay();
  pickChallenge();
  showCurrentChallenge();
}

void challengeSuccess() {
  score++;
  updateScoreDisplay();
  if (score >= winThreshold) {
    myDFPlayer.play(TRACK_WIN);
    showWin();
    rotatePlus90();
    flashWinLED();
    EEPROM.write(EEPROM_ADDR, STATE_WIN);
    showRestartPrompt();   // waits for '#'
    rotateMinus90();       // relock the safe
    EEPROM.write(EEPROM_ADDR, STATE_NORMAL);
    restartGame();
    return;
  }
  showSuccess();

  if (ACTION_TIMEOUT > MIN_TIMEOUT) {
    ACTION_TIMEOUT -= TIMEOUT_STEP;
    if (ACTION_TIMEOUT < MIN_TIMEOUT) ACTION_TIMEOUT = MIN_TIMEOUT;
  }
  pickChallenge();
  showCurrentChallenge();
}

void challengeFail() {
  score = 0;
  myDFPlayer.play(TRACK_LOSE);
  updateScoreDisplay();
  showFail();
  flashLoseLED();
  EEPROM.write(EEPROM_ADDR, STATE_NORMAL);
  showRestartPrompt();
  restartGame();
}

void setup() {
  // ── Motor pins ──
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  releaseCoils();

  // ── Check EEPROM for win state ──
  uint8_t lastState = EEPROM.read(EEPROM_ADDR);
  if (lastState == STATE_WIN) {
    rotateMinus90();
    EEPROM.write(EEPROM_ADDR, STATE_NORMAL);
  }

  // ── LED pins ──
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  ledOff();

  pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);
  delay(100);
  lastSwitchState = digitalRead(LIMIT_SWITCH_PIN);

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  lastCLK = digitalRead(ENCODER_CLK);
  PCICR  |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT12);

  Serial1.begin(9600);
  myDFPlayer.begin(Serial1);
  myDFPlayer.volume(30);
  delay(500);

  if (!dispCode.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }
  dispCode.setTextColor(SSD1306_WHITE);

  scoreInit();
  updateScoreDisplay();

  randomSeed(analogRead(0));

  // ── Boot: pick demo or normal ──
  checkDemoMode();

  pickChallenge();
  showCurrentChallenge();
}

void loop() {
  // ── Beep ticker (speeds up with ACTION_TIMEOUT) ──
  unsigned long beepInterval = ACTION_TIMEOUT / 4;
  if (beepInterval < 300) beepInterval = 300;
  if (millis() - lastBeepTime >= beepInterval) {
    myDFPlayer.play(TRACK_BEEP);
    lastBeepTime = millis();
  }

  // ── Limit switch ──
  bool currentSwitchState = digitalRead(LIMIT_SWITCH_PIN);
  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    delay(20);
    if (digitalRead(LIMIT_SWITCH_PIN) == LOW) {
      if (currentChallenge == CHALLENGE_SWITCH) {
        challengeSuccess();
      } else {
        challengeFail();
      }
    }
  }
  lastSwitchState = currentSwitchState;

  // ── Timeout ──
  if (millis() - actionStartTime >= ACTION_TIMEOUT) {
    challengeFail();
  }

  // ── Keypad ──
  char key = keypad.getKey();
  if (key) {
    if (key == 'D') {
      toggleDemoPrompt();
    } else if (key == '#') {
      restartGame();
    } else if (currentChallenge == CHALLENGE_KEYPAD) {
      if (key == '*') {
        entryLen = 0;
        entry[0] = '\0';
      } else if (entryLen < 16) {
        entry[entryLen++] = key;
        entry[entryLen] = '\0';
      }
      if (strcmp(entry, secretCode) == 0) {
        challengeSuccess();
      } else {
        showCurrentChallenge();
      }
    } else {
      challengeFail();
    }
  }

  // ── Rotary encoder (Right 90, Left 180, Right 270) ──
  if (currentChallenge == CHALLENGE_ENCODER) {
    int current = encoderPos;
    if (current != lastEncoderPos) {
      lastEncoderPos = current;
      showCurrentChallenge();
    }
    bool rightDir = (crackTarget > 0 && current > 0) ||
                    (crackTarget < 0 && current < 0);
    bool inRange = rightDir && (abs(current - crackTarget) <= TOLERANCE);
    if (inRange) {
      delay(300);
      if (abs(encoderPos - crackTarget) <= TOLERANCE) {
        crackIndex++;
        if (crackIndex >= 3) {
          challengeSuccess();
        } else {
          loadCrackStep(crackIndex);
          actionStartTime = millis();  // fresh time for next sub-step
          showCurrentChallenge();
        }
      }
    }
  } else {
    if (encoderPos != 0) {
      challengeFail();
    }
  }

  delay(30);
}
