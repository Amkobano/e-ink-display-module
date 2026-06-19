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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_7C.h>
#include <HTTPClient.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
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

// Prayer times storage (struct defined in globals.h)
PrayerTimes prayerTimes;

// Weather data storage (struct defined in globals.h)
WeatherData weatherData;

// Forecast data storage (struct defined in globals.h)
ForecastDay forecast[3];

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

void displayPrayerTimes() {
  Serial.println("Updating display...");
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    // Initialize U8g2 fonts for this page
    u8g2Fonts.begin(display);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    // Vertical divider
    display.fillRect(399, 40, 2, 430, GxEPD_BLACK);

    // ========== LEFT SIDE: Prayer Times ==========
    int leftCenter = 200;

    // Header with large title
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    const char *title = "Prayer Times";
    int tw = u8g2Fonts.getUTF8Width(title);
    u8g2Fonts.setCursor(leftCenter - tw / 2, 90);
    u8g2Fonts.print(title);

    if (prayerTimes.location[0] != '\0') {
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      tw = u8g2Fonts.getUTF8Width(prayerTimes.location);
      u8g2Fonts.setCursor(leftCenter - tw / 2, 125);
      u8g2Fonts.print(prayerTimes.location);
    }

    // Prayer list
    int listStartY = 180;
    int rowHeight = 52;
    int paddingX = 40;
    int nameX = paddingX;
    int timeX = 400 - paddingX;

    String prayerNames[] = {"Fajr", "Sunrise", "Dhuhr",
                            "Asr",  "Maghrib", "Isha"};
    String prayerTimesArr[] = {prayerTimes.fajr,    prayerTimes.shuruq,
                               prayerTimes.dhuhr,   prayerTimes.asr,
                               prayerTimes.maghrib, prayerTimes.isha};

    for (int i = 0; i < 6; i++) {
      int rowY = listStartY + i * rowHeight;

      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      u8g2Fonts.setCursor(nameX, rowY);
      u8g2Fonts.print(prayerNames[i]);

      tw = u8g2Fonts.getUTF8Width(prayerTimesArr[i].c_str());
      u8g2Fonts.setCursor(timeX - tw, rowY);
      u8g2Fonts.print(prayerTimesArr[i]);

      // Dashed line below
      int lineY = rowY + 12;
      for (int x = nameX; x < timeX; x += 8) {
        display.drawLine(x, lineY, x + 4, lineY, GxEPD_BLACK);
        display.drawLine(x, lineY + 1, x + 4, lineY + 1, GxEPD_BLACK);
      }
    }

    // ========== RIGHT SIDE: Weather ==========
    int rightCenter = 600;

    // Get current date
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    tw = u8g2Fonts.getUTF8Width(dateStr);
    u8g2Fonts.setCursor(rightCenter - tw / 2, 90);
    u8g2Fonts.print(dateStr);

    // Weather icon and current data - centered around rightCenter
    // Icon center at rightCenter-70, text center at rightCenter+70
    int iconX = rightCenter - 70;
    int dataX = rightCenter + 70;
    int weatherY = 180;

    drawWeatherIcon(iconX, weatherY, weatherData.icon);

    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    String tempStr = String(weatherData.temperature) + " °C";
    tw = u8g2Fonts.getUTF8Width(tempStr.c_str());
    int line1Y = weatherY - 25;
    u8g2Fonts.setCursor(dataX - tw / 2, line1Y);
    u8g2Fonts.print(tempStr);

    String condStr = weatherData.condition;
    tw = u8g2Fonts.getUTF8Width(condStr.c_str());
    int line2Y = weatherY + 15;
    u8g2Fonts.setCursor(dataX - tw / 2, line2Y);
    u8g2Fonts.print(condStr);

    String rainStr = String(forecast[0].rainChance) + "% Rain";
    tw = u8g2Fonts.getUTF8Width(rainStr.c_str());
    int line3Y = weatherY + 55;
    u8g2Fonts.setCursor(dataX - tw / 2, line3Y);
    u8g2Fonts.print(rainStr);

    // ========== 3-DAY FORECAST ==========
    int forecastY = 290;
    int boxWidth = 110;
    int boxHeight = 155;
    int boxSpacing = 15;
    int totalWidth = 3 * boxWidth + 2 * boxSpacing;
    int startX = rightCenter - totalWidth / 2;

    for (int i = 0; i < 3; i++) {
      int boxX = startX + i * (boxWidth + boxSpacing);
      int boxCenterX = boxX + boxWidth / 2;

      // Rounded rectangle
      display.drawRoundRect(boxX, forecastY, boxWidth, boxHeight, 8, GxEPD_BLACK);

      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      char dayLabel[8] = "";
      if (strlen(forecast[i].date) >= 10) {
        // date format: YYYY-MM-DD — extract DD.MM
        snprintf(dayLabel, sizeof(dayLabel), "%.2s.%.2s",
                 forecast[i].date + 8, forecast[i].date + 5);
      }
      tw = u8g2Fonts.getUTF8Width(dayLabel);
      u8g2Fonts.setCursor(boxCenterX - tw / 2, forecastY + 28);
      u8g2Fonts.print(dayLabel);

      drawSmallWeatherIcon(boxCenterX, forecastY + 66, forecast[i].condition);

      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      String temps = String(forecast[i].temperature) + "°C";
      tw = u8g2Fonts.getUTF8Width(temps.c_str());
      u8g2Fonts.setCursor(boxCenterX - tw / 2, forecastY + 122);
      u8g2Fonts.print(temps);

      u8g2Fonts.setFont(u8g2_font_helvR14_tf);
      String rStr = String(forecast[i].rainChance) + "%";
      tw = u8g2Fonts.getUTF8Width(rStr.c_str());
      u8g2Fonts.setCursor(boxCenterX - tw / 2, forecastY + 142);
      u8g2Fonts.print(rStr);
    }

  } while (display.nextPage());

  Serial.println("Display updated!");
}

void displayError() {
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    // Initialize U8g2 fonts
    u8g2Fonts.begin(display);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(60, 200);
    u8g2Fonts.print("Error");

    u8g2Fonts.setFont(u8g2_font_helvR18_tf);
    u8g2Fonts.setCursor(60, 260);
    u8g2Fonts.print(errorMsg);

  } while (display.nextPage());
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

  // Sync time to calculate wake time
  syncTime();
  unsigned long sleepSeconds = calculateSleepSeconds();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  display.hibernate();

  Serial.println("Going to deep sleep...");
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // Initialize display
  display.init(115200, true, 2, false);

  // Full white clear first to eliminate ghosting from previous image
  clearDisplay();

  // Connect, fetch, display
  if (connectWiFi() && fetchPrayerTimes()) {
    displayPrayerTimes();
  } else {
    displayError();
  }

  // Sleep for 1 hour then wake up and repeat
  goToSleep();
}

void loop() {
  // Never reached - deep sleep resets to setup()
}