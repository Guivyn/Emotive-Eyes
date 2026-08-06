#pragma once

#include <stdint.h>

#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0x0000

class TFT_eSprite {
public:
  void drawFastHLine(int, int, int, uint16_t) {}
  void fillSprite(uint16_t) {}
  void pushSprite(int, int) {}
};
