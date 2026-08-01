# Assets

This folder holds the images and media referenced elsewhere in the repository (primarily the main [README](../README.md)). No image files are included yet — this document exists so contributors know exactly what's expected and can drop in correctly-named files without needing to hunt through other docs for the right filename.

## Expected Files

| Filename | Used In | Description | Suggested Size |
|---|---|---|---|
| `banner.png` | README.md (top banner) | Wide project banner — logo/name treatment, doesn't need to show the device itself | 1200×400px (3:1) |
| `demo.gif` | README.md (demo section) | Short screen-recording or phone-camera capture of DeskBuddy cycling through its screens, ideally showing a notification arrive | Under 5MB if possible for fast loading on GitHub; 600px wide is plenty |
| `hardware.jpg` | README.md / docs | Photo of the bare wiring — ESP8266, OLED, LEDs, buttons, and buzzer connected on breadboard or perfboard, before any enclosure | Any reasonable photo resolution, landscape orientation preferred |
| `clock.jpg` | README.md (screenshots) | Photo or close-up of the Clock screen actively displayed on the OLED | Close enough to read the display clearly |
| `weather.jpg` | README.md (screenshots) | Photo or close-up of the Weather screen actively displayed | Same as above |
| `notification.jpg` | README.md (screenshots) | Photo or close-up of a notification popup or the Notifications screen in use | Same as above |
| `configportal.jpg` | docs/CONFIGURATION.md or README.md | Screenshot of the configuration portal web page, taken from a phone or browser | Standard mobile screenshot resolution is fine |
| `logo.png` | (optional) | A small square logo/icon version, useful for favicons or smaller placements | 256×256px, transparent background preferred |

## Guidelines

- **Photos over renders.** Real photos of an actual build are more useful and more credible than mockups — this is a hobbyist hardware project, not a product page.
- **Show the display clearly.** OLED screens can be tricky to photograph (glare, low contrast in photos) — a slight angle and good lighting helps far more than a high-resolution camera.
- **GIFs should be short.** 5–10 seconds cycling through a couple of screens communicates the idea better than a longer clip, and keeps the file size reasonable for a GitHub-hosted README.
- **Keep filenames exactly as listed above.** The README and other docs reference these exact filenames — using a different name means updating the Markdown links too.

## Contributing Assets

If you've built a DeskBuddy and want to contribute photos, this is one of the easiest ways to help the project — see [`GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md) for the relevant entry, or just open a PR adding your files here with the correct filenames.
