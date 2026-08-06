#pragma once

#include <Arduino.h>
#include "Animation.h"
#include "Config.h"

struct EyeConfig {
  int16_t OffsetX, OffsetY, Height, Width;
  float Slope_Top, Slope_Bottom;
  int16_t Radius_Top, Radius_Bottom;
};

struct Transformation {
  float MoveX = 0.0f, MoveY = 0.0f, ScaleX = 1.0f, ScaleY = 1.0f;
};

class EyeTransition {
public:
  EyeTransition();

  EyeConfig Output, Snapshot, Destin;
  bool Active;
  RampAnimation Animation;

  void Update();
  void Start();
  void Apply(float t);
};

class EyeTransformation {
public:
  EyeTransformation();

  EyeConfig* Input;
  EyeConfig Output;
  Transformation Origin, Destin;
  RampAnimation Animation;

  void Update();
  void Apply();
  void SetDestin(Transformation t);
  void Sync();
};

class EyeVariation {
public:
  EyeVariation();

  EyeConfig* Input;
  EyeConfig Output;
  TrapeziumPulseAnimation Animation;
  EyeConfig Values;
  EyeConfig FadeValues;
  unsigned long FadeStart;
  bool IsFading;

  void Clear();
  void Update();
  void Apply(float t);
  void StartFadeOut();
  bool IsActive() const;
};

class EyeBlink {
public:
  EyeBlink();

  EyeConfig* Input;
  EyeConfig Output;
  TrapeziumAnimation Animation;
  int32_t BlinkWidth = 60;

  void Update();
  void Apply(float t);
};

struct PolygonPoint {
  float x, y;
};

static const int MAX_POLY_PTS = 64;

// Procedural eye rasterizer: builds a rounded-quad boundary and fills it with
// a scanline algorithm, so no image assets are required.
class EyeDrawer {
public:
  static int GenerateBoundary(const EyeConfig* cfg, float cx, float cy,
                              PolygonPoint* pts, int maxPts);

  template<typename T>
  static void RasterizePolygon(T& d, const PolygonPoint* pts, int count,
                               uint16_t color) {
    if (count < 3) return;

    float minY = pts[0].y;
    float maxY = pts[0].y;
    for (int i = 1; i < count; i++) {
      if (pts[i].y < minY) minY = pts[i].y;
      if (pts[i].y > maxY) maxY = pts[i].y;
    }
    if (minY < 0.0f) minY = 0.0f;
    if (maxY >= SCREEN_HEIGHT) maxY = SCREEN_HEIGHT - 1;

    float xs[MAX_POLY_PTS];
    for (int y = (int)minY; y <= (int)maxY; y++) {
      float fy = (float)y;
      int n = 0;
      for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        float x1 = pts[i].x, y1 = pts[i].y;
        float x2 = pts[j].x, y2 = pts[j].y;
        if ((y1 <= fy && y2 > fy) || (y2 <= fy && y1 > fy)) {
          float dx = x2 - x1;
          float dy = y2 - y1;
          float x = x1 + dx * (fy - y1) / dy;
          if (n < MAX_POLY_PTS) xs[n++] = x;
        }
      }

      if (n == 0) continue;
      for (int i = 1; i < n; i++) {
        float key = xs[i];
        int j = i - 1;
        while (j >= 0 && xs[j] > key) {
          xs[j + 1] = xs[j];
          j--;
        }
        xs[j + 1] = key;
      }
      for (int i = 0; i + 1 < n; i += 2) {
        int x0 = (int)roundf(xs[i]);
        int x1 = (int)roundf(xs[i + 1]);
        if (x1 > x0) d.drawFastHLine(x0, y, x1 - x0, color);
      }
    }
  }

  template<typename T>
  static void Draw(T& dpy, float cx, float cy, const EyeConfig* cfg) {
    PolygonPoint boundary[MAX_POLY_PTS];
    int n = GenerateBoundary(cfg, cx, cy, boundary, MAX_POLY_PTS);
    if (n >= 3) RasterizePolygon(dpy, boundary, n, EYE_COLOR);
  }
};

class Eye {
public:
  Eye();

  int16_t CenterX, CenterY;
  bool IsMirrored;
  EyeTransition Transition;
  EyeTransformation Transformation;
  EyeVariation Variation1, Variation2;
  EyeBlink BlinkTransformation;
  EyeConfig* FinalConfig;

  void ChainOperators();
  bool NeedsRedraw() const;
  void Update();
  void TransitionTo(const EyeConfig config);
};
