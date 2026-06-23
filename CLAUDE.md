# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A wall-mounted e-ink display on an ESP32-S3 that shows Islamic prayer times, live weather, and a daily Dua/Dhikr. The device wakes once per day at 01:00 CET via WiFi to fetch fresh data from GitHub, then enters deep sleep. A physical button (GPIO 2) toggles between pages without triggering a WiFi wake.

**Critical constraint:** There is no `loop()`. All logic runs in `setup()` and ends in `esp_deep_sleep_start()`. Each wake-up is a short-lived program — not a running process.

## Repository Layout

```
├── esp32-firmware/         # PlatformIO C++ project
│   ├── src/
│   │   ├── main.cpp        # Entry point: wakeup routing, WiFi fetch, sleep scheduling
│   │   ├── globals.h       # Shared structs (PrayerTimes, WeatherData, ForecastDay) + extern display/font
│   │   ├── pins.h          # Hardware pin definitions
│   │   ├── display_utils.cpp/.h  # Display init, clearDisplay(), displayError()
│   │   ├── page_prayer.cpp/.h    # Page 0: prayer times + weather rendering
│   │   ├── page_dua.cpp/.h       # Page 1: loads dua_NNN.bmp from LittleFS, renders via PSRAM
│   │   └── secrets.h       # GITIGNORED — defines WIFI_SSID / WIFI_PASSWORD
│   ├── data/               # LittleFS filesystem image (flashed separately)
│   │   ├── dua_000.bmp … dua_083.bmp  # Pre-rendered 800×480 1-bit BMPs
│   │   └── dua-dhikr.json  # Dua text (Arabic + shaped + translations)
│   ├── partitions.csv      # Custom 16MB layout: 4MB app + ~12MB LittleFS
│   └── platformio.ini      # Board: esp32-s3-devkitm-1, 16MB flash, LittleFS
├── data-collection/        # Python data pipeline
│   ├── aggregator.py       # Orchestrates all extractors → output/display_data.json
│   ├── extract_weather.py  # OpenWeatherMap API fetch
│   ├── extract_prayer_times.py  # GITIGNORED — private scraper using PRAYER_TIMES_URL
│   ├── render_dua_images.py     # Renders Arabic dua text → BMP files in esp32-firmware/data/
│   ├── arabic_shaper.py    # Shapes Arabic ligatures into dua-dhikr.json arabic_shaped field
│   └── fonts/NotoNaskhArabic-Bold.ttf
├── data/
│   └── dua-dhikr.json      # Source of truth for dua content (repo root copy)
└── .github/workflows/update-data.yml  # Daily GitHub Actions: runs aggregator, commits JSON
```

## Architecture: Wake-Up Flow

```
esp_reset / timer / button (GPIO 2)
        │
        ▼
    setup()
        │
        ├─ ESP_SLEEP_WAKEUP_EXT0 (button)
        │       Toggle currentPage (0 → 1 → next dua → … → 0)
        │       Render from RTC cache — NO WiFi, NO NTP
        │       Re-arm timer using stored nextWakeTime epoch
        │
        └─ ESP_SLEEP_WAKEUP_TIMER or first boot
                currentPage = 0
                connectWiFi()
                syncTime() → fetchPrayerTimes() (parses JSON, fills RTC structs)
                displayPrayerTimes()
                goToSleep() → syncTime(), calculateSleepSeconds(), store nextWakeTime
        │
        ▼
    display.hibernate()
    esp_sleep_enable_timer_wakeup(...)
    esp_sleep_enable_ext0_wakeup(GPIO 2, LOW)
    esp_deep_sleep_start()
```

**Rule:** Always call `esp_sleep_get_wakeup_cause()` first in `setup()` — never call WiFi functions on a button wake.

## RTC Memory

Structs declared `RTC_DATA_ATTR` in `main.cpp` survive deep sleep (RTC SRAM, 8KB total):

- `PrayerTimes prayerTimes`, `WeatherData weatherData`, `ForecastDay forecast[3]` — data cache
- `uint8_t currentPage` — 0 = prayer/weather, 1 = dua
- `uint8_t duaIndex` — which of the 84 dua BMPs to show
- `time_t nextWakeTime` — epoch of next scheduled refresh (used by button wake to re-arm timer without NTP)

**RTC struct rules:** Use `char[]` with fixed length (never `String`), `int`/`float` (never `double`). Copy JSON values immediately with `strlcpy()` — never store `JsonVariant` references.

## Display

- **Panel:** Waveshare 7.3" 7-color ACeP (GDEY073D46), 800×480 px
- **Left half:** x 0–399 (prayer times), **right half:** x 400–799 (weather)
- Use `GxEPD_` color constants — never raw hex for display colors
- Full refresh only — use `display.firstPage()` / `display.nextPage()` loop, never `display.display()` directly
- Always call `display.hibernate()` before deep sleep

## Arabic Text Pipeline

Arabic requires offline ligature shaping — the ESP32 cannot do it at runtime.

1. Edit `arabic` field in `data/dua-dhikr.json`
2. `cd data-collection && python arabic_shaper.py` — writes `arabic_shaped` field
3. `python render_dua_images.py` — generates `esp32-firmware/data/dua_000.bmp … dua_083.bmp`
4. `cd esp32-firmware && pio run -t uploadfs` — flashes LittleFS
5. Firmware reads from `arabic_shaped` and from the BMP files — never from `arabic` directly

## PlatformIO Commands

```bash
# Build firmware
pio run

# Build + flash firmware
pio run -t upload

# Flash LittleFS filesystem (BMP images + dua-dhikr.json)
pio run -t uploadfs

# Monitor serial output (disconnects on each sleep cycle — expected)
pio device monitor --baud 115200

# Combined upload + monitor
pio run -t upload && pio device monitor
```

## Python Data Pipeline Commands

```bash
cd data-collection
pip install -r requirements.txt

# Required env vars
export OPENWEATHER_API_KEY='...'
export PRAYER_TIMES_URL='...'   # Mawaqit URL for mosque prayer times

# Run individual extractors
python extract_weather.py

# Full aggregation → data-collection/output/display_data.json
python aggregator.py
```

GitHub Actions runs `aggregator.py` daily and commits only `data-collection/output/display_data.json`. The firmware fetches this file from a GitHub raw URL at `DATA_URL` in `main.cpp`.

## Secrets

- `esp32-firmware/src/secrets.h` — gitignored, defines `WIFI_SSID` / `WIFI_PASSWORD`
- `data-collection/extract_prayer_times.py` — gitignored; created dynamically by GitHub Actions from `PRAYER_TIMES_URL` secret
- GitHub repo secrets: `OPENWEATHER_API_KEY`, `PRAYER_TIMES_URL`

## Key Coding Rules

- **No `loop()`** — deep sleep makes it unreachable; all logic in `setup()`
- **No `String` in RTC** — heap allocations are lost across sleep; use `char[]`
- **No WiFi on button wake** — check `esp_sleep_get_wakeup_cause()` first, always
- **No `double`** — ESP32-S3 FPU is 32-bit; use `float`
- **No `delay()` > a few ms** — use the sleep/wake architecture instead
- **ArduinoJson 7.x** — use `JsonDocument`, not the v6 `DynamicJsonDocument`
- **Naming:** files `snake_case`, functions `camelCase`, constants `UPPER_SNAKE`, types `PascalCase`, booleans `is`/`has` prefix
- **Functions:** one job, under ~40 lines, verb+noun name, early return on error

## Debugging

- Print wakeup cause at the very top of `setup()` — first thing to check
- If display shows garbage on button wake: `rtcIsValid`-style guard needed (RTC uninitialized on very first boot before any timer wake)
- LittleFS mount failures: `uploadfs` was never run after a flash erase
- Serial monitor disconnects on each sleep cycle — this is expected behavior
