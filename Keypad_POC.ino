#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},{'4','5','6','B'},
  {'7','8','9','C'},{'*','0','#','D'}
};
byte rowPins[ROWS] = {4, 2, 3, 5};
byte colPins[COLS] = {9, 6, 7, 8};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char entry[17] = "";   // holds up to 16 chars + terminator
byte len = 0;

void showScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Enter code:");
  display.setTextSize(2);
  display.setCursor(0, 24);
  display.println(entry);
  display.display();
}

void setup() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }
  display.setTextColor(SSD1306_WHITE);
  showScreen();
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    if (key == '*') {
      len = 0;
      entry[0] = '\0';        // * clears the entry
    } else if (len < 16) {
      entry[len++] = key;
      entry[len] = '\0';
    }
    showScreen();
  }
}