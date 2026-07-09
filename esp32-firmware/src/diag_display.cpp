/*
 * diag_display.cpp — Standalone ACeP panel diagnostic (NOT part of the firmware).
 *
 * Flash:   pio run -e diagnostic -t upload && pio device monitor -e diagnostic
 *
 * Tests the two non-wiring suspects for "clear during refresh, faint after finish":
 *   1. DRIVER-CLASS / waveform match — switch DIAG_PANEL (0/1/2) and see which one
 *      latches a crisp image that HOLDS.
 *   2. hibernate() TIMING — it draws the bars, holds them 30 s WITHOUT hibernating,
 *      then calls hibernate(); watch whether the image changes at each step.
 *   It also times the refresh (a real full refresh is ~25-35 s; a few seconds means
 *   the driver finished early → BUSY/waveform issue).
 *
 * Select the panel driver to test (rebuild/flash for each):
 *   DIAG_PANEL 0 = GDEY073D46 (what the firmware uses now)
 *   DIAG_PANEL 1 = ACeP_730   (Waveshare 7.3" ACeP)
 *   DIAG_PANEL 2 = GDEP073E01
 * Override without editing:  PLATFORMIO_BUILD_FLAGS="-DDIAG_PANEL=1" pio run -e diagnostic -t upload
 */

#include "pins.h"
#include <Arduino.h>
#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>

#ifndef DIAG_PANEL
#define DIAG_PANEL 0
#endif

#if DIAG_PANEL == 1
  #define PANEL GxEPD2_730c_ACeP_730
  #define PANEL_NAME "ACeP_730 (Waveshare 7.3\" F)"
#elif DIAG_PANEL == 2
  #define PANEL GxEPD2_730c_GDEP073E01
  #define PANEL_NAME "GDEP073E01"
#else
  #define PANEL GxEPD2_730c_GDEY073D46
  #define PANEL_NAME "GDEY073D46 (current firmware)"
#endif

GxEPD2_7C<PANEL, PANEL::HEIGHT> display(PANEL(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

static void drawColorBars() {
  const uint16_t colors[7] = {GxEPD_BLACK, GxEPD_WHITE, GxEPD_RED,
                              GxEPD_GREEN, GxEPD_BLUE, GxEPD_YELLOW, GxEPD_ORANGE};
  const char *names[7] = {"BLACK", "WHITE", "RED", "GREEN", "BLUE", "YELLOW", "ORANGE"};
  const int n  = 7;
  const int bw = 800 / n;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int i = 0; i < n; i++) {
      int x = i * bw;
      int w = (i == n - 1) ? (800 - x) : bw;
      display.fillRect(x, 0, w, 430, colors[i]);
      uint16_t txt = (colors[i] == GxEPD_WHITE || colors[i] == GxEPD_YELLOW ||
                      colors[i] == GxEPD_ORANGE)
                         ? GxEPD_BLACK
                         : GxEPD_WHITE;
      u8g2Fonts.setForegroundColor(txt);
      u8g2Fonts.setBackgroundColor(colors[i]);
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      int tw = u8g2Fonts.getUTF8Width(names[i]);
      u8g2Fonts.setCursor(x + (w - tw) / 2, 220);
      u8g2Fonts.print(names[i]);
    }
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setCursor(20, 462);
    u8g2Fonts.print("Driver: " PANEL_NAME);
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== PANEL DRIVER TEST: %s ===\n", PANEL_NAME);

  display.init(115200, true, 20, false);
  u8g2Fonts.begin(display);

  Serial.println("Full-refresh color bars...");
  uint32_t t0 = millis();
  drawColorBars();
  uint32_t dt = millis() - t0;
  Serial.printf("Refresh took %lu ms  (real full refresh ~25000-35000 ms; "
                "a few thousand ms = finished early -> BUSY/waveform).\n", dt);

  Serial.println(">>> STEP A: image drawn, NOT hibernated. Watch the panel for 30 s...");
  delay(30000);

  Serial.println(">>> STEP B: calling hibernate() now — watch if the image fades/changes.");
  display.hibernate();
  delay(3000);

  Serial.println("DONE. Report back:");
  Serial.println("  1) During STEP A (before hibernate): bars CRISP or FAINT?");
  Serial.println("  2) At STEP B (hibernate): did they change/fade?");
  Serial.println("  3) Refresh ms printed above.");
}

void loop() {}
