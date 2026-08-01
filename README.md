<p align="center">
  <img src="docs/assets/banner.png" alt="DeskBuddy banner" width="800">
</p>

<h1 align="center">🌸 DeskBuddy</h1>

<p align="center">
  A tiny ESP8266 desk companion that keeps you focused — clock, weather, and phone notifications on a screen the size of your palm, so your phone can stay in your bag.
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-ESP8266-blue">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-green">
  <img alt="Status" src="https://img.shields.io/badge/status-active-brightgreen">
  <img alt="PRs Welcome" src="https://img.shields.io/badge/PRs-welcome-orange">
</p>

<p align="center">
  <img src="docs/assets/demo.gif" alt="DeskBuddy demo GIF" width="600">
</p>

---

## What is DeskBuddy?

DeskBuddy is a small, single-board desktop companion built around an ESP8266 (Wemos D1 Mini) and a 128×64 OLED display. It shows the time, local weather, and forwarded phone notifications, wrapped around an animated face called **Mochi** that reacts with 15 different moods.

The whole point is to give you *just enough* information at a glance — without giving you a reason to unlock your phone.

## Why I Built DeskBuddy

I'm a student, and I kept noticing the same pattern: I'd pick up my phone just to check the time, and ten minutes later I'd still be scrolling. DeskBuddy exists to break that loop. It sits on the desk, shows the clock, the weather, and only the notifications that matter, and lets the phone stay face-down and out of reach.

## Problem Statement

Phones are the single biggest source of distraction during focused work or study, even when they're only used for "quick, harmless" things like checking the time or a notification. Existing smart displays are either expensive, cloud-locked, or overloaded with features that reintroduce the same distraction they're meant to solve.

## Objectives

- Build a low-cost, self-contained desk display using parts a hobbyist likely already owns
- Forward only real phone notifications, with no social media feed, no browsing, no infinite scroll
- Keep the whole thing hackable — one `.ino` file, readable code, documented pinouts
- Make it something other people can build, fork, and extend

## Features

- 🕒 **Clock screen** — 12-hour time with date and a minute progress bar, synced via NTP
- 🌤️ **Weather screen** — live conditions from OpenWeatherMap with custom pixel-art icons
- 🐣 **Mochi face** — a physics-driven animated face with 15 moods, blinking, saccades, and idle breathing
- 🔔 **Notifications** — phone notifications forwarded over MQTT (via MacroDroid) or plain HTTP, shown as a popup and stored in a scrollable history
- 🌐 **Web config portal** — configure WiFi, weather, and MQTT from a phone browser, no reflashing required
- 🔴🟡🟢 **Status LEDs** — silent mode, unread notifications, and WiFi/MQTT connection state at a glance
- 🔇 **Silent mode** — mute the buzzer without losing any functionality
- 🔊 **Buzzer feedback** — distinct tones for boot, screen changes, moods, and notifications

## Hardware Used

| Component | Notes |
|---|---|
| ESP8266 (Wemos D1 Mini) | Main microcontroller |
| SH1106 OLED, 128×64, I2C | Primary display |
| 3× LED (Red / Yellow / Green) | Status indicators, with 470Ω resistors |
| 2× Push buttons | Mode / mood cycling and boot-time config trigger |
| Piezo buzzer | Audio feedback |
| 100µF + 0.1µF capacitors | Decoupling across 3V3/GND (recommended, prevents brown-out resets) |

See [`hardware/PINOUT.md`](hardware/PINOUT.md) for the full wiring table.

## Software Stack

- **Arduino IDE** targeting the ESP8266 core
- [U8g2](https://github.com/olikraus/u8g2) — OLED rendering
- [PubSubClient](https://github.com/knolleary/pubsubclient) — MQTT over TLS
- [WiFiManager](https://github.com/tzapu/WiFiManager) — captive-portal WiFi setup
- [ArduinoJson](https://arduinojson.org/) — config and notification payload parsing
- [LittleFS](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html) — persistent configuration storage
- [HiveMQ Cloud](https://www.hivemq.com/mqtt-cloud-broker/) — MQTT broker (TLS, port 8883)
- [OpenWeatherMap](https://openweathermap.org/api) — weather data
- [MacroDroid](https://www.macrodroid.com/) — Android-side notification forwarding

## Folder Structure

```
deskbuddy/
├── firmware/
│   └── deskbuddy.ino          # Complete firmware, single sketch
├── hardware/
│   ├── PINOUT.md              # Full pin mapping and wiring notes
│   └── kicad/                 # PCB design files (in progress)
├── docs/
│   ├── assets/                # Banner, demo GIF, screenshots
│   ├── PROJECT_OVERVIEW.md
│   └── ROADMAP.md
├── CONTRIBUTING.md
├── GOOD_FIRST_ISSUES.md
├── CHANGELOG.md
├── LICENSE
└── README.md
```

## Installation

1. Install the [ESP8266 Arduino core](https://github.com/esp8266/Arduino) via Boards Manager.
2. Install the required libraries through Library Manager:
   - `PubSubClient`
   - `U8g2`
   - `WiFiManager` (tzapu)
   - `ArduinoJson`
3. Clone this repository:
   ```bash
   git clone https://github.com/<your-username>/deskbuddy.git
   ```
4. Wire up the hardware according to [`hardware/PINOUT.md`](hardware/PINOUT.md).

## Flashing

1. Open `firmware/deskbuddy.ino` in the Arduino IDE.
2. Select **Board → ESP8266 → NodeMCU 1.0 (ESP-12E Module)**.
3. Select the correct COM port.
4. Click **Upload**.
5. Open the Serial Monitor at **115200 baud** to watch the boot sequence.

## Configuration

DeskBuddy stores its configuration in flash (LittleFS) as JSON, so nothing is hardcoded after the first setup. There are two ways to configure it:

- **Web config portal** (recommended) — hold **BTN2** while powering on or resetting the device.
- **Direct edit** — modify the default `Config` struct in the firmware before flashing, for a fully pre-configured build.

## Web Portal

Holding BTN2 during boot puts DeskBuddy into config mode:

1. The OLED shows `CONFIG MODE` along with an IP address.
2. Connect your phone to the `DeskBuddy_Config` WiFi network.
3. Open `http://192.168.4.1` in a browser.
4. Fill in weather, MQTT, and HTTP notification settings.
5. Tap **Save & Restart** — the device reboots into normal operation with the new configuration.

You can also trigger a full WiFi reset from the same portal if you need to connect the device to a different network.

## Weather

DeskBuddy fetches current conditions from OpenWeatherMap every 10 minutes, using the city and units configured through the web portal. It displays temperature, feels-like temperature, humidity, wind speed, and a matching pixel-art icon (sun, cloud, rain, thunderstorm, snow, or mist).

## MQTT

Notifications are primarily delivered over **MQTT with TLS** (port 8883), by default via a HiveMQ Cloud broker. DeskBuddy subscribes to a configurable topic and expects a small JSON payload:

```json
{"app": "WhatsApp", "title": "John", "body": "Hey, are you free later?"}
```

Reconnection uses exponential backoff, so a dropped broker connection won't spam retries or block the rest of the device.

## HTTP Notifications

For simpler setups, or as a fallback when MQTT isn't available, DeskBuddy also exposes a lightweight HTTP endpoint:

```
GET http://<device-ip>/notify?title=Mother&msg=Dinner+is+ready
```

This can be toggled and reconfigured (port and path) from the web portal, and runs alongside MQTT without conflict.

## Automate Integration

DeskBuddy is designed to pair with **MacroDroid** on Android:

1. Create a macro triggered by **Notification Received**.
2. Add an **HTTP Request** or **MQTT Publish** action pointing at your DeskBuddy instance.
3. Map the notification's app, title, and text into the payload described above.

A full macro walkthrough will live in `docs/` as the project matures — contributions with screenshots are very welcome.

## Screenshots

<p align="center">
  <img src="docs/assets/screenshot-clock.png" width="250">
  <img src="docs/assets/screenshot-weather.png" width="250">
  <img src="docs/assets/screenshot-mochi.png" width="250">
</p>

*(Screenshots coming soon — see [`GOOD_FIRST_ISSUES.md`](GOOD_FIRST_ISSUES.md) if you'd like to contribute photos of your build.)*

## Demo

<p align="center">
  <img src="docs/assets/demo.gif" alt="DeskBuddy in action" width="500">
</p>

## Future Plans

- Custom single-sided PCB (KiCad) for a clean, solderable build
- OTA firmware updates
- Battery monitoring and standalone power operation
- Particle effects and refined Mochi physics
- ESP32 support

See [`ROADMAP.md`](docs/ROADMAP.md) for the full version-by-version plan.

## Contributing

Contributions are very welcome, from first-timers to experienced embedded developers. Start with [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`GOOD_FIRST_ISSUES.md`](GOOD_FIRST_ISSUES.md).

## License

DeskBuddy is released under the **MIT License** — see [`LICENSE`](LICENSE) for details.

MIT was chosen because it's short, permissive, and puts almost no restrictions on how people use, modify, or build on this project — commercially or otherwise. For a hobby/educational hardware project meant to be forked, remixed, and built on desks around the world, that low-friction openness matters more than the stronger copyleft guarantees of something like GPL.
