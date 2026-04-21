/*
________________________________________________________________________
  _   _    _    ____  _   _ _____   _     ____ 
 | | | |  / \  / ___|| | | |_   _| / \   / ___|
 | |_| | / _ \ \___ \| |_| | | |  / _ \ | |  _ 
 |  _  |/ ___ \ ___) |  _  | | | / ___ \| |_| |
 |_| |_/_/   \_\____/|_| |_| |_|/_/   \_\\____|

  Memory_Hunter_v1.5  — Using Arduino UNO + WS2812B
  Wokwi Simulation - "https://wokwi.com/projects/461780873004005377"
_________________________________________________________________________
  
  Name: Aniket Chowdhury [Hashtag]
  Email: micro.aniket@gmail.com
  GitHub: https://github.com/itzzhashtag
  Instagram: https://instagram.com/itzz_hashtag
  LinkedIn: https://www.linkedin.com/in/itzz-hashtag/
_________________________________________________________________________

  CHANGELOG v1.4 → v1.5
  ────────────────────────────────────────────────────────────────────────
  FIX   — Dead display no longer triggers while in idle/lobby state.
          TIMEOUT_MS now only starts counting once the game has actually
          begun (gameRunning == true). Idle waits forever with no penalty.

  NEW   — EEPROM high score persistence + idle marquee display.
          High score survives power cycles. Idle display cycles through:
            [0--0]  ← decorative "radar" frame
            [HiSr]  ← "High Score" label
            [_XX_]  ← the actual saved score (2-digit centred)
          Each frame shows for HISCORE_FRAME_MS milliseconds.
          Beat the high score → saved immediately + celebration flash.
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
      – Idle         →  0 - - 0 / H i S r / _ X X _ (rotating)
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
    (timeout ONLY fires during active gameplay, never in idle)
  • Organic LED breathe animation during idle (each LED independent)
  • EEPROM high score — persists across power cycles

  HARDWARE
  ─────────
  Arduino UNO  |  PWM pins used: 3, 5, 6, 9, 10, 11
  ⚠ Pin 12 is NOT PWM on UNO — LED 4 is on pin 3 instead

  PIN MAP
  ────────────────────────────────
  Buttons      →  4, 5, 7, 8      (INPUT_PULLUP, active LOW)
  LEDs         →  6, 9, 10, 11    (all PWM)
  Speaker      →  12              (tone())
  TM1637 CLK   →  2
  TM1637 DIO   →  3
  NeoPixel     →  13
  RNG seed     →  A5              (leave floating)
  EEPROM addr  →  0 (score byte) + 1 (magic byte 0xAB for init check)
  ────────────────────────────────

  Original Simon sketch: Uri Shaked (2023) — MIT License
  Extended & redesigned: Aniket Chowdhury [Hashtag] — 2025
________________________________________________________________________
*/


#include <Arduino.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "pitches.h"

/* ============================================================
   CONFIG — tune everything here
   ============================================================ */

// ── Simon hardware ───────────────────────────────────────────
const uint8_t BUTTON_PINS[4] = {4, 5, 7, 8};   // Active-LOW buttons
const uint8_t LED_PINS[4]    = {6, 9, 10, 11};  // Game LEDs (PWM-capable)
#define SPEAKER_PIN   12

// ── TM1637 display ───────────────────────────────────────────
#define TM_CLK_PIN    2
#define TM_DIO_PIN    3
#define TM_BRIGHTNESS 7   // 0 (dim) – 7 (max)

// ── NeoPixel strip ───────────────────────────────────────────
#define NEO_PIN        13
#define NEO_COUNT      46
#define NEO_BRIGHTNESS 255

// ── Idle RGB cycle ───────────────────────────────────────────
#define IDLE_HUE_STEP       300
#define IDLE_CYCLE_DELAY_MS  20
#define IDLE_FADE_IN_STEPS   60

// ── NeoPixel game colour (light pink) ───────────────────────
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180
#define NEO_FADE_STEPS    40
#define NEO_FADE_DELAY_MS 15

// ── LED idle random-fade ─────────────────────────────────────
#define LED_IDLE_MIN_BRIGHT   0
#define LED_IDLE_MAX_BRIGHT 180
#define LED_IDLE_FADE_SPEED   3
#define LED_IDLE_TICK_MS      8

// ── Gameplay timing ──────────────────────────────────────────
#define GAME_TONE_ON_MS    300
#define GAME_TONE_GAP_MS    50
#define LEVEL_UP_NOTE_MS   150
#define GAME_OVER_DELAY_MS 200
#define POST_GAMEOVER_WAIT 1000
#define TIMEOUT_MS       60000UL   // only fires during active gameplay

// ── Countdown timing ─────────────────────────────────────────
#define COUNTDOWN_STEP_MS 600

// ── Game constants ───────────────────────────────────────────
#define MAX_GAME_LENGTH 100
const int GAME_TONES[4] = {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

// ── EEPROM high score ────────────────────────────────────────
#define EEPROM_ADDR_SCORE  0     // 1 byte: the high score value (0–255)
#define EEPROM_ADDR_MAGIC  1     // 1 byte: magic value to detect first run
#define EEPROM_MAGIC_VAL   0xAB  // arbitrary sentinel

// ── Idle high-score marquee ──────────────────────────────────
// Each of the 3 frames shows for this many milliseconds before cycling
#define HISCORE_FRAME_MS  1500UL

/* ============================================================
   Segment encodings
   ============================================================
   TM1637 bit order: bit 6=g 5=f 4=e 3=d 2=c 1=b 0=a
   ============================================================ */
const uint8_t SEG_DASH  = 0b01000000;   // -
const uint8_t MY_SEG_d  = 0b01011110;   // d
const uint8_t MY_SEG_E  = 0b01111001;   // E
const uint8_t MY_SEG_A  = 0b01110111;   // A
const uint8_t MY_SEG_G  = 0b00111101;   // G
const uint8_t MY_SEG_O  = 0b00111111;   // O
// High-score label: H i S r
const uint8_t MY_SEG_H  = 0b01110110;   // H
const uint8_t MY_SEG_i  = 0b00010000;   // i  (segments e+d → lower-case i)
const uint8_t MY_SEG_S  = 0b01101101;   // S
const uint8_t MY_SEG_r  = 0b01010000;   // r  (segments g+e)
// Zero with dash wings: 0 - - 0
const uint8_t MY_SEG_0  = 0b00111111;   // 0

const uint8_t SEG_ALL_DASH[4] = {SEG_DASH, SEG_DASH, SEG_DASH, SEG_DASH};
const uint8_t SEG_DEAD[4]     = {MY_SEG_d, MY_SEG_E, MY_SEG_A, MY_SEG_d};
const uint8_t SEG_GO[4]       = {0x00, MY_SEG_G, MY_SEG_O, 0x00};  // _GO_
const uint8_t SEG_HISR[4]     = {MY_SEG_H, MY_SEG_i, MY_SEG_S, MY_SEG_r};
const uint8_t SEG_0DD0[4]     = {MY_SEG_0, SEG_DASH, SEG_DASH, MY_SEG_0};

/* ============================================================
   Global state
   ============================================================ */
TM1637Display     tm(TM_CLK_PIN, TM_DIO_PIN);
Adafruit_NeoPixel strip(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex   = 0;
bool    gameRunning = false;  // true only while player is actively playing

// Per-LED idle fade state
int16_t ledBright[4];
int8_t  ledDir[4];

/* ============================================================
   EEPROM HIGH SCORE
   ============================================================ */

/**
   Read high score from EEPROM.
   On first boot (magic byte absent) initialise score to 0.
*/
uint8_t loadHighScore() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_VAL) {
    // First run — write magic + zero score
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VAL);
    EEPROM.write(EEPROM_ADDR_SCORE, 0);
    Serial.println("[EEPROM] First run — high score initialised to 0");
    return 0;
  }
  uint8_t hs = EEPROM.read(EEPROM_ADDR_SCORE);
  Serial.print("[EEPROM] High score loaded: "); Serial.println(hs);
  return hs;
}

/**
   Save a new high score to EEPROM.
   Only writes if the value actually changed (protects write cycles).
*/
void saveHighScore(uint8_t hs) {
  if (EEPROM.read(EEPROM_ADDR_SCORE) != hs) {
    EEPROM.write(EEPROM_ADDR_SCORE, hs);
    Serial.print("[EEPROM] New high score saved: "); Serial.println(hs);
  }
}

// Globally accessible high score (kept in RAM, synced to EEPROM on update)
uint8_t highScore = 0;

/* ============================================================
   NEOPIXEL HELPERS
   ============================================================ */

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

uint16_t neoRainbowTick(uint16_t hue, uint8_t brightness = 255) {
  for (int i = 0; i < strip.numPixels(); i++) {
    uint16_t pixelHue = hue + ((uint32_t)i * 65536 / strip.numPixels());
    uint32_t color    = strip.gamma32(strip.ColorHSV(pixelHue));
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >>  8) & 0xFF) * brightness / 255;
    uint8_t b = ( color        & 0xFF) * brightness / 255;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  hue += IDLE_HUE_STEP;
  return hue;
}

uint16_t neoFadeInRainbow() {
  Serial.println("[NEO] Fading rainbow in from black");
  uint16_t hue = 0;
  for (int step = 0; step <= IDLE_FADE_IN_STEPS; step++) {
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

void initIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] = random(LED_IDLE_MIN_BRIGHT, LED_IDLE_MAX_BRIGHT);
    ledDir[i]    = random(0, 2) ? 1 : -1;
    analogWrite(LED_PINS[i], ledBright[i]);
  }
  Serial.println("[LED] Idle fade initialised");
}

void tickIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] += ledDir[i] * LED_IDLE_FADE_SPEED;
    if (ledBright[i] >= LED_IDLE_MAX_BRIGHT) {
      ledBright[i] = LED_IDLE_MAX_BRIGHT;
      ledDir[i]    = -1;
    } else if (ledBright[i] <= LED_IDLE_MIN_BRIGHT) {
      ledBright[i] = LED_IDLE_MIN_BRIGHT;
      ledDir[i]    = 1;
    }
    if (random(80) == 0) ledDir[i] = -ledDir[i];
    analogWrite(LED_PINS[i], ledBright[i]);
  }
}

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

/**
   Show the idle high-score marquee on the display.
   Advances through 3 frames (0--0 / HiSr / _XX_) based on elapsed time.
   Takes current millis() so the caller can drive it non-blockingly.

   frameIndex  0 → 0--0
               1 → HiSr
               2 → _XX_  (the actual high score, blank-padded)
   Returns the updated frameIndex.
*/
uint8_t updateIdleDisplay(uint32_t now,
                          uint8_t  frameIndex,
                          uint32_t &frameStart)
{
  if (now - frameStart >= HISCORE_FRAME_MS) {
    frameIndex = (frameIndex + 1) % 3;
    frameStart = now;

    switch (frameIndex) {
      case 0:
        tm.setSegments(SEG_0DD0, 4, 0);
        Serial.println("[DISPLAY] Idle frame: 0--0");
        break;

      case 1:
        tm.setSegments(SEG_HISR, 4, 0);
        Serial.println("[DISPLAY] Idle frame: HiSr");
        break;

      case 2: {
        // Centre the score: score < 10 → _X__  (pos 2), else _XX_ (pos 1-2)
        uint8_t blank = 0x00;
        tm.setSegments(&blank, 1, 0);    // pos 0 blank
        tm.setSegments(&blank, 1, 3);    // pos 3 blank
        if (highScore < 10) {
          tm.setSegments(&blank, 1, 1);
          tm.showNumberDecEx(highScore, 0, false, 1, 2);
        } else {
          tm.showNumberDecEx(highScore % 100, 0, false, 2, 1);
        }
        Serial.print("[DISPLAY] Idle frame: score "); Serial.println(highScore);
        break;
      }
    }
  }
  return frameIndex;
}

/**
   Flash  d E A d  3 times then leave it on.
   Only called from gameOverTransition() — never from idle.
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
    tm.showNumberDecEx(score % 100, 0, false, 2, 1);
    tm.setSegments(&blank, 1, 3);
    Serial.print("[DISPLAY] Score: "); Serial.print(score); Serial.println(" (pos 1-2)");
  }
}

/**
   Blocking countdown before first round.
   Sequence: 33 → 22 → 11 → GO
*/
void displayCountdown() {
  Serial.println("[DISPLAY] Countdown: 33->22->11->GO");

  uint8_t blank = 0x00;

  uint8_t seg33[4] = {blank, tm.encodeDigit(3), tm.encodeDigit(3), blank};
  tm.setSegments(seg33, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  uint8_t seg22[4] = {blank, tm.encodeDigit(2), tm.encodeDigit(2), blank};
  tm.setSegments(seg22, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  uint8_t seg11[4] = {blank, tm.encodeDigit(1), tm.encodeDigit(1), blank};
  tm.setSegments(seg11, 4, 0);
  tone(SPEAKER_PIN, NOTE_C4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);

  tm.setSegments(SEG_GO, 4, 0);
  tone(SPEAKER_PIN, NOTE_G4); delay(COUNTDOWN_STEP_MS); noTone(SPEAKER_PIN);
  delay(1000);
  Serial.println("[DISPLAY] Countdown done — GO!");
}

/* ============================================================
   TONE / SEQUENCE HELPERS
   ============================================================ */

void lightLedAndPlayTone(byte ledIndex) {
  Serial.print("[TONE] LED "); Serial.print(ledIndex);
  Serial.print(", note "); Serial.print(GAME_TONES[ledIndex]); Serial.println(" Hz");
  digitalWrite(LED_PINS[ledIndex], HIGH);
  tone(SPEAKER_PIN, GAME_TONES[ledIndex]);
  delay(GAME_TONE_ON_MS);
  digitalWrite(LED_PINS[ledIndex], LOW);
  noTone(SPEAKER_PIN);
}

void playSequence() {
  Serial.print("[SEQ] Playing sequence of length "); Serial.println(gameIndex);
  for (int i = 0; i < gameIndex; i++) {
    lightLedAndPlayTone(gameSequence[i]);
    delay(GAME_TONE_GAP_MS);
  }
}

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

/**
   Short celebratory flash + pip when a new high score is set.
   All 4 LEDs flash twice and a quick ascending pip plays.
*/
void playNewHighScoreEffect() {
  Serial.println("[SFX] NEW HIGH SCORE!");
  for (byte f = 0; f < 3; f++) {
    for (byte i = 0; i < 4; i++) digitalWrite(LED_PINS[i], HIGH);
    tone(SPEAKER_PIN, NOTE_A5);
    delay(120);
    for (byte i = 0; i < 4; i++) digitalWrite(LED_PINS[i], LOW);
    noTone(SPEAKER_PIN);
    delay(80);
  }
  tone(SPEAKER_PIN, NOTE_C6); delay(300); noTone(SPEAKER_PIN);
}

/* ============================================================
   FLOW FUNCTIONS
   ============================================================ */

/**
   IDLE MODE
   Blocks until any button is pressed. No timeout, no death.
   • NeoPixel: rainbow fades in, then chases
   • LEDs: organic breathe
   • Display: rotating 3-frame high-score marquee

   Returns index (0–3) of the button that started the game.
*/
byte idleMode()
{
  Serial.println("[IDLE] Entering idle mode");
  initIdleLeds();

  uint16_t hue        = neoFadeInRainbow();
  uint32_t lastLedTick = millis();
  uint32_t lastHueTick = millis();

  // High-score marquee state — start on frame 0 immediately
  uint8_t  hsFrame      = 0;
  uint32_t hsFrameStart = millis() - HISCORE_FRAME_MS;  // force first draw now

  while (true) {
    uint32_t now = millis();

    // NeoPixel rainbow tick
    if (now - lastHueTick >= IDLE_CYCLE_DELAY_MS) {
      hue = neoRainbowTick(hue, 255);
      lastHueTick = now;
    }

    // LED breathe tick
    if (now - lastLedTick >= LED_IDLE_TICK_MS) {
      tickIdleLeds();
      lastLedTick = now;
    }

    // High-score display marquee (non-blocking)
    hsFrame = updateIdleDisplay(now, hsFrame, hsFrameStart);

    // Button check — no timeout here, player waits in idle indefinitely
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) {
        Serial.print("[IDLE] Button "); Serial.print(i);
        Serial.println(" pressed — starting game");

        // Confirmation beep
        tone(SPEAKER_PIN, NOTE_E5);
        delay(80);
        noTone(SPEAKER_PIN);

        return i;
      }
    }
  }
}

/**
   GAME START TRANSITION
   Idle visuals → game visuals + countdown.
*/
void startGameTransition() {
  Serial.println("[TRANSITION] Idle -> Game");
  neoFadeTo(GAME_NEO_R, GAME_NEO_G, GAME_NEO_B);
  ledsOff();
  displayCountdown();
  Serial.println("[TRANSITION] Done — game starting");
}

/**
   GAME OVER TRANSITION
   Handles score saving, death animation, sound, and reset.
   displayDead() is ONLY ever called from here — never from idle.
*/
void gameOverTransition() {
  // Score at game-over = rounds completed = gameIndex - 1
  // (gameIndex was already incremented before the failed round)
  int finalScore = (int)gameIndex - 1;
  if (finalScore < 0) finalScore = 0;

  Serial.print("[GAME OVER] Final score: "); Serial.println(finalScore);

  // ── Check and save high score ────────────────────────────
  if ((uint8_t)finalScore > highScore) {
    highScore = (uint8_t)finalScore;
    saveHighScore(highScore);
    // Play new-high-score effect BEFORE the death sequence
    playNewHighScoreEffect();
    Serial.print("[GAME OVER] New high score: "); Serial.println(highScore);
  }

  // Flash dEAd on display
  displayDead();

  delay(GAME_OVER_DELAY_MS);

  // Wah-wah
  playGameOverSound();

  Serial.println("[GAME OVER] Waiting before idle...");
  delay(POST_GAMEOVER_WAIT);

  // Fade NeoPixel to black — idleMode() fades rainbow back in
  neoFadeTo(0, 0, 0);

  ledsOff();

  // Reset game state
  gameIndex   = 0;
  gameRunning = false;

  Serial.println("[GAME OVER] Transition complete — returning to idle");
}

/**
   READ BUTTON WITH TIMEOUT
   Only called during active gameplay (gameRunning == true).
   Returns 0–3 for button index, 255 on timeout.
*/
byte readButtonWithTimeout() {
  uint32_t startWait = millis();

  while (true) {
    if (millis() - startWait >= TIMEOUT_MS) {
      Serial.println("[INPUT] Timeout! No button pressed for 1 minute");
      return 255;
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
   Only called during active gameplay.
   Returns true if fully correct, false on error or timeout.
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
  Serial.println("\n[BOOT] Memory Hunter v1.5 starting...");

  for (byte i = 0; i < 4; i++) {
    pinMode(LED_PINS[i],    OUTPUT);
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);

  tm.setBrightness(TM_BRIGHTNESS);
  Serial.println("[BOOT] TM1637 ready");

  strip.begin();
  strip.setBrightness(NEO_BRIGHTNESS);
  strip.clear();
  strip.show();
  Serial.println("[BOOT] NeoPixel ready");

  // Load high score from EEPROM before first idleMode() call
  highScore = loadHighScore();

  randomSeed(analogRead(A5));
  Serial.println("[BOOT] Setup complete\n");
}

void loop() {
  // ── IDLE: rainbow + breathe + high-score marquee ──────────
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
      return;  // exit loop() → re-enters → idleMode()
    }

    delay(300);
    playLevelUpSound();
    delay(300);
  }
}
