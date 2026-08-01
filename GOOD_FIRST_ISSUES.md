# Good First Issues

New to DeskBuddy or to embedded development in general? Start here. These are scoped to be approachable without needing to understand the entire codebase first. Pick one, comment on the corresponding GitHub issue (or open one referencing this list) to claim it, and see [`CONTRIBUTING.md`](CONTRIBUTING.md) for workflow details.

Difficulty legend: 🟢 Easy · 🟡 Medium · 🔴 Advanced (still beginner-approachable, just more moving parts)

---

## Display & Animation

1. 🟢 **Add a new Mochi mood** — design a new facial expression (e.g. "confused" or "excited") following the pattern of existing moods in the eye-drawing functions.
2. 🟢 **New weather icon variant** — add a pixel-art icon for a weather condition not currently covered (e.g. haze vs. mist as distinct icons).
3. 🟡 **Idle mood auto-cycling** — make Mochi occasionally switch moods on its own after a period of inactivity on the face screen.
4. 🟡 **Screen transition animation** — add a simple wipe or fade effect when cycling between screens instead of a hard cut.
5. 🟡 **Notification popup animation** — animate the popup overlay sliding in/out instead of appearing instantly.
6. 🟢 **Add a boot splash animation** — a short animated sequence (a few frames) shown before the clock screen on first boot.
7. 🟡 **Battery indicator icon** — design a small battery glyph for a future battery-powered build (icon only; wiring is a separate, harder issue).
8. 🟢 **12h/24h time format toggle** — add a config option to switch the clock screen between 12-hour and 24-hour display.
9. 🟡 **Weekly forecast mini-screen** — extend the weather screen (or add a new one) showing a 2–3 day outlook.
10. 🟢 **Improve humidity/wind bar graphics** — polish the existing bar-graph rendering on the weather screen for better readability.

## Notifications & Integrations

11. 🟡 **Telegram bot notification source** — add support for receiving notifications via a Telegram bot as an alternative to MacroDroid.
12. 🟡 **Discord webhook notification source** — similar to above, but sourcing from a Discord webhook.
13. 🟢 **Per-app notification icons** — show a small glyph based on the `app` field (e.g. a distinct icon for WhatsApp vs. SMS vs. generic).
14. 🟡 **Notification priority levels** — allow certain apps/keywords to bypass silent mode (e.g. always alert for calls).
15. 🟢 **"Mark all read" gesture** — add a long-press or double-press shortcut to clear all unread notifications at once.
16. 🟡 **Notification history persistence** — optionally persist the last few notifications to LittleFS so they survive a reboot.
17. 🟢 **Document the MacroDroid macro setup** — write a step-by-step guide (with screenshots) for configuring MacroDroid to forward notifications.
18. 🔴 **iOS Shortcuts integration** — investigate and document a path for iPhone users to send notifications via the HTTP endpoint using Shortcuts automation.

## Connectivity & Reliability

19. 🟡 **WiFi signal strength indicator** — surface RSSI somewhere on a screen or via Serial for debugging weak connections.
20. 🟡 **MQTT last-will message** — configure a proper MQTT LWT so other systems know when DeskBuddy goes offline.
21. 🟢 **Improve config portal input validation** — reject obviously invalid values (empty MQTT broker, out-of-range port numbers) before saving.
22. 🔴 **OTA firmware updates** — add `ArduinoOTA` support so future flashes don't require a USB cable.
23. 🟡 **Config export/import** — add a way to download the current config as JSON from the web portal, and re-upload it (handy for multiple devices).
24. 🟡 **NTP server fallback** — allow a secondary NTP server if the primary is unreachable.

## Hardware & Power

25. 🟡 **Battery monitoring** — read battery voltage via ADC and surface it on a status screen (requires basic hardware addition).
26. 🔴 **Power optimization pass** — investigate light-sleep modes between frame renders to reduce power draw.
27. 🟢 **Document alternate OLED wiring** — add notes for SSD1306-based displays as an alternative to SH1106, if pin-compatible.
28. 🔴 **ESP32 portability audit** — identify which parts of the firmware would need to change to support ESP32 boards, and document findings (implementation can be a follow-up issue).
29. 🟢 **3D-printable enclosure** — design and share a basic enclosure (STL files) sized for the current component layout.

## Documentation & Tooling

30. 🟢 **Add photos of a real build** — if you've built a DeskBuddy, contribute photos for the README's screenshot section.

---

## Claiming an Issue

1. Comment on the relevant GitHub issue (or open one referencing the item number from this list, e.g. "Good First Issue #14").
2. Wait for a maintainer to confirm it's not already in progress.
3. Fork, branch, and follow [`CONTRIBUTING.md`](CONTRIBUTING.md) for the rest of the workflow.

If none of these fit what you're interested in, open a new issue — this list isn't exhaustive, just a starting point.
