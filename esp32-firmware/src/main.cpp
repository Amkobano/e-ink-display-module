/*
 * ESP32 E-Ink Display - Fajr Time
 * Fetches prayer times from GitHub and displays on e-ink
 * 
 * WiFi credentials stored in secrets.h (gitignored)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "pins.h"
#include "secrets.h"  // Contains WIFI_SSID and WIFI_PASSWORD (gitignored)

// ============================================
// CONFIGURATION
// ============================================
// GitHub raw URL for your JSON data
const char* DATA_URL = "https://raw.githubusercontent.com/Amkobano/e-ink-display-module/main/data-collection/output/display_data.json";

// Deep sleep duration (1 hour = 3600 seconds)
#define SLEEP_SECONDS 3600
// ============================================

// Display: Waveshare 7.3" 7-color (GDEY073D46), 800x480 pixels
GxEPD2_7C<GxEPD2_730c_GDEY073D46, GxEPD2_730c_GDEY073D46::HEIGHT> display(
    GxEPD2_730c_GDEY073D46(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

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
    Serial.println("Fetching JSON from GitHub...");
    
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification (OK for public GitHub)
    
    HTTPClient http;
    http.begin(client, DATA_URL);
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
    
    // Extract weather data
    JsonObject weather = doc["weather"];
    if (!weather.isNull()) {
        weatherData.temperature = weather["temperature"] | 0;
        weatherData.condition = weather["condition"] | "N/A";
        weatherData.windSpeed = weather["wind_speed"] | 0.0f;
        weatherData.icon = weather["icon"] | "";
        
        Serial.println("Weather loaded:");
        Serial.println("  Temp:      " + String(weatherData.temperature) + "°C");
        Serial.println("  Condition: " + weatherData.condition);
        Serial.println("  Wind:      " + String(weatherData.windSpeed) + " m/s");
        Serial.println("  Icon:      " + weatherData.icon);
    }
    
    return true;
}

// Draw weather icon based on OpenWeatherMap icon code
void drawWeatherIcon(int x, int y, String iconCode) {
    int size = 120;  // Large icon size
    
    // Clear/sunny (01d, 01n)
    if (iconCode.startsWith("01")) {
        // Sun - filled circle with rays (YELLOW)
        display.fillCircle(x, y, size/3, GxEPD_YELLOW);
        for (int i = 0; i < 8; i++) {
            float angle = i * PI / 4;
            int x1 = x + cos(angle) * (size/3 + 12);
            int y1 = y + sin(angle) * (size/3 + 12);
            int x2 = x + cos(angle) * (size/2);
            int y2 = y + sin(angle) * (size/2);
            display.drawLine(x1, y1, x2, y2, GxEPD_YELLOW);
            display.drawLine(x1+1, y1, x2+1, y2, GxEPD_YELLOW);
        }
    }
    // Few clouds (02d, 02n)
    else if (iconCode.startsWith("02")) {
        // Small sun (yellow) + cloud
        display.fillCircle(x - 30, y - 20, 24, GxEPD_BLACK);
        display.fillCircle(x + 10, y + 20, 36, GxEPD_BLACK);
        display.fillCircle(x + 50, y + 20, 28, GxEPD_BLACK);
        display.fillCircle(x + 30, y, 32, GxEPD_BLACK);
        display.fillRect(x - 26, y + 20, 104, 36, GxEPD_BLACK);
    }
    // Scattered/broken clouds (03d, 03n, 04d, 04n)
    else if (iconCode.startsWith("03") || iconCode.startsWith("04")) {
        // Cloud shape - larger
        display.fillCircle(x - 20, y + 10, 36, GxEPD_BLACK);
        display.fillCircle(x + 30, y + 10, 28, GxEPD_BLACK);
        display.fillCircle(x + 10, y - 16, 32, GxEPD_BLACK);
        display.fillRect(x - 56, y + 10, 116, 40, GxEPD_BLACK);
    }
    // Rain (09d, 09n, 10d, 10n)
    else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
        // Cloud + rain drops - larger
        display.fillCircle(x - 20, y - 20, 28, GxEPD_BLACK);
        display.fillCircle(x + 20, y - 20, 24, GxEPD_BLACK);
        display.fillCircle(x, y - 36, 24, GxEPD_BLACK);
        display.fillRect(x - 48, y - 20, 96, 28, GxEPD_BLACK);
        // Rain drops - larger and thicker
        for (int i = 0; i < 4; i++) {
            int dx = x - 30 + i * 20;
            display.drawLine(dx, y + 20, dx - 10, y + 50, GxEPD_BLACK);
            display.drawLine(dx + 1, y + 20, dx - 9, y + 50, GxEPD_BLACK);
            display.drawLine(dx + 2, y + 20, dx - 8, y + 50, GxEPD_BLACK);
        }
    }
    // Thunderstorm (11d, 11n)
    else if (iconCode.startsWith("11")) {
        // Cloud + lightning - larger
        display.fillCircle(x - 20, y - 20, 28, GxEPD_BLACK);
        display.fillCircle(x + 20, y - 20, 24, GxEPD_BLACK);
        display.fillRect(x - 48, y - 20, 96, 28, GxEPD_BLACK);
        // Lightning bolt - larger
        display.fillTriangle(x, y + 10, x + 20, y + 10, x + 10, y + 40, GxEPD_BLACK);
        display.fillTriangle(x + 10, y + 30, x + 30, y + 30, x, y + 70, GxEPD_BLACK);
    }
    // Snow (13d, 13n)
    else if (iconCode.startsWith("13")) {
        // Snowflake pattern - larger
        for (int i = 0; i < 3; i++) {
            float angle = i * PI / 3;
            display.drawLine(x - cos(angle) * 50, y - sin(angle) * 50, 
                           x + cos(angle) * 50, y + sin(angle) * 50, GxEPD_BLACK);
            display.drawLine(x - cos(angle) * 50 + 1, y - sin(angle) * 50, 
                           x + cos(angle) * 50 + 1, y + sin(angle) * 50, GxEPD_BLACK);
        }
        display.drawCircle(x, y, 16, GxEPD_BLACK);
        display.drawCircle(x, y, 15, GxEPD_BLACK);
    }
    // Mist/fog (50d, 50n)
    else if (iconCode.startsWith("50")) {
        // Horizontal lines - larger
        for (int i = 0; i < 5; i++) {
            display.drawLine(x - 50, y - 30 + i * 15, x + 50, y - 30 + i * 15, GxEPD_BLACK);
            display.drawLine(x - 50, y - 30 + i * 15 + 1, x + 50, y - 30 + i * 15 + 1, GxEPD_BLACK);
            display.drawLine(x - 50, y - 30 + i * 15 + 2, x + 50, y - 30 + i * 15 + 2, GxEPD_BLACK);
        }
    }
    // Default - question mark
    else {
        display.drawCircle(x, y, size/2, GxEPD_BLACK);
        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(x - 12, y + 12);
        display.print("?");
    }
}

void displayPrayerTimes() {
    Serial.println("Updating display...");
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        
        // ========== LEFT SIDE: Prayer Times ==========
        int labelX = 60;
        int timeX = 240;
        int startY = 70;
        int lineHeight = 58;
        
        // Title - bold, larger font
        display.setFont(&FreeSansBold24pt7b);
        if (prayerTimes.location.length() > 0) {
            display.setCursor(labelX, startY);
            display.print(prayerTimes.location);
        }
        
        // Thin separator line (left section only)
        display.drawLine(labelX, startY + 20, 380, startY + 20, GxEPD_BLACK);
        
        // Prayer times - clean sans font
        display.setFont(&FreeSans18pt7b);
        int y = startY + 75;
        
        // Fajr
        display.setCursor(labelX, y);
        display.print("Fajr");
        display.setCursor(timeX, y);
        display.print(prayerTimes.fajr);
        
        // Shuruq (sunrise) - slightly smaller
        y += lineHeight;
        display.setFont(&FreeSans12pt7b);
        display.setCursor(labelX, y);
        display.print("Sunrise");
        display.setCursor(timeX, y);
        display.print(prayerTimes.shuruq);
        display.setFont(&FreeSans18pt7b);
        
        // Dhuhr
        y += lineHeight;
        display.setCursor(labelX, y);
        display.print("Dhuhr");
        display.setCursor(timeX, y);
        display.print(prayerTimes.dhuhr);
        
        // Asr
        y += lineHeight;
        display.setCursor(labelX, y);
        display.print("Asr");
        display.setCursor(timeX, y);
        display.print(prayerTimes.asr);
        
        // Maghrib
        y += lineHeight;
        display.setCursor(labelX, y);
        display.print("Maghrib");
        display.setCursor(timeX, y);
        display.print(prayerTimes.maghrib);
        
        // Isha
        y += lineHeight;
        display.setCursor(labelX, y);
        display.print("Isha");
        display.setCursor(timeX, y);
        display.print(prayerTimes.isha);
        
        // ========== RIGHT SIDE: Weather ==========
        int weatherX = 500;
        int weatherStartY = 100;
        
        // Vertical divider line
        display.drawLine(420, startY + 20, 420, 450, GxEPD_BLACK);
        
        // Weather icon (centered)
        drawWeatherIcon(weatherX + 100, weatherStartY + 50, weatherData.icon);
        
        // Temperature - large and bold
        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(weatherX, weatherStartY + 160);
        display.print(String(weatherData.temperature) + " C");
        // Degree symbol
        display.drawCircle(weatherX + 58, weatherStartY + 132, 5, GxEPD_BLACK);
        
        // Condition
        display.setFont(&FreeSans12pt7b);
        display.setCursor(weatherX, weatherStartY + 200);
        display.print(weatherData.condition);
        
        // Wind speed
        display.setFont(&FreeSans9pt7b);
        display.setCursor(weatherX, weatherStartY + 240);
        display.print("Wind: " + String(weatherData.windSpeed, 1) + " m/s");
        
    } while (display.nextPage());
    
    Serial.println("Display updated!");
}

void displayError() {
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold24pt7b);
        
        display.setCursor(60, 200);
        display.print("Error");
        
        display.setFont(&FreeSans18pt7b);
        display.setCursor(60, 260);
        display.print(errorMsg);
        
    } while (display.nextPage());
}

void goToSleep() {
    Serial.println("Going to deep sleep...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    display.hibernate();
    esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n===========================================");
    Serial.println("  ESP32 Fajr Display");
    Serial.println("===========================================\n");

    // Initialize display
    display.init(115200, true, 2, false);

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