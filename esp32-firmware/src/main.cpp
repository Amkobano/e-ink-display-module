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

// ============================================
// CONFIGURATION
// ============================================
// GitHub raw URL for your JSON data
const char *DATA_URL =
    "https://raw.githubusercontent.com/Amkobano/e-ink-display-module/main/"
    "data-collection/output/display_data.json";

// Wake time: 01:00 CET - safely after workflow completes (even with GitHub
// delays)
#define WAKE_HOUR 1
#define WAKE_MINUTE 0

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

#define NUM_DUAS 73   // total pre-rendered BMP images

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
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  // EXT1 ANY_LOW: wakes when GPIO2 is pulled LOW by the button.
  // rtc_gpio_init + direction must be set before ext1 enable on ESP32-S3.
  rtc_gpio_init((gpio_num_t)BUTTON_PIN);
  rtc_gpio_set_direction((gpio_num_t)BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
  esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  // Initialize display on every wakeup
  display.init(115200, true, 20, false);

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    // ---- Button press: toggle page, no WiFi needed ----
    Serial.println("Wakeup: button press");

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

    // Re-enable both wakeup sources.
    // Always recalculate sleep duration: time() can be near-zero after button
    // wake (timezone not yet re-applied), making epoch arithmetic overflow the
    // 48-bit RTC timer and cause an immediate spurious timer wakeup.
    display.hibernate();
    unsigned long sleepSecs = calculateSleepSeconds();
    nextWakeTime = time(nullptr) + sleepSecs;
    esp_sleep_enable_timer_wakeup(sleepSecs * 1000000ULL);
    rtc_gpio_init((gpio_num_t)BUTTON_PIN);
    rtc_gpio_set_direction((gpio_num_t)BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();

  } else {
    // ---- Timer wakeup or first boot: fetch fresh data ----
    Serial.println("Wakeup: timer / first boot");
    currentPage = 0;

    clearDisplay();

    if (connectWiFi() && fetchPrayerTimes()) {
      // Shut down WiFi before the display refresh so the radio's ~200mA draw
      // doesn't overlap with the display boost converter's peak current demand.
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      displayPrayerTimes(prayerTimes, weatherData, forecast);
    } else {
      displayError(errorMsg.c_str());
    }

    goToSleep();
  }
}

void loop() {
  // Never reached — deep sleep resets to setup()
}