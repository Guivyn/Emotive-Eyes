#pragma once

#include <string.h>
#include <stdint.h>

#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0x0000

class TFT_eSprite {
public:
  void drawFastHLine(int x, int y, int w, uint16_t color) {
    if (y < 0 || y >= kHeight || w <= 0 || x >= kWidth) return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (x + w > kWidth) w = kWidth - x;
    if (w <= 0) return;
    uint16_t* row = Pixels() + y * kWidth + x;
    for (int i = 0; i < w; i++) row[i] = color;
  }
  void fillSprite(uint16_t color) {
    for (int i = 0; i < kWidth * kHeight; i++) Pixels()[i] = color;
  }
  void pushSprite(int, int) {}
  void* getPointer() { return Pixels(); }
  int16_t width() const { return kWidth; }
  int16_t height() const { return kHeight; }

private:
  static const int kWidth = 160;
  static const int kHeight = 240;

  static uint16_t* Pixels() {
    static uint16_t pixels[kWidth * kHeight];
    return pixels;
  }
};
