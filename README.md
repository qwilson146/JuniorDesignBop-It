# Break-Into_it

Junior Design project for Pitt ECE 1895 — a "Bop It"-style electronic game where the player
races against a shrinking timer to clear randomized security challenges: crack a keypad code,
turn a dial through a randomized combination, and pull the door handle. Get one wrong (or run out of
time) and the score resets.

**Contributors:** Quentin Wilson, Maddie Dimarco, Jack Wasco, Jasmine Horton

## Gameplay

Each round presents one of three randomly chosen challenges:

| Challenge | Prompt | Player action |
|---|---|---|
| **Hack it** | 4-digit code shown on the screen | Enter the matching code on the keypad |
| **Crack it** | Right/Left turn sequence shown on the screen | Turn the rotary encoder through the sequence |
| **Pull it** | "PULL IT!" shown on the screen| Trigger the limit switch |

- A countdown timer drives a speeding-up beep; missing the timeout fails the round.
- Each success increases the score and shortens the next timeout.
- Reaching the win threshold rotates a stepper-driven lock open, flashes the RGB LED green,
  and plays a win song; The safe stays unlocked until the user restarts the game. 
- A wrong key/turn/switch resets the score, flashes the RGB LED red and blue, and plays police sirens. 
- Entering "AAAA#" at startup launches **Demo Mode** (lower win
  threshold to 3 actions, for quick demonstrations) versus normal play. 

## Libraries

Install via the Arduino Library Manager:

- `Adafruit_GFX`
- `Adafruit_SSD1306`
- `Keypad`
- `DFRobotDFPlayerMini`
- `Wire` and `EEPROM`

## Repo layout

- [`WorkingVersion_Timer_Included.ino`](WorkingVersion_Timer_Included.ino) — the current full
  build: all three challenges, dual OLEDs, timer/scoring, demo mode, stepper lock, LED and
  sound feedback.
- Proof-of-concept sketches used to bring up individual subsystems before integrating them:
  - [`OLED_POC.ino`](OLED_POC.ino) — single OLED bring-up/address test
  - [`Double_OLED_POC.ino`](Double_OLED_POC.ino) — driving two SSD1306 displays at the same
    I2C address (one hardware I2C, one bit-banged)
  - [`Keypad_POC.ino`](Keypad_POC.ino) — 4x4 keypad entry shown on an OLED
  - [`Limit_Switch_POC.ino`](Limit_Switch_POC.ino) — debounced limit switch scoring over serial
  - [`Motor_POC.ino`](Motor_POC.ino) — standalone stepper motor half-step test
