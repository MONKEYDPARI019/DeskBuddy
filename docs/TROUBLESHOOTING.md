# Troubleshooting

A structured, symptom-first guide. For a broader Q&A format, see [`FAQ.md`](FAQ.md).

## Boot Issues

**Device doesn't power on / no Serial output at all**
- Confirm the USB cable supports data, not just power (a charge-only cable will power the board but you'll see nothing useful, and in some cases the board won't boot reliably at all).
- Try a different USB port or power source.
- Confirm Serial Monitor is set to **115200 baud** — the wrong baud rate shows garbled or no output even though the device is actually running fine.

**Device resets repeatedly / boot-loops**
- Check power supply quality first — see the capacitor notes in [`hardware/WIRING.md`](../hardware/WIRING.md); insufficient current delivery during WiFi transmit bursts is the most common cause.
- Confirm nothing unexpected is wired to D3, D4, or D8 — these are boot-strapping GPIOs (see [`hardware/PINOUT.md`](../hardware/PINOUT.md)), and an unexpected pull state on them at power-on can prevent a clean boot.

**Device boots into WiFi setup mode every time, even though I configured it before**
- WiFiManager's saved credentials may have been cleared (e.g. via the config portal's "Reset WiFi Network" button, intentionally or not) — reconnect through the **DeskBuddy_Setup** access point again.

## OLED Blank

1. Confirm wiring: SDA → D2, SCL → D1, VCC → 3V3, GND → GND. A swapped SDA/SCL pair is a common mistake and will leave the display completely unresponsive.
2. Confirm the display is genuinely an SH1106 module — an SSD1306 module will not initialize correctly with this firmware's display driver.
3. Check Serial output at boot for any I2C-related errors or hangs during `display.begin()`.
4. If the display worked before and stopped, check for a loose connection rather than assuming a firmware issue — physical connections on breadboards are the most common failure point.

## Weather Not Updating

1. Confirm WiFi is actually connected (check Serial output or the green LED — it won't be lit at all if there's no WiFi).
2. Confirm the OpenWeatherMap API key entered in the config portal is valid and active.
3. Confirm the configured city name is one OpenWeatherMap recognizes — try a more specific query (e.g. adding a country code) if a common city name is ambiguous.
4. Remember the automatic refresh interval is 10 minutes — a stale-looking screen shortly after a config change is expected until the next fetch cycle (or the next reboot, which fetches immediately).

## MQTT Disconnected

1. Check Serial output — every reconnect attempt logs the broker, port, username, and topic being used, along with a specific failure reason if the connection fails.
2. Confirm both **MQTT Username** and **MQTT Password** are filled in — the firmware skips connecting entirely if either is blank, and this produces no error message, just silence.
3. Confirm the broker address and port are correct — port 8883 for TLS brokers like the default HiveMQ Cloud setup.
4. If you see `rc=4` (`MQTT_CONNECT_BAD_CREDENTIALS`) or `rc=5` (`MQTT_CONNECT_UNAUTHORIZED`), the broker is reachable but is rejecting your credentials — verify them directly against your broker's dashboard.
5. If you see `rc=3` (`MQTT_CONNECT_UNAVAILABLE`), the broker itself may be unreachable — check the broker address for typos and confirm the broker is actually running.

## HTTP Timeout / HTTP Notifications Not Working

1. Confirm **Enable HTTP Notifications** is checked in the config portal.
2. Confirm you're using the correct IP address (check Serial output at boot, which prints the device's IP) and the correct port/path combination configured in the portal.
3. Confirm the device sending the request (phone, script, MacroDroid) is on the same local network as DeskBuddy — the HTTP endpoint isn't exposed to the wider internet.
4. A `403` response means the endpoint is reachable but `http_enabled` is false — this is expected behavior, not a bug.

## Notification Not Showing

1. Determine whether the notification was expected to arrive via MQTT or HTTP, and troubleshoot that specific path using the sections above.
2. For MQTT: confirm the publishing topic exactly matches the configured `mqtt_topic` (case-sensitive).
3. For MQTT: confirm the published payload is valid JSON with `app`, `title`, and `body` fields — malformed JSON still produces a notification (the raw message becomes the body), but a completely empty or unparseable payload may not look like what you expected.
4. Check that the notification screen hasn't already been viewed — the popup only shows for 4 seconds, but the notification itself remains in history until the buffer fills past 8 entries.

## Configuration Portal Missing

**Holding BTN2 at boot doesn't enter config mode**
1. Confirm you're holding BTN2 down as power is applied or the device is reset, not after it has already finished booting — the check happens early in `setup()`, once.
2. Confirm BTN2 is wired correctly (D7 to GND through the button) — test it during normal operation first (short-press should cycle screens) to confirm the button itself works.
3. There's a 500ms debounce delay built into the check — a very brief, accidental touch may not register as a genuine hold.

## LittleFS Errors

**"LittleFS Mount Failed" in Serial output**
- The firmware automatically formats and re-mounts LittleFS when this happens, so it isn't fatal — but it does mean any previously saved configuration is gone, and the device falls back to firmware defaults.

**"Failed to parse config file" in Serial output**
- `/config.json` exists but isn't valid JSON — this can happen if a write was interrupted (e.g. power loss mid-save). The firmware falls back to defaults for that boot; re-saving your settings through the config portal will overwrite the corrupted file with a valid one.

**Settings don't persist across reboots**
- Confirm you're actually reaching the "Settings Saved!" confirmation page after submitting the config portal form — if the device resets or loses power before that confirmation, the write may not have completed.

## Memory Issues

**Random crashes or erratic behavior under normal use**
- Check Serial output for anything unusual right before the crash — this firmware is written to avoid heap fragmentation (see [`CONCEPTS.md`](CONCEPTS.md#heap-vs-stack)) by using fixed-size buffers throughout, so a memory-related crash is more likely tied to a specific modification than the baseline firmware.
- If you've modified the code and added `String` concatenation in a frequently-called function (e.g. inside `loop()` or a notification handler), that's the first place to look — repeated dynamic string operations are the most common source of heap fragmentation on this platform.

Still stuck? Check [`FAQ.md`](FAQ.md) for additional context, or open an issue with your Serial monitor output attached.
