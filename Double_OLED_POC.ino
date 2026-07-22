// ═══════════════════════════════════════════════════════════════════
// OLED bring-up test — two displays, nothing else
// Target : ATmega1284P (MightyCore "Standard")
//
//   Display 1 (challenge) : hardware I2C on Wire  PC0/PC1 (16/17) -> "Code:"
//   Display 2 (score)     : bit-bang I2C          PC2/PC3 (18/19) -> "Score:"
//
// Both modules are address 0x3C — that's fine, they're on separate buses.
// ═══════════════════════════════════════════════════════════════════

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Display 1: CHALLENGE (hardware I2C) ──────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_ADDR     0x3C
Adafruit_SSD1306 dispChallenge(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Display 2: SCORE (bit-bang I2C) ──────────────────────────────
#define BB_SCL 18   // PC2  physical 24
#define BB_SDA 19   // PC3  physical 25
#define BB_DELAY 5

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

  // GFXcanvas1 stores pixels HORIZONTALLY (8 pixels across per byte,
  // MSB = leftmost). The SSD1306 wants them VERTICALLY (8 pixels down
  // per byte, LSB = top). Transpose as we send, one page at a time.
  uint8_t* cbuf = scoreCanvas.getBuffer();   // 16 bytes per row, row-major
  for (uint8_t page = 0; page < 8; page++) {
    bb_start();
    bb_writeByte(addr << 1);
    bb_writeByte(0x40);                      // 0x40 = data stream
    for (uint8_t x = 0; x < 128; x++) {
      uint8_t colByte = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        uint16_t y   = page * 8 + bit;
        uint8_t  src = cbuf[(x >> 3) + y * 16];   // 16 = (128+7)/8
        if (src & (0x80 >> (x & 7))) colByte |= (1 << bit);
      }
      bb_writeByte(colByte);
    }
    bb_stop();
  }
}

void setup() {
  // ---- Score display (bit-bang) — "Score:" ----
  scoreInit();
  scoreCanvas.fillScreen(0);
  scoreCanvas.setTextColor(1);
  scoreCanvas.setTextSize(2);
  scoreCanvas.setCursor(0, 0);
  scoreCanvas.println("Score:");
  scoreFlush();

  // ---- Challenge display (hardware I2C) — "Code:" ----
  // NOTE: no for(;;) hang here on purpose — if this display fails to
  // init, the sketch keeps running so the bit-bang one still shows,
  // which tells you WHICH display is the problem.
  Wire.begin();
  dispChallenge.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  dispChallenge.clearDisplay();
  dispChallenge.setTextColor(SSD1306_WHITE);
  dispChallenge.setTextSize(2);
  dispChallenge.setCursor(0, 0);
  dispChallenge.println("Code:");
  dispChallenge.display();
}

void loop() {
  // static test — nothing to do
}
