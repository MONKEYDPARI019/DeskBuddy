# Code Explained

A top-to-bottom walkthrough of `Firmware/DeskBuddy_v1.0_Stable.ino`. This is the most detailed document in the repository — if you're planning to modify the firmware, start here after reading [`CONCEPTS.md`](CONCEPTS.md).

The sketch is organized into numbered sections (①–⑯), and this document follows that same order.

## Global Variables

| Variable | Type | Purpose |
|---|---|---|
| `config` | `Config` | The entire runtime configuration — weather, MQTT, HTTP, silent mode. Loaded from flash at boot, written to flash on every save. |
| `server` | `ESP8266WebServer*` | Pointer (not a static instance) because the same pointer is reused for two different purposes at different times: the config-portal server, or the normal-mode HTTP notification server. Only one of these runs at once. |
| `espClient` | `WiFiClientSecure` | The TLS socket underlying the MQTT connection. |
| `mqttClient` | `PubSubClient` | Wraps `espClient` with the MQTT protocol layer. |
| `isConfigPortalMode` | `bool` | Set once, in `startConfigPortal()`, and checked at the top of every `loop()` iteration to decide which code path to run. |
| `currentMode` | `ScreenMode` | Which of the four screens is currently displayed. Changed only by `handleButtons()`. |
| `currentMood` | `int` | Mochi's current mood (0–14). Changed only by `handleButtons()`. |
| `notifBuf`, `notifHead`, `notifCount`, `unreadNotifs`, `selectedNotifIdx` | various | The notification circular buffer and its read/write state — see [Notification System](#notification-system). |
| `popupActive`, `popupStartMs`, `popupTitle`, `popupBody` | various | State for the 4-second popup overlay shown on new notifications. |
| `leftEye`, `rightEye` | `Eye` | Physics state (position, target position, pupil offset, blink state) for Mochi's two eyes. |
| `wx` | `WX` | The most recently fetched weather data, plus a `valid` flag so the weather screen knows whether it has real data yet. |

## Configuration Structure

```cpp
struct Config {
  char owm_key[40];
  char owm_city[32];
  char owm_units[10];
  char mqtt_broker[80];
  int  mqtt_port;
  char mqtt_user[32];
  char mqtt_pass[32];
  char mqtt_topic[40];
  bool http_enabled;
  int  http_port;
  char http_endpoint[20];
  bool silent;
};
```

Every string field is a fixed-size `char` array, not a `String` — consistent with the memory-optimization approach described in [`CONCEPTS.md`](CONCEPTS.md#heap-vs-stack). The global `config` variable is initialized with hardcoded defaults (including a working default OpenWeatherMap key and the project's default HiveMQ broker address) so the device is usable immediately, before any configuration has been saved.

## `setup()`

Runs once at boot. In order:

1. Starts Serial at 115200 baud.
2. Configures button pins as `INPUT_PULLUP` and LED/buzzer pins as `OUTPUT`.
3. Seeds the random number generator from a floating analog pin read (used later for Mochi's idle eye movement — not a real sensor reading).
4. Initializes the I2C bus and the OLED display.
5. Initializes the eye physics structs with their starting positions.
6. Calls `loadConfig()`.
7. **Config-mode check:** reads BTN2; if held LOW, waits 500ms and re-checks (debounce), then calls `startConfigPortal()` and returns immediately — skipping the rest of `setup()` and all of `loop()`'s normal-mode logic (see `loop()` below).
8. If not entering config mode: connects WiFi via `WiFiManager::autoConnect()`, restarting the device if that fails outright.
9. Configures NTP time sync.
10. Configures the TLS client and MQTT server/callback (but does not connect yet — that happens lazily in `loop()`).
11. If `config.http_enabled`, starts the HTTP notification server.
12. Fetches weather once immediately (so the first screen isn't blank).
13. Plays the boot intro melody.

**Calls:** `loadConfig()`, `startConfigPortal()`, `fetchWeather()`, `buzzIntro()`
**Modifies:** nearly every global, since it's the entry point

## `loop()`

Runs continuously after `setup()`. Structure:

1. If `isConfigPortalMode` is true, only services the web server (`server->handleClient()`) and returns — none of the normal-mode logic below runs while in config mode.
2. Otherwise: services the HTTP notification server (if it exists), reads buttons, updates LEDs.
3. If WiFi is connected: reconnects MQTT if needed, otherwise services the existing MQTT connection (`mqttClient.loop()`); refetches weather if the 10-minute TTL has elapsed.
4. Clears the display buffer, draws whichever screen `currentMode` currently points to, draws the popup overlay on top if active, and sends the buffer to the OLED.
5. Delays `FRAME_MS` (50ms), giving roughly 20 frames per second.

**Calls:** `handleButtons()`, `updateLEDs()`, `mqttReconnect()`, `fetchWeather()`, one of `drawClock()`/`drawWeather()`/`drawMochi()`/`drawNotificationScreen()`, `drawPopupOverlay()`
**Modifies:** nothing directly — it's an orchestrator that delegates to the functions above

**Possible improvement:** the frame delay (`delay(50)`) is a blocking wait, which is a minor inconsistency with the "avoid `delay()`" pattern used elsewhere (see [`CONCEPTS.md`](CONCEPTS.md#timers-non-blocking)) — button response and MQTT servicing are all gated behind this same 50ms window. In practice this is imperceptible at 20 FPS, but a `millis()`-gated redraw would decouple display refresh rate from input latency.

## Display System

The OLED is wrapped in a single global `display` object (`U8G2_SH1106_128X64_NONAME_F_HW_I2C`), used directly by every drawing function — there's no intermediate display abstraction layer. Every screen function follows the same pattern: `display.clearBuffer()` happens once per frame in `loop()`, the active screen's draw function is called, and `display.sendBuffer()` pushes the finished frame to the physical display at the end of the frame.

## Drawing System

A set of small, focused helper functions used across multiple screens:

| Function | Purpose |
|---|---|
| `roundedBox()` | Draws a filled or outlined rectangle with rounded corners — used heavily by the mood eye-drawing functions. |
| `arcBottom()` | Draws a half-ellipse arc, used for "happy" style closed-eye curves. |
| `sparkle()` | Draws a small plus/X sparkle pattern at a point — used by several moods' particle effects. |
| `pixHeart()` | Draws a small pixel-art heart — used by the Love and Cute moods. |
| `drawPupil()` | Draws a pupil offset from a center point, clamped to a maximum radius so it can't visually leave the eye. |
| `centerStr()` | Centers a text string horizontally around x=64 (half the display width) at a given y — used by the clock screen and a few status messages. |

**Called by:** the mood-specific `eyeXxx()` functions and the clock/weather screens.

## Notification System

Notifications from both MQTT and HTTP sources funnel through a single shared function:

```cpp
void pushNotification(const char* app, const char* title, const char* body)
```

This pushes a new `Notification` struct into a fixed-size circular buffer (`notifBuf[MAX_NOTIFS]`, `MAX_NOTIFS = 8`), advances `notifHead`, increments `notifCount` (capped at `MAX_NOTIFS`) and `unreadNotifs`, sets the popup overlay fields and timer, and triggers `buzzNotification()`.

**Called by:** `mqttCallback()` (after parsing the incoming JSON payload) and `handleNotify()` (the HTTP endpoint handler).
**Modifies:** `notifBuf`, `notifHead`, `notifCount`, `unreadNotifs`, `popupActive`, `popupStartMs`, `popupTitle`, `popupBody`

Once the buffer is full, the oldest notification is silently overwritten by the newest — there's no separate "archive" or persistence beyond the 8 most recent messages, and the buffer is not saved across reboots.

`drawNotificationScreen()` reads from this buffer to show the currently selected notification (indexed backward from `notifHead`, so index 0 is always the most recent) and clears `unreadNotifs` to zero as a side effect of being drawn — meaning the yellow LED turns off simply by viewing this screen, with no separate "mark as read" action needed.

## Weather System

`fetchWeather()` makes a single blocking HTTP GET to OpenWeatherMap, parses the JSON response with `ArduinoJson`, and populates the global `wx` struct. It's called once at boot and then re-triggered from `loop()` whenever `millis() - wxLastFetch > WEATHER_TTL` (10 minutes). If the request fails for any reason (bad key, no connectivity, malformed response), `wx.valid` simply stays at whatever it was before — the weather screen shows "Fetching Weather..." until a request eventually succeeds.

**Calls:** none of note (uses `ESP8266HTTPClient` and `ArduinoJson` directly)
**Called by:** `setup()`, `loop()`
**Modifies:** `wx`, `wxLastFetch`

## MQTT

Three functions make up the MQTT subsystem:

- **`mqttCallback(topic, payload, length)`** — the library callback invoked whenever a subscribed-topic message arrives. Copies the raw payload into a fixed 256-byte buffer, attempts to parse it as JSON (`{"app","title","body"}`), and falls back to treating the whole raw message as the notification body if parsing fails. Delegates the actual buffering/display work to `pushNotification()`.
- **`mqttReconnect()`** — called from `loop()` whenever `mqttClient.connected()` is false and WiFi is up. Gated to at most one attempt every 5 seconds via a static `lastAttempt` timestamp, and skipped entirely if `mqtt_user` or `mqtt_pass` is empty. On failure, logs a human-readable reason based on `mqttClient.state()`.
- **`mqttClient.setCallback(mqttCallback)` / `setServer(...)`** — configured once in `setup()`; the actual `connect()` call happens inside `mqttReconnect()`.

**Possible improvement:** the current reconnect logic uses a flat 5-second retry interval rather than exponential backoff. Under a prolonged broker outage, this means a steady stream of reconnect attempts every 5 seconds indefinitely, rather than progressively backing off — worth revisiting if you're deploying against a broker with connection-rate limits.

## HTTP Server (Notification Endpoint)

`handleNotify()` is registered at `config.http_endpoint` on whichever `ESP8266WebServer` instance is currently running — this route is registered both on the config-portal's server (for testing while configuring) and on the normal-mode server started in `setup()` when `http_enabled` is true. It reads `title` and `msg` query arguments (defaulting to `"Notification"` and empty respectively), copies them into fixed-size stack buffers, and calls `pushNotification("HTTP", title, body)`. If `http_enabled` is false, it responds `403` without processing the request further — this check exists so that toggling the setting off doesn't require also removing/re-registering the route.

## Filesystem

Two functions handle all LittleFS interaction:

- **`loadConfig()`** — mounts LittleFS (formatting if the mount fails), checks for `/config.json`, and if present, parses it and copies each field into the global `config` struct individually (`doc["field"] | config.field` — falling back to the existing default if that specific key is missing, rather than failing the whole load).
- **`saveConfig()`** — serializes the current `config` struct into a `StaticJsonDocument<512>` and writes it to `/config.json`, overwriting any previous contents.

**Called by:** `setup()` (load only), `handleWebSave()` and the BTN2 long-press handler in `handleButtons()` (save only)

## Configuration Portal

- **`startConfigPortal()`** — switches to AP+STA mode, starts an open access point (`DeskBuddy_Config`), creates the `ESP8266WebServer` instance, registers `/`, `/save`, `/resetwifi`, and the notify endpoint, and draws a static "CONFIG MODE" screen with connection instructions. Sets `isConfigPortalMode = true`.
- **`handleWebRoot()`** — builds the settings form HTML by concatenating a PROGMEM header/footer with dynamically-generated `<input>` fields pre-filled from the current `config` values.
- **`handleWebSave()`** — reads each form field (guarded by `server->hasArg(...)` so missing fields don't overwrite existing values with empty strings), updates `config`, calls `saveConfig()`, sends a confirmation page, and restarts the device after a 2-second delay.
- **`handleResetWiFi()`** — calls `WiFiManager::resetSettings()`, sends a confirmation page, and restarts.

## Button Handling

`handleButtons()` runs every loop iteration and implements edge detection (comparing the current pin read against the previous one, stored in `lastBtn1`/`lastBtn2`) rather than just checking the current level — this is what makes each physical press register as exactly one logical event instead of firing repeatedly while held.

- **BTN1 falling edge** (HIGH→LOW): if on the Notifications screen and there's at least one notification, advances `selectedNotifIdx`; otherwise advances `currentMood`.
- **BTN2 falling edge**: records `btn2PressStart`.
- **BTN2 rising edge** (LOW→HIGH): computes press duration. Under `CYCLE_HOLD_MS` (2000ms) → advances `currentMode`. At or above it → toggles `config.silent` and immediately calls `saveConfig()`.

A 150ms `delay()` after each detected event provides basic software debounce, at the cost of briefly blocking the rest of `loop()` — acceptable given how infrequently button events actually occur relative to the frame rate.

**Modifies:** `currentMood`, `currentMode`, `selectedNotifIdx`, `config.silent`, `lastBtn1`, `lastBtn2`, `btn2PressStart`

## Mood Engine

15 moods (`MOOD_DEFAULT` through `MOOD_CUTE`, `MOOD_COUNT = 15`), each with:
- A dedicated eye-drawing function (`eyeHappy()`, `eyeLove()`, etc.), dispatched from `drawMochi()`'s `switch` statement
- A dedicated buzzer melody in `buzzEmotion()`'s `switch` statement
- In some cases, a mood-specific particle effect drawn by `drawMoodParticles()` (sleeping "z"s, love hearts, angry marks, happy/surprised sparkles, cute hearts)

Not every mood has particles — moods without a `case` in `drawMoodParticles()`'s switch simply show no extra effect, which is expected behavior, not a bug.

## Animation Engine

`updatePhysics()`, called once per frame from `drawMochi()`, drives all of Mochi's continuous motion:

- **Breathing:** a sine wave (`sinf(now / 900.0f)`) offsets eye Y-position slightly, giving an idle "breathing" bob.
- **Blinking:** a randomized timer (`nextBlinkTime`) triggers a blink; while blinking, eye height is driven toward a near-zero target and springs back afterward.
- **Saccades:** at randomized intervals (500–2000ms), a new random gaze direction is picked from a fixed set of 9 (including center), and the pupil position eases toward it every frame using `spf()` (a simple linear interpolation toward a target value — see below).

**`spf(cur, tgt, k)`** is the core "spring-like" easing helper used throughout: `cur + (tgt - cur) * k`. Every frame, this moves a value a fixed fraction of the remaining distance toward its target, which produces a smooth deceleration curve without needing actual physics simulation (mass, velocity, springs).

## Memory Optimization

Concrete choices in this codebase, beyond the general principles covered in [`CONCEPTS.md`](CONCEPTS.md#memory-optimization):

- Fixed-size `char` buffers with `strlcpy()` throughout the notification and HTTP-handling code, instead of `String` concatenation.
- `StaticJsonDocument<512>` (stack-allocated, fixed capacity) rather than `DynamicJsonDocument` for all JSON parsing — config file, weather response, and MQTT payloads.
- Large, unchanging HTML blocks (`WEB_HEADER`, `WEB_FOOTER`) stored in `PROGMEM` rather than RAM.
- `F()` wrapping for Serial debug strings and small HTML fragments throughout.
- `String html; html.reserve(2500);` in `handleWebRoot()` — even where `String` concatenation is used (building the dynamic form), capacity is pre-reserved up front to avoid repeated reallocation as fields are appended.

## Function Reference Summary

| Function | Called By | Primary Purpose |
|---|---|---|
| `setup()` | (framework entry point) | One-time initialization |
| `loop()` | (framework entry point) | Main runtime cycle |
| `loadConfig()` / `saveConfig()` | `setup()`, `handleWebSave()`, `handleButtons()` | LittleFS config persistence |
| `startConfigPortal()` | `setup()` | Enter configuration mode |
| `handleWebRoot()` / `handleWebSave()` / `handleResetWiFi()` | Web server routing | Config portal HTTP handlers |
| `handleNotify()` | Web server routing | HTTP notification endpoint |
| `pushNotification()` | `mqttCallback()`, `handleNotify()` | Shared notification buffer/popup logic |
| `mqttCallback()` | PubSubClient library | Parses incoming MQTT notification payloads |
| `mqttReconnect()` | `loop()` | MQTT connection management |
| `fetchWeather()` | `setup()`, `loop()` | OpenWeatherMap polling |
| `handleButtons()` | `loop()` | Button edge detection and dispatch |
| `updateLEDs()` | `loop()` | Status LED logic |
| `drawClock()` / `drawWeather()` / `drawMochi()` / `drawNotificationScreen()` | `loop()` | Per-screen rendering |
| `drawPopupOverlay()` | `loop()` | Notification popup rendering |
| `updatePhysics()` | `drawMochi()` | Eye animation state update |
| `buzzIntro()` / `buzzScreenChange()` / `buzzEmotion()` / `buzzNotification()` | `setup()`, `handleButtons()`, `pushNotification()` | Buzzer melodies |

## Possible Improvements

- MQTT reconnect backoff (noted above)
- Non-blocking frame timing (noted above)
- `espClient.setInsecure()` skips TLS certificate validation — see [`CONFIGURATION.md`](CONFIGURATION.md#mqtt) for the trade-off
- No in-firmware "factory reset" beyond WiFi credentials — see [`USER_MANUAL.md`](USER_MANUAL.md#factory-reset)
- The notification buffer isn't persisted across reboots, so a restart clears notification history
