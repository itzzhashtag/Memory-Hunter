/*
________________________________________________________________________
  _   _    _    ____  _   _ _____   _     ____ 
 | | | |  / \  / ___|| | | |_   _| / \   / ___|
 | |_| | / _ \ \___ \| |_| | | |  / _ \ | |  _ 
 |  _  |/ ___ \ ___) |  _  | | | / ___ \| |_| |
 |_| |_/_/   \_\____/|_| |_| |_|/_/   \_\\____|

  Memory_Hunter_v1.4  — Using Arduino UNO + WS2812B
  Wokwi Simulation - "https://wokwi.com/projects/461778477172063233"
_________________________________________________________________________
  
  Name: Aniket Chowdhury [Hashtag]
  Email: micro.aniket@gmail.com
  GitHub: https://github.com/itzzhashtag
  Instagram: https://instagram.com/itzz_hashtag
  LinkedIn: https://www.linkedin.com/in/itzz-hashtag/
_________________________________________________________________________

  ABOUT THE GAME — Memory Hunter
  ────────────────────────────────────────────────────────────────────────
  Memory Hunter is a hardware Simon-Says memory game built on Arduino UNO.
  The machine plays an ever-growing sequence of coloured light+sound cues,
  and the player must repeat them back in perfect order.

  Each round adds one new random step to the sequence. Get it right and
  you level up with a jingle. Get it wrong (or take too long) and the
  display flashes "dEAd", the buzzer wails, and it's back to idle.

  FEATURES
  ─────────
  • 4 coloured LEDs + 4 buttons — one per Simon colour
  • TM1637 4-digit 7-segment display
      – Idle         →  - - - -
      – Countdown    →  _33_ → _22_ → _11_ → _GO_
      – Score 0–9    →  _ _ X _   (3rd segment)
      – Score 10–99  →  _ X X _   (2nd + 4th)
      – Game over    →  d E A d   (flashing)
  • WS2812B NeoPixel strip
      – Idle   → chasing rainbow (each pixel offset around colour wheel)
      – Game   → fades to solid light pink
      – Return → fades back to black, then rainbow fades in again
  • Passive buzzer — unique tone per button, countdown beeps, level-up
    jingle, game-over wah-wah, start-press confirmation beep
  • 1-minute inactivity timeout during player input → treated as a loss
  • Organic LED breathe animation during idle (each LED independent)

  HARDWARE
  ─────────
  Arduino UNO  |  PWM pins used: 3, 5, 6, 9, 10, 11
  ⚠ Pin 12 is NOT PWM on UNO — LED 4 is on pin 3 instead

  PIN MAP
  ────────────────────────────────
  Buttons      →  4, 5, 7, 8      (INPUT_PULLUP, active LOW)
  LEDs         →  6, 9, 10, 11    (all PWM)   ← wait, see CONFIG
  Speaker      →  12              (tone())
  TM1637 CLK   →  2
  TM1637 DIO   →  3
  NeoPixel     →  13
  RNG seed     →  A5              (leave floating)
  ────────────────────────────────

  Original Simon sketch: Uri Shaked (2023) — MIT License
  Extended & redesigned: Aniket Chowdhury [Hashtag] — 2025
________________________________________________________________________
*/


#include <Arduino.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include "pitches.h"

/* ============================================================
   CONFIG — tune everything here
   ============================================================ */

// ── Simon hardware ───────────────────────────────────────────
// LED_PINS must all be PWM-capable on UNO
const uint8_t BUTTON_PINS[4] = {4, 5, 7, 8};   // Active-LOW buttons
const uint8_t LED_PINS[4]    = {6, 9, 10, 11};   // Game LEDs (PWM-capable)
#define SPEAKER_PIN   12

// ── TM1637 display ───────────────────────────────────────────
#define TM_CLK_PIN    2
#define TM_DIO_PIN    3
#define TM_BRIGHTNESS 7   // 0 (dim) – 7 (max)

// ── NeoPixel strip ───────────────────────────────────────────
#define NEO_PIN       13   // Data pin for NeoPixel strip
#define NEO_COUNT     46   // Number of pixels in your strip
#define NEO_BRIGHTNESS 255  // Global strip brightness 0–255
// ── Idle RGB cycle ───────────────────────────────────────────
#define IDLE_HUE_STEP       300  // Hue advance per tick (0–65535 range)
#define IDLE_CYCLE_DELAY_MS  20  // ms between hue steps
#define IDLE_FADE_IN_STEPS   60  // Steps to fade rainbow in from black on re-entry

// ── NeoPixel game colour (light pink) ───────────────────────
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180
#define NEO_FADE_STEPS    40  // Steps for fade-in / fade-out transitions
#define NEO_FADE_DELAY_MS 15  // ms per fade step

// ── LED idle random-fade ─────────────────────────────────────
#define LED_IDLE_MIN_BRIGHT   0  // PWM floor during idle breathe
#define LED_IDLE_MAX_BRIGHT 180  // PWM ceiling during idle breathe
#define LED_IDLE_FADE_SPEED   3  // PWM step size per tick
#define LED_IDLE_TICK_MS      8  // ms per idle LED tick

// ── Gameplay timing ──────────────────────────────────────────
#define GAME_TONE_ON_MS    300    // LED+tone on duration
#define GAME_TONE_GAP_MS    50   // Gap between sequence notes
#define LEVEL_UP_NOTE_MS   150   // Duration of each level-up note
#define GAME_OVER_DELAY_MS 200   // Pause before game-over sound
#define POST_GAMEOVER_WAIT 1000  // Wait (ms) after game-over sound before idle
#define TIMEOUT_MS       60000UL // 1 minute inactivity → auto-lose

// ── Countdown timing ─────────────────────────────────────────
#define COUNTDOWN_STEP_MS 600    // How long each countdown frame shows (ms)

// ── Game constants ───────────────────────────────────────────
#define MAX_GAME_LENGTH 100
const int GAME_TONES[4] = {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

/* ============================================================
   Segment encodings
   ============================================================
   TM1637 bit order: bit 6=g 5=f 4=e 3=d 2=c 1=b 0=a
   d (lower): g=1 e=1 d=1 c=1 b=1 a=0  → 0b01011110
   E        : g=1 f=1 e=1 d=1 a=1      → 0b01111001
   A        : g=1 f=1 e=1 c=1 b=1 a=1  → 0b01110111
   G        : f=1 e=1 d=1 c=1 a=1      → 0b00111101
   O  = 0   : f=1 e=1 d=1 c=1 b=1 a=1  → 0b00111111
   ============================================================ */
const uint8_t SEG_DASH  = 0b01000000;
const uint8_t MY_SEG_d  = 0b01011110;
const uint8_t MY_SEG_E  = 0b01111001;
const uint8_t MY_SEG_A  = 0b01110111;
const uint8_t MY_SEG_G  = 0b00111101;  // G for "GO"
const uint8_t MY_SEG_O  = 0b00111111;  // O for "GO"

const uint8_t SEG_ALL_DASH[4] = {SEG_DASH, SEG_DASH, SEG_DASH, SEG_DASH};
const uint8_t SEG_DEAD[4]     = {MY_SEG_d, MY_SEG_E, MY_SEG_A, MY_SEG_d};

// Countdown frames: 33, 22, 11, GO
// showNumberDecEx handles 33/22/11; GO needs raw segments
const uint8_t SEG_GO[4] = {0x00, MY_SEG_G, MY_SEG_O, 0x00};  // _GO_  

/* ============================================================
   Global state
   ============================================================ */
TM1637Display     tm(TM_CLK_PIN, TM_DIO_PIN);
Adafruit_NeoPixel strip(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex   = 0;
bool    gameRunning = false;

// Per-LED idle fade state
int16_t ledBright[4];  // current PWM value 0–255
int8_t  ledDir[4];     // +1 rising, -1 falling

/* ============================================================
   NEOPIXEL HELPERS
   ============================================================ */

/**
   Fade NeoPixel strip from its current colour (read from pixel 0)
   to the target RGB over NEO_FADE_STEPS steps.
*/
void neoFadeTo(uint8_t toR, uint8_t toG, uint8_t toB) {
  uint32_t from  = strip.getPixelColor(0);
  uint8_t  fromR = (from >> 16) & 0xFF;
  uint8_t  fromG = (from >>  8) & 0xFF;
  uint8_t  fromB =  from        & 0xFF;

  Serial.print("[NEO] Fading (");
  Serial.print(fromR); Serial.print(",");
  Serial.print(fromG); Serial.print(",");
  Serial.print(fromB); Serial.print(") -> (");
  Serial.print(toR);   Serial.print(",");
  Serial.print(toG);   Serial.print(",");
  Serial.print(toB);   Serial.println(")");

  for (int step = 0; step <= NEO_FADE_STEPS; step++) {
    float   t = (float)step / NEO_FADE_STEPS;
    uint8_t r = fromR + t * (toR - fromR);
    uint8_t g = fromG + t * (toG - fromG);
    uint8_t b = fromB + t * (toB - fromB);
    strip.fill(strip.Color(r, g, b));
    strip.show();
    delay(NEO_FADE_DELAY_MS);
  }
}

/**
   One tick of the idle HSV rainbow cycle.
   Call repeatedly; pass hue in, get updated hue back.
   brightness: 0–255 scale applied on top of NEO_BRIGHTNESS,
               used during fade-in (pass 255 for normal operation).
*/
uint16_t neoRainbowTick(uint16_t hue, uint8_t brightness = 255) 
{
  // Spread a full colour wheel across the strip by offsetting
  // each pixel's hue by an equal fraction of the 65536 hue range.
  // As hue advances each tick, the whole pattern rotates → chasing rainbow.
  for (int i = 0; i < strip.numPixels(); i++) {
    uint16_t pixelHue = hue + ((uint32_t)i * 65536 / strip.numPixels());
    uint32_t color    = strip.gamma32(strip.ColorHSV(pixelHue));
    // Scale by brightness (used during fade-in ramp)
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >>  8) & 0xFF) * brightness / 255;
    uint8_t b = ( color        & 0xFF) * brightness / 255;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  hue += IDLE_HUE_STEP;  // advance the whole pattern each tick → rotation
  return hue;
}

/**
   Fade the rainbow in from black over IDLE_FADE_IN_STEPS ticks.
   Runs a blocking ramp so re-entry to idle looks smooth.
   Returns the hue value to continue rainbow from.
*/
uint16_t neoFadeInRainbow() {
  Serial.println("[NEO] Fading rainbow in from black");
  uint16_t hue = 0;
  for (int step = 0; step <= IDLE_FADE_IN_STEPS; step++) {
    // Linearly ramp brightness 0→255 over the fade-in steps
    uint8_t brightness = (uint8_t)((255L * step) / IDLE_FADE_IN_STEPS);
    hue = neoRainbowTick(hue, brightness);
    delay(IDLE_CYCLE_DELAY_MS);
  }
  Serial.println("[NEO] Rainbow fade-in complete");
  return hue;
}

/* ============================================================
   GAME-LED HELPERS
   ============================================================ */

/**
   Initialise each LED at a random brightness and direction
   so they don't all breathe in sync during idle.
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
   One tick of the organic idle LED breathe animation.
   Each LED walks independently; 1-in-80 chance to randomly
   flip direction for a non-mechanical feel.
*/
void tickIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] += ledDir[i] * LED_IDLE_FADE_SPEED;

    // Clamp and bounce at limits
    if (ledBright[i] >= LED_IDLE_MAX_BRIGHT) {
      ledBright[i] = LED_IDLE_MAX_BRIGHT;
      ledDir[i]    = -1;
    } else if (ledBright[i] <= LED_IDLE_MIN_BRIGHT) {
      ledBright[i] = LED_IDLE_MIN_BRIGHT;
      ledDir[i]    = 1;
    }

    // Occasional random direction flip → organic breathing
    if (random(80) == 0) ledDir[i] = -ledDir[i];

    analogWrite(LED_PINS[i], ledBright[i]);
  }
}

/**
   Turn all game LEDs off and zero their brightness tracking.
   Used when transitioning into the game or resetting.
*/
void ledsOff() {
  for (byte i = 0; i < 4; i++) {
    analogWrite(LED_PINS[i], 0);
    ledBright[i] = 0;
  }
  Serial.println("[LED] All LEDs off");
}

/* ============================================================
   TM1637 DISPLAY HELPERS
   ============================================================ */

/** Show  - - - -  on all 4 digits (idle / ready state). */
void displayIdle() {
  tm.setSegments(SEG_ALL_DASH, 4, 0);
  Serial.println("[DISPLAY] ----");
}

/**
   Flash  d E A d  3 times then leave it on.
   Called on wrong button press or timeout.
*/
void displayDead() {
  Serial.println("[DISPLAY] dEAd");
  for (byte f = 0; f < 3; f++) {
    tm.setSegments(SEG_DEAD, 4, 0);
    delay(250);
    tm.clear();
    delay(150);
  }
  tm.setSegments(SEG_DEAD, 4, 0);  // leave on
}

/**
   Show current round score on segments 2–3; blank 0–1.
     Score 0–9  → [_][_][X][_]
     Score 10–99→ [_][_][X][X]
*/
void displayScore() {
  uint8_t blank = 0x00;
  tm.setSegments(&blank, 1, 0);
  tm.setSegments(&blank, 1, 1);

  int score = (int)gameIndex;
  if (score < 10) {
    tm.showNumberDecEx(score, 0, false, 1, 2);
    tm.setSegments(&blank, 1, 3);
    Serial.print("[DISPLAY] Score: "); Serial.print(score); Serial.println(" (pos 2)");
  } else {
  tm.showNumberDecEx(score % 100, 0, false, 2, 1);  // 2 digits pos 1–2 → _XX_
  tm.setSegments(&blank, 1, 3);                      // blank pos 3
  Serial.print("[DISPLAY] Score: "); Serial.print(score); Serial.println(" (pos 1-2)");
  }
}

/**
   Blocking countdown shown before the first round begins.
   Sequence: 33 → 22 → 11 → GO
   Gives the player time to get ready after pressing start.
*/
void displayCountdown() {
  Serial.println("[DISPLAY] Countdown: 33->22->11->GO");

  // AFTER — build each frame as [blank][digit][digit][blank]
  uint8_t blank = 0x00;

  // _33_
  uint8_t seg33[4] = {blank, tm.encodeDigit(3), tm.encodeDigit(3), blank};
  tm.setSegments(seg33, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  // _22_
  uint8_t seg22[4] = {blank, tm.encodeDigit(2), tm.encodeDigit(2), blank};
  tm.setSegments(seg22, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  // _11_
  uint8_t seg11[4] = {blank, tm.encodeDigit(1), tm.encodeDigit(1), blank};
  tm.setSegments(seg11, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  // GO — raw segments, higher pitch beep
  tm.setSegments(SEG_GO, 4, 0);
  tone(SPEAKER_PIN, NOTE_G4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);
  delay(1000);
  Serial.println("[DISPLAY] Countdown done — GO!");
}

/* ============================================================
   TONE / SEQUENCE HELPERS
   ============================================================ */

/** Light one LED and play its associated tone, then turn off. */
void lightLedAndPlayTone(byte ledIndex) {
  Serial.print("[TONE] LED "); Serial.print(ledIndex);
  Serial.print(", note "); Serial.print(GAME_TONES[ledIndex]); Serial.println(" Hz");
  digitalWrite(LED_PINS[ledIndex], HIGH);
  tone(SPEAKER_PIN, GAME_TONES[ledIndex]);
  delay(GAME_TONE_ON_MS);
  digitalWrite(LED_PINS[ledIndex], LOW);
  noTone(SPEAKER_PIN);
}

/** Play back the full sequence for the current round. */
void playSequence() {
  Serial.print("[SEQ] Playing sequence of length "); Serial.println(gameIndex);
  for (int i = 0; i < gameIndex; i++) {
    lightLedAndPlayTone(gameSequence[i]);
    delay(GAME_TONE_GAP_MS);
  }
}

/** Ascending jingle on successful round completion. */
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

/** Descending wah-wah game-over sound. */
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
   FLOW FUNCTIONS
   ============================================================ */

/**
   IDLE MODE
   Blocks until any button is pressed.
   • NeoPixel: rainbow fades in from black, then cycles
   • LEDs: organic random breathe
   • Display: ----
   Returns index (0–3) of the button that started the game.
*/
byte idleMode()
{
  Serial.println("[IDLE] Entering idle mode");
  displayIdle();
  initIdleLeds();

  // Fade rainbow in from black instead of snapping to colour
  uint16_t hue        = neoFadeInRainbow();
  uint32_t lastLedTick = millis();
  uint32_t lastHueTick = millis();

  while (true) {
    uint32_t now = millis();

    // NeoPixel rainbow tick (full brightness now)
    if (now - lastHueTick >= IDLE_CYCLE_DELAY_MS) {
      hue = neoRainbowTick(hue, 255);
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
        Serial.print("[IDLE] Button "); Serial.print(i);
        Serial.println(" pressed - starting game");

        // ── ADD THIS: short confirmation beep on start button press ──
        tone(SPEAKER_PIN, NOTE_E5);
        delay(80);
        noTone(SPEAKER_PIN);
        // ─────────────────────────────────────────────────────────────

        return i;
      }
    }
  }
}

/**
   GAME START TRANSITION
   idle visuals → game visuals + countdown
   • NeoPixel fades from rainbow → light pink
   • LEDs go dark (game mechanics light them individually)
   • Countdown 33→22→11→GO shown on display
*/
void startGameTransition() {
  Serial.println("[TRANSITION] Idle -> Game");

  // NeoPixel: fade from whatever rainbow colour we're on → light pink
  neoFadeTo(GAME_NEO_R, GAME_NEO_G, GAME_NEO_B);

  // LEDs off — game will light them one at a time during sequence
  ledsOff();

  // Give player time to get ready
  displayCountdown();

  Serial.println("[TRANSITION] Done — game starting");
}

/**
   GAME OVER TRANSITION
   • Display dEAd (flashing)
   • Play wah-wah sound
   • Wait POST_GAMEOVER_WAIT ms
   • Fade NeoPixel to black (idle will fade rainbow back in)
   • LEDs off
   • Reset game state
*/
void gameOverTransition() {
  int finalScore = (int)gameIndex - 1;
  Serial.print("[GAME OVER] Score was "); Serial.println(finalScore);

  // Flash dEAd on display
  displayDead();

  delay(GAME_OVER_DELAY_MS);

  // Play wah-wah
  playGameOverSound();

  // Wait before returning to idle so the moment isn't rushed
  Serial.println("[GAME OVER] Waiting before idle...");
  delay(POST_GAMEOVER_WAIT);

  // Fade NeoPixel to black — idleMode() will fade rainbow back in
  neoFadeTo(0, 0, 0);

  // LEDs off
  ledsOff();

  // Reset game state
  gameIndex   = 0;
  gameRunning = false;

  Serial.println("[GAME OVER] Transition complete — returning to idle");
}

/**
   READ BUTTON WITH TIMEOUT
   Blocks until a button is pressed or TIMEOUT_MS elapses.
   Returns 0–3 for button index, or 255 as timeout sentinel.
*/
byte readButtonWithTimeout() {
  uint32_t startWait = millis();

  while (true) {
    if (millis() - startWait >= TIMEOUT_MS) {
      Serial.println("[INPUT] Timeout! No button pressed for 1 minute");
      return 255;  // sentinel → caller treats as game lost
    }

    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) {
        Serial.print("[INPUT] Button "); Serial.print(i);
        Serial.print(" pressed (");
        Serial.print((millis() - startWait) / 1000.0, 1);
        Serial.println("s elapsed)");
        return i;
      }
    }
    delay(1);
  }
}

/**
   CHECK USER SEQUENCE
   Reads buttons and compares to expected sequence.
   Returns true if fully correct, false on any error or timeout.
*/
bool checkUserSequence() {
  Serial.println("[INPUT] Waiting for user to repeat sequence");

  for (int i = 0; i < gameIndex; i++) {
    byte expectedButton = gameSequence[i];
    byte actualButton   = readButtonWithTimeout();

    if (actualButton == 255) {
      Serial.println("[INPUT] Timeout during sequence check");
      return false;
    }

    lightLedAndPlayTone(actualButton);

    if (expectedButton != actualButton) {
      Serial.print("[INPUT] Wrong! Expected "); Serial.print(expectedButton);
      Serial.print(", got "); Serial.println(actualButton);
      return false;
    }

    Serial.print("[INPUT] Step "); Serial.print(i + 1);
    Serial.print("/"); Serial.print(gameIndex); Serial.println(" correct");
  }

  Serial.println("[INPUT] Sequence correct!");
  return true;
}

/* ============================================================
   SETUP & LOOP
   ============================================================ */

void setup() {
  Serial.begin(9600);
  Serial.println("\n[BOOT] Simon Game starting...");

  for (byte i = 0; i < 4; i++) {
    pinMode(LED_PINS[i],    OUTPUT);
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);

  tm.setBrightness(TM_BRIGHTNESS);
  displayIdle();
  Serial.println("[BOOT] TM1637 ready");

  strip.begin();
  strip.setBrightness(NEO_BRIGHTNESS);
  strip.clear();
  strip.show();
  Serial.println("[BOOT] NeoPixel ready");

  randomSeed(analogRead(A5));  // A5 floating for entropy
  Serial.println("[BOOT] Setup complete\n");
}

void loop() {
  // ── IDLE: rainbow + breathe LEDs until button pressed ─────
  byte firstPress = idleMode();

  // ── TRANSITION: visuals swap + countdown ──────────────────
  startGameTransition();

  // ── GAME LOOP ─────────────────────────────────────────────
  gameRunning = true;
  gameIndex   = 0;

  while (gameRunning) {
    displayScore();

    // Append a new random step
    gameSequence[gameIndex] = random(0, 4);
    gameIndex++;
    if (gameIndex >= MAX_GAME_LENGTH) gameIndex = MAX_GAME_LENGTH - 1;

    Serial.println();
    Serial.print("[ROUND] Round "); Serial.print(gameIndex); Serial.println(" begin");

    playSequence();

    if (!checkUserSequence()) {
      gameOverTransition();
      return;  // exit loop() → re-enters from top → idleMode()
    }

    delay(300);
    playLevelUpSound();
    delay(300);
  }
}