#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// ── Display 1: CODE (hardware I2C, physical pins 22/23) ──────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_ADDR     0x3C

Adafruit_SSD1306 dispCode(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Display 2: SCORE (bit-bang I2C, physical pins 24/25) ─────────
#define BB_SCL 18
#define BB_SDA 19
#define BB_DELAY 10

// ── Limit switch (physical pin 18 = PD4) ─────────────────────────
#define LIMIT_SWITCH_PIN 12

// ── Keypad (physical pins 33-40 = PA0-PA7) ───────────────────────
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {27, 31, 30, 28};
byte colPins[COLS] = {24, 29, 26, 25};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char entry[17] = "";
byte entryLen = 0;

// ── Bit-bang I2C ─────────────────────────────────────────────────
void bb_sclHigh() { pinMode(BB_SCL, INPUT);  }
void bb_sclLow()  { pinMode(BB_SCL, OUTPUT); digitalWrite(BB_SCL, LOW); }
void bb_sdaHigh() { pinMode(BB_SDA, INPUT);  }
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
        uint16_t y   = page * 8 + bit;
        uint8_t  src = cbuf[(x >> 3) + y * 16];
        if (src & (0x80 >> (x & 7))) colByte |= (1 << bit);
      }
      bb_writeByte(colByte);
    }
    bb_stop();
  }
}

void updateScoreDisplay(int score) {
  scoreCanvas.fillScreen(0);
  scoreCanvas.setTextColor(1);
  scoreCanvas.setTextSize(2);
  scoreCanvas.setCursor(0, 0);
  scoreCanvas.println("SCORE:");
  scoreCanvas.println(score);
  scoreFlush();
}

void updateCodeDisplay() {
  dispCode.clearDisplay();
  dispCode.setTextSize(1);
  dispCode.setCursor(0, 0);
  dispCode.println("Enter code:");
  dispCode.setTextSize(2);
  dispCode.setCursor(0, 24);
  dispCode.println(entry);
  dispCode.display();
}

int score = 0;
bool lastSwitchState = HIGH;

void setup() {
  pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);
  delay(100);
  lastSwitchState = digitalRead(LIMIT_SWITCH_PIN);

  // ── OLED 1: Code ──
  if (!dispCode.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }
  dispCode.setTextColor(SSD1306_WHITE);
  updateCodeDisplay();

  // ── OLED 2: Score ──
  scoreInit();
  updateScoreDisplay(score);
}

void loop() {
  // ── Limit switch ──
  bool currentSwitchState = digitalRead(LIMIT_SWITCH_PIN);
  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    delay(20);
    if (digitalRead(LIMIT_SWITCH_PIN) == LOW) {
      score++;
      updateScoreDisplay(score);
    }
  }
  lastSwitchState = currentSwitchState;

  // ── Keypad ──
  char key = keypad.getKey();
  if (key) {
    if (key == '*') {
      entryLen = 0;
      entry[0] = '\0';
    } else if (entryLen < 16) {
      entry[entryLen++] = key;
      entry[entryLen]   = '\0';
    }
    updateCodeDisplay();
  }
}
