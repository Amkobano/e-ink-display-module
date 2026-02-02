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
#include <Fonts/FreeMono18pt7b.h>
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
    String dhuhr = "N/A";
    String asr = "N/A";
    String maghrib = "N/A";
    String isha = "N/A";
    String location = "";
} prayerTimes;

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
    prayerTimes.dhuhr = times["dhuhr"] | "N/A";
    prayerTimes.asr = times["asr"] | "N/A";
    prayerTimes.maghrib = times["maghrib"] | "N/A";
    prayerTimes.isha = times["isha"] | "N/A";
    prayerTimes.location = doc["location"] | "";
    
    Serial.println("Prayer times loaded:");
    Serial.println("  Fajr:    " + prayerTimes.fajr);
    Serial.println("  Dhuhr:   " + prayerTimes.dhuhr);
    Serial.println("  Asr:     " + prayerTimes.asr);
    Serial.println("  Maghrib: " + prayerTimes.maghrib);
    Serial.println("  Isha:    " + prayerTimes.isha);
    
    return true;
}

void displayPrayerTimes() {
    Serial.println("Updating display...");
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeMono18pt7b);
        
        int startY = 60;
        int lineHeight = 70;
        int labelX = 40;
        int timeX = 280;
        
        // Title / Location
        if (prayerTimes.location.length() > 0) {
            display.setCursor(labelX, startY);
            display.print("Prayer Times - ");
            display.print(prayerTimes.location);
        } else {
            display.setCursor(labelX, startY);
            display.print("Prayer Times");
        }
        
        // Draw a separator line
        display.drawLine(labelX, startY + 15, 760, startY + 15, GxEPD_BLACK);
        
        // Fajr
        display.setCursor(labelX, startY + lineHeight);
        display.print("Fajr:");
        display.setCursor(timeX, startY + lineHeight);
        display.print(prayerTimes.fajr);
        
        // Dhuhr
        display.setCursor(labelX, startY + lineHeight * 2);
        display.print("Dhuhr:");
        display.setCursor(timeX, startY + lineHeight * 2);
        display.print(prayerTimes.dhuhr);
        
        // Asr
        display.setCursor(labelX, startY + lineHeight * 3);
        display.print("Asr:");
        display.setCursor(timeX, startY + lineHeight * 3);
        display.print(prayerTimes.asr);
        
        // Maghrib
        display.setCursor(labelX, startY + lineHeight * 4);
        display.print("Maghrib:");
        display.setCursor(timeX, startY + lineHeight * 4);
        display.print(prayerTimes.maghrib);
        
        // Isha
        display.setCursor(labelX, startY + lineHeight * 5);
        display.print("Isha:");
        display.setCursor(timeX, startY + lineHeight * 5);
        display.print(prayerTimes.isha);
        
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
        display.setFont(&FreeMono18pt7b);
        
        display.setCursor(20, 60);
        display.print("Error:");
        
        display.setCursor(20, 120);
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