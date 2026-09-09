#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "PowerDiagnostics.h"

// Enable only after verifying a physical divider from BAT to GPIO35.
#ifndef POWER_BATTERY_SENSE_ENABLED
#define POWER_BATTERY_SENSE_ENABLED 0
#endif
#ifndef POWER_BATTERY_DIVIDER
#define POWER_BATTERY_DIVIDER 2.0f
#endif

static uint32_t SampleMv(uint8_t pin) {
  pinMode(pin, INPUT);
  analogSetPinAttenuation(pin, ADC_11db);
  analogReadMilliVolts(pin);
  uint32_t sum = 0;
  for (int i = 0; i < 32; ++i) { sum += analogReadMilliVolts(pin); delay(2); }
  return (sum + 16) / 32;
}

void PrintPowerDiagnostics(bool scan) {
  Serial.printf("[POWER] uptime_ms=%lu reset=%d wake=%d cpu=%uMHz\n",
                millis(), esp_reset_reason(), esp_sleep_get_wakeup_cause(), getCpuFrequencyMhz());
#if POWER_BATTERY_SENSE_ENABLED
  const uint32_t mv = SampleMv(35);
  if (mv < 150 || mv > 2450) Serial.printf("[BAT] INVALID adc35_mv=%lu\n", mv);
  else Serial.printf("[BAT] divider_verified_by_build adc35_mv=%lu battery_v=%.3f charging=UNKNOWN soc=UNKNOWN\n",
                     mv, mv * POWER_BATTERY_DIVIDER / 1000.0f);
#else
  Serial.println("[BAT] UNAVAILABLE: no verified battery sense wiring; charging=UNKNOWN soc=UNKNOWN");
#endif
  if (!scan) return;
  for (uint8_t pin : {uint8_t(35), uint8_t(39)}) {
    Serial.printf("[ADC] gpio=%u pin_mv=%lu UNVERIFIED_INPUT_NOT_BATTERY\n", pin, SampleMv(pin));
  }
  Wire.begin(21, 22, 100000);
  Wire.setTimeOut(20);
  unsigned found = 0, errors = 0;
  for (uint8_t addr = 8; addr < 120; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t result = Wire.endTransmission(); // address ACK only; no register writes
    if (result == 0) { ++found; Serial.printf("[I2C] ack=0x%02X identity=UNKNOWN\n", addr); }
    else if (result != 2) ++errors;
  }
  Wire.end();
  Serial.printf("[I2C] scan_done devices=%u bus_errors=%u SDA=21 SCL=22\n", found, errors);
}
