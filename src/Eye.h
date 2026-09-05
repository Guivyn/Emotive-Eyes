#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <string.h>
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

// GenerateBoundary currently emits at most 25 points. Keep a little headroom
// while avoiding an unnecessarily large per-draw stack allocation.
static const int MAX_POLY_PTS = 32;

// Procedural eye rasterizer: builds a rounded-quad boundary and fills it with
// a scanline algorithm, so no image assets are required.
class EyeDrawer {
public:
  static int GenerateBoundary(const EyeConfig* cfg, float cx, float cy,
                              PolygonPoint* pts, int maxPts);

  // Generic path used by the native tests and by direct TFT rendering.
  template<typename T>
  static void RasterizePolygon(T& d, const PolygonPoint* pts, int count,
                               uint16_t color) {
    if (count < 3) return;

    float minY = pts[0].y;
    float maxY = minY;
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

  // Sprite rendering is the hot path. Avoid recalculating every polygon edge
  // for every scanline and write the already byte-swapped pixels directly to
  // the 16-bit sprite buffer. The result keeps the same even-odd scanline
  // rule as RasterizePolygon, but removes a large amount of float division and
  // per-line clipping overhead from the frame loop.
  static void RasterizeSprite(TFT_eSprite& d, const PolygonPoint* pts,
                              int count, uint16_t color) {
    if (count < 3) return;

    uint16_t* pixels = static_cast<uint16_t*>(d.getPointer());
    const int32_t width = d.width();
    const int32_t height = d.height();
    if (!pixels || width <= 0 || height <= 0) {
      RasterizePolygon(d, pts, count, color);
      return;
    }

    struct ScanEdge {
      int32_t yStart;
      int32_t yEnd;
      int32_t xAtStart;
      int32_t xStep;
    };

    ScanEdge edges[MAX_POLY_PTS];
    int edgeCount = 0;
    int32_t scanStart = height;
    int32_t scanEnd = -1;

    for (int i = 0; i < count; i++) {
      PolygonPoint a = pts[i];
      PolygonPoint b = pts[(i + 1) % count];
      if (a.y == b.y) continue;
      if (a.y > b.y) {
        PolygonPoint tmp = a;
        a = b;
        b = tmp;
      }

      const float slope = (b.x - a.x) / (b.y - a.y);
      const int32_t first = (int32_t)ceilf(a.y);
      const int32_t last = (int32_t)ceilf(b.y) - 1;
      if (last < 0 || first >= height) continue;

      const int32_t clippedFirst = first < 0 ? 0 : first;
      const int32_t clippedLast = last >= height ? height - 1 : last;
      const float xAtFirst = a.x +
          (float) (clippedFirst - a.y) * slope;
      edges[edgeCount++] = {
        clippedFirst,
        clippedLast,
        (int32_t)roundf(xAtFirst * 65536.0f),
        (int32_t)roundf(slope * 65536.0f),
      };
      if (clippedFirst < scanStart) scanStart = clippedFirst;
      if (clippedLast > scanEnd) scanEnd = clippedLast;
    }

    if (edgeCount == 0 || scanStart > scanEnd) return;

    int32_t intersections[MAX_POLY_PTS];
    const uint16_t storedColor =
        (uint16_t)((color >> 8) | (color << 8));

    for (int32_t y = scanStart; y <= scanEnd; y++) {
      int intersectionCount = 0;
      for (int i = 0; i < edgeCount; i++) {
        const ScanEdge& edge = edges[i];
        if (y < edge.yStart || y > edge.yEnd) continue;
        intersections[intersectionCount++] = edge.xAtStart +
            (y - edge.yStart) * edge.xStep;
      }

      for (int i = 1; i < intersectionCount; i++) {
        int32_t key = intersections[i];
        int j = i - 1;
        while (j >= 0 && intersections[j] > key) {
          intersections[j + 1] = intersections[j];
          j--;
        }
        intersections[j + 1] = key;
      }

      for (int i = 0; i + 1 < intersectionCount; i += 2) {
        int32_t x0 = RoundFixed(intersections[i]);
        int32_t x1 = RoundFixed(intersections[i + 1]);
        if (x1 <= x0) continue;

        if (x0 < 0) x0 = 0;
        if (x1 > width) x1 = width;
        if (x1 <= x0) continue;

        uint16_t* row = pixels + y * width + x0;
        const size_t spanPixels = (size_t)(x1 - x0);
        if ((uint8_t)storedColor == (uint8_t)(storedColor >> 8)) {
          memset(row, (uint8_t)storedColor, spanPixels * sizeof(uint16_t));
        } else {
          for (size_t x = 0; x < spanPixels; x++) {
            row[x] = storedColor;
          }
        }
      }
    }
  }

  static int32_t RoundFixed(int32_t value) {
    if (value >= 0) return (value + 0x8000) >> 16;
    return -(((-value) + 0x8000) >> 16);
  }

  // Non-template overload wins for TFT_eSprite and routes the hot path above.
  static void Draw(TFT_eSprite& dpy, float cx, float cy,
                   const EyeConfig* cfg) {
    PolygonPoint boundary[MAX_POLY_PTS];
    int n = GenerateBoundary(cfg, cx, cy, boundary, MAX_POLY_PTS);
    if (n >= 3) RasterizeSprite(dpy, boundary, n, EYE_COLOR);
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
