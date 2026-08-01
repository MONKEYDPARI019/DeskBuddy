# Frequently Asked Questions

## General

**1. What is DeskBuddy?**
A small ESP8266-based desk display that shows the time, weather, and phone notifications, so you don't need to pick up your phone during focused work. See the [README](../README.md) for the full feature list.

**2. Do I need to know how to code to build one?**
No coding is required to build and flash the firmware as-is — you'll need to install the Arduino IDE, some libraries, and follow the [README](../README.md#installation) instructions. Coding knowledge helps if you want to modify behavior.

**3. Does DeskBuddy work with iPhone?**
The primary notification-forwarding path (MacroDroid) is Android-only. iPhone users could potentially use the HTTP endpoint via Shortcuts automation, but this isn't documented or tested yet — see [`GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md).

**4. Can I use this without an internet connection?**
No. WiFi is required for the clock (NTP time sync), weather, and both notification paths (MQTT and HTTP both need the phone and device to communicate over a network).

**5. Is my data sent anywhere besides my own devices?**
Weather requests go to OpenWeatherMap's API. MQTT notifications go through your configured broker (HiveMQ Cloud by default, though you can point it at your own broker). HTTP notifications go directly from your phone to the device on your local network. Nothing is sent to any DeskBuddy-specific service, since there isn't one.

## Compilation Issues

**6. I get "library not found" errors when compiling. What's missing?**
Install `PubSubClient`, `U8g2`, `WiFiManager` (by tzapu), and `ArduinoJson` through the Arduino IDE's Library Manager. `LittleFS` and `ESP8266WebServer` come bundled with the ESP8266 board core, so you shouldn't need to install those separately.

**7. Compilation fails with errors about `LittleFS.h` not found.**
Make sure you've selected an ESP8266 board (not AVR/Uno/etc.) under Tools → Board, and that the ESP8266 board core itself is installed via Boards Manager — `LittleFS.h` comes from the core, not a separately installed library.

**8. I get warnings about deprecated SPIFFS during compilation.**
These are typically harmless and come from a dependency, not DeskBuddy's own code (which uses LittleFS, not SPIFFS). If compilation completes successfully, you can ignore them.

**9. The sketch is too large for the selected board.**
Double check your board is set to a D1 Mini / NodeMCU-class ESP8266 with a standard flash size (4MB), and that your "Flash Size" build setting allocates enough space to the sketch (rather than a large SPIFFS/LittleFS partition you're not using).

**10. Upload fails with a timeout or "failed to connect" error.**
This is almost always a cable, port, or driver issue rather than a code issue — try a different USB cable (must support data, not just power), confirm the correct COM port is selected, and make sure you're not holding any buttons during the upload itself.

## WiFi Issues

**11. DeskBuddy won't create the DeskBuddy_Setup access point.**
Confirm it's actually a fresh device (no saved WiFi credentials) — if WiFiManager already has a saved network, it won't open the setup AP; it'll just try to connect. Use the config portal's "Reset WiFi Network" button, or hold BTN2 at boot to reach the portal if the device did connect.

**12. It connects to WiFi but the clock never shows the right time.**
Time sync depends on reaching an NTP server (`pool.ntp.org` by default) over UDP port 123 after WiFi connects. Check that your network doesn't block outbound UDP on that port.

**13. WiFi keeps disconnecting randomly.**
Usually a signal strength or interference issue rather than a firmware issue — try moving the device closer to your router, and check for 2.4GHz interference sources (the ESP8266 doesn't support 5GHz).

**14. Can I hardcode WiFi credentials instead of using the setup portal?**
Not directly in the current firmware — WiFiManager handles WiFi credentials outside of DeskBuddy's own `config.json`. You could bypass this by calling `WiFi.begin(ssid, password)` directly in a modified `setup()`, but that's a firmware change, not a configuration option.

## Notification Issues

**15. Notifications aren't showing up at all.**
First confirm whether you're using MQTT or HTTP. For MQTT, check that both a username and password are set — the firmware skips connecting entirely if either is blank. For HTTP, confirm `http_enabled` is true and you're hitting the right IP/port/path.

**16. MQTT connects but I never receive notifications.**
Check that the topic your phone (via MacroDroid) is publishing to exactly matches `mqtt_topic` in the config — MQTT topics are case-sensitive and must match exactly.

**17. The notification popup doesn't show the full message.**
The body field is capped at 63 characters (plus a null terminator) in the current firmware — longer messages are truncated. This is a fixed buffer size, not adjustable without a firmware change.

**18. My notification's title/body has weird characters or looks broken.**
The display font used for notifications doesn't include a full Unicode character set — emoji and many non-Latin characters won't render correctly. Plain text works best.

**19. Old notifications are gone after I restart the device.**
Correct — the notification buffer lives in RAM only and isn't saved to flash, so it resets on every reboot.

**20. Can I get more than 8 notifications in history?**
Not without a firmware change — `MAX_NOTIFS` is currently fixed at 8. Increasing it is possible but adds to RAM usage.

## Weather Issues

**21. The weather screen just says "Fetching Weather..." forever.**
Usually means the OpenWeatherMap request is failing — check your API key is valid and active (new OpenWeatherMap keys can take a little time to activate after signup), and that the city name is spelled in a way OpenWeatherMap recognizes.

**22. Weather shows the wrong city.**
Update the city name in the configuration portal — OpenWeatherMap sometimes needs a more specific query (e.g. "Springfield,US" instead of just "Springfield") if there are multiple cities with the same name.

**23. Temperature units are wrong (showing Fahrenheit when I want Celsius, or vice versa).**
Set `owm_units` to `"metric"` (Celsius) or `"imperial"` (Fahrenheit) in the configuration portal.

**24. Weather only updates once and never again.**
Check the Serial monitor for repeated fetch failures after the first successful one — a bad API key or rate limit would cause the first request (right after boot) to sometimes succeed differently from later ones only if your key/quota state changed in between, which is unusual; more likely, all requests are failing and you just didn't notice the first one already showed stale data.

## MQTT Issues

**25. MQTT never connects, and I don't see any connection attempt in Serial.**
This is expected behavior if `mqtt_user` or `mqtt_pass` is empty — the firmware intentionally skips the connection attempt entirely rather than trying anonymous auth. Fill in both fields.

**26. MQTT fails with "Bad username/password" (rc=4 / `MQTT_CONNECT_BAD_CREDENTIALS`).**
Double-check credentials in the config portal for typos, and confirm the account/credentials are active on your broker (HiveMQ Cloud clusters can have credentials that need to be explicitly created per-cluster).

**27. MQTT fails with "Server unavailable" (rc=3 / `MQTT_CONNECT_UNAVAILABLE`).**
Usually means the broker address or port is wrong, or the broker is temporarily down. Confirm you're using port 8883 for TLS brokers like HiveMQ Cloud.

**28. Is my MQTT connection actually secure?**
It's encrypted (TLS), but the firmware uses `setInsecure()`, meaning it doesn't verify the broker's certificate against a trusted authority. See [`CONFIGURATION.md`](CONFIGURATION.md#mqtt) for the reasoning and trade-offs.

## Display Issues

**29. The OLED stays completely blank.**
Check I2C wiring (SDA/SCL not swapped), confirm the display is actually an SH1106 (an SSD1306 module will not work correctly with this firmware's display driver without a code change), and check the display's power connection.

**30. The display shows garbled or scrambled pixels.**
Usually an I2C wiring or power issue — check for a loose connection, and make sure you're not running long, unshielded I2C wires that pick up noise.

**31. Mochi's face is missing a mouth or looks incomplete.**
If you're running a firmware version older than the audited release described in this repository's [`CHANGELOG.md`](../CHANGELOG.md), this was a known bug (missing `drawMouth()` call) — update to the current firmware.

**32. The clock's minute progress bar resets or jumps unexpectedly.**
This is expected right after an NTP resync if the clock had drifted — the progress bar is derived from the current second value.

## Buttons & LEDs

**33. My button presses aren't registering.**
Confirm the button is wired between the GPIO and GND (not 3V3) — the firmware relies on `INPUT_PULLUP`, meaning a press should pull the pin LOW.

**34. Both LEDs and buttons work, but the device resets when I press a button.**
Check that you haven't wired anything unexpected onto D3, D4, or D8 — these are boot-strapping pins on the ESP8266 and are sensitive to unexpected pull states. See [`hardware/PINOUT.md`](../hardware/PINOUT.md).

**35. Silent mode doesn't seem to mute everything.**
By design — silent mode mutes mood-change and notification buzzer tones, but not the screen-change chirp, and not the notification popup/display itself. See [`USER_MANUAL.md`](USER_MANUAL.md#silent-mode).

Didn't find your question here? Check [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) for a more structured, symptom-first guide, or open an issue.
