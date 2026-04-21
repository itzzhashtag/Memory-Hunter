```cpp
/*
________________________________________________________________________
  __  __ _____ __  __  ___  ____  __   __   _   _ _   _ _   _ _____ _____ ____  
 |  \/  | ____|  \/  |/ _ \|  _ \ \ \ / /  | | | | | | | \ | |_   _| ____|  _ \ 
 | |\/| |  _| | |\/| | | | | |_) | \ V /   | |_| | | | |  \| | | | |  _| | |_) |
 | |  | | |___| |  | | |_| |  _ <   | |    |  _  | |_| | |\  | | | | |___|  _ < 
 |_|  |_|_____|_|  |_|\___/|_| \_\  |_|    |_| |_|\___/|_| \_| |_| |_____|_| \_\

  Memory_Hunter_v1.5  —  EEPROM High Score + Idle Marquee + Timeout Fix
________________________________________________________________________
*/
```

<div align="center">

# 🧠 Memory Hunter — Version v1.5

**EEPROM High Score · Idle Marquee · Gameplay-Scoped Timeout**

**by Aniket Chowdhury (aka `#Hashtag`)**

![Version](https://img.shields.io/badge/Version-v1.5-brightgreen?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-Arduino%20Uno-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Stable-success?style=for-the-badge&logo=arduino)
![EEPROM](https://img.shields.io/badge/Feature-EEPROM%20High%20Score-orange?style=for-the-badge)

</div>

---

# 📋 CHANGELOG  v1.4 → v1.5

---

## 🐛 FIX — Timeout Scoped to Gameplay Only

**Problem in v1.4:**  
`TIMEOUT_MS` was counting even during idle/lobby state.  
Leaving the device alone would trigger a `dEAd` animation in idle — which made no sense.

**Fix in v1.5:**  
`TIMEOUT_MS` now only starts counting when `gameRunning == true`.  
Idle mode waits forever with zero penalty — as it should.

```cpp
// TIMEOUT only fires inside gameplay — never in idle
bool gameRunning = false;   // set true only after game begins

byte readButtonWithTimeout() {
  uint32_t startWait = millis();
  while (true) {
    if (millis() - startWait >= TIMEOUT_MS) return 255; // fire only here
    for (byte i = 0; i < 4; i++) {
      if (digitalRead(BUTTON_PINS[i]) == LOW) return i;
    }
    delay(1);
  }
}
```

---

## 🆕 NEW — EEPROM High Score Persistence

**What it does:**  
The high score now **survives power cycles**.  
No more resetting to 0 every time the Arduino reboots.

**How it works:**  
- Address `0` stores the score byte (0–255)  
- Address `1` stores a magic byte `0xAB` to detect first boot  
- On first run → initialises to 0 and writes magic  
- On subsequent boots → reads and restores saved score  
- Only writes to EEPROM when the value actually changes (protects write cycles)

```cpp
#define EEPROM_ADDR_SCORE  0
#define EEPROM_ADDR_MAGIC  1
#define EEPROM_MAGIC_VAL   0xAB

uint8_t loadHighScore() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_VAL) {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VAL);
    EEPROM.write(EEPROM_ADDR_SCORE, 0);
    return 0;
  }
  return EEPROM.read(EEPROM_ADDR_SCORE);
}

void saveHighScore(uint8_t hs) {
  if (EEPROM.read(EEPROM_ADDR_SCORE) != hs)
    EEPROM.write(EEPROM_ADDR_SCORE, hs);
}
```

---

## 🆕 NEW — Idle Marquee Display

**What it does:**  
During idle, the TM1637 display cycles through 3 rotating frames to show the saved high score in a stylish way.

**Frame sequence:**

| Frame | Display | Description |
|-------|---------|-------------|
| 0 | `0--0` | Decorative "radar" frame |
| 1 | `HiSr` | "High Score" label |
| 2 | `_XX_` | Actual saved score (centred) |

Each frame shows for `HISCORE_FRAME_MS` (default: 1500ms) before cycling.

**Score centering logic:**
- Score 0–9  → `_X__` (single digit at pos 2)
- Score 10–99 → `_XX_` (two digits at pos 1–2)

```cpp
switch (frameIndex) {
  case 0: tm.setSegments(SEG_0DD0, 4, 0);   break;  // 0--0
  case 1: tm.setSegments(SEG_HISR, 4, 0);   break;  // HiSr
  case 2:
    if (highScore < 10)
      tm.showNumberDecEx(highScore, 0, false, 1, 2);
    else
      tm.showNumberDecEx(highScore % 100, 0, false, 2, 1);
    break;
}
```

The marquee runs **non-blocking** via timestamp comparison — no `delay()` in idle.

---

## 🆕 NEW — New High Score Celebration Effect

**What it does:**  
When the player beats the high score, a celebration runs **before** the `dEAd` sequence:

- All 4 LEDs flash 3 times rapidly  
- `NOTE_A5` plays during each flash  
- Ends with a `NOTE_C6` victory pip  

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

# ⚙️ v1.5 FULL FEATURE SET

✔ EEPROM high score — persists across power cycles  
✔ Magic byte first-boot detection  
✔ Write-cycle-safe EEPROM save (only writes on value change)  
✔ 3-frame idle display marquee — `0--0` / `HiSr` / `_XX_`  
✔ Non-blocking marquee loop (no delay in idle)  
✔ Gameplay-scoped 1-minute timeout (never fires in idle)  
✔ New high score celebration flash + tone  
✔ NeoPixel rainbow → pink fade transitions  
✔ Organic independent LED breathe animation  
✔ Countdown: `33 → 22 → 11 → GO`  
✔ Per-button unique tones  
✔ Level-up jingle  
✔ Game-over wah-wah sweep  
✔ `dEAd` flash display on loss  

---

# 📐 SEGMENT ENCODINGS (TM1637)

```
TM1637 bit order: bit6=g  5=f  4=e  3=d  2=c  1=b  0=a
```

| Symbol | Hex | Segments |
|--------|-----|----------|
| `-` (dash) | `0x40` | g |
| `d` | `0x5E` | g,e,d,c,b |
| `E` | `0x79` | g,f,e,d,a |
| `A` | `0x77` | g,f,e,c,b,a |
| `H` | `0x76` | g,f,e,c,b |
| `i` | `0x10` | e,d |
| `S` | `0x6D` | g,f,d,c,a |
| `r` | `0x50` | g,e |
| `G` | `0x3D` | f,e,d,c,a |
| `O` | `0x3F` | f,e,d,c,b,a |
| `0` | `0x3F` | f,e,d,c,b,a |

---

# 🔧 CONFIG REFERENCE

```cpp
// ── EEPROM ───────────────────────────────────
#define EEPROM_ADDR_SCORE  0       // Score byte location
#define EEPROM_ADDR_MAGIC  1       // Magic byte location
#define EEPROM_MAGIC_VAL   0xAB    // First-boot sentinel

// ── Idle Marquee ─────────────────────────────
#define HISCORE_FRAME_MS   1500UL  // Time per marquee frame (ms)

// ── Timeout ──────────────────────────────────
#define TIMEOUT_MS         60000UL // 1 min — gameplay only

// ── NeoPixel ─────────────────────────────────
#define NEO_COUNT          46
#define IDLE_HUE_STEP      300
#define IDLE_CYCLE_DELAY_MS 20
#define IDLE_FADE_IN_STEPS  60
#define NEO_FADE_STEPS      40
#define NEO_FADE_DELAY_MS   15

// ── Game Pink Colour ─────────────────────────
#define GAME_NEO_R  255
#define GAME_NEO_G  105
#define GAME_NEO_B  180

// ── LED Breathe ──────────────────────────────
#define LED_IDLE_MIN_BRIGHT   0
#define LED_IDLE_MAX_BRIGHT   180
#define LED_IDLE_FADE_SPEED   3
#define LED_IDLE_TICK_MS      8
```

---

# 🧠 ARCHITECTURE OVERVIEW

```
setup()
  └── loadHighScore()         // EEPROM read on boot

loop()
  ├── idleMode()              // Blocks until button press
  │     ├── neoFadeInRainbow()
  │     ├── neoRainbowTick()  // Non-blocking rainbow
  │     ├── tickIdleLeds()    // Non-blocking LED breathe
  │     └── updateIdleDisplay() // Non-blocking marquee
  │
  ├── startGameTransition()
  │     ├── neoFadeTo(pink)
  │     └── displayCountdown()
  │
  ├── [Game Loop]
  │     ├── displayScore()
  │     ├── playSequence()
  │     └── checkUserSequence()
  │           └── readButtonWithTimeout()  // Timeout active here only
  │
  └── gameOverTransition()
        ├── saveHighScore()    // EEPROM write if new record
        ├── playNewHighScoreEffect()
        ├── displayDead()
        ├── playGameOverSound()
        └── neoFadeTo(black)
```

---

# 🔌 HARDWARE & PIN MAP

| Component | Pin | Notes |
|-----------|-----|-------|
| Button 1 | D4 | INPUT_PULLUP, active LOW |
| Button 2 | D5 | INPUT_PULLUP, active LOW |
| Button 3 | D7 | INPUT_PULLUP, active LOW |
| Button 4 | D8 | INPUT_PULLUP, active LOW |
| LED 1 | D6 | PWM |
| LED 2 | D9 | PWM |
| LED 3 | D10 | PWM |
| LED 4 | D11 | PWM |
| Buzzer | D12 | tone() |
| TM1637 CLK | D2 | |
| TM1637 DIO | D3 | |
| NeoPixel | D13 | 46 LEDs |
| RNG Seed | A5 | Leave floating |

⚠️ D12 is not PWM on UNO — LED 4 uses D3 for PWM breathe  
⚠️ EEPROM addresses 0 and 1 are reserved by this sketch

---

# 📦 REQUIRED LIBRARIES

| Library | Source |
|---------|--------|
| `TM1637Display` | Arduino Library Manager (Avishorp) |
| `Adafruit NeoPixel` | Arduino Library Manager |
| `EEPROM` | Built-in (AVR core) |

---

# 🎬 SIMULATION

👉 Wokwi Project:  
https://wokwi.com/projects/461780873004005377

---

## 👤 Author & Contact

👨 **Name:** Aniket Chowdhury (aka Hashtag)  
📧 **Email:** [micro.aniket@gmail.com](mailto:micro.aniket@gmail.com)  
💼 **LinkedIn:** [itzz-hashtag](https://www.linkedin.com/in/itzz-hashtag/)  
🐙 **GitHub:** [itzzhashtag](https://github.com/itzzhashtag)  
📸 **Instagram:** [@itzz_hashtag](https://instagram.com/itzz_hashtag)

---

```cpp
// v1.5 — The machine now remembers.
// High score saved. Power off. Come back.
// It still knows your best.
```
