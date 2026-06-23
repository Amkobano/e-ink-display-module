// page_dua.cpp
// Renders Page 1: loads a pre-rendered Arabic dua BMP from LittleFS and
// displays it full-screen. Images are generated offline by render_dua_images.py.
// No text rendering or JSON parsing happens here — the firmware just displays
// a pixel-perfect 800×480 1-bit BMP that was crafted with a proper Arabic font.

#include "page_dua.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// -----------------------------------------------------------------------
// BMP helpers
// -----------------------------------------------------------------------

// Read a little-endian 16-bit value from a byte array at offset.
static uint16_t read16(const uint8_t *buf, int offset) {
  return (uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8);
}

// Read a little-endian 32-bit value from a byte array at offset.
static uint32_t read32(const uint8_t *buf, int offset) {
  return (uint32_t)buf[offset]       | ((uint32_t)buf[offset + 1] << 8) |
         ((uint32_t)buf[offset + 2] << 16) | ((uint32_t)buf[offset + 3] << 24);
}

// -----------------------------------------------------------------------
// Main dua page renderer
// Loads dua_XXX.bmp from LittleFS into PSRAM, then renders via GxEPD2.
// -----------------------------------------------------------------------
void displayDua(uint8_t duaIndex) {
  Serial.printf("Rendering page 1: dua index %d\n", duaIndex);

  if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("LittleFS mount failed!");
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      u8g2Fonts.begin(display);
      u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      u8g2Fonts.setCursor(40, 240);
      u8g2Fonts.print("LittleFS not mounted.");
      u8g2Fonts.setCursor(40, 275);
      u8g2Fonts.print("Run: pio run -t uploadfs");
    } while (display.nextPage());
    return;
  }

  char path[20];
  snprintf(path, sizeof(path), "/dua_%03d.bmp", duaIndex);
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("Cannot open %s\n", path);
    LittleFS.end();
    return;
  }

  // ---- Parse BMP header (54 bytes) ----
  uint8_t header[54];
  if (f.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') {
    Serial.println("Invalid BMP header");
    f.close();
    LittleFS.end();
    return;
  }

  uint32_t dataOffset = read32(header, 10);
  int32_t  bmpW       = (int32_t)read32(header, 18);
  int32_t  bmpH       = (int32_t)read32(header, 22); // negative = top-down
  uint16_t bpp        = read16(header, 28);
  bool     topDown    = (bmpH < 0);
  if (topDown) bmpH   = -bmpH;

  Serial.printf("BMP: %dx%d  bpp=%d  offset=%d  topDown=%d\n",
                bmpW, bmpH, bpp, dataOffset, topDown);

  if (bpp != 1) {
    Serial.println("Only 1-bit BMP supported");
    f.close();
    LittleFS.end();
    return;
  }

  // 1bpp rows are padded to 4-byte boundary
  uint32_t rowStride = ((uint32_t)(bmpW + 31) / 32) * 4;
  uint32_t dataSize  = rowStride * (uint32_t)bmpH;

  // ---- Load pixel data into PSRAM ----
  // Seek to the pixel data section
  f.seek(dataOffset);
  uint8_t *buf = (uint8_t *)heap_caps_malloc(dataSize, MALLOC_CAP_SPIRAM);
  if (!buf) {
    Serial.printf("PSRAM alloc failed (%u bytes), trying heap\n", dataSize);
    buf = (uint8_t *)malloc(dataSize);
  }
  if (!buf) {
    Serial.printf("Heap alloc also failed (%u bytes)\n", dataSize);
    f.close();
    LittleFS.end();
    return;
  }
  f.read(buf, dataSize);
  f.close();
  LittleFS.end();

  Serial.printf("Loaded %u bytes into PSRAM\n", dataSize);

  // ---- Render via GxEPD2 page loop ----
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    for (int y = 0; y < bmpH; y++) {
      // BMP rows are stored bottom-to-top unless topDown flag is set
      int bmpRow = topDown ? y : (bmpH - 1 - y);
      const uint8_t *row = buf + (uint32_t)bmpRow * rowStride;

      for (int x = 0; x < bmpW; x++) {
        // Extract bit: MSB first within each byte
        uint8_t byte = row[x / 8];
        bool    isWhite = (byte >> (7 - (x % 8))) & 1;
        if (!isWhite) {
          display.drawPixel(x, y, GxEPD_BLACK);
        }
        // White pixels are already filled by fillScreen — skip for speed
      }
    }

  } while (display.nextPage());

  heap_caps_free(buf);
  Serial.println("Page 1 rendered.");
}

