# User Manual

## Introduction

This manual covers day-to-day use of DeskBuddy once it's built and flashed. For assembly, see [`hardware/WIRING.md`](../hardware/WIRING.md); for flashing instructions, see the main [README](../README.md).

## First Boot

On first power-up (or after a full flash with no saved configuration), DeskBuddy has no WiFi network stored and no `config.json` in flash. It boots straight into WiFi setup mode via WiFiManager:

1. The device creates its own WiFi access point named **DeskBuddy_Setup**.
2. Connect to it from your phone or laptop.
3. A captive portal should open automatically (or open a browser and navigate to `192.168.4.1`).
4. Select your home WiFi network and enter its password.
5. The device connects, saves the credentials internally (via WiFiManager, separately from DeskBuddy's own config file), and reboots into normal operation using the default configuration (default weather city, default HiveMQ broker address, HTTP notifications enabled on port 80 at `/notify`).

## Connecting WiFi

Once WiFiManager has a saved network, DeskBuddy connects automatically on every boot. If it can't connect (wrong password, network out of range, router replaced), it will restart and re-enter the **DeskBuddy_Setup** access point again automatically.

## Configuration Portal

This is separate from the WiFi setup above — it's DeskBuddy's own settings page for weather, MQTT, and HTTP notification options.

**To enter:** hold **BTN2** while powering on or resetting the device, and keep holding for about half a second after boot begins.

1. The OLED displays `CONFIG MODE` along with connection instructions.
2. Connect your phone or laptop to the **DeskBuddy_Config** WiFi network (open, no password).
3. Open a browser to `http://192.168.4.1`.
4. You'll see a form with fields for weather city, OpenWeatherMap API key, MQTT broker/port/username/password/topic, HTTP notification port/endpoint, and a silent-mode checkbox.
5. Tap **Save & Restart** to write the settings to flash and reboot into normal operation, or use the **Reset WiFi Network** button on the same page to forget the saved WiFi network and restart into WiFi setup mode instead.

## Changing Weather

Enter the configuration portal (above) and update the **Weather City** and/or **OpenWeatherMap API Key** fields, then save. DeskBuddy fetches fresh weather data immediately on the next boot and refreshes automatically every 10 minutes after that.

## MQTT Setup

DeskBuddy connects to an MQTT broker over TLS (port 8883 by default) to receive forwarded phone notifications. To configure:

1. Enter the configuration portal.
2. Set **MQTT Broker**, **MQTT Port**, **MQTT Username**, **MQTT Password**, and **MQTT Topic**.
3. Save and let the device restart.

**Important:** DeskBuddy will not attempt to connect to MQTT at all unless both a username and password are set — if either is blank, the connection attempt is skipped entirely (this avoids spamming a broker with anonymous connection attempts). If you're using a broker that allows anonymous connections, you'll still need to fill in something in both fields for the firmware's current logic to proceed; this is a known limitation, tracked in [`GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md).

See [`docs/CONFIGURATION.md`](CONFIGURATION.md) for the expected MQTT message format.

## HTTP Notifications

As an alternative (or addition) to MQTT, DeskBuddy exposes a simple HTTP endpoint for notifications, enabled by default:

```
GET http://<device-ip>/notify?title=Mother&msg=Dinner+is+ready
```

This shows "Mother" / "Dinner is ready" as a popup and adds it to the notification history, exactly like an MQTT-forwarded notification. You can change the port and path (default `/notify`) from the configuration portal, or disable it entirely by unchecking **Enable HTTP Notifications**.

## Android Automate / MacroDroid Setup

DeskBuddy is designed to pair with **MacroDroid** on Android:

1. Create a new macro with the trigger **Notification Received** (optionally filtered to specific apps).
2. Add an action to send either:
   - An **HTTP GET Request** to `http://<device-ip>/notify?title=[App]&msg=[Text]`, or
   - An **MQTT Publish** action (if using a plugin that supports it) with a JSON payload: `{"app":"WhatsApp","title":"John","body":"Hey what's up"}`
3. Map MacroDroid's notification variables (title, text, app name) into the request.

A full macro walkthrough with screenshots is planned — see [`GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md) if you'd like to contribute one.

## Changing Settings

All settings changes go through the configuration portal (hold BTN2 at boot). There is currently no way to change weather, MQTT, or HTTP settings without physically accessing the device to trigger config mode — this is a deliberate simplicity trade-off for v1.0.

## Factory Reset

There are two levels of reset available:

- **WiFi reset only:** In the configuration portal, tap **Reset WiFi Network**. This forgets the saved WiFi network and restarts into WiFi setup mode. Your weather/MQTT/HTTP settings in `config.json` are untouched.
- **Full configuration reset:** There is currently no in-device button or menu option to wipe `config.json` back to firmware defaults. To fully reset, reflash the device with **Erase Flash** enabled in your upload settings (Arduino IDE: Tools → Erase Flash → "All Flash Contents"), which clears LittleFS along with everything else. This is a known gap — see [`GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md) for an open issue to add a proper in-portal factory reset.

## Buttons

| Button | Action | Effect |
|---|---|---|
| BTN1 | Short press, on Clock/Weather/Mochi screens | Cycles Mochi's mood (15 total) and plays that mood's buzzer tone |
| BTN1 | Short press, on Notification screen | Scrolls to the next notification in history |
| BTN2 | Short press (under 2 seconds) | Cycles screens: Clock → Weather → Mochi → Notifications → Clock |
| BTN2 | Long press (2 seconds or more) | Toggles silent mode and saves it immediately to flash |
| BTN2 | Held during power-on/reset | Enters the configuration portal instead of booting normally |

## LED Meanings

| LED | Color | Meaning |
|---|---|---|
| STATUS_LED | 🔴 Red | Solid ON while silent/DND mode is active. Off otherwise. |
| NOTIFY_LED | 🟡 Yellow | ON whenever there's at least one unread notification. Turns off automatically the moment you view the Notifications screen. |
| EMOTION_LED | 🟢 Green | Solid ON when WiFi and MQTT are both connected. Blinks (roughly twice a second) when WiFi is connected but MQTT isn't. Off when there's no WiFi connection. |

## Buzzer Meanings

| Event | Sound | Plays Even in Silent Mode? |
|---|---|---|
| Boot complete | Full intro jingle | Yes (only happens once, on boot) |
| Screen change (BTN2 short press) | Two-note chirp | Yes |
| Mood change (BTN1 press) | Mood-specific short melody (15 variants) | No |
| New notification (MQTT or HTTP) | Two-tone alert | No |
| Turning silent mode **off** | Two-tone alert (same as notification sound) | N/A — this is the confirmation that silent mode is now off |
| Turning silent mode **on** | Nothing | N/A — the alert sound is itself suppressed by the mode you just turned on |

## Weather Screen

Shows city name, current temperature (custom pixel digits), a matching weather icon (sun/cloud/rain/thunder/snow/mist), feels-like temperature, humidity as both a number and a bar graph, and wind speed. Data refreshes automatically every 10 minutes and immediately after any configuration change that requires a reboot.

## Clock Screen

Shows the day and date, a large 12-hour time display with AM/PM, and a progress bar along the bottom that fills over the course of each minute. Time is synced via NTP after WiFi connects.

## Notification Screen

Shows the currently selected notification's app name, title, and body, along with a `current/total` counter in the bottom-right corner. Press BTN1 while on this screen to scroll to the next notification (wraps back to the first after the last). Viewing this screen clears the unread indicator (yellow LED).

## Silent Mode

Long-press BTN2 (2+ seconds) to toggle silent mode on or off. While active, mood-change and notification sounds are suppressed, but screen-change chirps and notifications themselves (popup + display + yellow LED) still work normally — silent mode mutes the buzzer, not the notification pipeline. The setting is saved to flash immediately, so it persists across reboots and power loss.

## Troubleshooting

See [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) for structured, symptom-first debugging steps.

## Frequently Asked Questions

See [`FAQ.md`](FAQ.md) for common questions and quick answers.
