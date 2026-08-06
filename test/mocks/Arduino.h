#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef PI
  #define PI 3.14159265358979323846
#endif

extern unsigned long g_mockMillis;

inline unsigned long millis() { return g_mockMillis; }
inline long random(long min, long max) { return min; }
inline float cosf(float x) { return cos(x); }
inline float sinf(float x) { return sin(x); }
