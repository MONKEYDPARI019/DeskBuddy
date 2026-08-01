# Configuration Guide

This document covers everything about how DeskBuddy stores and applies its configuration. For a usage-focused walkthrough of the config portal, see [`USER_MANUAL.md`](USER_MANUAL.md#configuration-portal).

## WiFi Setup

WiFi credentials are handled separately from the rest of DeskBuddy's configuration, using the **WiFiManager** library. On boot, if no WiFi network is saved (or the saved network can't be reached), the device opens its own access point — **DeskBuddy_Setup** — and serves WiFiManager's own captive portal for selecting a network and entering its password. Once connected, WiFiManager persists these credentials internally (in its own reserved flash region, independent of DeskBuddy's `config.json`).

## Configuration Portal

DeskBuddy's own settings (weather, MQTT, HTTP notifications, silent mode) are configured through a separate, custom web portal — not WiFiManager's. This is triggered by holding **BTN2** during power-on or reset.

When active:
1. The device switches to `WIFI_AP_STA` mode and starts an open access point named **DeskBuddy_Config**.
2. A web server starts on port 80, serving a single-page form (`handleWebRoot()`), and accepts submissions at `/save` (`handleWebSave()`).
3. A `/resetwifi` route (`handleResetWiFi()`) is also available from the same page, for clearing the WiFiManager-saved network.
4. Saving writes the new values to `config.json` via `saveConfig()` and restarts the device.

## LittleFS

Configuration is persisted as JSON at `/config.json` on the ESP8266's LittleFS filesystem — flash storage that survives power loss and reflashing (as long as you don't erase flash entirely). LittleFS is initialized once, at the start of `loadConfig()`:

- If the filesystem fails to mount, it's formatted and re-mounted automatically.
- If `/config.json` doesn't exist yet (first boot, or after a flash erase), the firmware's hardcoded defaults are used and nothing is written until the first save.
- If the file exists but fails to parse as valid JSON, the firmware logs the failure and falls back to defaults for that boot — it does not crash or refuse to boot.

## Weather API

Weather comes from OpenWeatherMap's current-conditions endpoint, called with the configured city, API key, and units:

```
http://api.openweathermap.org/data/2.5/weather?q=<city>&appid=<key>&units=<units>
```

This is a plain HTTP (not HTTPS) request. A successful response populates city name, description, icon code, temperature, feels-like temperature, humidity, and wind speed; anything else (bad key, unknown city, network failure) leaves the weather screen showing "Fetching Weather..." until the next retry.

## MQTT

MQTT connects over TLS by default (port 8883, matching the default HiveMQ Cloud broker), using `WiFiClientSecure` with `setInsecure()` — meaning the server's TLS certificate is **not validated** against a trusted root. This keeps the implementation simple and avoids the memory cost of storing/checking a CA certificate on a constrained device, at the cost of not detecting a man-in-the-middle attack on the connection. For a personal desk device on a home network, this is a deliberate simplicity trade-off; it's worth knowing about if you're deploying this somewhere more sensitive.

Connection is only attempted if **both** `mqtt_user` and `mqtt_pass` are non-empty. Reconnection is retried at most once every 5 seconds (a simple time-gate, not exponential backoff) as long as WiFi is connected and MQTT isn't.

## HTTP Notifications

A lightweight alternative to MQTT. When `http_enabled` is true, a web server starts on `http_port` (default 80) with a single route at `http_endpoint` (default `/notify`), accepting GET requests with `title` and `msg` query parameters. This server runs independently of the MQTT client and of the config-portal's own web server (which only exists while the device is in config mode) — HTTP notifications work during normal operation.

## Notification Endpoint

```
GET http://<device-ip>:<http_port><http_endpoint>?title=<text>&msg=<text>
```

Both `title` and `msg` are optional — omitted values default to `"Notification"` and an empty body, respectively. If `http_enabled` is false, the endpoint responds with `403 Forbidden` and does nothing further.

## Changing WiFi

From the configuration portal, tap **Reset WiFi Network**. This calls WiFiManager's `resetSettings()` (clearing the saved network) and restarts the device, which will then boot into the **DeskBuddy_Setup** WiFi provisioning flow described above.

## Resetting WiFi

Same action as "Changing WiFi" above — there's a single reset mechanism, not separate "change" and "reset" flows.

## Saving Configuration

Every field in the configuration portal form is written to the in-memory `Config` struct on submit, then persisted to `/config.json` via `saveConfig()`, and the device restarts immediately afterward so the new values take effect cleanly (rather than trying to hot-swap, say, an active MQTT connection).

## Configuration File Format

`/config.json` is a flat JSON object with these fields:

```json
{
  "owm_key": "your_openweathermap_api_key",
  "owm_city": "Bengaluru",
  "owm_units": "metric",
  "mqtt_broker": "your-broker-address.hivemq.cloud",
  "mqtt_port": 8883,
  "mqtt_user": "",
  "mqtt_pass": "",
  "mqtt_topic": "deskbuddy/notify",
  "http_enabled": true,
  "http_port": 80,
  "http_endpoint": "/notify",
  "silent": false
}
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `owm_key` | string | (a working default key is baked in) | Your OpenWeatherMap API key |
| `owm_city` | string | `"Bengaluru"` | City name passed directly to the OpenWeatherMap query |
| `owm_units` | string | `"metric"` | `"metric"` or `"imperial"`, per OpenWeatherMap's API |
| `mqtt_broker` | string | HiveMQ Cloud default address | Broker hostname |
| `mqtt_port` | int | `8883` | TLS port by default; plain MQTT on 1883 would require firmware changes since TLS is currently hardcoded |
| `mqtt_user` / `mqtt_pass` | string | `""` / `""` | Both must be non-empty for MQTT to attempt a connection at all |
| `mqtt_topic` | string | `"deskbuddy/notify"` | Topic DeskBuddy subscribes to |
| `http_enabled` | bool | `true` | Enables/disables the `/notify` HTTP endpoint |
| `http_port` | int | `80` | Port for the HTTP notification server |
| `http_endpoint` | string | `"/notify"` | Path for the HTTP notification endpoint |
| `silent` | bool | `false` | Mutes mood/notification buzzer tones; also toggleable via BTN2 long-press |

If a field is missing from a saved `config.json` (e.g. an older config file from before a firmware update added a new field), `loadConfig()` falls back to the in-memory default for that field individually rather than failing the whole load.
