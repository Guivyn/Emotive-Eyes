#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Build/display constants shared by the renderer, power manager and main.
static constexpr uint16_t EYE_COLOR = TFT_WHITE;
// INVON inverts displayed colors and TFT_BGR swaps red/blue; compensate both
// so this renders as a pale warm white (approximately #FFF3E6).
static constexpr uint16_t BG_COLOR = 0x1860;
static constexpr uint16_t SCREEN_WIDTH = 320;
static constexpr uint16_t SCREEN_HEIGHT = 240;
// Render the complete display into one buffer when the heap can provide it.
// On boards without a sufficiently large contiguous block, Face falls back to
// two full-height tiles so every frame still covers the complete display.
static constexpr uint16_t FRAME_SPRITE_WIDTH = SCREEN_WIDTH;
static constexpr uint16_t FRAME_SPRITE_HEIGHT = SCREEN_HEIGHT;
static constexpr uint16_t FRAME_TILE_WIDTH = SCREEN_WIDTH / 2;
#ifndef POWER_ACTIVE_FRAME_INTERVAL_MS
  #if defined(PERF_TARGET_60FPS)
    #define POWER_ACTIVE_FRAME_INTERVAL_MS 16
  #elif defined(PERF_TARGET_50FPS)
    #define POWER_ACTIVE_FRAME_INTERVAL_MS 20
  #elif defined(PERF_TARGET_45FPS)
    #define POWER_ACTIVE_FRAME_INTERVAL_MS 22
  #else
    #define POWER_ACTIVE_FRAME_INTERVAL_MS 25
  #endif
#endif
#ifndef POWER_RELAX_FRAME_INTERVAL_MS
  #define POWER_RELAX_FRAME_INTERVAL_MS 25
#endif
#ifndef POWER_IDLE_FRAME_INTERVAL_MS
  #define POWER_IDLE_FRAME_INTERVAL_MS 33
#endif
static constexpr uint32_t FRAME_INTERVAL_ACTIVE_MS = POWER_ACTIVE_FRAME_INTERVAL_MS;
static constexpr uint32_t FRAME_INTERVAL_RELAX_MS = POWER_RELAX_FRAME_INTERVAL_MS;
static constexpr uint32_t FRAME_INTERVAL_IDLE_MS = POWER_IDLE_FRAME_INTERVAL_MS;
static constexpr uint32_t SLEEP_POLL_INTERVAL_MS = 50;
static constexpr uint8_t EYES_FRAME_MS_MIN = FRAME_INTERVAL_ACTIVE_MS;
static constexpr unsigned long STARTUP_TFT_SETTLE_MS = 20;
static constexpr unsigned long STARTUP_FACE_WAKE_MS = 500;

static const uint16_t TOUCH_PRESSURE_THRESHOLD = 600;
static const uint16_t TOUCH_PRESSURE_RELEASE_THRESHOLD = 400;
static const uint8_t TOUCH_STABLE_SAMPLES = 2;
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
  #define SERIAL_DEBUG_ENABLED 0
#endif
