/**
   ============================================================
    Simon Game — ESP32-C3
    TM1637 Score Display + NeoPixel Idle FX + LED PWM Fades
   ============================================================
    Original: Uri Shaked (2023) — MIT License
    Extended: Hashtag

    TM1637 layout  [ 0 ][ 1 ][ 2 ][ 3 ]
    Score rules:
      Idle          →  - - - -
      Score 0–9     →  _ _ X _   (3rd segment)
      Score 10–99   →  _ _ X X   (3rd + 4th)
      Wrong button  →  d E A d
   ============================================================
*/

#include <Arduino.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include "pitches.h"

/* ============================================================
   ██████╗  CONFIG — tune everything here
   ============================================================ */

// ── Simon hardware ───────────────────────────────────────────
const uint8_t BUTTON_PINS[4] = {4, 5, 7, 8};   // Active-LOW buttons
const uint8_t LED_PINS[4]    = {6, 9, 10, 11};   // Game LEDs (PWM-capable)
#define SPEAKER_PIN   12

// ── TM1637 display ───────────────────────────────────────────
#define TM_CLK_PIN    2
#define TM_DIO_PIN    3
#define TM_BRIGHTNESS 7   // 0 (dim) – 7 (max)

// ── NeoPixel strip ───────────────────────────────────────────
#define NEO_PIN       13   // Data pin for NeoPixel strip
#define NEO_COUNT     110   // Number of pixels in your strip
#define NEO_BRIGHTNESS 255  // Global strip brightness 0–255

// ── Idle RGB cycle ───────────────────────────────────────────
#define IDLE_HUE_STEP      300   // Hue advance per tick (0–65535 range)
#define IDLE_CYCLE_DELAY_MS  20  // ms between hue steps

// ── NeoPixel game colour (light pink) ───────────────────────
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180
#define NEO_FADE_STEPS  40   // Steps for fade-in / fade-out transitions
#define NEO_FADE_DELAY_MS 15 // ms per fade step

// ── LED idle random-fade ─────────────────────────────────────
#define LED_IDLE_MIN_BRIGHT  0    // PWM floor during idle breathe
#define LED_IDLE_MAX_BRIGHT 180   // PWM ceiling during idle breathe
#define LED_IDLE_FADE_SPEED   3   // PWM step size per tick
#define LED_IDLE_TICK_MS      8   // ms per idle LED tick

// ── Gameplay timing ──────────────────────────────────────────
#define GAME_TONE_ON_MS    300    // LED+tone on duration
#define GAME_TONE_GAP_MS    50   // Gap between sequence notes
#define LEVEL_UP_NOTE_MS   150   // Duration of each level-up note
#define GAME_OVER_DELAY_MS 200   // Pause before game-over sound
#define TIMEOUT_MS       60000UL // 1 minute inactivity → auto-lose

// ── Game constants ───────────────────────────────────────────
#define MAX_GAME_LENGTH 100
const int GAME_TONES[4] = {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

/* ============================================================
   Segment encodings
   ============================================================ */
const uint8_t SEG_DASH = 0b01000000;   // -
//  d  =  segments b,c,e,g  → 0b01011110  but TM1637 bit order:
//       bit: 6(g) 5(f) 4(e) 3(d) 2(c) 1(b) 0(a)
//  d:  g=1 f=0 e=1 d=1 c=1 b=1 a=0  → lower-case d = 0b01011110
//  E:  g=1 f=1 e=1 d=1 c=0 b=0 a=1  → 0b01111001
//  A:  g=1 f=1 e=1 d=0 c=1 b=1 a=1  → 0b01110111
// AFTER
const uint8_t MY_SEG_d = 0b01011110;   // d (lower-case)
const uint8_t MY_SEG_E = 0b01111001;   // E
const uint8_t MY_SEG_A = 0b01110111;   // A

const uint8_t SEG_ALL_DASH[4] = {SEG_DASH, SEG_DASH, SEG_DASH, SEG_DASH};
//const uint8_t SEG_DEAD[4]     = {SEG_d, SEG_E, SEG_A, SEG_d};
const uint8_t SEG_DEAD[4] = {MY_SEG_d, MY_SEG_E, MY_SEG_A, MY_SEG_d};

/* ============================================================
   Global state
   ============================================================ */
TM1637Display    tm(TM_CLK_PIN, TM_DIO_PIN);
Adafruit_NeoPixel strip(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

uint8_t  gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t  gameIndex   = 0;
bool     gameRunning = false;

// Per-LED idle fade state
int16_t  ledBright[4];       // current PWM value
int8_t   ledDir[4];          // +1 rising, -1 falling

/* ============================================================
   ██╗  ██╗███████╗██╗     ██████╗ ███████╗██████╗ ███████╗
   ██║  ██║██╔════╝██║     ██╔══██╗██╔════╝██╔══██╗██╔════╝
   ███████║█████╗  ██║     ██████╔╝█████╗  ██████╔╝███████╗
   ██╔══██║██╔══╝  ██║     ██╔═══╝ ██╔══╝  ██╔══██╗╚════██║
   ██║  ██║███████╗███████╗██║     ███████╗██║  ██║███████║
   ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝     ╚══════╝╚═╝  ╚═╝╚══════╝
   ============================================================ */

/* ── NeoPixel helpers ──────────────────────────────────────── */

/**
   Set all NeoPixels to one RGB colour at a given brightness scale (0–255).
   scale=255 → full NEO_BRIGHTNESS, scale=0 → off.
*/
void neoFill(uint8_t r, uint8_t g, uint8_t b, uint8_t scale = 255) {
  uint8_t rs = (uint16_t)r * scale / 255;
  uint8_t gs = (uint16_t)g * scale / 255;
  uint8_t bs = (uint16_t)b * scale / 255;
  strip.fill(strip.Color(rs, gs, bs));
  strip.show();
}

/**
   Fade NeoPixel strip from current colour to target RGB.
   Captures the "from" colour of pixel 0 as the starting point.
*/
void neoFadeTo(uint8_t toR, uint8_t toG, uint8_t toB) {
  uint32_t from  = strip.getPixelColor(0);  // packed RGB of first pixel
  uint8_t  fromR = (from >> 16) & 0xFF;
  uint8_t  fromG = (from >>  8) & 0xFF;
  uint8_t  fromB =  from        & 0xFF;

  //ESP Serial.print("[NEO] Fading (");
  Serial.print(fromR);
  Serial.print(",");
  Serial.print(fromG);
  Serial.print(",");
  Serial.print(fromB);
  Serial.print(") -> (");
  Serial.print(toR);
  Serial.print(",");
  Serial.print(toG);
  Serial.print(",");
  Serial.print(toB);
  Serial.println(")");
  for (int step = 0; step <= NEO_FADE_STEPS; step++) {
    float t  = (float)step / NEO_FADE_STEPS;
    uint8_t r = fromR + t * (toR - fromR);
    uint8_t g = fromG + t * (toG - fromG);
    uint8_t b = fromB + t * (toB - fromB);
    strip.fill(strip.Color(r, g, b));
    strip.show();
    delay(NEO_FADE_DELAY_MS);
  }
}

/**
   One tick of the idle RGB hue-cycle.  Call repeatedly while waiting.
   Uses HSV so every pixel steps through the rainbow in sync.
   Returns the updated hue (pass it back in next call).
*/
uint16_t neoRainbowTick(uint16_t hue) {
  strip.fill(strip.gamma32(strip.ColorHSV(hue)));
  strip.show();
  hue += IDLE_HUE_STEP;   // wraps automatically at 65536
  return hue;
}

/* ── Game-LED helpers ──────────────────────────────────────── */

/**
   Initialise idle fade state for each LED with random starting points
   so they don't all pulse in sync.
*/
void initIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] = random(LED_IDLE_MIN_BRIGHT, LED_IDLE_MAX_BRIGHT);
    ledDir[i]    = random(0, 2) ? 1 : -1;
    analogWrite(LED_PINS[i], ledBright[i]);
  }
  Serial.println("[LED] Idle fade initialised");
}

/**
   One tick of the random LED fade-in/fade-out idle animation.
   Each LED independently walks up/down in brightness, reversing at limits
   and occasionally randomly choosing a new direction for organic feel.
*/
void tickIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] += ledDir[i] * LED_IDLE_FADE_SPEED;

    // Clamp and reverse at limits
    if (ledBright[i] >= LED_IDLE_MAX_BRIGHT) {
      ledBright[i] = LED_IDLE_MAX_BRIGHT;
      ledDir[i]    = -1;
    } else if (ledBright[i] <= LED_IDLE_MIN_BRIGHT) {
      ledBright[i] = LED_IDLE_MIN_BRIGHT;
      ledDir[i]    = 1;
    }

    // 1-in-80 chance to randomly flip direction → organic "breathing" feel
    if (random(80) == 0) ledDir[i] = -ledDir[i];

    analogWrite(LED_PINS[i], ledBright[i]);
  }
}

/**
   Fade all game LEDs from their current idle brightness to full ON (255).
   Called when transitioning idle → game.
*/
void fadeLedsToFull() {
  Serial.println("[LED] Fading to full brightness for game");
  // Find max current brightness to set a consistent fade duration
  int maxSteps = 255 / max((int)LED_IDLE_FADE_SPEED, 1);
  for (int step = 0; step <= maxSteps; step++) {
    for (byte i = 0; i < 4; i++) {
      int val = ledBright[i] + ((255 - ledBright[i]) * step / maxSteps);
      analogWrite(LED_PINS[i], val);
    }
    delay(5);
  }
  // Ensure all exactly at 255
  for (byte i = 0; i < 4; i++) {
    ledBright[i] = 255;
    analogWrite(LED_PINS[i], 255);
  }
}

/**
   Turn all game LEDs off instantly (used during game-over / returning to idle).
*/
void ledsOff() {
  for (byte i = 0; i < 4; i++) {
    digitalWrite(LED_PINS[i], LOW);
    ledBright[i] = 0;
  }
}

/* ── TM1637 display helpers ────────────────────────────────── */

/** Show  - - - -  on all 4 digits (idle / ready state). */
void displayIdle() {
  tm.setSegments(SEG_ALL_DASH, 4, 0);
  Serial.println("[DISPLAY] ----");
}

/**
   Show 'd E A d' when wrong button pressed.
   Flashes 3× then stays on briefly.
*/
void displayDead() {
  Serial.println("[DISPLAY] dEAd");
  for (byte f = 0; f < 3; f++) {
    tm.setSegments(SEG_DEAD, 4, 0);
    delay(250);
    tm.clear();
    delay(150);
  }
  tm.setSegments(SEG_DEAD, 4, 0);
}

/**
   Show current round score on segments 2–3.
   Segments 0–1 are blanked.
   Score 0–9  → [_][_][X][_]
   Score 10–99→ [_][_][X][X]
*/
void displayScore()
{
  uint8_t blank = 0x00;
  tm.setSegments(&blank, 1, 0);
  tm.setSegments(&blank, 1, 1);

  int score = (int)gameIndex;

  if (score < 10) {
    tm.showNumberDecEx(score, 0, false, 1, 2);  // 1 digit at position 2
    tm.setSegments(&blank, 1, 3);               // blank position 3

    //ESP Serial.printf ("[DISPLAY] Score: %d (pos 2)\n", score);
    Serial.print("[DISPLAY] Score: ");
    Serial.print(score);
    Serial.println(" (pos 2-3)");
  } else {
    tm.showNumberDecEx(score % 100, 0, false, 2, 2);  // 2 digits pos 2–3

    //ESP //ESP Serial.printf ("[DISPLAY] Score: %d (pos 2-3)\n", score);
    Serial.print("[DISPLAY] Score: ");
    Serial.print(score);
    Serial.println(" (pos 2-3)");
  }
}
/* ── Tone / sequence helpers ───────────────────────────────── */

/** Light one LED and play its associated tone, then turn off. */
void lightLedAndPlayTone(byte ledIndex)
{
  //ESP //ESP Serial.printf ("[TONE] LED %d, note %d Hz\n", ledIndex, GAME_TONES[ledIndex]);
  Serial.print("[TONE] LED ");
  Serial.print(ledIndex);
  Serial.print(", note ");
  Serial.print(GAME_TONES[ledIndex]);
  Serial.println(" Hz");
  digitalWrite(LED_PINS[ledIndex], HIGH);
  tone(SPEAKER_PIN, GAME_TONES[ledIndex]);
  delay(GAME_TONE_ON_MS);
  digitalWrite(LED_PINS[ledIndex], LOW);
  noTone(SPEAKER_PIN);
}

/** Play back the full sequence for the current round. */
void playSequence() {
  //ESP Serial.printf ("[SEQ] Playing sequence of length %d\n", gameIndex);
  Serial.print("[SEQ] Playing sequence of length ");
  Serial.println(gameIndex);
  for (int i = 0; i < gameIndex; i++) {
    lightLedAndPlayTone(gameSequence[i]);
    delay(GAME_TONE_GAP_MS);
  }
}

/** Play ascending jingle when player completes a round correctly. */
void playLevelUpSound() {
  Serial.println("[SFX] Level up!");
  tone(SPEAKER_PIN, NOTE_E4); delay(LEVEL_UP_NOTE_MS);
  tone(SPEAKER_PIN, NOTE_G4); delay(LEVEL_UP_NOTE_MS);
  tone(SPEAKER_PIN, NOTE_E5); delay(LEVEL_UP_NOTE_MS);
  tone(SPEAKER_PIN, NOTE_C5); delay(LEVEL_UP_NOTE_MS);
  tone(SPEAKER_PIN, NOTE_D5); delay(LEVEL_UP_NOTE_MS);
  tone(SPEAKER_PIN, NOTE_G5); delay(LEVEL_UP_NOTE_MS);
  noTone(SPEAKER_PIN);
}

/** Play the descending wah-wah game-over sound. */
void playGameOverSound() {
  Serial.println("[SFX] Game over sound");
  tone(SPEAKER_PIN, NOTE_DS5); delay(300);
  tone(SPEAKER_PIN, NOTE_D5);  delay(300);
  tone(SPEAKER_PIN, NOTE_CS5); delay(300);
  for (byte i = 0; i < 10; i++) {
    for (int pitch = -10; pitch <= 10; pitch++) {
      tone(SPEAKER_PIN, NOTE_C5 + pitch);
      delay(6);
    }
  }
  noTone(SPEAKER_PIN);
}

/* ============================================================
   ██████╗  █████╗ ███╗   ███╗███████╗
  ██╔════╝ ██╔══██╗████╗ ████║██╔════╝
  ██║  ███╗███████║██╔████╔██║█████╗
  ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝
  ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗
   ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝
   FLOW FUNCTIONS
   ============================================================ */

/**
   IDLE MODE
   ─────────
   Runs continuously until a button is pressed.
   • NeoPixel: rainbow RGB cycle
   • LEDs: random independent fade-in/fade-out
   • Display: ----
   Returns the index of the button that was pressed to start the game.
*/
byte idleMode() {
  Serial.println("[IDLE] Entering idle mode");
  displayIdle();
  initIdleLeds();

  uint16_t hue        = 0;
  uint32_t lastLedTick = millis();
  uint32_t lastHueTick = millis();

  while (true) {
    uint32_t now = millis();

    // NeoPixel rainbow tick
    if (now - lastHueTick >= IDLE_CYCLE_DELAY_MS) {
      hue = neoRainbowTick(hue);
      lastHueTick = now;
    }

    // LED breathe tick
    if (now - lastLedTick >= LED_IDLE_TICK_MS) {
      tickIdleLeds();
      lastLedTick = now;
    }

    // Check for any button press to start game
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) {
        //ESP Serial.printf ("[IDLE] Button %d pressed — starting game\n", i);
        Serial.print("[IDLE] Button ");
        Serial.print(i);
        Serial.println(" pressed - starting game");
        return i;  // caller uses this as the first confirmed press
      }
    }
  }
}

/**
   GAME START TRANSITION
   ─────────────────────
   Called once when moving from idle → game.
   • NeoPixel fades from current rainbow colour → light pink
   • Game LEDs fade from idle brightness → full ON
*/
void startGameTransition() {
  Serial.println("[TRANSITION] Idle → Game");

  // Fade NeoPixel to light pink
  neoFadeTo(GAME_NEO_R, GAME_NEO_G, GAME_NEO_B);

  // Fade LEDs up to full
  ledsOff();

  Serial.println("[TRANSITION] Done — game ready");
}

/**
   GAME OVER TRANSITION
   ────────────────────
   Shows dEAd, plays wah-wah, fades NeoPixel back to off,
   turns LEDs off, resets game state, returns to idle display.
*/
void gameOverTransition() {
  int finalScore = (int)gameIndex - 1;
  //ESP Serial.printf ("[GAME OVER] Score was %d\n", finalScore);
  Serial.print("[GAME OVER] Score was ");
  Serial.println(finalScore);
  // Show dEAd on display + flash
  displayDead();

  delay(GAME_OVER_DELAY_MS);

  // Play wah-wah sound
  playGameOverSound();

  // Fade NeoPixel back to off
  neoFadeTo(0, 0, 0);

  // LEDs off
  ledsOff();

  // Reset game state
  gameIndex   = 0;
  gameRunning = false;

  Serial.println("[GAME OVER] Transition complete — returning to idle");
}

/**
   READ BUTTONS (with 1-minute timeout)
   ─────────────────────────────────────
   Blocks until a button is pressed OR timeout expires.
   Returns 0–3 for the pressed button.
   Returns 255 as a sentinel if timeout occurred.
*/
byte readButtonWithTimeout() {
  uint32_t startWait = millis();

  while (true) {
    // Check timeout
    if (millis() - startWait >= TIMEOUT_MS) {
      Serial.println("[INPUT] Timeout! No button pressed for 1 minute");
      return 255;  // sentinel value → treat as game lost
    }

    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) {
        //ESP Serial.printf ("[INPUT] Button %d pressed (%.1fs elapsed)\n",i, (millis() - startWait) / 1000.0f);
        Serial.print("[INPUT] Button ");
        Serial.print(i);
        Serial.print(" pressed (");

        float elapsed = (millis() - startWait) / 1000.0;
        Serial.print(elapsed, 1);  // 1 decimal place

        Serial.println("s elapsed)");
        return i;
      }
    }
    delay(1);
  }
}

/**
   CHECK USER SEQUENCE
   ───────────────────
   Reads button presses and compares to expected sequence.
   Returns true if correct, false on wrong press or timeout.
*/
bool checkUserSequence() {
  Serial.println("[INPUT] Waiting for user to repeat sequence");

  for (int i = 0; i < gameIndex; i++) {
    byte expectedButton = gameSequence[i];
    byte actualButton   = readButtonWithTimeout();

    // Timeout sentinel
    if (actualButton == 255) {
      Serial.println("[INPUT] Timeout during sequence check");
      return false;
    }

    lightLedAndPlayTone(actualButton);

    if (expectedButton != actualButton) {
      //ESP Serial.printf ("[INPUT] Wrong! Expected %d, got %d\n",expectedButton, actualButton);
      Serial.print("[INPUT] Wrong! Expected ");
      Serial.print(expectedButton);
      Serial.print(", got ");
      Serial.println(actualButton);
      return false;
    }
    //ESP Serial.printf ("[INPUT] Step %d/%d correct\n", i + 1, gameIndex);
    Serial.print("[INPUT] Step ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(gameIndex);
    Serial.println(" correct");
  }

  Serial.println("[INPUT] Sequence correct!");
  return true;
}

/* ============================================================
   MAIN SETUP & LOOP
   ============================================================ */

void setup() {
  Serial.begin(9600);
  Serial.println("\n[BOOT] Simon Game starting...");

  // Button and LED pins
  for (byte i = 0; i < 4; i++) {
    pinMode(LED_PINS[i],    OUTPUT);
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);

  // TM1637 display
  tm.setBrightness(TM_BRIGHTNESS);
  displayIdle();
  Serial.println("[BOOT] TM1637 ready");

  // NeoPixel strip
  strip.begin();
  strip.setBrightness(NEO_BRIGHTNESS);
  strip.clear();
  strip.show();
  Serial.println("[BOOT] NeoPixel ready");

  // Seed RNG from floating pin
  randomSeed(analogRead(4));
  Serial.println("[BOOT] Setup complete\n");
}

void loop()
{
  // ── IDLE: wait for first button press ─────────────────────
  byte firstPress = idleMode();   // blocks until button pressed

  // ── TRANSITION: idle visuals → game visuals ───────────────
  startGameTransition();

  // ── GAME LOOP ─────────────────────────────────────────────
  gameRunning = true;
  gameIndex   = 0;

  // Note: we discard firstPress here — game always starts with
  // Simon showing the first sequence step.  If you'd prefer the
  // player's first press to count as step 1, that's a minor tweak.

  while (gameRunning) {
    // Update score display before new round
    displayScore();

    // Add a new random step to sequence
    gameSequence[gameIndex] = random(0, 4);
    gameIndex++;
    if (gameIndex >= MAX_GAME_LENGTH) {
      gameIndex = MAX_GAME_LENGTH - 1;  // cap at max
    }

    //ESP Serial.printf ("\n[ROUND] Round %d begin\n", gameIndex);
    Serial.println();  // for the \n at the start
    Serial.print("[ROUND] Round ");
    Serial.print(gameIndex);
    Serial.println(" begin");
    // Show the sequence to the player
    playSequence();

    // Ask player to repeat it
    if (!checkUserSequence()) {
      // Wrong button or timeout → game over
      gameOverTransition();
      return;  // restart loop() → falls into idleMode() again
    }

    delay(300);
    playLevelUpSound();
    delay(300);
  }
}
