#pragma once

#include <Arduino.h>

// Board power telemetry for the Sunton CYD family:
//  - Injoinic power-bank SoC IP5306 / IP5603 (addr 0x75): charging state and
//    coarse 25%-step battery level;
//  - Maxim MAX17048 fuel gauge (addr 0x36): continuous VCELL / SOC;
//  - a one-shot read of the XPT2046 touch controller's VBAT channel, which is
//    routed to the battery on some 024 boards, sampled before tft.begin().
//
// IP5306 register map (IP5603 has no public datasheet; same interpretation):
//   I2C address : 0x75
//   reg 0x00    : SYS_CTL0  (0x37 = boost output + auto power-on + charger)
//   reg 0x70    : status    (bit3 = charging)
//   reg 0x71    : status    (bit3 = battery full)
//   reg 0x78    : battery level, 25% steps (upper nibble)
//
// MAX17048 register map:
//   I2C address : 0x36
//   reg 0x02    : VCELL, 16-bit big-endian, 78.125uV/LSB
//   reg 0x04    : SOC,   16-bit big-endian, 1/256 %/LSB (percent = hi byte)

#ifndef POWER_IC_ENABLED
  #define POWER_IC_ENABLED 0
#endif
#ifndef POWER_IC_I2C_SDA
  #define POWER_IC_I2C_SDA 21
#endif
#ifndef POWER_IC_I2C_SCL
  #define POWER_IC_I2C_SCL 22
#endif
#ifndef POWER_IC_BOOST_ENABLE
  #define POWER_IC_BOOST_ENABLE 1
#endif

class PowerIC {
public:
  // Read the XPT2046 touch controller's VBAT channel over the shared SPI bus.
  // The chip divides battery voltage by 4 internally (full scale 10V).
  // Returns volts, or a negative value when the pins are unavailable.
  // Must be called BEFORE tft.begin() so TFT_eSPI can re-claim the pins.
  static float ReadXpt2046BatteryVoltage();

  // Scan the primary (and fallback) pin pairs, dump registers if 0x75 ACKs.
  bool Probe();
  // Re-read charging status and battery level on the found bus.
  bool Refresh();

  bool Found() const { return _found; }
  // True when the detected IC is a MAX17048 fuel gauge (no charging info).
  bool IsMax17048() const { return _max17048; }
  int8_t BatteryPercent() const { return _percent; }
  bool PercentValid() const { return _percentValid; }
  bool Charging() const { return _charging; }
  bool ChargeFull() const { return _chargeFull; }
  float BatteryVoltage() const { return _batteryV; }

private:
  bool ReadByte(uint8_t reg, uint8_t& out);
  bool ReadWord(uint8_t reg, uint16_t& out);
  bool WriteByte(uint8_t reg, uint8_t value);
  bool DetectOnBus();   // check 0x75 then 0x36 on the currently wired bus
  bool SetupIp5306();
  bool SetupMax17048();
  bool RefreshMax17048();

  bool _found = false;
  bool _max17048 = false;
  uint8_t _addr = 0x75;
  bool _percentValid = false;
  bool _charging = false;
  bool _chargeFull = false;
  int8_t _percent = -1;
  float _batteryV = -1.0f;
};
