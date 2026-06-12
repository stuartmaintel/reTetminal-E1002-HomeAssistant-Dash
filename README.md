# reTerminal E1002 — Home Assistant Dashboard

A fully configurable e-ink smart home dashboard built with ESPHome for the Seeed Studio reTerminal E1002.

<img width="657" alt="image" src="https://github.com/user-attachments/assets/eb65be96-7b55-4d2d-a098-0683374e99e9" />

<img width="657" alt="image" src="https://github.com/user-attachments/assets/478c8a15-60eb-4c50-aa96-a54c0767de22" />



## Hardware

- **Device:** Seeed Studio reTerminal E1002
- **Display:** 7.3" E Ink Spectra 6 — 800×480, 6 colours (Black, White, Red, Yellow, Green, Blue)
- **SoC:** ESP32-S3 with 8MB octal PSRAM
- **Framework:** ESP-IDF

## Features

### 4 Screens
| Screen | Content |
|---|---|
| 1 — Main Dashboard | Weather, solar generation, home battery, status icon bar, alert lines, calendar |
| 2 — Climate | Heat pump status, room temperatures, heating/cooling calling indicators, delta bars |
| 3 — Presence & Transport | Motion sensors, now playing media, public transport departures |
| 4 — Security | Alarm status, door/window sensors, motion history, camera last motion |

### Configurable via web UI
- All Home Assistant entity IDs
- Icon assignments (MDI icons)
- Color per state for every icon slot (Spectra 6 palette)
- Alert slot conditions — supports `==`, `!=`, `>`, `<`, `contains`
- Up to 8 entities per alert slot with smart label building
- Thresholds (battery critical %, climate tolerance, etc.)
- WiFi credentials and HA connection details

### Smart features
- **Single HTTP request** per refresh — uses HA `/api/template` endpoint to fetch all entity states at once
- **Deep sleep** — 30 minute refresh cycle, ~3 month battery life
- **AP mode bootstrap** — on first boot shows WiFi QR code for easy setup
- **Battery & temperature guards** — skips refresh if battery critically low or display out of safe temp range
- **12h safety refresh** — prevents burn-in if deep sleep cycle stalls
- **Force refresh** — short press middle button
- **Config mode** — long press middle button, auto-closes after 10 minutes of inactivity

### Buttons
| Button | Short press | Long press |
|---|---|---|
| Left (white) | Previous screen | — |
| Middle (green) | Force refresh | Config mode |
| Right (white) | Next screen | — |

## File Structure

```
kitchen-panel.yaml          ESPHome main config
display_lambda.h            C++ display drawing functions (all 4 screens)
config_server.h             Custom HTTP REST API for web config
ha_templates.yaml           Home Assistant template sensors (calendar, media)
webconfig-ui.html           Web config UI reference (served from device)
secrets.yaml.example        Template for secrets.yaml (never commit secrets.yaml)
docs/
  all-screens.html          Interactive preview of all 4 screens
  mockup-screen1.html       Screen 1 mockup
  mockup-screen2.html       Screen 2 mockup
  mockup-screen3.html       Screen 3 mockup
  mockup-screen4.html       Screen 4 mockup
  mockup-apmode.html        AP mode vs config mode mockup
```

## Setup

### Requirements
- ESPHome 2025.11.1 or later
- Home Assistant with ESPHome add-on
- `materialdesignicons-webfont.ttf` in your ESPHome config directory
  - Download from [Templarian/MaterialDesign-Webfont](https://github.com/Templarian/MaterialDesign-Webfont)

### First flash (USB required)

```bash
# Copy secrets.yaml.example to secrets.yaml and fill in your values
cp secrets.yaml.example secrets.yaml

# Flash via USB (first time only)
esphome run kitchen-panel.yaml
```

All subsequent updates can be done wirelessly via OTA.

### Home Assistant templates

Add to `configuration.yaml`:

```yaml
homeassistant:
  packages:
    kitchen_panel: !include ha_templates.yaml
```

Restart HA, then go to Developer Tools → States and search for `sensor.panel_` to verify templates loaded.

### Configure entity IDs

1. Long press the middle (green) button to enter config mode
2. Scan the QR code or open the IP shown on screen
3. Go to the **Sensors** tab and fill in your entity IDs
4. Save each section — changes apply on next refresh

## Color palette

The Spectra 6 display supports exactly 6 colours. No greyscale, no gradients.

| Index | Colour | Usage |
|---|---|---|
| 1 | Black | Inactive icons, text |
| 2 | Red | Alerts, open doors, armed alarm |
| 3 | Yellow | Solar, discharging battery, arming |
| 4 | Green | Secure, charging, running |
| 5 | Blue | Windows open, scheduled, transport |

## Display protection

- Minimum 60 second cooldown between refreshes
- Refreshes blocked if battery below 5% (prevents mid-refresh power loss)
- Refreshes blocked if internal temperature below configured minimum (default 5°C)
- 12 hour forced refresh safety net prevents burn-in
- Red battery icon and thermometer shown in footer when warnings are active

## License

MIT — feel free to use, modify and share.
