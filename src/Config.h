#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Build/display constants shared by the renderer, power manager and main.
static constexpr uint16_t EYE_COLOR = TFT_WHITE;
static constexpr uint16_t BG_COLOR = TFT_BLACK;
static constexpr uint16_t SCREEN_WIDTH = 320;
static constexpr uint16_t SCREEN_HEIGHT = 240;
static constexpr uint16_t SPRITE_HEIGHT = 120;
static constexpr uint8_t EYES_FRAME_MS_MIN = 25; // 40 FPS active floor

static const uint16_t TOUCH_PRESSURE_THRESHOLD = 350;
static const uint8_t TAP_COUNT_FOR_ANGER = 3;
static const unsigned long TAP_WINDOW_MS = 1500;
static const unsigned long DOUBLE_TAP_WINDOW_MS = 300;
static const unsigned long LONG_PRESS_MS = 2000;
static const uint16_t TOUCH_SWIPE_THRESHOLD = 25;
static const unsigned long TOUCH_POLL_INTERVAL_MS = 30; // throttle SPI reads
// Raw touch -> screen calibration defaults (adjustable at runtime via the
// serial "cal <offset> <range>" command; persisted to LittleFS).
static const int32_t TOUCH_CAL_OFFSET = 300;
static const int32_t TOUCH_CAL_RANGE  = 3600;
static const unsigned long SLEEP_ENTER_DELAY_MS = 2000;
static const unsigned long BACKLIGHT_STEP_MS = 20;

#ifndef TFT_BL
  #define TFT_BL 27
#endif

#ifndef SERIAL_DEBUG_ENABLED
  #define SERIAL_DEBUG_ENABLED 1
#endif
