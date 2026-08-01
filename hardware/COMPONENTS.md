# Bill of Materials

Component list for a single DeskBuddy unit. Prices are rough estimates from Indian electronics suppliers (e.g. Robu.in) at the time of writing — check current listings before ordering.

| Component | Purpose | Qty | Typical Price (INR) | Recommended Alternatives |
|---|---|---|---|---|
| Wemos D1 Mini (ESP8266) | Main microcontroller, WiFi | 1 | ₹250–350 | Any ESP8266 board with equivalent GPIO breakout (NodeMCU works but is physically larger) |
| SH1106 OLED, 128×64, I2C | Primary display | 1 | ₹300–450 | None recommended — the display driver in firmware is SH1106-specific; a pin-compatible SSD1306 module will not render correctly without a firmware change |
| LED — Red, 5mm | Silent/DND indicator | 1 | ₹1–2 | Any 5mm LED; color is cosmetic |
| LED — Yellow, 5mm | Unread notification indicator | 1 | ₹1–2 | Any 5mm LED |
| LED — Green, 5mm | WiFi/MQTT status indicator | 1 | ₹1–2 | Any 5mm LED |
| Resistor, 470Ω | Current limiting for each LED | 3 | ₹1 each | 330Ω–1kΩ all work fine for indicator brightness at 3.3V logic |
| Push button (momentary, 6mm or 12mm) | BTN1 / BTN2 | 2 | ₹2–5 each | Any normally-open momentary switch |
| Piezo buzzer (passive) | Audio feedback | 1 | ₹15–30 | Must be a **passive** buzzer — the firmware drives it with `tone()`/`noTone()` at specific frequencies, which won't work correctly on an active (fixed-tone) buzzer |
| Electrolytic capacitor, 100µF | Bulk decoupling across 3V3/GND | 1 | ₹2–5 | 47–220µF all acceptable |
| Ceramic capacitor, 0.1µF | High-frequency decoupling across 3V3/GND | 1 | ₹1–2 | Standard value, not critical |
| Perfboard or breadboard | Assembly | 1 | ₹20–80 | Either works; perfboard gives a more permanent, compact result |
| Hookup wire | Connections | — | ₹20–40 (spool) | 22–26 AWG solid-core recommended for breadboard/perfboard work |
| Micro-USB cable | Power + programming | 1 | ₹50–100 | Must support data lines, not charge-only |

## Total Estimated Cost

Roughly **₹700–1,000** for a single unit, assuming a D1 Mini and OLED are the dominant costs and everything else is bought in small quantities. Buying passives (resistors, capacitors) in bulk packs brings the per-unit cost down further for anyone building more than one.

This list reflects the components actually used in the current build. For possible future hardware directions, see [`ROADMAP.md`](../docs/ROADMAP.md).
