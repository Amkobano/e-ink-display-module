/*
 * ESP32 E-Ink Display - Prayer Times & Weather
 * Fetches data from GitHub and displays on e-ink
 *
 * WiFi credentials stored in secrets.h (gitignored)
 * Font: Open Sans (similar to Jost) via U8g2_for_Adafruit_GFX
 */

#include "pins.h"
#include "secrets.h" // Contains WIFI_SSID and WIFI_PASSWORD (gitignored)
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

// Prayer times storage
struct PrayerTimes {
  String fajr = "N/A";
  String shuruq = "N/A";
  String dhuhr = "N/A";
  String asr = "N/A";
  String maghrib = "N/A";
  String isha = "N/A";
  String location = "";
} prayerTimes;

// Weather data storage
struct WeatherData {
  int temperature = 0;
  String condition = "N/A";
  float windSpeed = 0;
  String icon = "";
} weatherData;

// Forecast data storage
struct ForecastDay {
  String date = "";
  int temperature = 0;
  int rainChance = 0;
  String condition = "";
};
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

  prayerTimes.fajr = times["fajr"] | "N/A";
  prayerTimes.shuruq = times["shuruq"] | "N/A";
  prayerTimes.dhuhr = times["dhuhr"] | "N/A";
  prayerTimes.asr = times["asr"] | "N/A";
  prayerTimes.maghrib = times["maghrib"] | "N/A";
  prayerTimes.isha = times["isha"] | "N/A";
  prayerTimes.location = doc["location"] | "";

  Serial.println("Prayer times loaded:");
  Serial.println("  Fajr:    " + prayerTimes.fajr);
  Serial.println("  Sunrise:  " + prayerTimes.shuruq);
  Serial.println("  Dhuhr:   " + prayerTimes.dhuhr);
  Serial.println("  Asr:     " + prayerTimes.asr);
  Serial.println("  Maghrib: " + prayerTimes.maghrib);
  Serial.println("  Isha:    " + prayerTimes.isha);

  // Extract weather data (now nested under "current")
  JsonObject weather = doc["weather"];
  if (!weather.isNull()) {
    JsonObject current = weather["current"];
    if (!current.isNull()) {
      weatherData.temperature = current["temperature"] | 0;
      weatherData.condition = current["condition"] | "N/A";
      weatherData.windSpeed = current["wind_speed"] | 0.0f;
      weatherData.icon = current["icon"] | "";

      Serial.println("Weather loaded:");
      Serial.println("  Temp:      " + String(weatherData.temperature) + "°C");
      Serial.println("  Condition: " + weatherData.condition);
      Serial.println("  Wind:      " + String(weatherData.windSpeed) + " m/s");
      Serial.println("  Icon:      " + weatherData.icon);
    }

    // Extract 3-day forecast
    JsonArray forecastArray = weather["forecast"];
    if (!forecastArray.isNull()) {
      Serial.println("Forecast loaded:");
      for (int i = 0; i < 3 && i < forecastArray.size(); i++) {
        JsonObject day = forecastArray[i];
        forecast[i].date = day["date"] | "";
        forecast[i].temperature = day["temperature"] | 0;
        forecast[i].rainChance = day["rain_chance"] | 0;
        forecast[i].condition = day["condition"] | "";

        Serial.println("  " + forecast[i].date + ": " +
                       String(forecast[i].temperature) + "°C " +
                       String(forecast[i].rainChance) + "% " +
                       forecast[i].condition);
      }
    }
  }

  return true;
}

// Draw a dithered (grey) filled circle using checkerboard pattern
void fillCircleDithered(int cx, int cy, int radius) {
  for (int py = cy - radius; py <= cy + radius; py++) {
    for (int px = cx - radius; px <= cx + radius; px++) {
      int dx = px - cx;
      int dy = py - cy;
      if (dx * dx + dy * dy <= radius * radius) {
        // Checkerboard pattern: alternate black/white
        if ((px + py) % 2 == 0) {
          display.drawPixel(px, py, GxEPD_BLACK);
        }
        // Leave other pixels as background (white)
      }
    }
  }
}

// Draw a dithered (grey) filled rectangle using checkerboard pattern
void fillRectDithered(int x, int y, int w, int h) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      if ((px + py) % 2 == 0) {
        display.drawPixel(px, py, GxEPD_BLACK);
      }
    }
  }
}

// Draw small weather icon for forecast (based on condition text)
void drawSmallWeatherIcon(int x, int y, String condition) {
  condition.toLowerCase();

  // Clear/Sunny
  if (condition.indexOf("clear") >= 0 || condition.indexOf("sun") >= 0) {
    // Orange sun - larger and bolder
    display.fillCircle(x, y, 14, GxEPD_ORANGE);
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = x + cos(angle) * 18;
      int y1 = y + sin(angle) * 18;
      int x2 = x + cos(angle) * 26;
      int y2 = y + sin(angle) * 26;
      display.drawLine(x1, y1, x2, y2, GxEPD_ORANGE);
      display.drawLine(x1 + 1, y1, x2 + 1, y2, GxEPD_ORANGE);
    }
  }
  // Clouds
  else if (condition.indexOf("cloud") >= 0) {
    // Grey cloud - dithered for grey effect
    fillCircleDithered(x - 8, y, 12);
    fillCircleDithered(x + 8, y + 2, 10);
    fillCircleDithered(x, y - 6, 10);
    fillRectDithered(x - 18, y, 36, 14);
  }
  // Rain
  else if (condition.indexOf("rain") >= 0 ||
           condition.indexOf("drizzle") >= 0) {
    // Grey cloud + blue drops
    fillCircleDithered(x - 6, y - 8, 10);
    fillCircleDithered(x + 6, y - 6, 8);
    fillRectDithered(x - 16, y - 8, 32, 10);
    // Rain drops - thicker
    for (int i = 0; i < 3; i++) {
      int dx = x - 10 + i * 10;
      display.fillCircle(dx, y + 10, 2, GxEPD_BLUE);
      display.fillCircle(dx - 1, y + 14, 2, GxEPD_BLUE);
    }
  }
  // Snow
  else if (condition.indexOf("snow") >= 0) {
    // Blue snowflake - thicker lines
    for (int i = 0; i < 3; i++) {
      float angle = i * PI / 3;
      int x1 = x - cos(angle) * 16;
      int y1 = y - sin(angle) * 16;
      int x2 = x + cos(angle) * 16;
      int y2 = y + sin(angle) * 16;
      display.drawLine(x1, y1, x2, y2, GxEPD_BLUE);
      display.drawLine(x1 + 1, y1, x2 + 1, y2, GxEPD_BLUE);
    }
    display.fillCircle(x, y, 5, GxEPD_BLUE);
  }
  // Thunderstorm
  else if (condition.indexOf("thunder") >= 0 ||
           condition.indexOf("storm") >= 0) {
    // Grey cloud + yellow lightning
    fillCircleDithered(x - 6, y - 10, 10);
    fillCircleDithered(x + 6, y - 8, 8);
    fillRectDithered(x - 16, y - 10, 32, 10);
    // Yellow lightning bolt
    display.fillTriangle(x - 4, y + 2, x + 6, y + 2, x + 2, y + 12,
                         GxEPD_YELLOW);
    display.fillTriangle(x, y + 10, x + 8, y + 10, x - 4, y + 22, GxEPD_YELLOW);
  }
  // Mist/Fog
  else if (condition.indexOf("mist") >= 0 || condition.indexOf("fog") >= 0 ||
           condition.indexOf("haze") >= 0) {
    for (int i = 0; i < 4; i++) {
      display.drawLine(x - 16, y - 10 + i * 7, x + 16, y - 10 + i * 7,
                       GxEPD_BLACK);
      display.drawLine(x - 16, y - 10 + i * 7 + 1, x + 16, y - 10 + i * 7 + 1,
                       GxEPD_BLACK);
    }
  }
  // Default - question mark
  else {
    display.drawCircle(x, y, 12, GxEPD_BLACK);
    display.drawCircle(x, y, 11, GxEPD_BLACK);
  }
}

// Draw weather icon based on OpenWeatherMap icon code
void drawWeatherIcon(int x, int y, String iconCode) {
  int size = 120; // Large icon size

  // Clear/sunny (01d, 01n)
  if (iconCode.startsWith("01")) {
    // Sun - solid ORANGE circle with ORANGE rays
    display.fillCircle(x, y, size / 3, GxEPD_ORANGE);
    // Rays - all ORANGE, thick
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = x + cos(angle) * (size / 3 + 8);
      int y1 = y + sin(angle) * (size / 3 + 8);
      int x2 = x + cos(angle) * (size / 2 + 5);
      int y2 = y + sin(angle) * (size / 2 + 5);
      display.drawLine(x1, y1, x2, y2, GxEPD_ORANGE);
      display.drawLine(x1 + 1, y1, x2 + 1, y2, GxEPD_ORANGE);
      display.drawLine(x1, y1 + 1, x2, y2 + 1, GxEPD_ORANGE);
      display.drawLine(x1 + 1, y1 + 1, x2 + 1, y2 + 1, GxEPD_ORANGE);
    }
  }
  // Few clouds (02d, 02n)
  else if (iconCode.startsWith("02")) {
    // Small sun (all ORANGE) behind cloud
    display.fillCircle(x + 35, y - 25, 22, GxEPD_ORANGE);
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = x + 35 + cos(angle) * 26;
      int y1 = y - 25 + sin(angle) * 26;
      int x2 = x + 35 + cos(angle) * 38;
      int y2 = y - 25 + sin(angle) * 38;
      display.drawLine(x1, y1, x2, y2, GxEPD_ORANGE);
      display.drawLine(x1 + 1, y1, x2 + 1, y2, GxEPD_ORANGE);
    }
    // Cloud in front (DITHERED GREY)
    fillCircleDithered(x - 20, y + 10, 32);
    fillCircleDithered(x + 25, y + 15, 26);
    fillCircleDithered(x + 5, y - 8, 28);
    fillRectDithered(x - 52, y + 10, 104, 35);
  }
  // Scattered/broken clouds (03d, 03n, 04d, 04n)
  else if (iconCode.startsWith("03") || iconCode.startsWith("04")) {
    // Cloud shape (DITHERED GREY) - larger
    fillCircleDithered(x - 20, y + 10, 36);
    fillCircleDithered(x + 30, y + 10, 28);
    fillCircleDithered(x + 10, y - 16, 32);
    fillRectDithered(x - 56, y + 10, 116, 40);
  }
  // Rain (09d, 09n, 10d, 10n)
  else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
    // Cloud (DITHERED GREY) + rain drops (BLUE)
    fillCircleDithered(x - 20, y - 20, 28);
    fillCircleDithered(x + 20, y - 20, 24);
    fillCircleDithered(x, y - 36, 24);
    fillRectDithered(x - 48, y - 20, 96, 28);
    // Rain drops (BLUE) - larger and thicker
    for (int i = 0; i < 4; i++) {
      int dx = x - 30 + i * 20;
      display.drawLine(dx, y + 20, dx - 10, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 1, y + 20, dx - 9, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 2, y + 20, dx - 8, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 3, y + 20, dx - 7, y + 50, GxEPD_BLUE);
    }
  }
  // Thunderstorm (11d, 11n)
  else if (iconCode.startsWith("11")) {
    // Cloud (DITHERED GREY) + lightning (YELLOW)
    fillCircleDithered(x - 20, y - 20, 28);
    fillCircleDithered(x + 20, y - 20, 24);
    fillCircleDithered(x, y - 36, 24);
    fillRectDithered(x - 48, y - 20, 96, 28);
    // Lightning bolt (YELLOW) - larger
    display.fillTriangle(x - 5, y + 10, x + 18, y + 10, x + 8, y + 40,
                         GxEPD_YELLOW);
    display.fillTriangle(x + 5, y + 32, x + 28, y + 32, x - 8, y + 70,
                         GxEPD_YELLOW);
  }
  // Snow (13d, 13n)
  else if (iconCode.startsWith("13")) {
    // Snowflake pattern (BLUE) - larger
    for (int i = 0; i < 3; i++) {
      float angle = i * PI / 3;
      display.drawLine(x - cos(angle) * 50, y - sin(angle) * 50,
                       x + cos(angle) * 50, y + sin(angle) * 50, GxEPD_BLUE);
      display.drawLine(x - cos(angle) * 50 + 1, y - sin(angle) * 50,
                       x + cos(angle) * 50 + 1, y + sin(angle) * 50,
                       GxEPD_BLUE);
      display.drawLine(x - cos(angle) * 50 + 2, y - sin(angle) * 50,
                       x + cos(angle) * 50 + 2, y + sin(angle) * 50,
                       GxEPD_BLUE);
    }
    // Small branches on snowflake
    for (int i = 0; i < 6; i++) {
      float angle = i * PI / 3;
      int mx = x + cos(angle) * 30;
      int my = y + sin(angle) * 30;
      display.drawLine(mx, my, mx + cos(angle + PI / 6) * 15,
                       my + sin(angle + PI / 6) * 15, GxEPD_BLUE);
      display.drawLine(mx, my, mx + cos(angle - PI / 6) * 15,
                       my + sin(angle - PI / 6) * 15, GxEPD_BLUE);
    }
    display.fillCircle(x, y, 8, GxEPD_BLUE);
  }
  // Mist/fog (50d, 50n)
  else if (iconCode.startsWith("50")) {
    // Horizontal lines - larger
    for (int i = 0; i < 5; i++) {
      display.drawLine(x - 50, y - 30 + i * 15, x + 50, y - 30 + i * 15,
                       GxEPD_BLACK);
      display.drawLine(x - 50, y - 30 + i * 15 + 1, x + 50, y - 30 + i * 15 + 1,
                       GxEPD_BLACK);
      display.drawLine(x - 50, y - 30 + i * 15 + 2, x + 50, y - 30 + i * 15 + 2,
                       GxEPD_BLACK);
    }
  }
  // Default - question mark
  else {
    display.drawCircle(x, y, size / 2, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(x - 12, y + 12);
    u8g2Fonts.print("?");
  }
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

    if (prayerTimes.location.length() > 0) {
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      tw = u8g2Fonts.getUTF8Width(prayerTimes.location.c_str());
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
      String dayLabel = "";
      if (forecast[i].date.length() >= 10) {
        String month = forecast[i].date.substring(5, 7);
        String day = forecast[i].date.substring(8, 10);
        dayLabel = day + "." + month;
      }
      tw = u8g2Fonts.getUTF8Width(dayLabel.c_str());
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

void clearDisplay() {
  // Full white refresh to reset the display and eliminate ghosting
  Serial.println("Clearing display...");
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
  display.powerOff();
  delay(500);
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