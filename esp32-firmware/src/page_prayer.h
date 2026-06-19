#pragma once
#include "globals.h"

void displayPrayerTimes(const PrayerTimes &pt, const WeatherData &wd, const ForecastDay forecast[3]);
void displayError(const char *errorMsg);
