```cpp
/*
________________________________________________________________________
  _   _    _    ____  _   _ _____   _     __ 
 | | | |  / \  / ___|| | | |_   _| / \   / ___|
 | |_| | / _ \ \___ \| |_| | | |  / _ \ | |  _ 
 |  _  |/ ___ \ ___) |  _  | | | / ___ \| |_| |
 |_| |_/_/   \_\____/|_| |_| |_|/_/   \_\\____|

  Memory_Hunter_v1.5
  EEPROM High Score + Idle Marquee + Gameplay-Scoped Timeout
  Arduino UNO + WS2812B + TM1637 + Passive Buzzer

  Name    : Aniket Chowdhury [Hashtag]
  Email   : micro.aniket@gmail.com
  GitHub  : https://github.com/itzzhashtag
  Wokwi   : https://wokwi.com/projects/461780873004005377
________________________________________________________________________
*/
```

<div align="center">

# 🧠 Memory Hunter — v1.5
### Deep Technical Documentation

**EEPROM High Score · Idle Marquee · Gameplay-Scoped Timeout · Full System Breakdown**

**by Aniket Chowdhury (aka `#Hashtag`)**

![Version](https://img.shields.io/badge/Version-v1.5-brightgreen?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-Arduino%20Uno-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Stable-success?style=for-the-badge&logo=arduino)
![EEPROM](https://img.shields.io/badge/Feature-EEPROM%20Persistence-orange?style=for-the-badge)
![NeoPixel](https://img.shields.io/badge/Strip-WS2812B%2046px-purple?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-TM1637-red?style=for-the-badge)

</div>

---

# 📋 TABLE OF CONTENTS

```
1.  Changelog  v1.4 → v1.5
2.  Full Feature Set
3.  System Architecture
4.  State Machine
5.  EEPROM System
6.  NeoPixel Engine
7.  LED Breathe Engine
8.  TM1637 Display System
9.  Audio System
10. Input & Timeout System
11. Game Flow — Detailed
12. Segment Encoding Reference
13. Config Reference
14. Pin Map & Hardware
15. Required Libraries
16. Serial Debug Log Reference
```

---
---

# 1. 📋 CHANGELOG  v1.4 → v1.5

---

## 🐛 FIX — Dead Display Firing in Idle State

### Problem in v1.4

`TIMEOUT_MS` was a global countdown running from the moment the device
powered on — including during idle/lobby.

If the player left the device alone on the idle screen for 60 seconds,
the machine would trigger the `dEAd` animation and game-over sound
even though no game had been started. This was a critical logic error
that broke the idle experience entirely.

### Root Cause

The `readButtonWithTimeout()` function was being called or its timer
was being tracked without verifying `gameRunning` state. The timeout
had no awareness of whether the player was actually mid-game or just
watching the idle rainbow.

### Fix in v1.5

`TIMEOUT_MS` now only fires inside `readButtonWithTimeout()`, which is
only ever called from `checkUserSequence()`, which is only ever called
from the main game loop — and only when `gameRunning == true`.

Idle mode (`idleMode()`) has its own infinite loop that does NOT call
`readButtonWithTimeout()` at all. It polls buttons directly with no
timer attached. Idle waits forever — zero penalty, zero timeout.

```cpp
// gameRunning gates the entire timeout system
bool gameRunning = false;  // false = idle, true = active game

// readButtonWithTimeout() — ONLY called during gameplay
byte readButtonWithTimeout() {
  uint32_t startWait = millis();
  while (true) {
    if (millis() - startWait >= TIMEOUT_MS) {
      Serial.println("[INPUT] Timeout! No button pressed for 1 minute");
      return 255;   // 255 = sentinel value meaning "timed out"
    }
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) return i;
    }
    delay(1);
  }
}

// idleMode() — polls buttons directly, NO timeout
byte idleMode() {
  while (true) {
    // ... rainbow, breathe, marquee ticks ...
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) return i;  // no timer
    }
  }
}
```

Result → Idle never dies. Timeout only fires when the player has
actually started a game and goes silent for 60 consecutive seconds.

---

## 🆕 NEW — EEPROM High Score Persistence

### What It Does

The all-time high score now survives power cycles, resets, and
re-uploads. Previously, every boot started fresh at 0. Now the machine
remembers the best score ever recorded and displays it in idle.

### How EEPROM Is Used

Arduino UNO has 1024 bytes of internal EEPROM (rated ~100,000
write cycles per address). This sketch uses only 2 addresses:

```
Address 0  →  Score byte  (uint8_t, range 0–255)
Address 1  →  Magic byte  (fixed value 0xAB = first-boot sentinel)
```

The magic byte solves a real embedded problem: on a brand-new Arduino
(or after erasing EEPROM), address 0 contains 0xFF — not 0. Without
a sentinel, the device would read 255 as the "saved high score" on
first boot. The magic byte tells the firmware whether address 0 has
ever been written by this sketch or not.

### Boot Sequence Logic

```
Power on
  → Read address 1 (magic byte)
  → Is it 0xAB?
      YES → Read address 0 → restore high score into RAM
      NO  → First boot detected
             → Write 0xAB to address 1
             → Write 0 to address 0
             → Set highScore = 0 in RAM
```

### Write-Cycle Protection

EEPROM cells degrade after ~100,000 writes. To protect them, the
`saveHighScore()` function reads the current stored value before
writing. If the new score matches what's already there, no write
happens. This means repeated game-overs at the same score never
burn an unnecessary write cycle.

```cpp
#define EEPROM_ADDR_SCORE  0
#define EEPROM_ADDR_MAGIC  1
#define EEPROM_MAGIC_VAL   0xAB

uint8_t loadHighScore() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_VAL) {
    // First boot — initialise
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VAL);
    EEPROM.write(EEPROM_ADDR_SCORE, 0);
    Serial.println("[EEPROM] First run — high score initialised to 0");
    return 0;
  }
  uint8_t hs = EEPROM.read(EEPROM_ADDR_SCORE);
  Serial.print("[EEPROM] High score loaded: "); Serial.println(hs);
  return hs;
}

void saveHighScore(uint8_t hs) {
  if (EEPROM.read(EEPROM_ADDR_SCORE) != hs) {
    EEPROM.write(EEPROM_ADDR_SCORE, hs);
    Serial.print("[EEPROM] New high score saved: "); Serial.println(hs);
  }
}

uint8_t highScore = 0;  // RAM copy — synced to EEPROM on new record
```

### When Is It Called

```
setup()
  └── highScore = loadHighScore()   ← on every boot

gameOverTransition()
  └── if (finalScore > highScore)
        highScore = finalScore
        saveHighScore(highScore)    ← only on new record
```

---

## 🆕 NEW — Idle High Score Marquee Display

### What It Does

While the device sits in idle, the TM1637 display cycles through
3 rotating frames showing the saved high score in a stylised way.
This transforms the idle screen from a blank or static display
into an attractive attract-mode loop.

### Frame Sequence

```
Frame 0  →  0--0   (decorative radar / bracket)
Frame 1  →  HiSr   (High Score label)
Frame 2  →  _XX_   (the actual score, centred)

Each frame holds for HISCORE_FRAME_MS (default 1500ms) before advancing.
```

### Score Centering Logic

The score display adapts based on the number of digits:

```
Score 0–9   →  [blank][blank][ X  ][blank]   (1 digit at position 2)
Score 10–99 →  [blank][ X  ][ X  ][blank]   (2 digits at positions 1–2)
```

This keeps the score visually centred across both ranges.

### Non-Blocking Implementation

The marquee does NOT use `delay()`. It tracks elapsed time using
`millis()` timestamps. Every iteration of the idle loop calls
`updateIdleDisplay(now, frameIndex, frameStart)` which checks
whether enough time has passed to advance the frame. If not,
it returns immediately. This means the rainbow and LED breathe
animations run uninterrupted alongside the marquee.

```cpp
uint8_t updateIdleDisplay(uint32_t now,
                           uint8_t  frameIndex,
                           uint32_t &frameStart)
{
  if (now - frameStart >= HISCORE_FRAME_MS) {
    frameIndex = (frameIndex + 1) % 3;
    frameStart = now;

    switch (frameIndex) {
      case 0:
        tm.setSegments(SEG_0DD0, 4, 0);       // 0--0
        break;
      case 1:
        tm.setSegments(SEG_HISR, 4, 0);       // HiSr
        break;
      case 2:
        uint8_t blank = 0x00;
        tm.setSegments(&blank, 1, 0);
        tm.setSegments(&blank, 1, 3);
        if (highScore < 10)
          tm.showNumberDecEx(highScore, 0, false, 1, 2);
        else
          tm.showNumberDecEx(highScore % 100, 0, false, 2, 1);
        break;
    }
  }
  return frameIndex;
}
```

The first frame fires immediately on entering idle because
`frameStart` is initialised as:

```cpp
uint32_t hsFrameStart = millis() - HISCORE_FRAME_MS;
// Forces the first update condition to be true on first tick
```

---

## 🆕 NEW — New High Score Celebration Effect

### What It Does

When a player beats the existing high score, a short celebration
sequence plays before the `dEAd` animation begins. This rewards
the player for breaking their record even in the moment of loss.

### Sequence

```
3× flash cycle:
  → All 4 LEDs ON  +  NOTE_A5 tone  (120ms)
  → All 4 LEDs OFF +  silence        (80ms)

Then:
  → NOTE_C6 victory pip              (300ms)

Then:
  → Normal dEAd sequence begins
```

### Why Before dEAd

The high score save and celebration fire BEFORE the death sequence
intentionally. The player broke a record — that deserves recognition
even though the round was lost. Firing it after `dEAd` would feel
anticlimactic and confusing.

```cpp
void playNewHighScoreEffect() {
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
```

---
---

# 2. ✅ FULL FEATURE SET — v1.5

```
EEPROM
  ✔ High score persists across power cycles
  ✔ Magic byte 0xAB detects first boot
  ✔ Write-cycle-safe (only writes on value change)
  ✔ Loaded into RAM on boot, written only on new record

DISPLAY — TM1637
  ✔ Idle 3-frame marquee  →  0--0 / HiSr / score
  ✔ Non-blocking marquee (millis-based, no delay)
  ✔ Score centering by digit count (1-digit vs 2-digit)
  ✔ Countdown  →  33 / 22 / 11 / GO
  ✔ Live score during gameplay  →  _X_ / _XX_
  ✔ dEAd flash on game over (3x blink then hold)

NEOPIXEL — WS2812B
  ✔ Idle rainbow  →  HSV colour wheel with per-pixel hue offset
  ✔ Rainbow fades in from black on idle entry (60 steps)
  ✔ Smooth fade from rainbow to solid light pink on game start
  ✔ Smooth fade from pink to black on game over
  ✔ Gamma-corrected colour rendering via strip.gamma32()
  ✔ Configurable brightness scaling during fade-in

LEDS — 4x PWM
  ✔ Organic independent breathe in idle
  ✔ Random initial brightness + random direction per LED
  ✔ Random direction flip (1-in-80 chance per tick) for natural feel
  ✔ Full-off during gameplay transitions

AUDIO — Passive Buzzer
  ✔ Unique tone per button (G3, C4, E4, G5)
  ✔ Countdown beeps (C4 per step)
  ✔ Start-press confirmation (E5)
  ✔ Level-up jingle (E4→G4→E5→C5→D5→G5)
  ✔ Game-over wah-wah (DS5→D5→CS5 + pitch sweep)
  ✔ High score celebration (A5 flash × 3 + C6 pip)

INPUT SYSTEM
  ✔ Gameplay-scoped 60s inactivity timeout
  ✔ Timeout returns sentinel 255 — never fires in idle
  ✔ Active-LOW buttons with INPUT_PULLUP
  ✔ First button press in idle triggers game start

GAME ENGINE
  ✔ Up to 100-step sequence (MAX_GAME_LENGTH)
  ✔ Random seed from floating analog pin A5
  ✔ Score = rounds completed (gameIndex - 1 at death)
  ✔ gameRunning flag gates timeout and death logic
  ✔ Clean state reset after every game over
```

---
---

# 3. 🏗️ SYSTEM ARCHITECTURE

```
┌─────────────────────────────────────────────────────────┐
│                        setup()                          │
│   pinMode × 8  →  TM1637 init  →  NeoPixel init        │
│   loadHighScore() from EEPROM  →  randomSeed(A5)        │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                      loop()                             │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │                  idleMode()                      │   │
│  │  neoFadeInRainbow()  →  while(true):             │   │
│  │    neoRainbowTick()     (millis-gated, ~20ms)    │   │
│  │    tickIdleLeds()       (millis-gated, ~8ms)     │   │
│  │    updateIdleDisplay()  (millis-gated, 1500ms)   │   │
│  │    button poll → return pressed index            │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                               │
│                         ▼                               │
│  ┌──────────────────────────────────────────────────┐   │
│  │             startGameTransition()                │   │
│  │   neoFadeTo(pink)  →  ledsOff()                  │   │
│  │   displayCountdown()  33→22→11→GO                │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                               │
│              gameRunning = true                         │
│              gameIndex   = 0                            │
│                         │                               │
│  ┌──────────────────────────────────────────────────┐   │
│  │              while(gameRunning)                  │   │
│  │                                                  │   │
│  │   displayScore()                                 │   │
│  │   gameSequence[gameIndex] = random(0,4)          │   │
│  │   gameIndex++                                    │   │
│  │   playSequence()    ← light + tone each step     │   │
│  │   checkUserSequence()                            │   │
│  │     └── readButtonWithTimeout()  ← TIMEOUT HERE  │   │
│  │           returns 255 on timeout                 │   │
│  │                                                  │   │
│  │   if WRONG or TIMEOUT → gameOverTransition()     │   │
│  │   if CORRECT          → playLevelUpSound()       │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                               │
│  ┌──────────────────────────────────────────────────┐   │
│  │             gameOverTransition()                 │   │
│  │   compute finalScore  (gameIndex - 1)            │   │
│  │   if new record:                                 │   │
│  │     saveHighScore() → playNewHighScoreEffect()   │   │
│  │   displayDead()  →  playGameOverSound()          │   │
│  │   neoFadeTo(0,0,0)  →  ledsOff()                 │   │
│  │   gameIndex=0  gameRunning=false                 │   │
│  │   return  →  loop() re-enters  →  idleMode()     │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---
---

# 4. 🔄 STATE MACHINE

Memory Hunter runs a linear implicit state machine driven by the
`loop()` function and the `gameRunning` boolean flag.

```
┌───────────┐    button press    ┌─────────────────┐
│   IDLE    │ ──────────────────▶│   TRANSITION    │
│           │                   │  neo→pink        │
│ rainbow   │                   │  countdown       │
│ breathe   │                   │  33→22→11→GO     │
│ marquee   │                   └────────┬────────┘
└───────────┘                            │
      ▲                        gameRunning = true
      │                                  │
      │                                  ▼
      │                        ┌─────────────────┐
      │                        │   SEQUENCE OUT  │
      │                        │  playSequence() │
      │                        │  light + tone   │
      │                        └────────┬────────┘
      │                                 │
      │                                 ▼
      │                        ┌─────────────────┐
      │                        │   PLAYER INPUT  │
      │                        │  readButton()   │
      │                        │  timeout = 60s  │
      │                        └────────┬────────┘
      │                                 │
      │                    ┌────────────┴────────────┐
      │                 CORRECT                   WRONG or
      │                    │                     TIMEOUT
      │                    ▼                         │
      │           ┌─────────────────┐               │
      │           │   LEVEL UP      │               ▼
      │           │  jingle + delay │    ┌─────────────────┐
      │           │  loop repeats   │    │   GAME OVER     │
      │           └─────────────────┘    │  check record   │
      │                                  │  celebration?   │
      │                                  │  dEAd flash     │
      │                                  │  wah-wah sound  │
      │                                  │  neo→black      │
      └──────────────────────────────────│  reset state    │
                                         └─────────────────┘
```

---
---

# 5. 💾 EEPROM SYSTEM — DETAILED

### Memory Map

```
EEPROM Address Layout (Arduino UNO — 1024 bytes total)
┌──────────┬──────────────────────────────────────────┐
│ Addr  0  │  High Score  (uint8_t, 0–255)            │
│ Addr  1  │  Magic Byte  (0xAB = initialised flag)   │
│ Addr  2+ │  UNUSED by this sketch                   │
└──────────┴──────────────────────────────────────────┘
```

### First Boot Detection

```
Read addr 1
    │
    ├── == 0xAB  →  Normal boot
    │               Read addr 0 → highScore in RAM
    │
    └── != 0xAB  →  First boot (or EEPROM erased)
                    Write 0xAB to addr 1
                    Write 0x00 to addr 0
                    highScore = 0 in RAM
```

### Write Frequency

```
EEPROM writes happen ONLY in these conditions:
  1. First boot  →  2 writes (magic + score)
  2. New record  →  1 write  (score only, if changed)

Normal gameplay never writes EEPROM.
Repeated same-score game-overs never write EEPROM.
```

### Serial Debug Output

```
[EEPROM] First run — high score initialised to 0
[EEPROM] High score loaded: 14
[EEPROM] New high score saved: 21
```

---
---

# 6. 🌈 NEOPIXEL ENGINE — DETAILED

### Strip Configuration

```cpp
#define NEO_PIN        13
#define NEO_COUNT      46
#define NEO_BRIGHTNESS 255   // hardware brightness ceiling
```

### Idle Rainbow

Each pixel is assigned a hue offset based on its position in the strip,
creating a smooth travelling rainbow. The global hue advances by
`IDLE_HUE_STEP` (300) per tick, making the pattern rotate.

```cpp
uint16_t neoRainbowTick(uint16_t hue, uint8_t brightness = 255) {
  for (int i = 0; i < strip.numPixels(); i++) {
    uint16_t pixelHue = hue + ((uint32_t)i * 65536 / strip.numPixels());
    uint32_t color    = strip.gamma32(strip.ColorHSV(pixelHue));
    // Scale each channel by brightness factor
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >>  8) & 0xFF) * brightness / 255;
    uint8_t b = ( color        & 0xFF) * brightness / 255;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  hue += IDLE_HUE_STEP;
  return hue;
}
```

`strip.gamma32()` applies Adafruit's gamma correction table,
compensating for the non-linear human perception of LED brightness.
This makes colour transitions look perceptually smooth rather than
mathematically linear.

### Rainbow Fade-In

On idle entry, the rainbow doesn't snap on — it fades in from
black over `IDLE_FADE_IN_STEPS` (60) steps. The brightness
parameter scales from 0→255 across those steps.

```cpp
uint16_t neoFadeInRainbow() {
  uint16_t hue = 0;
  for (int step = 0; step <= IDLE_FADE_IN_STEPS; step++) {
    uint8_t brightness = (uint8_t)((255L * step) / IDLE_FADE_IN_STEPS);
    hue = neoRainbowTick(hue, brightness);
    delay(IDLE_CYCLE_DELAY_MS);   // 20ms per step = ~1.2s total fade
  }
  return hue;
}
```

### Fade Between Colours

`neoFadeTo(r, g, b)` reads the current colour of pixel 0 as the
start point and linearly interpolates to the target across
`NEO_FADE_STEPS` (40) steps with `NEO_FADE_DELAY_MS` (15ms) gaps.

```
Transitions used:
  Idle  → Game  :  rainbow last frame  →  RGB(255, 105, 180)  pink
  Game  → Idle  :  pink                →  RGB(0, 0, 0)        black
  Black → Idle  :  neoFadeInRainbow()  (separate function)
```

### Game Colour — Light Pink

```cpp
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180
// Hot pink / light pink — sets a vibrant but calm game atmosphere
```

---
---

# 7. 💡 LED BREATHE ENGINE — DETAILED

### Per-LED State

Each of the 4 game LEDs maintains its own independent brightness
value and fade direction:

```cpp
int16_t ledBright[4];   // current PWM brightness (0–180)
int8_t  ledDir[4];      // fade direction: +1 = brighter, -1 = dimmer
```

### Initialisation

On idle entry, each LED starts at a random brightness and random
direction — so all 4 LEDs are immediately out of phase and
breathing independently from the first tick.

```cpp
void initIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] = random(LED_IDLE_MIN_BRIGHT, LED_IDLE_MAX_BRIGHT);
    ledDir[i]    = random(0, 2) ? 1 : -1;
    analogWrite(LED_PINS[i], ledBright[i]);
  }
}
```

### Tick Logic

Every `LED_IDLE_TICK_MS` (8ms), each LED steps by `LED_IDLE_FADE_SPEED`
(3 PWM units) in its current direction. On hitting min or max, the
direction reverses. A 1-in-80 random chance flips direction at any
time — this produces the organic, irregular breathing feel instead
of a mechanical on/off pulse.

```cpp
void tickIdleLeds() {
  for (byte i = 0; i < 4; i++) {
    ledBright[i] += ledDir[i] * LED_IDLE_FADE_SPEED;
    if (ledBright[i] >= LED_IDLE_MAX_BRIGHT) {
      ledBright[i] = LED_IDLE_MAX_BRIGHT; ledDir[i] = -1;
    } else if (ledBright[i] <= LED_IDLE_MIN_BRIGHT) {
      ledBright[i] = LED_IDLE_MIN_BRIGHT; ledDir[i] = 1;
    }
    if (random(80) == 0) ledDir[i] = -ledDir[i];  // organic irregularity
    analogWrite(LED_PINS[i], ledBright[i]);
  }
}
```

### Brightness Range

```cpp
#define LED_IDLE_MIN_BRIGHT   0    // fully off at low point
#define LED_IDLE_MAX_BRIGHT 180    // not full 255 — keeps it soft
```

Max is capped at 180 (not 255) so the idle breathe stays gentle and
ambient rather than harsh and distracting during idle.

---
---

# 8. 📺 TM1637 DISPLAY SYSTEM — DETAILED

### Display Modes

```
MODE            SEGMENTS              WHEN
─────────────────────────────────────────────────────
Idle marquee    0--0 / HiSr / _XX_   idleMode()
Countdown       _33_ _22_ _11_ _GO_  startGameTransition()
Score 0–9       _ _ X _              gameplay, score < 10
Score 10–99     _ X X _              gameplay, score >= 10
dEAd            d E A d              gameOverTransition()
```

### Idle Marquee Frames

```
Frame 0  →  SEG_0DD0  =  [0][-][-][0]
Frame 1  →  SEG_HISR  =  [H][i][S][r]
Frame 2  →  score     =  [_][X][X][_]  or  [_][_][X][_]
```

Frame timing is non-blocking — managed by `updateIdleDisplay()`
using `millis()` comparison, not `delay()`.

### Score Display Positioning

```cpp
void displayScore() {
  uint8_t blank = 0x00;
  tm.setSegments(&blank, 1, 0);   // pos 0: always blank
  tm.setSegments(&blank, 1, 1);   // pos 1: blank for 1-digit

  int score = (int)gameIndex;
  if (score < 10) {
    tm.showNumberDecEx(score, 0, false, 1, 2);   // 1 digit at pos 2
    tm.setSegments(&blank, 1, 3);                 // pos 3: blank
  } else {
    tm.showNumberDecEx(score % 100, 0, false, 2, 1); // 2 digits at pos 1
    tm.setSegments(&blank, 1, 3);                     // pos 3: blank
  }
}
```

### Countdown Sequence

```cpp
void displayCountdown() {
  // _33_  with C4 beep
  // _22_  with C4 beep
  // _11_  with C4 beep
  // _GO_  with G4 beep  →  1s hold
}
```

Each step uses a manually constructed segment array to place digits
at positions 1–2 with blanks at 0 and 3.

### dEAd Flash

```cpp
void displayDead() {
  for (byte f = 0; f < 3; f++) {
    tm.setSegments(SEG_DEAD, 4, 0);   // show dEAd
    delay(250);
    tm.clear();                        // blank
    delay(150);
  }
  tm.setSegments(SEG_DEAD, 4, 0);    // leave on
}
```

---
---

# 9. 🔊 AUDIO SYSTEM — DETAILED

### Button Tones

Each of the 4 buttons maps to a musical note, creating a
recognisable audio signature for each colour:

```
Button 0  →  NOTE_G3  (196 Hz)  — deep / bass
Button 1  →  NOTE_C4  (262 Hz)  — mid-low
Button 2  →  NOTE_E4  (330 Hz)  — mid
Button 3  →  NOTE_G5  (784 Hz)  — high
```

These four notes form a G major chord spread across octaves,
so the sequence sounds musical even when played randomly.

### Level-Up Jingle

```
NOTE_E4 (150ms) → NOTE_G4 (150ms) → NOTE_E5 (150ms)
→ NOTE_C5 (150ms) → NOTE_D5 (150ms) → NOTE_G5 (150ms)
```

A short ascending fanfare — 6 notes, 900ms total.

### Game Over Sound

```
NOTE_DS5  (300ms)
NOTE_D5   (300ms)
NOTE_CS5  (300ms)
Then: 10 × pitch sweep  NOTE_C5-10 → NOTE_C5+10
  (each pitch step 6ms → ~120ms of wah-wah)
```

Total game-over audio duration: ~1.2 seconds.

### Start Confirmation

When any button is pressed in idle to start the game, a short
`NOTE_E5` confirmation beep (80ms) plays before the transition
begins. This gives the player immediate tactile audio feedback.

### High Score Celebration

```
3× cycle:
  ALL LEDs ON + NOTE_A5  (120ms)
  ALL LEDs OFF + silence (80ms)
Then:
  NOTE_C6 pip            (300ms)
```

Total duration: ~900ms before dEAd sequence begins.

---
---

# 10. ⌨️ INPUT & TIMEOUT SYSTEM — DETAILED

### Button Configuration

```cpp
const uint8_t BUTTON_PINS[4] = {4, 5, 7, 8};
// All INPUT_PULLUP — active LOW (pressed = LOW, idle = HIGH)
// No external pull-up resistors required
```

### Idle Input (No Timeout)

```cpp
// Inside idleMode() — direct poll, no timer
for (byte i = 0; i < 4; i++) {
  if (digitalRead(BUTTON_PINS[i]) == LOW) {
    // confirmation beep
    tone(SPEAKER_PIN, NOTE_E5); delay(80); noTone(SPEAKER_PIN);
    return i;
  }
}
```

No debounce is required here because the game immediately
transitions away from idle — any bounce presses are irrelevant.

### Gameplay Input (With Timeout)

```cpp
byte readButtonWithTimeout() {
  uint32_t startWait = millis();
  while (true) {
    if (millis() - startWait >= TIMEOUT_MS) return 255;
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) {
        lightLedAndPlayTone(i);   // immediate feedback
        return i;
      }
    }
    delay(1);   // 1ms yield — reduces tight-loop CPU burn
  }
}
```

Return value 255 is a sentinel — not a valid button index.
`checkUserSequence()` tests for `== 255` to detect timeout and
immediately returns `false`, triggering `gameOverTransition()`.

### Timeout Value

```cpp
#define TIMEOUT_MS  60000UL   // 60 seconds = 1 minute
```

The `UL` suffix ensures unsigned long arithmetic — prevents
overflow bugs on 16-bit int Arduino targets.

---
---

# 11. 🎮 GAME FLOW — STEP BY STEP

### Boot

```
1. Serial.begin(9600)
2. Set 4× LED pins OUTPUT, 4× button pins INPUT_PULLUP
3. Set speaker pin OUTPUT
4. TM1637 brightness → 7 (max)
5. NeoPixel begin, clear, show
6. loadHighScore() from EEPROM → highScore in RAM
7. randomSeed(analogRead(A5))  ← floating pin = random entropy
8. → loop()
```

### Idle Phase

```
1. initIdleLeds()             — randomise all 4 LED breathe states
2. neoFadeInRainbow()         — 60-step brightness fade-in, ~1.2s
3. hsFrameStart = now - 1500  — force first marquee frame immediately
4. while(true):
     a. neoRainbowTick()      if 20ms elapsed
     b. tickIdleLeds()        if 8ms elapsed
     c. updateIdleDisplay()   if 1500ms elapsed
     d. poll 4 buttons        — any LOW → return index
```

### Start Transition

```
1. E5 confirmation beep (80ms)
2. neoFadeTo(255, 105, 180)   — ~600ms rainbow → pink
3. ledsOff()                  — all PWM to 0
4. displayCountdown()         — _33_ / _22_ / _11_ / _GO_
                                each with C4 beep (600ms)
                                GO with G4 beep + 1s hold
5. gameRunning = true
6. gameIndex   = 0
```

### Round Loop

```
Each round:
1. displayScore()             — show current gameIndex on TM1637
2. gameSequence[gameIndex] = random(0, 4)
3. gameIndex++
4. playSequence():
     for each step in sequence:
       digitalWrite LED HIGH
       tone(note, 300ms)
       digitalWrite LED LOW
       gap(50ms)
5. checkUserSequence():
     for i in 0..gameIndex-1:
       readButtonWithTimeout()  ← returns 0-3 or 255
       if 255 → return false (timeout)
       lightLedAndPlayTone(button)
       if button != expected → return false (wrong)
       log correct step
6. if false → gameOverTransition()  → return from loop()
7. if true  → delay(300)
              playLevelUpSound()
              delay(300)
              continue
```

### Game Over Transition

```
1. finalScore = gameIndex - 1   (rounds completed, not attempted)
2. if finalScore > highScore:
     highScore = finalScore
     saveHighScore(highScore)     ← EEPROM write
     playNewHighScoreEffect()     ← flash + pip
3. displayDead()                 ← 3× blink then hold
4. delay(GAME_OVER_DELAY_MS)     ← 200ms
5. playGameOverSound()           ← DS5→D5→CS5 + sweep
6. delay(POST_GAMEOVER_WAIT)     ← 1000ms
7. neoFadeTo(0, 0, 0)            ← pink → black
8. ledsOff()
9. gameIndex   = 0
10. gameRunning = false
11. return                        ← re-enters loop() → idleMode()
```

---
---

# 12. 📐 SEGMENT ENCODING REFERENCE

```
TM1637 7-segment bit layout:
  bit 6 = g (middle bar)
  bit 5 = f (top-left)
  bit 4 = e (bottom-left)
  bit 3 = d (bottom bar)
  bit 2 = c (bottom-right)
  bit 1 = b (top-right)
  bit 0 = a (top bar)

       aaa
      f   b
      f   b
       ggg
      e   c
      e   c
       ddd
```

### Custom Segment Values Used

```
Symbol   Binary      Hex    Segments lit
──────────────────────────────────────────
-        0b01000000  0x40   g
d        0b01011110  0x5E   g,e,d,c,b
E        0b01111001  0x79   g,f,e,d,a
A        0b01110111  0x77   g,f,e,c,b,a
H        0b01110110  0x76   g,f,e,c,b
i        0b00010000  0x10   e,d  (lower-case)
S        0b01101101  0x6D   g,f,d,c,a
r        0b01010000  0x50   g,e  (lower-case)
G        0b00111101  0x3D   f,e,d,c,a
O        0b00111111  0x3F   f,e,d,c,b,a
0        0b00111111  0x3F   f,e,d,c,b,a  (same as O)
```

### Composite Segment Arrays

```cpp
SEG_DEAD  = { d,    E,    A,    d    }  →  dEAd
SEG_GO    = { 0x00, G,    O,    0x00 }  →  _GO_
SEG_HISR  = { H,    i,    S,    r    }  →  HiSr
SEG_0DD0  = { 0,    -,    -,    0    }  →  0--0
```

---
---

# 13. 🔧 CONFIG REFERENCE — ALL DEFINES

```cpp
// ── Simon Hardware ───────────────────────────────────────
const uint8_t BUTTON_PINS[4] = {4, 5, 7, 8};
const uint8_t LED_PINS[4]    = {6, 9, 10, 11};
#define SPEAKER_PIN   12

// ── TM1637 ───────────────────────────────────────────────
#define TM_CLK_PIN    2
#define TM_DIO_PIN    3
#define TM_BRIGHTNESS 7          // 0 (dim) – 7 (max)

// ── NeoPixel ─────────────────────────────────────────────
#define NEO_PIN        13
#define NEO_COUNT      46        // pixels in your strip
#define NEO_BRIGHTNESS 255       // hardware cap

// ── Idle Rainbow ─────────────────────────────────────────
#define IDLE_HUE_STEP        300  // hue advance per tick (faster = quicker spin)
#define IDLE_CYCLE_DELAY_MS   20  // ms between rainbow ticks
#define IDLE_FADE_IN_STEPS    60  // steps for rainbow fade-in (~1.2s)

// ── NeoPixel Game Colour (light pink) ────────────────────
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180

// ── NeoPixel Fade ────────────────────────────────────────
#define NEO_FADE_STEPS    40     // interpolation steps
#define NEO_FADE_DELAY_MS 15     // ms per step (~600ms total)

// ── LED Breathe ──────────────────────────────────────────
#define LED_IDLE_MIN_BRIGHT   0   // PWM floor
#define LED_IDLE_MAX_BRIGHT 180   // PWM ceiling (soft cap)
#define LED_IDLE_FADE_SPEED   3   // PWM units per tick
#define LED_IDLE_TICK_MS      8   // ms between breathe ticks

// ── Gameplay Timing ──────────────────────────────────────
#define GAME_TONE_ON_MS    300   // LED+tone hold per sequence step
#define GAME_TONE_GAP_MS    50   // silence gap between steps
#define LEVEL_UP_NOTE_MS   150   // ms per note in level-up jingle
#define GAME_OVER_DELAY_MS 200   // pause before game-over sound
#define POST_GAMEOVER_WAIT 1000  // pause after sound before idle
#define TIMEOUT_MS       60000UL // inactivity timeout (gameplay only)

// ── Countdown ────────────────────────────────────────────
#define COUNTDOWN_STEP_MS 600    // ms per countdown digit

// ── Game Constants ───────────────────────────────────────
#define MAX_GAME_LENGTH 100
const int GAME_TONES[4] = {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

// ── EEPROM ───────────────────────────────────────────────
#define EEPROM_ADDR_SCORE  0     // score storage address
#define EEPROM_ADDR_MAGIC  1     // first-boot sentinel address
#define EEPROM_MAGIC_VAL   0xAB  // arbitrary magic value

// ── Idle Marquee ─────────────────────────────────────────
#define HISCORE_FRAME_MS  1500UL // ms per marquee frame
```

---
---

# 14. 🔌 PIN MAP & HARDWARE

### Full Pin Map

```
Arduino UNO
─────────────────────────────────────────────────────
Pin  2   →  TM1637 CLK
Pin  3   →  TM1637 DIO
Pin  4   →  Button 1  (INPUT_PULLUP, active LOW)
Pin  5   →  Button 2  (INPUT_PULLUP, active LOW)
Pin  6   →  LED 1     (PWM, analogWrite)
Pin  7   →  Button 3  (INPUT_PULLUP, active LOW)
Pin  8   →  Button 4  (INPUT_PULLUP, active LOW)
Pin  9   →  LED 2     (PWM, analogWrite)
Pin 10   →  LED 3     (PWM, analogWrite)
Pin 11   →  LED 4     (PWM, analogWrite)
Pin 12   →  Buzzer    (tone() / noTone())
Pin 13   →  NeoPixel data
Pin A5   →  Floating  (randomSeed entropy)
─────────────────────────────────────────────────────
```

### PWM Notes

```
Arduino UNO PWM pins: 3, 5, 6, 9, 10, 11
Pin 12 is NOT PWM — used for tone() only (digital)
Pin 13 is NOT PWM — used for NeoPixel data

LED pins 6, 9, 10, 11 are all hardware PWM capable.
LED breathe uses analogWrite() on these pins.
```

### Components

```
Arduino UNO R3
WS2812B LED Strip  — 46 pixels, 5V, GND, Data to D13
TM1637 Display     — CLK to D2, DIO to D3, VCC 5V, GND
4× LEDs            — anode to pin via 220Ω resistor, cathode to GND
4× Push Buttons    — one leg to pin, other to GND (pullup handles rest)
Passive Buzzer     — + to D12 via 100Ω, - to GND
```

### Power

```
WS2812B at full white draws ~60mA per pixel.
46 pixels × 60mA = ~2.76A theoretical max.
Use an external 5V power supply for the LED strip.
Arduino 5V pin cannot source more than 200–500mA safely.

In practice, the game colour (pink, not white) draws far less —
but external supply is always recommended for strips > 10 pixels.
```

---
---

# 15. 📦 REQUIRED LIBRARIES

```
Library              Source                   Version Tested
──────────────────────────────────────────────────────────────
TM1637Display        Arduino Library Manager  1.x (Avishorp)
Adafruit NeoPixel    Arduino Library Manager  1.10+
EEPROM               Built-in AVR core        N/A
pitches.h            Included in sketch dir   N/A
```

`pitches.h` defines frequency constants like `NOTE_G3`, `NOTE_C4`
etc. Include it in the same directory as the sketch `.ino` file.
It is not a library — it is a local header.

---
---

# 16. 🖥️ SERIAL DEBUG LOG REFERENCE

Memory Hunter prints detailed debug output at 9600 baud.
All messages are prefixed with a tag for easy filtering.

```
Tag             Meaning
──────────────────────────────────────────────────────────
[BOOT]          Startup messages
[EEPROM]        High score load / save events
[NEO]           NeoPixel fade transitions
[LED]           LED state changes
[DISPLAY]       TM1637 frame updates
[IDLE]          Idle mode entry / button press
[TRANSITION]    Game start / game over transitions
[SEQ]           Sequence playback
[TONE]          Individual LED+tone events
[ROUND]         Round begin marker
[INPUT]         Player button presses + timeout
[GAME OVER]     Score, record check, transition steps
[SFX]           Sound effect triggers
```

### Example Boot + Game Session

```
[BOOT] Memory Hunter v1.5 starting...
[BOOT] TM1637 ready
[BOOT] NeoPixel ready
[EEPROM] High score loaded: 7
[BOOT] Setup complete

[NEO] Fading (0,0,0) -> (0,0,0)
[LED] Idle fade initialised
[NEO] Rainbow fade-in complete
[DISPLAY] Idle frame: 0--0
[DISPLAY] Idle frame: HiSr
[DISPLAY] Idle frame: score 7
[IDLE] Button 2 pressed — starting game

[TRANSITION] Idle -> Game
[NEO] Fading (...) -> (255,105,180)
[LED] All LEDs off
[DISPLAY] Countdown: 33->22->11->GO
[DISPLAY] Countdown done — GO!
[TRANSITION] Done — game starting

[ROUND] Round 1 begin
[SEQ] Playing sequence of length 1
[TONE] LED 3, note 784 Hz
[INPUT] Waiting for user to repeat sequence
[INPUT] Button 3 pressed (0.6s elapsed)
[INPUT] Step 1/1 correct
[INPUT] Sequence correct!
[SFX] Level up!

[ROUND] Round 2 begin
...

[INPUT] Timeout! No button pressed for 1 minute
[GAME OVER] Final score: 4
[SFX] NEW HIGH SCORE!
[EEPROM] New high score saved: 4   ← wait — 4 > 3? only if last was 3
[DISPLAY] dEAd
[SFX] Game over sound
[GAME OVER] Waiting before idle...
[NEO] Fading (255,105,180) -> (0,0,0)
[LED] All LEDs off
[GAME OVER] Transition complete — returning to idle
```

---
---

```cpp
// ─────────────────────────────────────────────────────────
// Author  : Aniket Chowdhury [Hashtag]
// Email   : micro.aniket@gmail.com
// GitHub  : https://github.com/itzzhashtag
// LinkedIn: https://www.linkedin.com/in/itzz-hashtag/
// Insta   : https://instagram.com/itzz_hashtag
// Wokwi   : https://wokwi.com/projects/461780873004005377
// ─────────────────────────────────────────────────────────

// v1.5 — The machine now remembers.
// High score saved. Power off. Come back.
// It still knows your best.
// The question is — can you beat it?
```
