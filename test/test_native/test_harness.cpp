#include <unity.h>

void test_ramp_midpoint();
void test_ramp_holds_at_end();
void test_trapezium_peak();
void test_pulse_wraps_around();
void test_timer_expiry();
void test_boundary_generates_closed_polygon();
void test_boundary_handles_null_config();
void test_transition_reaches_destination();

unsigned long g_mockMillis = 0;

void setUp() {
  g_mockMillis = 0;
}

void tearDown() {
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_ramp_midpoint);
  RUN_TEST(test_ramp_holds_at_end);
  RUN_TEST(test_trapezium_peak);
  RUN_TEST(test_pulse_wraps_around);
  RUN_TEST(test_timer_expiry);
  RUN_TEST(test_boundary_generates_closed_polygon);
  RUN_TEST(test_boundary_handles_null_config);
  RUN_TEST(test_transition_reaches_destination);
  return UNITY_END();
}
