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
  int32_t BlinkWidth = 72;

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

  struct ScanEdge {
    int32_t yStart;
    int32_t yEnd;
    int32_t xAtStart;
    int32_t xStep;
  };

  struct ScanlineSet {
    ScanEdge edges[MAX_POLY_PTS];
    int count;
    int32_t yStart;
    int32_t yEnd;
  };

  struct ConvexScanlineSet {
    ScanEdge edges[MAX_POLY_PTS];
    int count;
    int firstChainCount;
    int secondChainStart;
    int secondChainCount;
    int32_t yStart;
    int32_t yEnd;
  };

  static const int COVERAGE_SAMPLE_COUNT = 8;

  struct CoverageInterval {
    int32_t left;
    int32_t right;
    bool valid;
  };

  struct CoverageRow {
    int32_t left[COVERAGE_SAMPLE_COUNT];
    int32_t right[COVERAGE_SAMPLE_COUNT];
    bool valid[COVERAGE_SAMPLE_COUNT];
    int32_t firstPixel;
    int32_t endPixel;
    int32_t fullStart;
    int32_t fullEnd;
    bool hasFull;
  };

  // Eight vertical samples turn each output row into a small area-coverage
  // estimate. Horizontal interiors remain a single fast-filled run.
  static float SampleOffset(int index) {
    return ((float)index + 0.5f) / COVERAGE_SAMPLE_COUNT;
  }

  static bool BuildScanlineSet(const PolygonPoint* pts, int count,
                               float sampleOffset, int32_t width,
                               int32_t height, ScanlineSet& set) {
    set.count = 0;
    set.yStart = height;
    set.yEnd = -1;
    if (!pts || count < 3 || count > MAX_POLY_PTS || width <= 0 ||
        height <= 0) return false;

    for (int i = 0; i < count; i++) {
      PolygonPoint a = pts[i];
      PolygonPoint b = pts[(i + 1) % count];
      if (a.y == b.y) continue;
      if (a.y > b.y) {
        PolygonPoint tmp = a;
        a = b;
        b = tmp;
      }

      const int32_t first = (int32_t)ceilf(a.y - sampleOffset);
      const int32_t last = (int32_t)ceilf(b.y - sampleOffset) - 1;
      if (last < 0 || first >= height) continue;

      const int32_t clippedFirst = first < 0 ? 0 : first;
      const int32_t clippedLast = last >= height ? height - 1 : last;
      const float slope = (b.x - a.x) / (b.y - a.y);
      const float sampleY = (float)clippedFirst + sampleOffset;
      const float xAtFirst = a.x + (sampleY - a.y) * slope;
      ScanEdge& edge = set.edges[set.count++];
      edge.yStart = clippedFirst;
      edge.yEnd = clippedLast;
      edge.xAtStart = (int32_t)roundf(xAtFirst * 65536.0f);
      edge.xStep = (int32_t)roundf(slope * 65536.0f);
      if (clippedFirst < set.yStart) set.yStart = clippedFirst;
      if (clippedLast > set.yEnd) set.yEnd = clippedLast;
    }
    return set.count != 0;
  }

  // The generated eye boundary is a simple polygon. A convex polygon keeps
  // every vertex on the same side of each directed boundary edge.
  static bool IsConvexPolygon(const PolygonPoint* pts, int count) {
    if (!pts || count < 3 || count > MAX_POLY_PTS) return false;
    float area = 0.0f;
    for (int i = 0; i < count; i++) {
      const PolygonPoint& a = pts[i];
      const PolygonPoint& b = pts[(i + 1) % count];
      area += a.x * b.y - b.x * a.y;
    }
    if (area > -0.0001f && area < 0.0001f) return false;
    const float orientation = area > 0.0f ? 1.0f : -1.0f;

    for (int edgeIndex = 0; edgeIndex < count; edgeIndex++) {
      const PolygonPoint& a = pts[edgeIndex];
      const PolygonPoint& b = pts[(edgeIndex + 1) % count];
      const float dx = b.x - a.x;
      const float dy = b.y - a.y;
      const float epsilon = 0.0001f * (fabsf(dx) + fabsf(dy) + 1.0f);
      for (int pointIndex = 0; pointIndex < count; pointIndex++) {
        const PolygonPoint& p = pts[pointIndex];
        const float cross = dx * (p.y - a.y) - dy * (p.x - a.x);
        if (orientation * cross < -epsilon) return false;
      }
    }
    return true;
  }

  static bool AppendConvexScanEdge(ConvexScanlineSet& set,
                                   PolygonPoint a, PolygonPoint b,
                                   float sampleOffset, int32_t height) {
    if (a.y == b.y) return true;
    if (a.y > b.y) {
      PolygonPoint tmp = a;
      a = b;
      b = tmp;
    }

    const int32_t first = (int32_t)ceilf(a.y - sampleOffset);
    const int32_t last = (int32_t)ceilf(b.y - sampleOffset) - 1;
    if (last < 0 || first >= height) return true;

    const int32_t clippedFirst = first < 0 ? 0 : first;
    const int32_t clippedLast = last >= height ? height - 1 : last;
    if (set.count >= MAX_POLY_PTS) return false;
    const float slope = (b.x - a.x) / (b.y - a.y);
    const float sampleY = (float)clippedFirst + sampleOffset;
    const float xAtFirst = a.x + (sampleY - a.y) * slope;
    ScanEdge& edge = set.edges[set.count++];
    edge.yStart = clippedFirst;
    edge.yEnd = clippedLast;
    edge.xAtStart = (int32_t)roundf(xAtFirst * 65536.0f);
    edge.xStep = (int32_t)roundf(slope * 65536.0f);
    if (clippedFirst < set.yStart) set.yStart = clippedFirst;
    if (clippedLast > set.yEnd) set.yEnd = clippedLast;
    return true;
  }

  static bool AppendConvexChain(const PolygonPoint* pts, int count,
                               int topIndex, int bottomIndex, bool forward,
                               float sampleOffset, int32_t height,
                               ConvexScanlineSet& set) {
    int index = topIndex;
    while (index != bottomIndex) {
      const int next = forward ? (index + 1) % count
                               : (index == 0 ? count - 1 : index - 1);
      if (pts[next].y < pts[index].y) return false;
      if (!AppendConvexScanEdge(set, pts[index], pts[next], sampleOffset,
                                height)) return false;
      index = next;
    }
    return true;
  }

  static bool BuildConvexScanlineSet(const PolygonPoint* pts, int count,
                                     float sampleOffset, int32_t width,
                                     int32_t height,
                                     ConvexScanlineSet& set) {
    set.count = 0;
    set.firstChainCount = 0;
    set.secondChainStart = 0;
    set.secondChainCount = 0;
    set.yStart = height;
    set.yEnd = -1;
    if (!pts || count < 3 || count > MAX_POLY_PTS || width <= 0 ||
        height <= 0) return false;

    int topIndex = 0;
    int bottomIndex = 0;
    for (int i = 1; i < count; i++) {
      if (pts[i].y < pts[topIndex].y) topIndex = i;
      if (pts[i].y > pts[bottomIndex].y) bottomIndex = i;
    }
    if (topIndex == bottomIndex) return false;

    if (!AppendConvexChain(pts, count, topIndex, bottomIndex, true,
                           sampleOffset, height, set)) return false;
    set.firstChainCount = set.count;
    set.secondChainStart = set.count;
    if (!AppendConvexChain(pts, count, topIndex, bottomIndex, false,
                           sampleOffset, height, set)) return false;
    set.secondChainCount = set.count - set.secondChainStart;
    return set.firstChainCount > 0 && set.secondChainCount > 0;
  }

  static bool CollectConvexChainX(const ConvexScanlineSet& set,
                                  int chainStart, int chainCount, int32_t y,
                                  int& edgeCursor, int32_t& x) {
    while (edgeCursor < chainCount &&
           y > set.edges[chainStart + edgeCursor].yEnd) {
      edgeCursor++;
    }
    if (edgeCursor >= chainCount) return false;
    const ScanEdge& edge = set.edges[chainStart + edgeCursor];
    if (y < edge.yStart || y > edge.yEnd) return false;
    x = edge.xAtStart + (y - edge.yStart) * edge.xStep;
    return true;
  }

  static int CollectIntersections(const ScanlineSet& set, int32_t y,
                                  int32_t* intersections) {
    int count = 0;
    for (int i = 0; i < set.count; i++) {
      const ScanEdge& edge = set.edges[i];
      if (y < edge.yStart || y > edge.yEnd) continue;
      intersections[count++] = edge.xAtStart +
          (y - edge.yStart) * edge.xStep;
    }
    for (int i = 1; i < count; i++) {
      const int32_t key = intersections[i];
      int j = i - 1;
      while (j >= 0 && intersections[j] > key) {
        intersections[j + 1] = intersections[j];
        j--;
      }
      intersections[j + 1] = key;
    }
    return count;
  }

  static bool BuildCoverageRow(const CoverageInterval* intervals,
                               int32_t width, CoverageRow& row) {
    if (!intervals || width <= 0) return false;
    const int32_t maxX = width << 16;
    int32_t minLeft = maxX;
    int32_t maxRight = 0;
    int32_t fullStart = 0;
    int32_t fullEnd = width;
    int validCount = 0;
    bool allSamplesValid = true;
    for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
      const int32_t left = intervals[sample].left;
      const int32_t right = intervals[sample].right;
      row.left[sample] = left < 0 ? 0 : (left > maxX ? maxX : left);
      row.right[sample] = right < 0 ? 0 : (right > maxX ? maxX : right);
      row.valid[sample] = intervals[sample].valid &&
          row.right[sample] > row.left[sample];
      if (!row.valid[sample]) {
        allSamplesValid = false;
        continue;
      }
      validCount++;
      if (row.left[sample] < minLeft) minLeft = row.left[sample];
      if (row.right[sample] > maxRight) maxRight = row.right[sample];

      const int32_t sampleFullStart = (row.left[sample] + 0xFFFF) >> 16;
      const int32_t sampleFullEnd = row.right[sample] >> 16;
      if (sampleFullStart > fullStart) fullStart = sampleFullStart;
      if (sampleFullEnd < fullEnd) fullEnd = sampleFullEnd;
    }
    if (validCount == 0) return false;

    row.firstPixel = minLeft >> 16;
    row.endPixel = (maxRight + 0xFFFF) >> 16;
    row.fullStart = fullStart;
    row.fullEnd = fullEnd;
    row.hasFull = allSamplesValid && row.fullEnd > row.fullStart;
    return true;
  }

  static uint8_t CoverageAtPixel(const CoverageRow& row, int32_t x) {
    const int32_t pixelLeft = x << 16;
    const int32_t pixelRight = pixelLeft + 0x10000;
    uint32_t total = 0;
    for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
      if (!row.valid[sample]) continue;
      int32_t overlapLeft = row.left[sample] > pixelLeft
          ? row.left[sample] : pixelLeft;
      int32_t overlapRight = row.right[sample] < pixelRight
          ? row.right[sample] : pixelRight;
      if (overlapRight > overlapLeft) {
        total += (uint32_t)(overlapRight - overlapLeft);
      }
    }
    const uint32_t totalArea = 65536U * COVERAGE_SAMPLE_COUNT;
    return (uint8_t)((total * 255U + totalArea / 2U) / totalArea);
  }

  // The eye interior is white on a black framebuffer. Convert coverage from
  // linear light to an sRGB-like intensity before packing RGB565 so partial
  // edge pixels do not appear too dark on the display.
  static uint8_t CoverageToIntensity(uint8_t coverage) {
    uint16_t remainder = (uint16_t)coverage * 255U;
    uint16_t root = 0;
    uint16_t bit = 1U << 14;
    while (bit > remainder) bit >>= 2;
    while (bit != 0) {
      if (remainder >= root + bit) {
        remainder -= root + bit;
        root = (root >> 1) + bit;
      } else {
        root >>= 1;
      }
      bit >>= 2;
    }
    return (uint8_t)root;
  }

  static uint16_t CoverageColor565(uint16_t color, uint8_t coverage) {
    if (coverage == 255) return color;
    const uint32_t intensity = CoverageToIntensity(coverage);
    const uint32_t r = ((color >> 11) & 0x1F) * intensity;
    const uint32_t g = ((color >> 5) & 0x3F) * intensity;
    const uint32_t b = (color & 0x1F) * intensity;
    return (uint16_t)((((r + 127U) / 255U) << 11) |
                      (((g + 127U) / 255U) << 5) |
                      ((b + 127U) / 255U));
  }

  template<typename T>
  static void DrawCoverageEdgeRange(T& d, int32_t y, int32_t first,
                                    int32_t end, const CoverageRow& row,
                                    uint16_t color) {
    for (int32_t x = first; x < end; x++) {
      const uint8_t coverage = CoverageAtPixel(row, x);
      if (coverage) {
        d.drawFastHLine(x, y, 1, CoverageColor565(color, coverage));
      }
    }
  }

  template<typename T>
  static void RasterizeCoverageRow(T& d, int32_t y,
                                  const CoverageInterval* intervals,
                                  int32_t width, uint16_t color) {
    CoverageRow row;
    if (!BuildCoverageRow(intervals, width, row)) return;
    if (!row.hasFull) {
      DrawCoverageEdgeRange(d, y, row.firstPixel, row.endPixel, row, color);
      return;
    }
    DrawCoverageEdgeRange(d, y, row.firstPixel, row.fullStart, row, color);
    d.drawFastHLine(row.fullStart, y, row.fullEnd - row.fullStart, color);
    DrawCoverageEdgeRange(d, y, row.fullEnd, row.endPixel, row, color);
  }

  static uint16_t ToStoredPixel565(uint16_t color, uint8_t coverage) {
    const uint16_t blended = CoverageColor565(color, coverage);
    return (uint16_t)((blended >> 8) | (blended << 8));
  }

  static void FillSpriteSpan(uint16_t* row, int32_t x, int32_t length,
                             uint16_t storedColor) {
    if (length <= 0) return;
    if ((uint8_t)storedColor == (uint8_t)(storedColor >> 8)) {
      memset(row + x, (uint8_t)storedColor,
             (size_t)length * sizeof(uint16_t));
    } else {
      for (int32_t i = 0; i < length; i++) row[x + i] = storedColor;
    }
  }

  static void DrawSpriteCoverageEdgeRange(uint16_t* row, int32_t first,
                                          int32_t end,
                                          const CoverageRow& coverageRow,
                                          uint16_t color) {
    for (int32_t x = first; x < end; x++) {
      const uint8_t coverage = CoverageAtPixel(coverageRow, x);
      if (coverage) row[x] = ToStoredPixel565(color, coverage);
    }
  }

  static void RasterizeSpriteCoverageRow(uint16_t* pixels, int32_t width,
                                         int32_t y,
                                         const CoverageInterval* intervals,
                                         uint16_t color,
                                         uint16_t storedColor) {
    CoverageRow row;
    if (!BuildCoverageRow(intervals, width, row)) return;
    uint16_t* out = pixels + y * width;
    if (!row.hasFull) {
      DrawSpriteCoverageEdgeRange(out, row.firstPixel, row.endPixel, row,
                                  color);
      return;
    }
    DrawSpriteCoverageEdgeRange(out, row.firstPixel, row.fullStart, row,
                                color);
    FillSpriteSpan(out, row.fullStart, row.fullEnd - row.fullStart,
                   storedColor);
    DrawSpriteCoverageEdgeRange(out, row.fullEnd, row.endPixel, row, color);
  }

  template<typename DrawRow>
  static bool RasterizeConvexScanlines(const PolygonPoint* pts, int count,
                                       int32_t width, int32_t height,
                                       DrawRow drawRow) {
    ConvexScanlineSet samples[COVERAGE_SAMPLE_COUNT];
    int32_t yStart = height;
    int32_t yEnd = -1;
    for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
      if (!BuildConvexScanlineSet(pts, count, SampleOffset(sample), width,
                                  height, samples[sample])) return false;
      if (samples[sample].yStart < yStart) yStart = samples[sample].yStart;
      if (samples[sample].yEnd > yEnd) yEnd = samples[sample].yEnd;
    }
    if (yEnd < yStart) return true;

    int firstChainCursor[COVERAGE_SAMPLE_COUNT] = {};
    int secondChainCursor[COVERAGE_SAMPLE_COUNT] = {};
    CoverageInterval intervals[COVERAGE_SAMPLE_COUNT];
    for (int32_t y = yStart; y <= yEnd; y++) {
      for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
        int32_t x0 = 0;
        int32_t x1 = 0;
        const bool active0 = CollectConvexChainX(samples[sample], 0,
            samples[sample].firstChainCount, y,
            firstChainCursor[sample], x0);
        const bool active1 = CollectConvexChainX(samples[sample],
            samples[sample].secondChainStart,
            samples[sample].secondChainCount, y,
            secondChainCursor[sample], x1);
        intervals[sample].valid = active0 && active1;
        if (intervals[sample].valid) {
          intervals[sample].left = x0 < x1 ? x0 : x1;
          intervals[sample].right = x0 < x1 ? x1 : x0;
        } else {
          intervals[sample].left = 0;
          intervals[sample].right = 0;
        }
      }
      drawRow(y, intervals);
    }
    return true;
  }

  // Nonconvex or unusual polygons use the general even-odd scanline path.
  template<typename T>
  static void RasterizePolygonEvenOdd(T& d, const PolygonPoint* pts,
                                      int count, uint16_t color) {
    if (!pts || count < 3 || count > MAX_POLY_PTS) return;
    ScanlineSet samples[COVERAGE_SAMPLE_COUNT];
    bool active[COVERAGE_SAMPLE_COUNT];
    int32_t yStart = SCREEN_HEIGHT;
    int32_t yEnd = -1;
    for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
      active[sample] = BuildScanlineSet(pts, count, SampleOffset(sample),
          SCREEN_WIDTH, SCREEN_HEIGHT, samples[sample]);
      if (!active[sample]) continue;
      if (samples[sample].yStart < yStart) yStart = samples[sample].yStart;
      if (samples[sample].yEnd > yEnd) yEnd = samples[sample].yEnd;
    }
    if (yEnd < yStart) return;

    int32_t intersections[COVERAGE_SAMPLE_COUNT][MAX_POLY_PTS];
    int intersectionCounts[COVERAGE_SAMPLE_COUNT];
    CoverageInterval intervals[COVERAGE_SAMPLE_COUNT];
    for (int32_t y = yStart; y <= yEnd; y++) {
      int pairCount = 0;
      for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
        intersectionCounts[sample] = active[sample]
            ? CollectIntersections(samples[sample], y, intersections[sample])
            : 0;
        if (intersectionCounts[sample] > pairCount) {
          pairCount = intersectionCounts[sample];
        }
      }
      for (int i = 0; i + 1 < pairCount; i += 2) {
        for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
          const bool valid = i + 1 < intersectionCounts[sample];
          intervals[sample].left = valid ? intersections[sample][i] : 0;
          intervals[sample].right = valid ? intersections[sample][i + 1] : 0;
          intervals[sample].valid = valid;
        }
        RasterizeCoverageRow(d, y, intervals, SCREEN_WIDTH, color);
      }
    }
  }

  // Convex eye boundaries use two monotone chains, avoiding per-row
  // intersection collection and sorting. Other polygons retain even-odd fill.
  template<typename T>
  static void RasterizePolygon(T& d, const PolygonPoint* pts, int count,
                               uint16_t color) {
    if (!pts || count < 3 || count > MAX_POLY_PTS) return;
    if (IsConvexPolygon(pts, count)) {
      const bool usedFastPath = RasterizeConvexScanlines(
          pts, count, SCREEN_WIDTH, SCREEN_HEIGHT,
          [&](int32_t y, const CoverageInterval* intervals) {
            RasterizeCoverageRow(d, y, intervals, SCREEN_WIDTH, color);
          });
      if (usedFastPath) return;
    }
    RasterizePolygonEvenOdd(d, pts, count, color);
  }

  // General sprite fallback for nonconvex or unusual boundaries.
  static void RasterizeSpriteEvenOdd(uint16_t* pixels, int32_t width,
                                     int32_t height, const PolygonPoint* pts,
                                     int count, uint16_t color) {
    if (!pixels || !pts || count < 3 || count > MAX_POLY_PTS ||
        width <= 0 || height <= 0) return;
    ScanlineSet samples[COVERAGE_SAMPLE_COUNT];
    bool active[COVERAGE_SAMPLE_COUNT];
    int32_t yStart = height;
    int32_t yEnd = -1;
    for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
      active[sample] = BuildScanlineSet(pts, count, SampleOffset(sample),
          width, height, samples[sample]);
      if (!active[sample]) continue;
      if (samples[sample].yStart < yStart) yStart = samples[sample].yStart;
      if (samples[sample].yEnd > yEnd) yEnd = samples[sample].yEnd;
    }
    if (yEnd < yStart) return;

    int32_t intersections[COVERAGE_SAMPLE_COUNT][MAX_POLY_PTS];
    int intersectionCounts[COVERAGE_SAMPLE_COUNT];
    CoverageInterval intervals[COVERAGE_SAMPLE_COUNT];
    const uint16_t storedColor =
        (uint16_t)((color >> 8) | (color << 8));

    for (int32_t y = yStart; y <= yEnd; y++) {
      int pairCount = 0;
      for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
        intersectionCounts[sample] = active[sample]
            ? CollectIntersections(samples[sample], y, intersections[sample])
            : 0;
        if (intersectionCounts[sample] > pairCount) {
          pairCount = intersectionCounts[sample];
        }
      }
      for (int i = 0; i + 1 < pairCount; i += 2) {
        for (int sample = 0; sample < COVERAGE_SAMPLE_COUNT; sample++) {
          const bool valid = i + 1 < intersectionCounts[sample];
          intervals[sample].left = valid ? intersections[sample][i] : 0;
          intervals[sample].right = valid ? intersections[sample][i + 1] : 0;
          intervals[sample].valid = valid;
        }
        RasterizeSpriteCoverageRow(pixels, width, y, intervals, color,
                                   storedColor);
      }
    }
  }

  // Sprite rendering is the hot path. Convex eyes walk their left and right
  // edge chains directly; nonconvex callers keep the general even-odd path.
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

    if (count > MAX_POLY_PTS) return;
    if (IsConvexPolygon(pts, count)) {
      const uint16_t storedColor =
          (uint16_t)((color >> 8) | (color << 8));
      const bool usedFastPath = RasterizeConvexScanlines(
          pts, count, width, height,
          [&](int32_t y, const CoverageInterval* intervals) {
            RasterizeSpriteCoverageRow(pixels, width, y, intervals, color,
                                       storedColor);
          });
      if (usedFastPath) return;
    }
    RasterizeSpriteEvenOdd(pixels, width, height, pts, count, color);
  }

  static void Draw(TFT_eSprite& dpy, float cx, float cy,
                   const EyeConfig* cfg) {
    PolygonPoint boundary[MAX_POLY_PTS];
    int n = GenerateBoundary(cfg, cx, cy, boundary, MAX_POLY_PTS);
    if (n >= 3) {
      RasterizeSprite(dpy, boundary, n, EYE_COLOR);
    }
  }

  template<typename T>
  static void Draw(T& dpy, float cx, float cy, const EyeConfig* cfg) {
    PolygonPoint boundary[MAX_POLY_PTS];
    int n = GenerateBoundary(cfg, cx, cy, boundary, MAX_POLY_PTS);
    if (n >= 3) {
      RasterizePolygon(dpy, boundary, n, EYE_COLOR);
    }
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
