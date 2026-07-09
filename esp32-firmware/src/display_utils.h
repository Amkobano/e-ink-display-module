#pragma once
#include "globals.h"

void fillCircleDithered(int cx, int cy, int radius);
void fillRectDithered(int x, int y, int w, int h);
void drawSmallWeatherIcon(int x, int y, String condition);
void drawWeatherIcon(int x, int y, String iconCode);
void drawBatteryIcon(int x, int y, int pct);
void clearDisplay();
