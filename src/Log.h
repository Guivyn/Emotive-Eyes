#pragma once

#include <Arduino.h>
#include "Config.h"

#if SERIAL_DEBUG_ENABLED
  #define LOG_PRINTF(...)    do { Serial.printf(__VA_ARGS__); } while (0)
  #define LOG_PRINT(...)     do { Serial.print(__VA_ARGS__); } while (0)
  #define LOG_PRINTLN(...)   do { Serial.println(__VA_ARGS__); } while (0)
#else
  #define LOG_PRINTF(...)    do {} while (0)
  #define LOG_PRINT(...)     do {} while (0)
  #define LOG_PRINTLN(...)   do {} while (0)
#endif
