/**
   Simon Game for ESP32-C3 with TM1637 Score Display

   Copyright (C) 2023, Uri Shaked
   TM1637 adaptation by Hashtag

   Released under the MIT License.

   TM1637 digit layout (4 segments, indices 0–3):
     [ 0 ][ 1 ][ 2 ][ 3 ]
   
   Score display rules:
     - Idle (no game running): shows ----
     - Score 0–9  (1 digit):  shown at index 3 only  →  [  ][ ][ ][X]
                               wait — per spec: "starts from 3rd segment"
                               so single digit at index 2  →  [  ][ ][X][ ]
     - Score 10–99 (2 digits): uses indices 2+3       →  [  ][ ][X][X]
*/

#include <Arduino.h>
#include <TM1637Display.h>
#include "pitches.h"

/* Simon button/LED/speaker pins */
const uint8_t buttonPins[] = {0, 1, 2, 3};
const uint8_t ledPins[]    = {8, 7, 6, 5};
#define SPEAKER_PIN 10

/* TM1637 pins */
#define TM_CLK_PIN  9
#define TM_DIO_PIN  18

TM1637Display display(TM_CLK_PIN, TM_DIO_PIN);

/* Segment encoding for dash '-' */
const uint8_t SEG_DASH = 0b01000000;

/* All-dash pattern for idle screen */
const uint8_t SEG_ALL_DASH[] = {SEG_DASH, SEG_DASH, SEG_DASH, SEG_DASH};

#define MAX_GAME_LENGTH 100
const int gameTones[] = {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex = 0;

/* ── Display helpers ─────────────────────────────────────── */

/**
 * Show ---- across all 4 digits (idle state)
 */
void displayIdle() {
  display.setSegments(SEG_ALL_DASH, 4, 0);
}

/**
 * Show current score:
 *   - 1-digit score (0–9)  → digit at position 2, position 3 blank
 *   - 2-digit score (10–99)→ digits at positions 2 and 3
 * Positions 0 and 1 are always blank during gameplay.
 */
void displayScore() {
  // Clear positions 0 and 1
  uint8_t blank = 0x00;
  display.setSegments(&blank, 1, 0);
  display.setSegments(&blank, 1, 1);

  int score = gameIndex;  // gameIndex already incremented before display

  if (score < 10) {
    // Single digit at position 2, blank at position 3
    display.showNumberDecEx(score, 0, false, 1, 2);  // 1 digit at pos 2
    display.setSegments(&blank, 1, 3);               // pos 3 blank
  } else {
    // Two digits at positions 2–3
    display.showNumberDecEx(score % 100, 0, false, 2, 2);
  }
}

/* ── Game logic ──────────────────────────────────────────── */

void setup() {
  Serial.begin(9600);

  for (byte i = 0; i < 4; i++) {
    pinMode(ledPins[i],    OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);

  display.setBrightness(7);  // Max brightness (0–7)
  displayIdle();

  randomSeed(analogRead(4));
}

void lightLedAndPlayTone(byte ledIndex) {
  digitalWrite(ledPins[ledIndex], HIGH);
  tone(SPEAKER_PIN, gameTones[ledIndex]);
  delay(300);
  digitalWrite(ledPins[ledIndex], LOW);
  noTone(SPEAKER_PIN);
}

void playSequence() {
  for (int i = 0; i < gameIndex; i++) {
    lightLedAndPlayTone(gameSequence[i]);
    delay(50);
  }
}

byte readButtons() {
  while (true) {
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        return i;
      }
    }
    delay(1);
  }
}

bool checkUserSequence() {
  for (int i = 0; i < gameIndex; i++) {
    byte expectedButton = gameSequence[i];
    byte actualButton   = readButtons();
    lightLedAndPlayTone(actualButton);
    if (expectedButton != actualButton) {
      return false;
    }
  }
  return true;
}

void gameOver() {
  Serial.print("Game over! Your score: ");
  Serial.println(gameIndex - 1);

  // Flash ---- to signal game over
  for (byte i = 0; i < 3; i++) {
    displayIdle();
    delay(300);
    uint8_t blank = 0x00;
    display.setSegments(&blank, 4, 0);  // blank all
    delay(300);
  }
  displayIdle();  // Settle back to ----

  gameIndex = 0;
  delay(200);

  // Wah-wah-wah sound
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

  // Return to idle after game over
  displayIdle();
  delay(500);
}

void playLevelUpSound() {
  tone(SPEAKER_PIN, NOTE_E4); delay(150);
  tone(SPEAKER_PIN, NOTE_G4); delay(150);
  tone(SPEAKER_PIN, NOTE_E5); delay(150);
  tone(SPEAKER_PIN, NOTE_C5); delay(150);
  tone(SPEAKER_PIN, NOTE_D5); delay(150);
  tone(SPEAKER_PIN, NOTE_G5); delay(150);
  noTone(SPEAKER_PIN);
}

void loop() {
  displayScore();

  // Add next random step
  gameSequence[gameIndex] = random(0, 4);
  gameIndex++;
  if (gameIndex >= MAX_GAME_LENGTH) {
    gameIndex = MAX_GAME_LENGTH - 1;
  }

  playSequence();

  if (!checkUserSequence()) {
    gameOver();
    return;  // Skip level-up sound and go back to idle display
  }

  delay(300);

  if (gameIndex > 0) {
    playLevelUpSound();
    delay(300);
  }
}