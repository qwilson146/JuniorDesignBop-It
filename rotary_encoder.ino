#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ENCODER_CLK  2 //digital pin for CLK for rotary encoder
#define ENCODER_DT   3 //digital pin for DT for rotary encoder

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct Challenge {
  int  degrees;
  bool isLeft;
};

// Generated randomly each game — right, left, right
// degrees are randomly 90, 180, or 270 for each step
Challenge challenges[3];
const int NUM_CHALLENGES = 3;

volatile int encoderPos   = 0;
volatile int lastCLK      = HIGH;
int          currentIndex = 0;
int          targetPos    = 0;
bool         stepDone     = false;
bool         gameOver     = false;

unsigned long gameStartTime = 0;
#define GAME_TIME_MS  20000

#define DETENTS_PER_REV  20
#define COUNTS_PER_REV   (DETENTS_PER_REV * 2)
#define TOLERANCE        2

void encoderISR() {
  int clk = digitalRead(ENCODER_CLK);
  int dt  = digitalRead(ENCODER_DT);
  if (clk != lastCLK) {
    encoderPos += (dt != clk) ? 1 : -1;
    lastCLK = clk;
  }
}

void generateChallenges() {
  // Directions fixed: right, left, right
  // Degrees randomly picked from 90, 180, or 270 each reset
  int options[] = { 90, 180, 270 };
  challenges[0] = { options[random(3)], false };  // right
  challenges[1] = { options[random(3)], true  };  // left
  challenges[2] = { options[random(3)], false };  // right
}

int degreesToCounts(int deg) {
  return (int)round((deg / 360.0) * COUNTS_PER_REV);
}

void loadChallenge(int idx) {
  encoderPos = 0;
  stepDone   = false;
  int counts = degreesToCounts(challenges[idx].degrees);
  targetPos  = challenges[idx].isLeft ? -counts : counts;
}

void drawPromptWithTimer(int secondsLeft) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  char line1[10];
  sprintf(line1, "%d deg", challenges[currentIndex].degrees);
  int x1 = (SCREEN_WIDTH - (strlen(line1) * 12)) / 2;
  display.setCursor(max(0, x1), 0);
  display.print(line1);

  const char* dir = challenges[currentIndex].isLeft ? "LEFT" : "RIGHT";
  int x2 = (SCREEN_WIDTH - (strlen(dir) * 12)) / 2;
  display.setCursor(max(0, x2), 24);
  display.print(dir);

  // Timer at bottom of screen
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("Time: ");
  display.print(secondsLeft);
  display.print("s");

  display.display();
}

void drawStep(int step) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 8);
  display.print("Step ");
  display.print(step + 1);
  display.print("/");
  display.print(NUM_CHALLENGES);
  display.setCursor(16, 36);
  display.print("DONE!");
  display.display();
  delay(800);
}

void drawWin() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 24);
  display.print("WIN!");
  display.display();
}

void drawFail() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 24);
  display.print("FAIL!");
  display.display();
}

void resetGame() {
  gameOver      = false;
  currentIndex  = 0;
  gameStartTime = millis();
  generateChallenges();  // pick new random degrees each reset
  loadChallenge(0);
}

void setup() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT,  INPUT_PULLUP);

  lastCLK = digitalRead(ENCODER_CLK);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, CHANGE);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  randomSeed(analogRead(0));  // seed random from floating analog pin
  resetGame();
}

void loop() {
  if (gameOver) return;

  unsigned long elapsed = millis() - gameStartTime;
  int secondsLeft = max(0, (int)((GAME_TIME_MS - elapsed) / 1000) + 1);

  if (elapsed >= GAME_TIME_MS) {
    gameOver = true;
    drawFail();
    return;
  }

  if (stepDone) return;

  int current = encoderPos;

  bool rightDirection = (targetPos > 0 && current > 0) ||
                        (targetPos < 0 && current < 0) ||
                        current == 0;

  bool inRange = rightDirection &&
                 abs(abs(current) - abs(targetPos)) <= TOLERANCE;

  if (inRange && current != 0) {
    delay(1000);

    // Check timer didn't expire during hold
    if (millis() - gameStartTime >= GAME_TIME_MS) {
      gameOver = true;
      drawFail();
      return;
    }

    int confirm = encoderPos;
    bool stillInRange = ((targetPos > 0 && confirm > 0) || (targetPos < 0 && confirm < 0)) &&
                        abs(abs(confirm) - abs(targetPos)) <= TOLERANCE;

    if (stillInRange) {
      stepDone = true;
      if (currentIndex < NUM_CHALLENGES - 1) {
        drawStep(currentIndex);
        currentIndex++;
        loadChallenge(currentIndex);
      } else {
        gameOver = true;
        drawWin();
      }
    } else {
      // Moved during hold — fail
      gameOver = true;
      drawFail();
    }
  } else {
    drawPromptWithTimer(secondsLeft);
  }

  delay(30);
}