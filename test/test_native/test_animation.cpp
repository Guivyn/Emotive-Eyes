#include <unity.h>

#include "Animation.h"

void test_ramp_midpoint() {
  RampAnimation animation(100);
  g_mockMillis = 50;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, animation.GetValue());
}

void test_ramp_holds_at_end() {
  RampAnimation animation(100);
  g_mockMillis = 1000;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, animation.GetValue());
}

void test_trapezium_peak() {
  TrapeziumAnimation animation(100, 50, 100);
  g_mockMillis = 125;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, animation.GetValue());
}

void test_pulse_wraps_around() {
  TrapeziumPulseAnimation pulse(100, 100, 100, 100, 0);
  g_mockMillis = 301;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.99f, pulse.GetValue());
  g_mockMillis = 400;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pulse.GetValue());
}

void test_timer_expiry() {
  AsyncTimer timer(10);
  timer.Start();
  g_mockMillis = 10;
  TEST_ASSERT_TRUE(timer.Update());
  TEST_ASSERT_TRUE(timer.IsExpired());
}
