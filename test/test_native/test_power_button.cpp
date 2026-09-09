#include <unity.h>
#include "PowerButton.h"

void test_power_button_release_and_bounce() {
  PowerButton b;
  using E = PowerButton::Event;
  TEST_ASSERT_TRUE(b.Update(true, 100) == E::NONE);
  TEST_ASSERT_TRUE(b.Update(true, 140) == E::PRESS);
  TEST_ASSERT_TRUE(b.Update(true, 3000) == E::NONE); // holding cannot retrigger
  TEST_ASSERT_TRUE(b.Update(false, 3010) == E::NONE);
  TEST_ASSERT_TRUE(b.Update(true, 3030) == E::NONE); // release bounce
  TEST_ASSERT_TRUE(b.Update(false, 3040) == E::NONE);
  TEST_ASSERT_TRUE(b.Update(false, 3080) == E::LONG_PRESS);
  TEST_ASSERT_TRUE(b.Update(false, 9000) == E::NONE);
  b.Update(true, 9100); b.Update(true, 9140); b.Update(false, 9200);
  TEST_ASSERT_TRUE(b.Update(false, 9240) == E::SHORT_PRESS);
}

void test_power_button_millis_wrap() {
  PowerButton b;
  using E = PowerButton::Event;
  b.Update(true, UINT32_MAX - 1000);
  b.Update(true, UINT32_MAX - 960);
  b.Update(false, 1100);
  TEST_ASSERT_TRUE(b.Update(false, 1140) == E::LONG_PRESS);
}
