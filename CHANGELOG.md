# Changelog

All notable changes to DeskBuddy are documented here. This project follows [Semantic Versioning](https://semver.org/) where practical, though early pre-1.0 versions were more experimental than strictly versioned.

---

## [v1.0.0] — Core Experience

The first release considered feature-complete and reliable enough for daily use.

### Added
- Web-based configuration portal (WiFi, weather, MQTT, HTTP settings), backed by LittleFS
- HTTP notification endpoint (`/notify?title=&msg=`) as an MQTT-free alternative, running alongside MQTT without conflict
- MQTT reconnection with exponential backoff (1s base delay, 5-minute ceiling, capped retry count)
- Status LED logic: red (silent mode), yellow (unread notifications), green (WiFi/MQTT state)
- Silent / DND mode, toggled via long-press, persisted to flash

### Fixed
- `loop()` had broken brace structure leaving duplicate, unreachable weather-fetch code
- `MOOD_COUNT` was set to 13 while 15 moods were defined, causing mood cycling to skip the last two
- `drawMouth()` was defined but never called from `drawMochi()`, leaving Mochi's face without a mouth
- `startConfigPortal()` was defined but never invoked from `setup()`, so holding BTN2 at boot had no effect
- PubSubClient's default 256-byte buffer was too small for HiveMQ TLS packets, causing dropped/truncated MQTT messages — fixed with `setBufferSize(1024)`
- Blank MQTT credentials previously caused a silent bail-out instead of a validated, logged failure
- BTN2 boot-time config detection existed but wasn't wired into `setup()`

### Changed
- Notification push logic (MQTT and HTTP) consolidated into a single shared function to remove duplicated buffer-handling code
- Buzzer melodies (`buzzIntro`, `buzzScreenChange`, `buzzEmotion`, `buzzNotification`) — previously composed but never called — wired into boot, button handling, and notification events

---

## [v0.5.0] — Notifications & Configuration

The point where DeskBuddy became more than a clock — real-time notifications and persistent configuration arrived.

### Added
- MQTT client (PubSubClient) with TLS support, connecting to a configurable broker
- MQTT callback parsing JSON payloads (`app`, `title`, `body`) into a circular notification buffer
- Fourth screen: notification history, with BTN1 scrolling through recent messages
- Popup overlay shown for a few seconds on incoming notifications
- `Config` struct backed by LittleFS (`/config.json`), replacing hardcoded WiFi/weather/MQTT values
- WiFiManager integration for captive-portal WiFi setup on first boot

### Changed
- Weather city, API key, and units moved from hardcoded constants into the persisted config struct

### Known Issues (carried into later fixes)
- Config portal existed in code but wasn't reachable from normal boot flow
- Several LED and buzzer functions were written but not yet connected to actual events

---

## [v0.1.0] — First Prototype

The initial working sketch — a proof of concept, not yet a product.

### Added
- OLED initialization and rendering (SH1106, 128×64, I2C)
- Clock screen with NTP-synced time, 12-hour format, and date
- Weather screen with basic OpenWeatherMap integration
- Early version of the Mochi face — static eyes, no physics or mood system yet
- Basic button handling for screen cycling
- Hardcoded WiFi credentials and API keys (no persistent config yet)

---

## Versioning Notes

Given DeskBuddy's history as a personal project before becoming open source, versions prior to v1.0 are reconstructed from development notes rather than tagged releases. Going forward, releases will be tagged in GitHub and this file will be updated as part of the release process — see [`CONTRIBUTING.md`](CONTRIBUTING.md).
