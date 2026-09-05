#include <unity.h>

#include "CalibrationCodec.h"

void test_calibration_record_validates() {
  CalibrationCodec::Record record = CalibrationCodec::Make(320, 3620);

  TEST_ASSERT_TRUE(CalibrationCodec::IsValid(record));
  TEST_ASSERT_EQUAL_INT32(320, record.offset);
  TEST_ASSERT_EQUAL_INT32(3620, record.range);
}

void test_calibration_record_rejects_corruption() {
  CalibrationCodec::Record record = CalibrationCodec::Make(320, 3620);

  record.offset++;
  TEST_ASSERT_FALSE(CalibrationCodec::IsValid(record));

  record = CalibrationCodec::Make(320, 3620);
  record.range = 0;
  TEST_ASSERT_FALSE(CalibrationCodec::IsValid(record));
}
