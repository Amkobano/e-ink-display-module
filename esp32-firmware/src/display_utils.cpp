#include "display_utils.h"
#include <Arduino.h>

// P1 fix: compute horizontal span per row instead of checking every pixel in
// the bounding box. Eliminates the per-pixel circle test and the branch inside
// the inner loop, roughly halving drawPixel calls for the rect and reducing
// them by ~(1 - π/4) ≈ 21 % for the circle on top of that.

void fillCircleDithered(int cx, int cy, int radius) {
  for (int py = cy - radius; py <= cy + radius; py++) {
    int dy = py - cy;
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    int x0 = cx - dx;
    // Align first pixel to checkerboard phase: draw where (px + py) is even
    int startX = x0 + (((x0 + py) & 1) ? 1 : 0);
    for (int px = startX; px <= cx + dx; px += 2) {
      display.drawPixel(px, py, GxEPD_BLACK);
    }
  }
}

void fillRectDithered(int x, int y, int w, int h) {
  for (int py = y; py < y + h; py++) {
    // Align first pixel to checkerboard phase: draw where (px + py) is even
    int startX = x + (((x + py) & 1) ? 1 : 0);
    for (int px = startX; px < x + w; px += 2) {
      display.drawPixel(px, py, GxEPD_BLACK);
    }
  }
}

void drawSmallWeatherIcon(int x, int y, String condition) {
  condition.toLowerCase();

  if (condition.indexOf("clear") >= 0 || condition.indexOf("sun") >= 0) {
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
  } else if (condition.indexOf("cloud") >= 0) {
    fillCircleDithered(x - 8, y, 12);
    fillCircleDithered(x + 8, y + 2, 10);
    fillCircleDithered(x, y - 6, 10);
    fillRectDithered(x - 18, y, 36, 14);
  } else if (condition.indexOf("rain") >= 0 || condition.indexOf("drizzle") >= 0) {
    fillCircleDithered(x - 6, y - 8, 10);
    fillCircleDithered(x + 6, y - 6, 8);
    fillRectDithered(x - 16, y - 8, 32, 10);
    for (int i = 0; i < 3; i++) {
      int dx = x - 10 + i * 10;
      display.fillCircle(dx, y + 10, 2, GxEPD_BLUE);
      display.fillCircle(dx - 1, y + 14, 2, GxEPD_BLUE);
    }
  } else if (condition.indexOf("snow") >= 0) {
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
  } else if (condition.indexOf("thunder") >= 0 || condition.indexOf("storm") >= 0) {
    fillCircleDithered(x - 6, y - 10, 10);
    fillCircleDithered(x + 6, y - 8, 8);
    fillRectDithered(x - 16, y - 10, 32, 10);
    display.fillTriangle(x - 4, y + 2, x + 6, y + 2, x + 2, y + 12, GxEPD_YELLOW);
    display.fillTriangle(x, y + 10, x + 8, y + 10, x - 4, y + 22, GxEPD_YELLOW);
  } else if (condition.indexOf("mist") >= 0 || condition.indexOf("fog") >= 0 ||
             condition.indexOf("haze") >= 0) {
    for (int i = 0; i < 4; i++) {
      display.drawLine(x - 16, y - 10 + i * 7, x + 16, y - 10 + i * 7, GxEPD_BLACK);
      display.drawLine(x - 16, y - 10 + i * 7 + 1, x + 16, y - 10 + i * 7 + 1, GxEPD_BLACK);
    }
  } else {
    display.drawCircle(x, y, 12, GxEPD_BLACK);
    display.drawCircle(x, y, 11, GxEPD_BLACK);
  }
}

void drawWeatherIcon(int x, int y, String iconCode) {
  int size = 120;

  if (iconCode.startsWith("01")) {
    display.fillCircle(x, y, size / 3, GxEPD_ORANGE);
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
  } else if (iconCode.startsWith("02")) {
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
    fillCircleDithered(x - 20, y + 10, 32);
    fillCircleDithered(x + 25, y + 15, 26);
    fillCircleDithered(x + 5, y - 8, 28);
    fillRectDithered(x - 52, y + 10, 104, 35);
  } else if (iconCode.startsWith("03") || iconCode.startsWith("04")) {
    fillCircleDithered(x - 20, y + 10, 36);
    fillCircleDithered(x + 30, y + 10, 28);
    fillCircleDithered(x + 10, y - 16, 32);
    fillRectDithered(x - 56, y + 10, 116, 40);
  } else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
    fillCircleDithered(x - 20, y - 20, 28);
    fillCircleDithered(x + 20, y - 20, 24);
    fillCircleDithered(x, y - 36, 24);
    fillRectDithered(x - 48, y - 20, 96, 28);
    for (int i = 0; i < 4; i++) {
      int dx = x - 30 + i * 20;
      display.drawLine(dx, y + 20, dx - 10, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 1, y + 20, dx - 9, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 2, y + 20, dx - 8, y + 50, GxEPD_BLUE);
      display.drawLine(dx + 3, y + 20, dx - 7, y + 50, GxEPD_BLUE);
    }
  } else if (iconCode.startsWith("11")) {
    fillCircleDithered(x - 20, y - 20, 28);
    fillCircleDithered(x + 20, y - 20, 24);
    fillCircleDithered(x, y - 36, 24);
    fillRectDithered(x - 48, y - 20, 96, 28);
    display.fillTriangle(x - 5, y + 10, x + 18, y + 10, x + 8, y + 40, GxEPD_YELLOW);
    display.fillTriangle(x + 5, y + 32, x + 28, y + 32, x - 8, y + 70, GxEPD_YELLOW);
  } else if (iconCode.startsWith("13")) {
    for (int i = 0; i < 3; i++) {
      float angle = i * PI / 3;
      display.drawLine(x - cos(angle) * 50, y - sin(angle) * 50,
                       x + cos(angle) * 50, y + sin(angle) * 50, GxEPD_BLUE);
      display.drawLine(x - cos(angle) * 50 + 1, y - sin(angle) * 50,
                       x + cos(angle) * 50 + 1, y + sin(angle) * 50, GxEPD_BLUE);
      display.drawLine(x - cos(angle) * 50 + 2, y - sin(angle) * 50,
                       x + cos(angle) * 50 + 2, y + sin(angle) * 50, GxEPD_BLUE);
    }
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
  } else if (iconCode.startsWith("50")) {
    for (int i = 0; i < 5; i++) {
      display.drawLine(x - 50, y - 30 + i * 15, x + 50, y - 30 + i * 15, GxEPD_BLACK);
      display.drawLine(x - 50, y - 30 + i * 15 + 1, x + 50, y - 30 + i * 15 + 1, GxEPD_BLACK);
      display.drawLine(x - 50, y - 30 + i * 15 + 2, x + 50, y - 30 + i * 15 + 2, GxEPD_BLACK);
    }
  } else {
    display.drawCircle(x, y, size / 2, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(x - 12, y + 12);
    u8g2Fonts.print("?");
  }
}

void clearDisplay() {
  // Flush residual charge to restore contrast on ACeP 7-color panels.
  // Does NOT power off — caller re-inits the display after this.
  Serial.println("Clearing display...");
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
  delay(500); // let the panel settle after the clear cycle
}
