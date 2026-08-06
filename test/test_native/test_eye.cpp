#include <unity.h>

#include "Eye.h"
#include "Eye.cpp"

void test_boundary_generates_closed_polygon() {
  EyeConfig cfg = {0, 0, 50, 50, 0, 0, 10, 10};
  PolygonPoint points[MAX_POLY_PTS];
  int count = EyeDrawer::GenerateBoundary(&cfg, 0, 0, points, MAX_POLY_PTS);

  TEST_ASSERT_GREATER_OR_EQUAL(3, count);
  TEST_ASSERT_LESS_OR_EQUAL(MAX_POLY_PTS, count);
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
