/*
 * ESP32 E-Ink Display - Prayer Times & Weather
 * Fetches data from GitHub and displays on e-ink
 *
 * WiFi credentials stored in secrets.h (gitignored)
 * Font: Open Sans (similar to Jost) via U8g2_for_Adafruit_GFX
 */

#include "pins.h"
#include "secrets.h" // Contains WIFI_SSID and WIFI_PASSWORD (gitignored)
#include "display_utils.h"
#include "page_dua.h"
#include "page_prayer.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_7C.h>
#include <HTTPClient.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/rtc_io.h>
#include <time.h>

// Forward declarations to fix compilation errors
void syncTime();
unsigned long calculateSleepSeconds();
void goToSleep();
void armWakeSources(unsigned long sleepSeconds);
void finishAndSleep();
void doCloudRefresh();
bool triggerWorkflow();
String fetchTimestamp();
void ledOff();
void ledPulseDelay(uint32_t totalMs);

// ============================================
// CONFIGURATION
// ============================================
// GitHub raw URL for your JSON data
const char *DATA_URL =
    "https://raw.githubusercontent.com/Amkobano/e-ink-display-module/main/"
    "data-collection/output/display_data.json";

// Wake time: 00:05 CET. The device triggers the workflow itself on this wake,
// so it no longer needs to wait for the (delay-prone) cron run. The 5-minute
// margin guarantees the date has rolled over to the new day before the run.
#define WAKE_HOUR 0
#define WAKE_MINUTE 5

// Timezone: Germany (CET/CEST with automatic DST)
const char *NTP_SERVER = "pool.ntp.org";
const char *TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
// ============================================

// Display: Waveshare 7.3" 7-color (GDEY073D46), 800x480 pixels
GxEPD2_7C<GxEPD2_730c_GDEY073D46, GxEPD2_730c_GDEY073D46::HEIGHT>
    display(GxEPD2_730c_GDEY073D46(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// U8g2 fonts for Adafruit GFX - provides clean modern fonts like Open Sans
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// --- RTC memory: survives deep sleep ---
// Cached display data (structs use char[] so they are RTC-safe)
RTC_DATA_ATTR static PrayerTimes prayerTimes;
RTC_DATA_ATTR static WeatherData weatherData;
RTC_DATA_ATTR static ForecastDay forecast[3];
// Page state
RTC_DATA_ATTR static uint8_t  currentPage  = 0;   // 0 = weather/prayer, 1 = dua
RTC_DATA_ATTR static uint8_t  duaIndex     = 0;   // which dua to show next
RTC_DATA_ATTR static time_t   nextWakeTime = 0;   // epoch of next scheduled refresh
// ----------------------------------------

#define NUM_DUAS 45   // total pre-rendered BMP images

// --- On-demand refresh: trigger the GitHub Actions workflow via its REST API ---
const char *GH_API_HOST      = "api.github.com";
const char *GH_OWNER_REPO    = "Amkobano/e-ink-display-module";
const char *GH_WORKFLOW_FILE = "update-data.yml";
const char *GH_REF           = "main";
#ifndef GITHUB_TOKEN
// Define in secrets.h: a fine-grained PAT scoped to THIS repo, "Actions: read/write".
#define GITHUB_TOKEN ""
#endif
#define REFRESH_POLL_TIMEOUT_S  90   // give up waiting for fresh data after this long
#define REFRESH_POLL_INTERVAL_S 5    // re-check GitHub this often while waiting

String errorMsg = "";

bool connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println(" FAILED!");
  errorMsg = "WiFi failed";
  return false;
}

bool fetchPrayerTimes() {
  Serial.println("Fetching JSON from GitHub Raw...");

  // Sync time first to get a valid timestamp for cache busting
  syncTime();
  time_t now = time(nullptr);

  // Build cache-busting URL: DATA_URL + "?t=" + timestamp
  String urlWithCacheBuster = String(DATA_URL) + "?t=" + String(now);
  Serial.println("URL: " + urlWithCacheBuster);

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification (OK for public content)

  HTTPClient http;
  http.setTimeout(15000); // 15 second timeout
  http.begin(client, urlWithCacheBuster);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    errorMsg = "HTTP " + String(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  Serial.println("Parsing JSON...");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    errorMsg = "JSON error";
    return false;
  }

  // Extract all prayer times
  JsonObject times = doc["prayer_times"];
  if (times.isNull()) {
    errorMsg = "No prayer_times";
    return false;
  }

  strlcpy(prayerTimes.fajr,    times["fajr"]    | "N/A", sizeof(prayerTimes.fajr));
  strlcpy(prayerTimes.shuruq,  times["shuruq"]  | "N/A", sizeof(prayerTimes.shuruq));
  strlcpy(prayerTimes.dhuhr,   times["dhuhr"]   | "N/A", sizeof(prayerTimes.dhuhr));
  strlcpy(prayerTimes.asr,     times["asr"]     | "N/A", sizeof(prayerTimes.asr));
  strlcpy(prayerTimes.maghrib, times["maghrib"] | "N/A", sizeof(prayerTimes.maghrib));
  strlcpy(prayerTimes.isha,    times["isha"]    | "N/A", sizeof(prayerTimes.isha));
  strlcpy(prayerTimes.location, doc["location"] | "",    sizeof(prayerTimes.location));

  Serial.println("Prayer times loaded:");
  Serial.println("  Fajr:    " + String(prayerTimes.fajr));
  Serial.println("  Sunrise:  " + String(prayerTimes.shuruq));
  Serial.println("  Dhuhr:   " + String(prayerTimes.dhuhr));
  Serial.println("  Asr:     " + String(prayerTimes.asr));
  Serial.println("  Maghrib: " + String(prayerTimes.maghrib));
  Serial.println("  Isha:    " + String(prayerTimes.isha));

  // Extract weather data (now nested under "current")
  JsonObject weather = doc["weather"];
  if (!weather.isNull()) {
    JsonObject current = weather["current"];
    if (!current.isNull()) {
      weatherData.temperature = current["temperature"] | 0;
      strlcpy(weatherData.condition, current["condition"] | "N/A", sizeof(weatherData.condition));
      weatherData.windSpeed = current["wind_speed"] | 0.0f;
      strlcpy(weatherData.icon, current["icon"] | "", sizeof(weatherData.icon));

      Serial.println("Weather loaded:");
      Serial.println("  Temp:      " + String(weatherData.temperature) + "°C");
      Serial.println("  Condition: " + String(weatherData.condition));
      Serial.println("  Wind:      " + String(weatherData.windSpeed) + " m/s");
      Serial.println("  Icon:      " + String(weatherData.icon));
    }

    // Extract 3-day forecast
    JsonArray forecastArray = weather["forecast"];
    if (!forecastArray.isNull()) {
      Serial.println("Forecast loaded:");
      for (int i = 0; i < 3 && i < forecastArray.size(); i++) {
        JsonObject day = forecastArray[i];
        strlcpy(forecast[i].date,      day["date"]        | "", sizeof(forecast[i].date));
        forecast[i].temperature = day["temperature"] | 0;
        forecast[i].rainChance  = day["rain_chance"]  | 0;
        strlcpy(forecast[i].condition, day["condition"]    | "", sizeof(forecast[i].condition));

        Serial.println("  " + String(forecast[i].date) + ": " +
                       String(forecast[i].temperature) + "°C " +
                       String(forecast[i].rainChance) + "% " +
                       String(forecast[i].condition));
      }
    }
  }

  return true;
}

void syncTime() {
  Serial.println("Syncing time with NTP...");
  configTzTime(TIMEZONE, NTP_SERVER);

  // Wait for time to sync (max 10 seconds)
  int attempts = 0;
  while (time(nullptr) < 1000000000 && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println(" Done!");
}

unsigned long calculateSleepSeconds() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time, using 24h fallback");
    return 86400; // Fallback: 24 hours
  }

  Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour,
                timeinfo.tm_min, timeinfo.tm_sec);

  // Calculate seconds until next 3 AM
  int currentSeconds =
      timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  int targetSeconds = WAKE_HOUR * 3600 + WAKE_MINUTE * 60;

  int sleepSeconds = targetSeconds - currentSeconds;

  // If target time has passed today, wake up tomorrow
  if (sleepSeconds <= 0) {
    sleepSeconds += 86400; // Add 24 hours
  }

  Serial.printf("Sleeping for %d seconds (%.1f hours) until %02d:%02d\n",
                sleepSeconds, sleepSeconds / 3600.0, WAKE_HOUR, WAKE_MINUTE);

  return sleepSeconds;
}

// Arm the timer + both buttons as deep-sleep wake sources.
// EXT1 ANY_LOW wakes when EITHER button pulls its GPIO LOW. rtc_gpio_init +
// direction + pullup must be set on each pin before ext1 enable on the ESP32-S3.
void armWakeSources(unsigned long sleepSeconds) {
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

  rtc_gpio_init((gpio_num_t)BUTTON_PIN);
  rtc_gpio_set_direction((gpio_num_t)BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
  esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
}

void goToSleep() {
  Serial.println("Preparing for deep sleep...");

  syncTime();
  unsigned long sleepSeconds = calculateSleepSeconds();

  // Store the absolute epoch of the next scheduled wake so button-press
  // wakeups can calculate remaining sleep time without NTP.
  nextWakeTime = time(nullptr) + sleepSeconds;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  display.hibernate();

  Serial.println("Going to deep sleep...");
  armWakeSources(sleepSeconds);
  esp_deep_sleep_start();
}

// Sleep without re-syncing time (used by button wakes, where time() is already
// set or will fall back). Always recalculates the duration so a near-zero clock
// after a button wake can't overflow the 48-bit RTC timer.
void finishAndSleep() {
  display.hibernate();
  unsigned long sleepSecs = calculateSleepSeconds();
  nextWakeTime = time(nullptr) + sleepSecs;
  Serial.println("Going to deep sleep...");
  armWakeSources(sleepSecs);
  esp_deep_sleep_start();
}

// --- Onboard RGB LED (WS2812 on GPIO48) used as a "refreshing" indicator ---
void ledOff() { neopixelWrite(LED_PIN, 0, 0, 0); }

void ledPulseDelay(uint32_t totalMs) {
  uint32_t elapsed = 0;
  bool on = false;
  while (elapsed < totalMs) {
    on = !on;
    neopixelWrite(LED_PIN, 0, 0, on ? 40 : 0);   // dim blue pulse
    delay(250);
    elapsed += 250;
  }
}

// Fetch just the "timestamp" field of the committed JSON (with a unique
// cache-buster) so we can detect when a fresh workflow run has published data.
String fetchTimestamp() {
  String url = String(DATA_URL) + "?t=" + String((uint32_t)time(nullptr)) + String(millis());
  WiFiClientSecure client;
  client.setInsecure();   // public read-only content, same as the daily fetch
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) return "";
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return "";
  }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return "";
  return String((const char *)(doc["timestamp"] | ""));
}

// POST a workflow_dispatch event to GitHub Actions. Returns true on HTTP 204.
bool triggerWorkflow() {
  if (strlen(GITHUB_TOKEN) == 0) {
    Serial.println("GITHUB_TOKEN not set — cannot trigger workflow");
    return false;
  }

  WiFiClientSecure client;
#ifdef GITHUB_API_ROOT_CA
  client.setCACert(GITHUB_API_ROOT_CA);   // verify GitHub's TLS cert (recommended)
#else
  client.setInsecure();                    // define GITHUB_API_ROOT_CA in secrets.h to verify
#endif

  String url = String("https://") + GH_API_HOST + "/repos/" + GH_OWNER_REPO +
               "/actions/workflows/" + GH_WORKFLOW_FILE + "/dispatches";

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  http.addHeader("Authorization", String("Bearer ") + GITHUB_TOKEN);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  http.addHeader("User-Agent", "esp32-eink-display");   // GitHub rejects requests with no UA
  http.addHeader("Content-Type", "application/json");

  String body = String("{\"ref\":\"") + GH_REF + "\"}";
  int code = http.POST(body);
  Serial.printf("workflow_dispatch -> HTTP %d\n", code);
  http.end();
  return code == 204;   // GitHub returns 204 No Content on success
}

// Daily refresh flow: keep the current screen visible, trigger a fresh workflow
// run, blink the LED while polling until new data is published (or timeout),
// then fetch + display it. WiFi is turned off before the display refresh.
// Falls back to the latest committed data (kept fresh by the cron backup) if the
// trigger or poll fails.
void doCloudRefresh() {
  Serial.println("Cloud refresh requested");

  if (!connectWiFi()) {
    displayError("WiFi failed");
    return;
  }
  syncTime();

  String before = fetchTimestamp();
  if (triggerWorkflow()) {
    uint32_t waited = 0;
    String latest = before;
    while (latest == before && waited < REFRESH_POLL_TIMEOUT_S * 1000UL) {
      ledPulseDelay(REFRESH_POLL_INTERVAL_S * 1000);
      waited += REFRESH_POLL_INTERVAL_S * 1000;
      latest = fetchTimestamp();
      Serial.printf("poll: before='%s' latest='%s'\n", before.c_str(), latest.c_str());
    }
    if (latest == before) Serial.println("Refresh timed out; showing latest available data");
  } else {
    Serial.println("Trigger failed; showing latest available data");
  }
  ledOff();

  if (fetchPrayerTimes()) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    displayPrayerTimes(prayerTimes, weatherData, forecast);
  } else {
    displayError(errorMsg.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  // Initialize display on every wakeup
  display.init(115200, true, 20, false);

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    // ---- Page toggle button (GPIO2): no WiFi needed ----
    Serial.println("Wakeup: page button");

    if (currentPage == 0) {
      // Pick a new random dua every time we enter the dua page
      duaIndex = esp_random() % NUM_DUAS;
      currentPage = 1;
    } else {
      currentPage = 0;
    }

    Serial.printf("Page: %d, Dua: %d\n", currentPage, duaIndex);

    if (currentPage == 0) {
      displayPrayerTimes(prayerTimes, weatherData, forecast);
    } else {
      displayDua(duaIndex);
    }

    finishAndSleep();   // never returns

  } else {
    // ---- Timer wakeup or first boot: trigger workflow + fetch fresh data ----
    // workflow_dispatch runs promptly (unlike the delay-prone cron schedule), so
    // the device gets up-to-date data on its own wake instead of relying on cron
    // having finished in time.
    Serial.println("Wakeup: timer / first boot");
    currentPage = 0;

    doCloudRefresh();
    goToSleep();
  }
}

void loop() {
  // Never reached — deep sleep resets to setup()
}