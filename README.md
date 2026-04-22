<div align="center">
  
# 🧠 Memory Hunter
**A Hardware Memory Game on Arduino UNO**
**by [Aniket Chowdhury](mailto:micro.aniket@gmail.com) (aka `#Hashtag`)**

![Status](https://img.shields.io/badge/Status-Stable-brightgreen?style=for-the-badge&logo=arduino)
![Platform](https://img.shields.io/badge/Platform-Arduino%20Uno-blue?style=for-the-badge)
![Type](https://img.shields.io/badge/Type-Embedded%20Memory%20Game-orange?style=for-the-badge)

</div>

---

## 🎬 PROJECT OVERVIEW

Memory Hunter is a fully hardware-based memory game built on Arduino UNO
with WS2812B NeoPixel strip, TM1637 7-segment display, and a passive buzzer.

The machine plays an ever-growing sequence of coloured light + sound cues.
The player must repeat them back — in perfect order.

One new step is added every round. Get it right and you level up.
Get it wrong — or take too long — and it's dEAd.

🧠 How far can your memory take you?

---

## ⚡ CORE IDEA

- LED Strip  = Atmosphere Engine
- 4 LEDs     = Game Colours
- 4 Buttons  = Player Controls
- TM1637     = Score + Countdown Display
- Buzzer     = Audio Feedback System
- EEPROM     = High Score Memory

---

## 🎮 GAMEPLAY LOOP

- IDLE → BUTTON PRESS → COUNTDOWN (33→22→11→GO)
- SEQUENCE PLAYS → PLAYER REPEATS
- CORRECT = LEVEL UP → LONGER SEQUENCE
- WRONG / TIMEOUT = dEAd → GAME OVER
- HIGH SCORE SAVED → RETURN TO IDLE

---

## 🧠 WHAT MAKES THIS SPECIAL

- ✔ EEPROM High Score — survives power cycles
- ✔ Idle Marquee Display — 0--0 / HiSr / score cycling
- ✔ NeoPixel atmosphere — rainbow idle, pink game mode
- ✔ Organic LED breathe — each LED independently animated
- ✔ 1-minute inactivity timeout — only fires during gameplay
- ✔ Countdown sequence — 33 → 22 → 11 → GO
- ✔ Unique tone per button — full audio feedback
- ✔ New high score celebration flash + jingle

---

## ⚙️ GAME STATES

- IDLE       → Rainbow NeoPixel + LED breathe + high score marquee
- TRANSITION → NeoPixel fade to pink + countdown
- PLAYING    → Sequence display + player input
- LEVEL UP   → Jingle + score increment
- GAME OVER  → dEAd flash + wah-wah sound
- HIGH SCORE → Celebration flash + save to EEPROM

---

## 🎨 VISUAL SYSTEM

State        | NeoPixel            | LEDs            | Display
-------------|---------------------|-----------------|-------------------------
Idle         | 🌈 Chasing rainbow  | Organic breathe | 0--0 / HiSr / score
Transition   | 🩷 Fade to pink     | Off             | 33→22→11→GO
Game         | 🩷 Solid pink       | Game colours    | Score
Game Over    | ⬛ Fade to black    | Flash + off     | dEAd

---

## 🔊 SOUND SYSTEM

Event        | Tone
-------------|------------------------------
Button 1     | G3
Button 2     | C4
Button 3     | E4
Button 4     | G5
Countdown    | C4 beeps
Level Up     | E4→G4→E5→C5→D5→G5 jingle
Game Over    | DS5→D5→CS5 wah-wah sweep
High Score   | A5 flash → C6 pip
Start Press  | E5 confirmation beep

---

## 🏆 HIGH SCORE SYSTEM

- Stored in EEPROM — survives power cycles
- Magic byte 0xAB detects first boot and initialises to 0
- Beat the record → immediate save + celebration effect
- Idle marquee cycles through 0--0 → HiSr → score display

---

## 🔌 HARDWARE

Components:
- Arduino UNO
- WS2812B NeoPixel LED Strip (46 LEDs)
- TM1637 4-digit 7-segment display
- 4x PWM LEDs (game colours)
- 4x Push Buttons
- Passive Buzzer

---

## ⚡ PIN MAP

Component      | Pin
---------------|----
Buttons        | D4, D5, D7, D8
Game LEDs      | D6, D9, D10, D11
Buzzer         | D12
TM1637 CLK     | D2
TM1637 DIO     | D3
NeoPixel Data  | D13
RNG Seed       | A5 (floating)

⚠ Pin 12 is NOT PWM on UNO — LED 4 uses pin 3 workaround
⚠ EEPROM: Address 0 = score, Address 1 = magic byte 0xAB

---

## 📦 REQUIRED LIBRARIES

Install via Arduino Library Manager:
- TM1637Display  (by Avishorp)
- Adafruit NeoPixel

---

## 🧠 ENGINEERING CONCEPTS

- EEPROM non-volatile memory management
- Non-blocking multi-LED animation engine
- Finite state idle / game transition system
- NeoPixel HSV rainbow + smooth fading
- Independent per-LED organic breathe animation
- Inactivity timeout scoped to gameplay only
- Random seed from floating analog pin

---

## 🎬 SIMULATION

👉 Wokwi Project:
https://wokwi.com/projects/461780873004005377

---

## 🚀 FUTURE UPGRADES

- Difficulty selector (slow / normal / hard)
- 2-player competitive mode
- Bluetooth leaderboard via ESP32
- Web score dashboard
- LCD extended stats display
- Animated sequence preview

---

## 📁 VERSIONS

Version | Highlights
--------|--------------------------------------------------
v1.0    | Base game
v1.1    | NeoPixel rainbow idle
v1.2    | TM1637 score display
v1.3    | Countdown + level jingle
v1.4    | Timeout + LED breathe
v1.5    | EEPROM high score + idle marquee + timeout fix

---

## 📸 Simulation

<img width="721" height="725" alt="image" src="https://github.com/user-attachments/assets/75df04f1-64db-44a1-a5de-6d8470fad5f0" />

---

## 📸 Wiring & Schematic

<img width="1231" height="843" alt="image" src="https://github.com/user-attachments/assets/d7eb3362-39dc-4320-8193-e9efe62c924d" />
.
<img width="1254" height="847" alt="image" src="https://github.com/user-attachments/assets/101fff39-0948-41fb-8d6a-bb2828a91c5f" />
.

---

## 👤 Author & Contact

👨 **Name:** Aniket Chowdhury (aka Hashtag)  
📧 **Email:** [micro.aniket@gmail.com](mailto:micro.aniket@gmail.com)  
💼 **LinkedIn:** [itzz-hashtag](https://www.linkedin.com/in/itzz-hashtag/)  
🐙 **GitHub:** [itzzhashtag](https://github.com/itzzhashtag)  
📸 **Instagram:** [@itzz_hashtag](https://instagram.com/itzz_hashtag)

---

## 📜 License

This project is released under a Modified MIT License.
It is intended for personal and non-commercial use only.

🚫 Commercial use or distribution for profit is not permitted without prior written permission.
🤝 For collaboration, reuse, or licensing inquiries, please contact the author.

📄 View Full License <br>
[![License: MIT–NC](https://img.shields.io/badge/license-MIT--NC-blue.svg)](./LICENSE)

---

## ❤️ Acknowledgements

This is a solo passion project, built with countless nights of tinkering, testing, and debugging.  
If you find it useful or inspiring, feel free to ⭐ the repository or connect with me on social media!

---

> _ "The machine remembers everything. The question is — can you?"
