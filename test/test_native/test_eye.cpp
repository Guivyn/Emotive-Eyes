#include <unity.h>

#include "Eye.h"
#include "Eye.cpp"

struct RasterCapture {
  static const int kWidth = 160;
  static const int kHeight = 240;
  uint16_t pixels[kWidth * kHeight];

  void drawFastHLine(int x, int y, int w, uint16_t color) {
    if (y < 0 || y >= kHeight || w <= 0 || x >= kWidth) return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (x + w > kWidth) w = kWidth - x;
    if (w <= 0) return;
    for (int i = 0; i < w; i++) pixels[y * kWidth + x + i] = color;
  }
};

void test_sprite_scanline_matches_generic() {
  static RasterCapture generic;
  TEST_ASSERT_TRUE(EyeDrawer::CoverageColor565(TFT_WHITE, 128) > 0x8410);
  static const EyeConfig cases[] = {
    {0, 0, 50, 50, 0, 0, 10, 10},
    {-4, 0, 25, 50, 0.3f, 0, 2, 15},
    {0, -2, 18, 50, -0.5f, -0.5f, 4, 4},
    {2, -4, 30, 40, 0.1f, -0.1f, 6, 8},
  };

  TFT_eSprite sprite;
  for (const EyeConfig& cfg : cases) {
    PolygonPoint points[MAX_POLY_PTS];
    int count = EyeDrawer::GenerateBoundary(&cfg, 80.0f, 120.0f,
                                             points, MAX_POLY_PTS);
    memset(generic.pixels, 0, sizeof(generic.pixels));
    sprite.fillSprite(TFT_BLACK);
    EyeDrawer::RasterizePolygon(generic, points, count, TFT_WHITE);
    EyeDrawer::RasterizeSprite(sprite, points, count, TFT_WHITE);
    const uint16_t* stored = static_cast<const uint16_t*>(sprite.getPointer());
    for (int i = 0; i < RasterCapture::kWidth * RasterCapture::kHeight; i++) {
      const uint16_t expected = generic.pixels[i];
      const uint16_t storedExpected =
          (uint16_t)((expected >> 8) | (expected << 8));
      TEST_ASSERT_EQUAL_HEX16(storedExpected, stored[i]);
    }
  }
}

void test_boundary_generates_closed_polygon() {
  EyeConfig cfg = {0, 0, 50, 50, 0, 0, 10, 10};
  PolygonPoint points[MAX_POLY_PTS];
  int count = EyeDrawer::GenerateBoundary(&cfg, 0, 0, points, MAX_POLY_PTS);

  TEST_ASSERT_GREATER_OR_EQUAL(3, count);
  TEST_ASSERT_EQUAL(25, count);
  TEST_ASSERT_LESS_OR_EQUAL(MAX_POLY_PTS, count);

  // A compressed one-sided rounded eye can occur during gaze scaling. Its
  // arc must stay within the final width and height instead of folding over.
  EyeConfig compressed = {0, 0, 5, 20, 0, 0, 12, 0};
  count = EyeDrawer::GenerateBoundary(&compressed, 0, 0, points,
                                      MAX_POLY_PTS);
  TEST_ASSERT_EQUAL(15, count);
  TEST_ASSERT_TRUE(points[6].x <= points[7].x);
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(points[i].x >= -10.001f);
    TEST_ASSERT_TRUE(points[i].x <= 10.001f);
    TEST_ASSERT_TRUE(points[i].y >= -2.501f);
    TEST_ASSERT_TRUE(points[i].y <= 2.501f);
  }
}

void test_boundary_handles_null_config() {
  PolygonPoint points[MAX_POLY_PTS];
  TEST_ASSERT_EQUAL(0, EyeDrawer::GenerateBoundary(nullptr, 0, 0, points,
                                                   MAX_POLY_PTS));
}

void test_transition_reaches_destination() {
  Eye eye;
  EyeConfig target = {2, -4, 30, 40, 0.1f, -0.1f, 6, 8};
  eye.TransitionTo(target);
  g_mockMillis = 5000;
  eye.Update();

  TEST_ASSERT_EQUAL(target.OffsetX, eye.Transition.Output.OffsetX);
  TEST_ASSERT_EQUAL(target.Height, eye.Transition.Output.Height);
  TEST_ASSERT_EQUAL(target.Radius_Bottom, eye.Transition.Output.Radius_Bottom);
}
