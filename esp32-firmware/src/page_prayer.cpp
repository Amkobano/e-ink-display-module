#include "page_prayer.h"
#include "display_utils.h"
#include <Arduino.h>
#include <time.h>

void displayPrayerTimes(const PrayerTimes &pt, const WeatherData &wd, const ForecastDay forecast[3]) {
  Serial.println("Rendering page 0: prayer times + weather...");
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    u8g2Fonts.begin(display);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    // Vertical divider
    display.fillRect(399, 40, 2, 430, GxEPD_BLACK);

    // ========== LEFT SIDE: Prayer Times ==========
    int leftCenter = 200;
    int tw;

    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    const char *title = "Prayer Times";
    tw = u8g2Fonts.getUTF8Width(title);
    u8g2Fonts.setCursor(leftCenter - tw / 2, 90);
    u8g2Fonts.print(title);

    if (strlen(pt.location) > 0) {
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      tw = u8g2Fonts.getUTF8Width(pt.location);
      u8g2Fonts.setCursor(leftCenter - tw / 2, 125);
      u8g2Fonts.print(pt.location);
    }

    int listStartY = 180;
    int rowHeight  = 52;
    int paddingX   = 40;
    int nameX      = paddingX;
    int timeX      = 400 - paddingX;

    const char *prayerNames[6]  = {"Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"};
    const char *prayerValues[6] = {pt.fajr, pt.shuruq, pt.dhuhr, pt.asr, pt.maghrib, pt.isha};

    for (int i = 0; i < 6; i++) {
      int rowY = listStartY + i * rowHeight;

      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      u8g2Fonts.setCursor(nameX, rowY);
      u8g2Fonts.print(prayerNames[i]);

      tw = u8g2Fonts.getUTF8Width(prayerValues[i]);
      u8g2Fonts.setCursor(timeX - tw, rowY);
      u8g2Fonts.print(prayerValues[i]);

      // Dashed separator line
      int lineY = rowY + 12;
      for (int x = nameX; x < timeX; x += 8) {
        display.drawLine(x, lineY, x + 4, lineY, GxEPD_BLACK);
        display.drawLine(x, lineY + 1, x + 4, lineY + 1, GxEPD_BLACK);
      }
    }

    // ========== RIGHT SIDE: Weather ==========
    int rightCenter = 600;

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

    int iconX    = rightCenter - 70;
    int dataX    = rightCenter + 70;
    int weatherY = 180;

    drawWeatherIcon(iconX, weatherY, String(wd.icon));

    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    String tempStr = String(wd.temperature) + " \xc2\xb0\x43"; // °C in UTF-8
    tw = u8g2Fonts.getUTF8Width(tempStr.c_str());
    u8g2Fonts.setCursor(dataX - tw / 2, weatherY - 25);
    u8g2Fonts.print(tempStr);

    String condStr = String(wd.condition);
    tw = u8g2Fonts.getUTF8Width(condStr.c_str());
    u8g2Fonts.setCursor(dataX - tw / 2, weatherY + 15);
    u8g2Fonts.print(condStr);

    String rainStr = String(forecast[0].rainChance) + "% Rain";
    tw = u8g2Fonts.getUTF8Width(rainStr.c_str());
    u8g2Fonts.setCursor(dataX - tw / 2, weatherY + 55);
    u8g2Fonts.print(rainStr);

    // ========== 3-DAY FORECAST ==========
    int forecastY   = 290;
    int boxWidth    = 110;
    int boxHeight   = 155;
    int boxSpacing  = 15;
    int totalWidth  = 3 * boxWidth + 2 * boxSpacing;
    int startX      = rightCenter - totalWidth / 2;

    for (int i = 0; i < 3; i++) {
      int boxX      = startX + i * (boxWidth + boxSpacing);
      int boxCenterX = boxX + boxWidth / 2;

      display.drawRoundRect(boxX, forecastY, boxWidth, boxHeight, 8, GxEPD_BLACK);

      // Date label DD.MM
      char dayLabel[8] = "";
      if (strlen(forecast[i].date) >= 10) {
        snprintf(dayLabel, sizeof(dayLabel), "%.2s.%.2s",
                 forecast[i].date + 8,   // day
                 forecast[i].date + 5);  // month
      }
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      tw = u8g2Fonts.getUTF8Width(dayLabel);
      u8g2Fonts.setCursor(boxCenterX - tw / 2, forecastY + 28);
      u8g2Fonts.print(dayLabel);

      drawSmallWeatherIcon(boxCenterX, forecastY + 66, String(forecast[i].condition));

      String temps = String(forecast[i].temperature) + "\xc2\xb0\x43";
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

  Serial.println("Page 0 rendered.");
}

void displayError(const char *errorMsg) {
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
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
