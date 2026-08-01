# Hardware Overview

This document explains what each hardware component does in DeskBuddy and why it was chosen. For wiring diagrams and exact pin tables, see the [`hardware/`](../hardware) folder — this file focuses on the reasoning, not the connections.

## ESP8266 (Wemos D1 Mini)

The main microcontroller. Chosen primarily because it was already on hand, but it's also a reasonable fit for this project on its own merits: built-in WiFi (required for weather, MQTT, and the HTTP endpoint), enough flash for LittleFS-based configuration storage, and a large enough hobbyist ecosystem that libraries for every subsystem DeskBuddy needs (OLED, MQTT, JSON, web server, WiFi provisioning) already exist and are well-maintained.

The trade-off is limited RAM (roughly 80KB usable) and a handful of GPIOs with boot-time constraints — both of which directly shaped firmware decisions documented in [`docs/CONCEPTS.md`](CONCEPTS.md) and [`docs/CODE_EXPLAINED.md`](CODE_EXPLAINED.md).

## SH1106 OLED (128×64, I2C)

The primary display. A monochrome OLED was chosen over a color LCD for three reasons: lower power draw, higher contrast for at-a-glance readability, and a smaller RAM footprint for frame buffering — all of which matter more on an ESP8266 than raw visual fidelity does. The firmware's display driver is written specifically for the SH1106 controller — a visually similar but electrically different OLED (like an SSD1306 module) will not render correctly without a driver-level code change, even though the two are pin-compatible.

I2C was chosen over SPI for wiring simplicity — two signal wires instead of four or five, at the cost of a slower maximum refresh rate. At DeskBuddy's animation frame rate (20 FPS, `FRAME_MS = 50`), I2C's bandwidth isn't a bottleneck.

## Buttons (BTN1, BTN2)

Two buttons were chosen deliberately, not more. The whole point of DeskBuddy is to reduce interaction, not add a second interface to fuss with — so the button scheme relies on press duration (short vs. long) and context (which screen is active) to multiply two physical inputs into five distinct actions, documented in [`docs/USER_MANUAL.md`](USER_MANUAL.md#buttons).

## LEDs (Red / Yellow / Green)

Three LEDs provide status at a glance without needing to look at the OLED at all — useful since the OLED cycles between four different screens and won't always be showing the information you want a quick read on. Color choices follow common conventions: red for "do not disturb," yellow for "something needs your attention," green for "connectivity is healthy" — deliberately intuitive without needing to consult a manual.

## Buzzer (Piezo, Passive)

A passive piezo buzzer was chosen specifically because it's driven with `tone()`, which lets the firmware play actual melodies (different frequencies and durations per note) rather than a single fixed beep. This is what makes the 15 distinct mood tones and the multi-note intro jingle possible — an active buzzer (fixed internal oscillator) couldn't do this.

## Power & Capacitors

DeskBuddy runs off USB power. Two decoupling capacitors — 100µF electrolytic and 0.1µF ceramic — sit across the 3V3/GND rail specifically because the combination of WiFi transmit bursts, OLED refreshes, and simultaneous LED/buzzer activity creates current draw spikes that a bare ESP8266 board's onboard regulator doesn't always absorb cleanly on its own. These were added after observing occasional resets during testing under combined load — a direct, empirical fix rather than a precaution added speculatively.

## Pin Descriptions

Full pin-by-pin details, including which GPIOs have boot-time constraints, live in [`hardware/PINOUT.md`](../hardware/PINOUT.md). The short version: I2C (D1/D2) drives the display, two GPIOs (D5/D7) read the buttons, three GPIOs (D6/D3/D4) drive the LEDs, and one GPIO (D8) drives the buzzer.

## Why Each Component Was Chosen — Summary

| Component | Primary Reason |
|---|---|
| ESP8266 (D1 Mini) | Already owned; built-in WiFi; adequate flash/RAM for this feature set |
| SH1106 OLED | Low power, high contrast, small frame buffer footprint; already owned |
| Buttons ×2 | Minimal physical interface, deliberately constrained |
| LEDs ×3 | At-a-glance status without needing to read the screen |
| Piezo buzzer (passive) | Needed for multi-tone melodies, not just a single beep |
| Decoupling capacitors | Fixed observed reset issues under combined WiFi/display/LED/buzzer load |

## Hardware Limitations

- **Display driver is SH1106-specific.** Visually similar SSD1306 OLED modules are not a drop-in replacement without firmware changes, despite matching pinouts.
- **Three GPIOs used for LEDs/buzzer have boot-time constraints** (D3, D4, D8 — see [`hardware/PINOUT.md`](../hardware/PINOUT.md)). No issues have been observed in practice, but this limits how freely those pins can be repurposed or extended.
- **USB-powered only.** There is no onboard battery or power switch — the device must stay connected to a USB power source to run.
- **Fixed component set.** The current firmware assumes exactly two buttons, three LEDs, one buzzer, and one OLED — adding or removing peripherals requires firmware changes, not just configuration.
- **No enclosure.** The current build is bare — breadboard or perfboard, with no protective housing.

Ideas for hardware evolution beyond the current build (alternate displays, battery power, a custom PCB, and so on) are intentionally kept out of this document — see [`ROADMAP.md`](ROADMAP.md) for planned future improvements. This document only covers what's actually built.
