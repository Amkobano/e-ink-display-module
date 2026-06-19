# copilot-instructions.md — ESP32-S3 E-Ink Prayer Times & Dua Display

This file tells Copilot how to work in this codebase. Read it fully before making any changes.

---

## Project Overview

A wall-mounted e-ink display built on an ESP32-S3 that shows Islamic prayer times, live weather,
and a daily Dua/Dhikr. The device wakes once per day over WiFi to fetch fresh data, then enters
deep sleep. A physical button (GPIO 2) lets the user toggle between two pages without a WiFi wake.

**Key constraint:** There is no `loop()`. All logic runs in `setup()` and ends in
`esp_deep_sleep_start()`. Think of each wake-up as a short-lived program, not a running process.

---

## Repository Layout

```
├── src/
│   ├── main.cpp            # Wakeup routing — the single entry point
│   ├── page_prayer.cpp/.h  # Page 0: prayer times + weather
│   ├── page_dua.cpp/.h     # Page 1: Dua of the Day
│   ├── display.cpp/.h      # Display init, power, shared drawing helpers
│   ├── wifi_fetch.cpp/.h   # WiFi connect, HTTPS fetch, disconnect
│   ├── rtc_data.h          # RTC_DATA_ATTR structs — survives deep sleep
│   └── secrets.h           # WiFi credentials — GITIGNORED, never commit
├── data/
│   └── dua-dhikr.json      # Arabic + pre-shaped text + translations
├── scripts/
│   └── arabic_shaper.py    # Offline: shapes Arabic text into dua-dhikr.json
├── platformio.ini
```

---

## Architecture: How a Wake-Up Works

```
esp_reset / timer / button
        │
        ▼
    setup()
        │
        ├─ ESP_SLEEP_WAKEUP_EXT0 (button, GPIO 2)
        │       Toggle currentPage (0 ↔ 1)
        │       Render from RTC cache — NO WiFi
        │
        └─ ESP_SLEEP_WAKEUP_TIMER or first boot
                currentPage = 0
                Connect WiFi
                Fetch display_data.json (GitHub raw URL)
                Parse → populate RTC structs
                Disconnect WiFi
                Render Page 0
        │
        ▼
    display.powerOff()
    enable timer wakeup  (01:00 CET daily)
    enable EXT0 wakeup   (GPIO 2, LOW)
    esp_deep_sleep_start()
```

**Rule:** Never call WiFi functions on a button wake. Check `esp_sleep_get_wakeup_cause()` first,
always, before doing anything else in `setup()`.

---

## RTC Memory

Declared in `rtc_data.h`. These variables survive deep sleep because they live in RTC SRAM.

```cpp
// Good — fixed-size char arrays, safe across sleep cycles
RTC_DATA_ATTR char prayerFajr[6];   // "05:12"

// Bad — heap-allocated, lost after deep sleep
RTC_DATA_ATTR String prayerFajr;    // Do NOT use String in RTC structs
```

**Rules for RTC structs:**
- Use `char[]` with a fixed max length, never `String` or pointers.
- Use `int` or `float` for numbers, never `double` (wastes RTC space).
- Keep struct sizes small — RTC SRAM is 8 KB total.
- Document the max length of every `char[]` field with a comment.

---

## Coding Standards

### Naming

| Thing | Convention | Example |
|---|---|---|
| Files | `snake_case` | `page_prayer.cpp` |
| Functions | `camelCase` | `renderPrayerRow()` |
| Constants / macros | `UPPER_SNAKE` | `DISPLAY_WIDTH` |
| Struct types | `PascalCase` | `PrayerTimes` |
| Local variables | `camelCase` | `int rowY` |
| Boolean variables | `is` / `has` prefix | `bool isConnected` |

### Functions

- **One job per function.** If a function fetches data AND parses it AND updates the display,
  split it into three functions.
- **Keep functions under ~40 lines.** If it's longer, look for a helper to extract.
- **Name functions as verb + noun:** `fetchWeather()`, `drawForecastBox()`, `parsePrayerTimes()`.
- Always return early on error rather than deeply nesting:

```cpp
// Good
bool fetchData(const char* url, JsonDocument& doc) {
    if (!connectWifi()) return false;
    if (!httpsGet(url, doc)) return false;
    return true;
}

// Avoid
bool fetchData(const char* url, JsonDocument& doc) {
    if (connectWifi()) {
        if (httpsGet(url, doc)) {
            return true;
        }
    }
    return false;
}
```

### Comments

Write comments that explain **why**, not **what**. The code shows what; comments explain intent.

```cpp
// Good — explains a non-obvious constraint
// Button wake skips WiFi entirely — RTC cache is always fresh enough for rendering.
if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0) {
    togglePage();
    renderCurrentPage();
}

// Unnecessary — just restates the code
// Toggle the page
togglePage();
```

Add a short block comment at the top of each `.cpp` file:

```cpp
// page_prayer.cpp
// Renders Page 0: prayer times on the left half, weather on the right half.
// All data is read from RTC memory — no WiFi calls here.
```

### Constants Over Magic Numbers

```cpp
// Good
const int PRAYER_ROW_HEIGHT = 36;
const int LEFT_COLUMN_X     = 20;

// Bad
display.setCursor(20, y + 36);
```

---

## Display Rules

- **Resolution:** 800 × 480 px. Left half = x 0–399, right half = x 400–799.
- **Colors:** GxEPD2_730c_GDEY073D46 supports 7 ACeP colors. Use the `GxEPD_` color constants;
  never use raw hex values for display colors.
- **Refresh:** Full refresh only — partial refresh is not supported on this panel. Always call
  `display.firstPage()` / `display.nextPage()` loop, never `display.display()` directly.
- **Power:** Call `display.powerOff()` before entering deep sleep, every single time.
- **Fonts:** Set the font before drawing any text. Don't assume the font state from a previous
  render call.

### Arabic Text

Arabic requires offline reshaping — the ESP32 cannot do ligature shaping at runtime.

1. Edit the `arabic` field in `dua-dhikr.json`.
2. Run `python scripts/arabic_shaper.py` — this writes the shaped result into `arabic_shaped`.
3. Flash the filesystem: `pio run -t uploadfs`.
4. In firmware, always render from `arabic_shaped`, never from `arabic`.

Font for Arabic: `u8g2_font_unifont_t_arabic` via U8g2_for_Adafruit_GFX.

---

## JSON & Data Fetching

- Use **ArduinoJson 7.x** (`JsonDocument`, not the v6 `DynamicJsonDocument`).
- Size the document using `ArduinoJson Assistant` or a generous fixed size; document the chosen
  size with a comment explaining what it covers.
- After parsing, copy values into RTC structs immediately using `strlcpy()` — never store a
  `JsonVariant` reference across function boundaries.

```cpp
// Good — copy into RTC struct right away
strlcpy(rtcPrayer.fajr, doc["fajr"] | "??:??", sizeof(rtcPrayer.fajr));

// Bad — JsonVariant is invalid once the doc goes out of scope
const char* fajr = doc["fajr"];
```

---

## Secrets & Environment Variables

`secrets.h` is gitignored. It must define:

```cpp
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"
```

The Python data pipeline reads these environment variables — set them in your shell or `.env`:

```
OPENWEATHER_API_KEY=...
PRAYER_TIMES_URL=...
LOCATION=...
```

Never hardcode credentials anywhere in source. If you see a key or password in source, move it
to `secrets.h` or an env var immediately.

---

## PlatformIO Workflow

```bash
# Build and upload firmware
pio run -t upload

# Flash the LittleFS filesystem (BMP images + dua-dhikr.json)
pio run -t uploadfs

# Monitor serial output
pio device monitor --baud 115200

# If dua-dhikr.json changed: re-render BMP images, then flash filesystem
cd data-collection && python render_dua_images.py
cd ../esp32-firmware && pio run -t uploadfs
```

**Dua image pipeline:**
- `data-collection/fonts/NotoNaskhArabic-Bold.ttf` — Arabic TTF font used for rendering
- `data-collection/render_dua_images.py` — renders 84 × 800×480 1-bit BMP files into `esp32-firmware/data/`
- Images are named `dua_000.bmp` … `dua_083.bmp`
- The firmware loads these from LittleFS into PSRAM and renders pixel-by-pixel via GxEPD2
- Do NOT edit the BMP files manually — always regenerate via `render_dua_images.py`

**Partition table:** `esp32-firmware/partitions.csv` — custom 16MB layout (4MB app + ~12MB LittleFS).
Required for the N16R8 board to fit 84 BMP images (~4MB) + dua-dhikr.json (~100KB).

Deep sleep makes the serial monitor disconnect on each sleep cycle. This is expected.

---

## Debugging Tips

- `Serial.printf()` is preferred over `Serial.print()` for formatted output.
- Print the wakeup cause at the very top of `setup()` — it's the first thing to check when
  behavior seems wrong.
- If the display shows garbage after a button wake, the RTC structs are probably uninitialized
  (first boot with no timer wakeup yet). Guard against this with an `rtcIsValid` flag in RTC mem.
- If WiFi never connects, check that `secrets.h` exists and the credentials are correct before
  suspecting a firmware bug.
- LittleFS mount failures usually mean `uploadfs` was never run after a full flash erase.

---

## What Copilot Must Not Do

- **Do not add `loop()`** — deep sleep makes it unreachable. All logic belongs in `setup()`.
- **Do not use `String`** in RTC structs or anywhere memory could be fragmented across wake cycles.
- **Do not call WiFi** on a button wake. Only on timer wake or first boot.
- **Do not use `delay()`** for anything longer than a few ms — use the sleep/wake architecture
  instead.
- **Do not commit `secrets.h`** — verify `.gitignore` covers it before any git operation.
- **Do not use `double`** — the ESP32-S3 FPU is 32-bit; use `float`.
- **Do not resize or reformat `dua-dhikr.json` manually** — always go through `arabic_shaper.py`
  to keep `arabic_shaped` in sync.
