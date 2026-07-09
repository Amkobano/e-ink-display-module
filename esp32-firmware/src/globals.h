#pragma once

#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "pins.h"

// Display and font objects defined in main.cpp, shared across all page files.
// Panel: Waveshare 7.3" (F) 7-color ACeP — the GDEP073E01 waveform is the correct
// match (GDEY073D46 only latched marginally, causing faint/intermittent images).
extern GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Shared data structures (also used as RTC_DATA_ATTR in main.cpp)
struct PrayerTimes {
  char fajr[8]    = "N/A";
  char shuruq[8]  = "N/A";
  char dhuhr[8]   = "N/A";
  char asr[8]     = "N/A";
  char maghrib[8] = "N/A";
  char isha[8]    = "N/A";
  char location[64] = "";
};

struct WeatherData {
  int   temperature = 0;
  char  condition[32] = "N/A";
  float windSpeed = 0.0f;
  char  icon[8] = "";
};

struct ForecastDay {
  char date[12] = "";
  int  temperature = 0;
  int  rainChance = 0;
  char condition[32] = "";
};
