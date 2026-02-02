/*
 * Pin Configuration for ESP32-S3 + Waveshare 7.3" (F) 7-Color Display
 * 
 * SPI Interface Pins:
 */

#ifndef PINS_H
#define PINS_H

// E-ink Display SPI Pins
#define EPD_MOSI    11  // DIN (Master Out Slave In)
#define EPD_MISO    13  // DOUT (Master In Slave Out) - not used by e-ink but needed for SPI
#define EPD_SCK     12  // CLK (Clock)
#define EPD_CS      10  // CS (Chip Select)
#define EPD_DC      8   // DC (Data/Command)
#define EPD_RST     9   // RST (Reset)
#define EPD_BUSY    7   // BUSY (status pin)

// Optional: Battery monitoring
#define BATTERY_PIN 1   // ADC pin for battery voltage

// Optional: Status LED
#define LED_PIN     48  // Built-in LED on most ESP32-S3 boards

#endif
