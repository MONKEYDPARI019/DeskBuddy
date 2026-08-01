# Concepts

This document explains every underlying concept the DeskBuddy firmware relies on, aimed at someone who understands basic C/C++ but hasn't necessarily worked with embedded systems or IoT before. Each section covers what the concept is, and specifically why it's used here rather than as a generic definition.

## ESP8266

A low-cost microcontroller with built-in WiFi, popular for hobbyist IoT projects. Unlike a general-purpose computer, it runs one program in a loop with no operating system managing multiple processes — when DeskBuddy's `loop()` function is drawing the display, nothing else is happening at the same time on the chip. This single-threaded nature is why blocking operations (like an HTTP request during weather fetch) briefly pause everything else, including animation.

**Why it's used here:** it's cheap, has built-in WiFi (required for weather/MQTT/notifications), and has just enough RAM and flash to run this feature set without needing anything larger.

## OLED (Organic LED Display)

A display technology where each pixel emits its own light, rather than being backlit like an LCD. This means "off" pixels draw essentially no power and contrast is very high, since there's no backlight bleeding through dark areas.

**Why it's used here:** low power draw and high contrast matter more for a small always-on status display than color does.

## I2C

A two-wire communication protocol (one clock line, one data line) used to talk to peripheral chips. Multiple devices can share the same two wires, each identified by an address, though DeskBuddy only has one device (the OLED) on its bus.

**Why it's used here:** it needs far fewer pins than the alternative (SPI), which matters when a lot of other things (buttons, LEDs, buzzer) are also competing for a limited number of GPIOs.

## LittleFS

A filesystem designed for flash memory on embedded devices, exposed through a familiar file-open/read/write/close API. DeskBuddy uses it to store `config.json`, so settings survive power loss and firmware updates (as long as flash isn't explicitly erased).

**Why it's used here:** without it, every configuration value would have to be hardcoded and baked into each firmware build — LittleFS turns configuration into something the device manages itself.

## HTTP

The protocol web browsers and most internet-connected devices use to request and send data. DeskBuddy both makes HTTP requests (to OpenWeatherMap, as a client) and receives them (on its `/notify` endpoint and config portal, as a server).

**Why it's used here:** it's simple, universally supported, and doesn't require a persistent connection — well suited to occasional weather polling and one-off notification pushes.

## REST

A common style of designing HTTP APIs around resources and standard verbs (GET, POST, etc.). DeskBuddy's HTTP notification endpoint is REST-*ish* in spirit — a single GET endpoint with query parameters — rather than a strict REST implementation with multiple resource types and full verb usage. Worth knowing the term, but don't expect a textbook REST API here.

## MQTT

A lightweight publish/subscribe messaging protocol built for constrained devices and unreliable networks. A device subscribes to a "topic," and any message published to that topic by anyone (a phone, a script, another device) gets delivered to every subscriber.

**Why it's used here:** unlike DeskBuddy's HTTP endpoint, MQTT keeps a persistent connection open to a broker, which means the broker can hold and deliver messages, and either intermediate hops can drop and reconnect independently. Since the point of DeskBuddy is receiving notifications reliably from a phone that might be on a flaky mobile connection, MQTT's persistent, broker-mediated model is a better fit than the phone needing to reach the ESP8266 directly over HTTP (which also requires the phone and DeskBuddy to be on a network where the phone can reach the device's IP — not always true on mobile data).

## JSON

A lightweight, human-readable data format for structured data — objects with named fields, arrays, strings, numbers, booleans. DeskBuddy uses it in three places: `config.json` on disk, the OpenWeatherMap API response, and MQTT notification payloads.

**Why it's used here:** it's the de facto standard for this kind of structured data exchange, and the `ArduinoJson` library makes parsing it on a constrained device straightforward without writing a custom parser.

## Web Server

A program that listens for incoming HTTP requests and responds to them. DeskBuddy runs one (via `ESP8266WebServer`) in two different contexts: the configuration portal (only active in config mode) and the HTTP notification endpoint (active during normal operation).

**Why it's used here:** it's how both the phone-facing configuration form and the notification endpoint are exposed to the network — the alternative would be a fixed, unchangeable configuration or some other protocol entirely.

## WiFiManager

A library that handles the "how does this device get onto my WiFi network in the first place" problem, without hardcoding credentials into the firmware. It works by having the device create its own temporary access point with a captive portal for network selection, then remembering the chosen network afterward.

**Why it's used here:** without it, connecting to a new WiFi network would mean reflashing the firmware with new hardcoded credentials every time — not practical for something meant to be built by other people on their own networks.

## OpenWeatherMap

A third-party API that provides current weather conditions for a given city name. DeskBuddy calls its free-tier `/weather` endpoint once at boot and every 10 minutes afterward.

**Why it's used here:** it's free for this usage level, well-documented, and returns exactly the fields (temperature, humidity, wind, description, icon code) the weather screen needs.

## NTP (Network Time Protocol)

A protocol for synchronizing a device's clock against internet time servers. DeskBuddy calls `configTime()` once, after WiFi connects, pointed at `pool.ntp.org`.

**Why it's used here:** the ESP8266 has no real-time clock hardware of its own that persists through power loss — without NTP, the clock screen would have no idea what time it actually is.

## Animation

DeskBuddy's Mochi face isn't a set of static images — eye positions, pupil offsets, and blink states are calculated continuously as floating-point values and redrawn every frame, rather than switching between a fixed set of pre-drawn frames.

**Why it's used here:** continuous, physics-based movement (eyes drifting toward a target position, smoothly rather than snapping) reads as more "alive" than a small number of static sprite frames would, at very little extra CPU cost on top of what's already being redrawn each frame.

## Timers (Non-Blocking)

Rather than using `delay()` to wait for things (which would freeze the whole device, including animation and button reading), most of DeskBuddy's time-based behavior is implemented by comparing the current time against a stored "last happened at" timestamp, every loop iteration.

**Why it's used here:** it's the standard pattern for keeping a single-threaded loop responsive — the weather refresh timer, the blink timer, and the popup-dismiss timer are all implemented this way instead of with a blocking wait.

## `millis()`

Returns the number of milliseconds since the device booted, as an unsigned 32-bit integer. It's the building block for every non-blocking timer in the firmware (`if (millis() - lastEvent > interval)`), and for things like the minute-progress bar on the clock screen and the popup overlay's 4-second display window.

One subtlety worth knowing: `millis()` overflows (wraps back to zero) after about 49 days of continuous uptime. DeskBuddy's subtraction-based comparisons (`millis() - lastEvent`) handle this correctly due to how unsigned integer overflow works in C++, but it's worth understanding if you're adding new timers.

## State Machines

The screen-cycling logic (`ScreenMode` enum: Clock → Weather → Mochi → Notifications → back to Clock) is a small state machine — the device is always in exactly one of a fixed set of states, and a well-defined trigger (BTN2 short press) moves it to the next one.

**Why it's used here:** an enum plus a single "current mode" variable is a simple, readable way to manage "what am I showing right now" without needing a more complex framework — appropriate for a small, fixed number of screens.

## Memory Optimization

The ESP8266 has roughly 80KB of usable RAM shared between everything the firmware needs — string buffers, the display frame buffer, the WiFi stack, TLS state for MQTT, and more. Running out causes crashes or corrupted behavior, not a graceful "out of memory" error in most cases. This is why DeskBuddy leans on fixed-size `char` buffers instead of dynamically-growing `String` objects in most of the notification and networking code — see [Heap vs Stack](#heap-vs-stack) below.

## PROGMEM

A keyword/macro that tells the compiler to leave a constant in flash memory rather than copying it into RAM at startup. DeskBuddy's web portal HTML (`WEB_HEADER`, `WEB_FOOTER`) is stored this way, since that HTML is relatively large and never changes at runtime.

**Why it's used here:** flash is much more plentiful than RAM on this chip — moving large, unchanging data into flash frees up RAM for everything else.

## `F()`

A macro that wraps a string literal so it's also kept in flash instead of RAM, used throughout the firmware for `Serial.println(F("..."))` calls and small HTML fragments. It's a lighter-weight sibling of PROGMEM for individual string literals rather than large blocks.

**Why it's used here:** DeskBuddy has a lot of Serial debug logging and small HTML snippets; wrapping all of them in `F()` avoids permanently consuming RAM for text that's only needed transiently (or only for debugging).

## Heap vs Stack

The **stack** holds local variables with a size known at compile time, and is automatically reclaimed when a function returns. The **heap** holds dynamically-sized or dynamically-allocated data (like a `String` that grows as you append to it), and has to be explicitly managed — or, in the case of repeated `String` concatenation, can fragment over time as pieces are allocated and freed in a way that leaves unusable gaps.

**Why it matters here:** the notification and web-form handling code favors fixed-size stack buffers (`char title[24]`, `strlcpy(...)`) over heap-allocated `String` concatenation specifically to avoid that fragmentation risk — on a device with only tens of kilobytes of RAM and no MMU to work around it, heap fragmentation can eventually cause allocation failures even when there's technically enough total free memory, just not in one contiguous block.
