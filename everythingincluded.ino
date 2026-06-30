// ═══════════════════════════════════════════════════════════════════
// Pitt BOP IT! — v8  (score OLED driven by raw bit-bang I2C)
// Target : ATmega1284P bare metal  (MightyCore "Standard" pinout)
// Team   : Maddie Dimarco · Quentin Wilson · Jack Wasco · Jasmine Horton
//
// ── PIN MAP ────────────────────────────────────────────────────────
//  Stepper IN1                0   PB0  1
//  Stepper IN2                1   PB1  2
//  Stepper IN3                2   PB2  3
//  Stepper IN4                3   PB3  4
//  Encoder CLK                4   PB4  5
//  Encoder DT                 5   PB5  6
//  Encoder SW                 6   PB6  7
//  Serial1 RX1 <- DFPlayer TX 10  PD2  16
//  Serial1 TX1 -> 1kOhm -> DFPlayer RX 11 PD3 17
//  Switch                     12  PD4  18
//  RGB Red   (220Ohm to GND)  13  PD5  19
//  RGB Green (220Ohm to GND)  14  PD6  20
//  RGB Blue  (220Ohm to GND)  15  PD7  21
//  OLED1 SCL  hw I2C          16  PC0  22  <- challenge display
//  OLED1 SDA  hw I2C          17  PC1  23  <- challenge display
//  OLED2 SCL  bit-bang        18  PC2  24  <- score display
//  OLED2 SDA  bit-bang        19  PC3  25  <- score display
//  Keypad col C1              24  PA0  40
//  Keypad col C2              25  PA1  39
//  Keypad col C3              26  PA2  38
//  Keypad col C4              27  PA3  37
//  Keypad row R1              28  PA4  36
//  Keypad row R2              29  PA5  35
//  Keypad row R3              30  PA6  34
//  Keypad row R4              31  PA7  33
// ═══════════════════════════════════════════════════════════════════

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <DFRobotDFPlayerMini.h>

// ── Bit-bang I2C for score OLED ──────────────────────────────────
#define BB_SCL 18   // PC2  physical 24
#define BB_SDA 19   // PC3  physical 25
#define BB_DELAY 5  // microseconds per half-bit

void bb_sclHigh()  { pinMode(BB_SCL, INPUT);          }
void bb_sclLow()   { pinMode(BB_SCL, OUTPUT); digitalWrite(BB_SCL, LOW); }
void bb_sdaHigh()  { pinMode(BB_SDA, INPUT);          }
void bb_sdaLow()   { pinMode(BB_SDA, OUTPUT); digitalWrite(BB_SDA, LOW); }

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

void bb_sendData(uint8_t addr, uint8_t* buf, uint16_t len) {
  bb_start();
  bb_writeByte(addr << 1);
  bb_writeByte(0x40);
  for (uint16_t i = 0; i < len; i++) bb_writeByte(buf[i]);
  bb_stop();
}

// ── Score OLED framebuffer ────────────────────────────────────────
// We drive it manually using Adafruit_SSD1306 in hardware I2C mode
// pointed at Wire, then swap the display buffer out over bit-bang.
// Simpler: just use a second Adafruit_SSD1306 on Wire and send its
// buffer via bit-bang after every .display() call.
//
// Actually cleanest: use ONE Adafruit_GFX canvas for score,
// render into it, then push the buffer over bit-bang.

#include <Adafruit_GrayOLED.h>

// Score canvas — renders into RAM, we push manually
static uint8_t scoreBuf[128 * 64 / 8];
GFXcanvas1 scoreCanvas(128, 64);

void scoreFlush() {
  uint8_t addr = 0x3C;
  // Set column and page address to full screen
  bb_sendCmd(addr, 0x21); bb_sendCmd(addr, 0); bb_sendCmd(addr, 127);
  bb_sendCmd(addr, 0x22); bb_sendCmd(addr, 0); bb_sendCmd(addr, 7);
  // Push buffer in chunks of 16 bytes (I2C stop/start between chunks)
  uint8_t* buf = scoreCanvas.getBuffer();
  for (uint16_t i = 0; i < 1024; i += 16) {
    bb_start();
    bb_writeByte(addr << 1);
    bb_writeByte(0x40);
    for (uint8_t j = 0; j < 16; j++) bb_writeByte(buf[i + j]);
    bb_stop();
  }
}

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

// ── Hardware I2C OLED (challenge display) ────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_ADDR     0x3C
Adafruit_SSD1306 dispChallenge(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Keypad ───────────────────────────────────────────────────────
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {28, 29, 30, 31};
byte colPins[COLS] = {24, 25, 26, 27};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── DFPlayer ─────────────────────────────────────────────────────
DFRobotDFPlayerMini dfPlayer;
#define AUDIO_HACK  1
#define AUDIO_PULL  2
#define AUDIO_CRACK 3
#define AUDIO_FAIL  4
#define AUDIO_WIN   5

// ── Encoder ──────────────────────────────────────────────────────
#define ENC_CLK 4
#define ENC_DT  5
#define ENC_SW  6

// ── Switch ───────────────────────────────────────────────────────
#define SWITCH_PIN 12

// ── RGB LED ──────────────────────────────────────────────────────
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

const char* SECRET_CODE = "1683";

enum GameState { GS_IDLE, GS_KEYPAD, GS_SWITCH, GS_ENCODER, GS_OVER, GS_WIN };
GameState state = GS_IDLE;

int           score          = 0;
unsigned long actionDeadline = 0;
unsigned long ACTION_TIMEOUT = 5000;

char keypadEntry[17] = "";
byte keypadLen       = 0;
bool switchLastState = HIGH;

int  encTargetCW  = 0, encTargetCCW = 0;
int  encCountCW   = 0, encCountCCW  = 0;
bool encPhaseCW   = true;
bool encLastCLK   = HIGH;

// ═══════════════════════════════════════════════════════════════════
//  RGB
// ═══════════════════════════════════════════════════════════════════
void rgbOff() { digitalWrite(RGB_R,LOW); digitalWrite(RGB_G,LOW); digitalWrite(RGB_B,LOW); }
void rgbSet(bool r, bool g, bool b) { digitalWrite(RGB_R,r); digitalWrite(RGB_G,g); digitalWrite(RGB_B,b); }
void flashPassLED() { rgbSet(0,1,0); delay(300); rgbOff(); delay(100); rgbSet(0,1,0); delay(300); rgbOff(); }
void flashFailLED(unsigned long dur) {
  unsigned long s = millis();
  while (millis()-s < dur) { rgbSet(1,0,0); delay(120); rgbSet(0,0,1); delay(120); }
  rgbOff();
}
void setWinLED() { rgbSet(0,1,0); }

// ═══════════════════════════════════════════════════════════════════
//  Stepper
// ═══════════════════════════════════════════════════════════════════
void stepOnce(int dir) {
  stepperPos = (stepperPos+dir+8)%8;
  digitalWrite(IN1,halfStep[stepperPos][0]); digitalWrite(IN2,halfStep[stepperPos][1]);
  digitalWrite(IN3,halfStep[stepperPos][2]); digitalWrite(IN4,halfStep[stepperPos][3]);
  delayMicroseconds(STEP_DELAY_US);
}
void releaseCoils() { digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,LOW); }
void unlockSafe() { for(int i=0;i<STEPS_PER_REV;i++) stepOnce(1); releaseCoils(); }

// ═══════════════════════════════════════════════════════════════════
//  Score display helpers (GFXcanvas1 + bit-bang flush)
// ═══════════════════════════════════════════════════════════════════
void scoreDraw(const char* l1, const char* l2=nullptr, const char* l3=nullptr, uint8_t sz1=1, uint8_t sz2=1) {
  scoreCanvas.fillScreen(0);
  scoreCanvas.setTextColor(1);
  scoreCanvas.setTextSize(sz1); scoreCanvas.setCursor(0, 0);  scoreCanvas.println(l1);
  if (l2) { scoreCanvas.setTextSize(sz2); scoreCanvas.setCursor(0,20); scoreCanvas.println(l2); }
  if (l3) { scoreCanvas.setTextSize(1);   scoreCanvas.setCursor(0,46); scoreCanvas.println(l3); }
  scoreFlush();
}

void scoreShowIdle()     { scoreDraw("BOP IT!", "Press *", "to start"); }
void scoreShowScore()    {
  scoreCanvas.fillScreen(0);
  scoreCanvas.setTextColor(1);
  scoreCanvas.setTextSize(1); scoreCanvas.setCursor(0,0);  scoreCanvas.println("Score:");
  scoreCanvas.setTextSize(4); scoreCanvas.setCursor(0,16); scoreCanvas.println(score);
  scoreFlush();
}
void scoreShowGameOver() {
  char buf[12]; sprintf(buf,"Score: %d", score);
  scoreDraw("GAME", "OVER", buf, 2, 2);
}
void scoreShowWin()      { scoreDraw("YOU", "WIN!", "Score: 99", 2, 2); }

// ═══════════════════════════════════════════════════════════════════
//  Challenge display helpers
// ═══════════════════════════════════════════════════════════════════
void challengeShowIdle() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,20); dispChallenge.println("Waiting...");
  dispChallenge.display();
}
void challengeShowHack() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2); dispChallenge.setCursor(0,0);  dispChallenge.println("HACK IT!");
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,20); dispChallenge.println("Enter code + #");
  dispChallenge.setCursor(0,32); dispChallenge.println("* to clear");
  dispChallenge.setTextSize(2); dispChallenge.setCursor(0,46); dispChallenge.println(keypadEntry);
  dispChallenge.display();
}
void challengeShowPull() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2); dispChallenge.setCursor(0,0); dispChallenge.println("PULL IT!");
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,24); dispChallenge.println("Hit the switch!");
  dispChallenge.display();
}
void challengeShowCrack() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2); dispChallenge.setCursor(0,0); dispChallenge.println("CRACK IT!");
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,18);
  dispChallenge.print("CW:"); dispChallenge.print(encTargetCW);
  dispChallenge.print("  CCW:"); dispChallenge.println(encTargetCCW);
  dispChallenge.setCursor(0,30);
  if (encPhaseCW) { dispChallenge.print(">>> CW: ");  dispChallenge.print(encCountCW);  dispChallenge.print("/"); dispChallenge.println(encTargetCW); }
  else            { dispChallenge.print(">>> CCW: "); dispChallenge.print(encCountCCW); dispChallenge.print("/"); dispChallenge.println(encTargetCCW); }
  int total=encTargetCW+encTargetCCW;
  int done=encPhaseCW?encCountCW:(encTargetCW+encCountCCW);
  int barW=(int)(done*124.0/total);
  dispChallenge.drawRect(2,50,124,10,SSD1306_WHITE);
  if(barW>0) dispChallenge.fillRect(2,50,barW,10,SSD1306_WHITE);
  dispChallenge.display();
}
void challengeShowGameOver() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,20); dispChallenge.println("Press * to");
  dispChallenge.setCursor(0,32); dispChallenge.println("play again");
  dispChallenge.display();
}
void challengeShowWin() {
  dispChallenge.clearDisplay(); dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(1); dispChallenge.setCursor(0,16); dispChallenge.println("Safe unlocked!");
  dispChallenge.setCursor(0,32); dispChallenge.println("Press * to");
  dispChallenge.setCursor(0,44); dispChallenge.println("play again");
  dispChallenge.display();
}

// ═══════════════════════════════════════════════════════════════════
//  Encoder
// ═══════════════════════════════════════════════════════════════════
int readEncoder() {
  bool clk = digitalRead(ENC_CLK);
  if (clk==encLastCLK) return 0;
  encLastCLK=clk;
  if (clk==LOW) return (digitalRead(ENC_DT)==HIGH)?+1:-1;
  return 0;
}

// ═══════════════════════════════════════════════════════════════════
//  Challenge launchers
// ═══════════════════════════════════════════════════════════════════
void startKeypadChallenge() {
  keypadLen=0; keypadEntry[0]='\0'; state=GS_KEYPAD;
  actionDeadline=millis()+ACTION_TIMEOUT; dfPlayer.play(AUDIO_HACK); challengeShowHack();
}
void startSwitchChallenge() {
  switchLastState=digitalRead(SWITCH_PIN); state=GS_SWITCH;
  actionDeadline=millis()+ACTION_TIMEOUT; dfPlayer.play(AUDIO_PULL); challengeShowPull();
}
void startEncoderChallenge() {
  encTargetCW=random(2,8); encTargetCCW=random(2,8);
  encCountCW=0; encCountCCW=0; encPhaseCW=true;
  encLastCLK=digitalRead(ENC_CLK); state=GS_ENCODER;
  actionDeadline=millis()+ACTION_TIMEOUT; dfPlayer.play(AUDIO_CRACK); challengeShowCrack();
}
void pickNextChallenge() {
  switch(random(3)) {
    case 0: startKeypadChallenge(); break;
    case 1: startSwitchChallenge(); break;
    case 2: startEncoderChallenge(); break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  Pass / Fail
// ═══════════════════════════════════════════════════════════════════
void onPass() {
  score++;
  if (score>=99) {
    state=GS_WIN; dfPlayer.play(AUDIO_WIN); setWinLED();
    scoreShowWin(); challengeShowWin(); delay(500); unlockSafe(); return;
  }
  flashPassLED();
  if (score%5==0 && ACTION_TIMEOUT>2000) ACTION_TIMEOUT-=500;
  scoreShowScore(); delay(500); pickNextChallenge();
}
void onFail() {
  state=GS_OVER; dfPlayer.play(AUDIO_FAIL);
  scoreShowGameOver(); challengeShowGameOver(); flashFailLED(4000);
}

// ═══════════════════════════════════════════════════════════════════
//  Setup
// ═══════════════════════════════════════════════════════════════════
void setup() {
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT); pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  releaseCoils();
  pinMode(ENC_CLK,INPUT_PULLUP); pinMode(ENC_DT,INPUT_PULLUP); pinMode(ENC_SW,INPUT_PULLUP);
  encLastCLK=digitalRead(ENC_CLK);
  pinMode(SWITCH_PIN,INPUT_PULLUP);
  pinMode(RGB_R,OUTPUT); pinMode(RGB_G,OUTPUT); pinMode(RGB_B,OUTPUT);
  rgbOff();

  Serial1.begin(9600);
  if (!dfPlayer.begin(Serial1)) {
    for(int i=0;i<10;i++){rgbSet(1,0,0);delay(150);rgbOff();delay(150);}
    for(;;);
  }
  dfPlayer.volume(25);

  Wire.begin();
  if (!dispChallenge.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) for(;;);

  scoreInit();
  randomSeed(analogRead(0));

  scoreShowIdle();
  challengeShowIdle();
  state=GS_IDLE;
}

// ═══════════════════════════════════════════════════════════════════
//  Loop
// ═══════════════════════════════════════════════════════════════════
void loop() {
  char key=keypad.getKey();

  if (state==GS_IDLE) {
    if (key=='*') { score=0; ACTION_TIMEOUT=5000; rgbOff(); scoreShowScore(); challengeShowIdle(); delay(500); pickNextChallenge(); }
    return;
  }
  if (state==GS_WIN) {
    if (key=='*') { state=GS_IDLE; rgbOff(); scoreShowIdle(); challengeShowIdle(); } return;
  }
  if (state==GS_OVER) {
    if (key=='*') { state=GS_IDLE; rgbOff(); scoreShowIdle(); challengeShowIdle(); } return;
  }

  if (millis()>actionDeadline) { onFail(); return; }

  if (state==GS_KEYPAD) {
    if (key) {
      if (key=='*') { keypadLen=0; keypadEntry[0]='\0'; }
      else if (key=='#') { if(strcmp(keypadEntry,SECRET_CODE)==0) onPass(); else onFail(); return; }
      else if (keypadLen<16) { keypadEntry[keypadLen++]=key; keypadEntry[keypadLen]='\0'; }
      challengeShowHack();
    }
  }
  if (state==GS_SWITCH) {
    bool cur=digitalRead(SWITCH_PIN);
    if (cur==LOW && switchLastState==HIGH) { delay(50); onPass(); return; }
    switchLastState=cur;
  }
  if (state==GS_ENCODER) {
    int dir=readEncoder();
    if (encPhaseCW) {
      if (dir==+1) { encCountCW++; challengeShowCrack(); if(encCountCW>=encTargetCW){encPhaseCW=false;challengeShowCrack();} }
      else if (dir==-1) { encCountCW=0; challengeShowCrack(); }
    } else {
      if (dir==-1) { encCountCCW++; challengeShowCrack(); if(encCountCCW>=encTargetCCW){onPass();return;} }
      else if (dir==+1) { encCountCCW=0; challengeShowCrack(); }
    }
    if (digitalRead(ENC_SW)==LOW) {
      delay(50);
      if (!encPhaseCW && encCountCCW>=encTargetCCW) onPass();
      while(digitalRead(ENC_SW)==LOW);
    }
  }
}
