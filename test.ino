// ═══════════════════════════════════════════════════════════════════
// OLED_POC — single display test
// ATmega1284P · MightyCore Standard pinout
// OLED: hardware I2C  →  SCL = pin 16 (PC0), SDA = pin 17 (PC1)
// ═══════════════════════════════════════════════════════════════════
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C disp(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  disp.begin();

  disp.clearBuffer();
  disp.setFont(u8g2_font_7x13_tr);
  disp.drawStr(0, 16, "OLED OK");
  disp.drawStr(0, 40, "pins 16/17");
  disp.sendBuffer();
}

void loop() {
}
