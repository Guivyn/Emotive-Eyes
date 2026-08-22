#include "PowerIC.h"
#include "Log.h"
#include <Wire.h>
#include <SPI.h>

static const uint8_t POWER_IC_ADDR = 0x75;
static const uint8_t MAX17048_ADDR  = 0x36;
static const uint8_t REG_SYS_CTL0  = 0x00;
static const uint8_t REG_STATUS0   = 0x70;
static const uint8_t REG_STATUS1   = 0x71;
static const uint8_t REG_BAT_LEVEL = 0x78;
static const uint8_t REG_MAX_VCELL = 0x02; // 78.125 uV/LSB, 16-bit BE
static const uint8_t REG_MAX_SOC   = 0x04; // 1/256 %/LSB, percent = hi byte

float PowerIC::ReadXpt2046BatteryVoltage() {
#if defined(TOUCH_CS) && defined(TFT_SCLK) && defined(TFT_MOSI) && defined(TFT_MISO)
  if (TOUCH_CS < 0) return -1.0f;

  // XPT2046 control byte for the VBAT channel:
  // S=1, A2A1A0=010, MODE=0 (12-bit), SER/DFR=1, PD=11
  const uint8_t cmd = 0xA7;
  // Same VSPI host as TFT_eSPI; beginTransaction serializes with the display.
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
  SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS, LOW);
  SPI.transfer(cmd);
  uint16_t raw16 = SPI.transfer16(0x0000);
  digitalWrite(TOUCH_CS, HIGH);
  SPI.endTransaction();

  // 12-bit result is left-justified in the 16-bit response; VBAT is divided
  // by 4 inside the chip, internal VREF = 2.5V -> full scale 10V.
  uint16_t v12 = (raw16 >> 4) & 0x0FFF;
  float volts = v12 * 10.0f / 4096.0f;
  LOG_PRINTF("[XPT] battery raw16=0x%04X v12=%u -> %.2fV\n",
             raw16, v12, volts);
  return volts;
#else
  return -1.0f;
#endif
}

bool PowerIC::ReadByte(uint8_t reg, uint8_t& out) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(_addr, (uint8_t)1) < 1) return false;
  out = Wire.read();
  return true;
}

bool PowerIC::ReadWord(uint8_t reg, uint16_t& out) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(_addr, (uint8_t)2) < 2) return false;
  uint16_t hi = Wire.read();
  uint16_t lo = Wire.read();
  out = (uint16_t)((hi << 8) | lo);
  return true;
}

bool PowerIC::WriteByte(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool PowerIC::Probe() {
#if !POWER_IC_ENABLED
  LOG_PRINTLN("[POWER-IC] disabled (POWER_IC_ENABLED=0)");
  return false;
#endif

  struct Pair { int8_t sda; int8_t scl; };
  const Pair pairs[] = {
    { POWER_IC_I2C_SDA, POWER_IC_I2C_SCL }, // default 21/22
    { 32, 22 },                              // alternate wiring on some CYDs
    { 25, 22 },
    { 18, 19 },                              // SD SPI pins (not used at boot)
    { 23, 19 },
    { 32, 21 },
    { 21, 32 },
    { 22, 32 },
    { 25, 26 },                              // GPIO25 free + speaker pin (no load)
    // NOTE: GPIO33 is the XPT2046 touch CS on the 024R, so it is never used
    // as a candidate here; repurposing it would break the touchscreen.
  };

  // Fast path: default pins, only the two known power-IC addresses.
  // Boards without a battery monitor ACK nothing, so this is ~10ms instead
  // of a multi-second full address scan.
  Wire.begin(POWER_IC_I2C_SDA, POWER_IC_I2C_SCL, 100000);
  pinMode(POWER_IC_I2C_SDA, INPUT_PULLUP);
  pinMode(POWER_IC_I2C_SCL, INPUT_PULLUP);
  if (DetectOnBus()) return true;

  // Fallback: same two addresses on alternate pin pairs. A full 0x03..0x78
  // scan is unnecessary for power telemetry and wastes seconds at boot.
  for (const Pair& p : pairs) {
    if (p.sda == POWER_IC_I2C_SDA && p.scl == POWER_IC_I2C_SCL) continue;
    if (p.sda < 0 || p.scl < 0) continue;
    LOG_PRINTF("[POWER-IC] probing SDA=%d SCL=%d ...\n", p.sda, p.scl);
    Wire.begin(p.sda, p.scl, 100000);
    pinMode(p.sda, INPUT_PULLUP);
    pinMode(p.scl, INPUT_PULLUP);
    if (DetectOnBus()) return true;
  }

  LOG_PRINTLN("[POWER-IC] no power IC (IP5306@0x75 / MAX17048@0x36) "
              "on candidate pins");
  return false;
}

bool PowerIC::DetectOnBus() {
  Wire.beginTransmission(POWER_IC_ADDR);
  if (Wire.endTransmission() == 0) return SetupIp5306();

  Wire.beginTransmission(MAX17048_ADDR);
  if (Wire.endTransmission() == 0) return SetupMax17048();

  return false;
}

bool PowerIC::SetupIp5306() {
  _found = true;
  _max17048 = false;
  _addr = POWER_IC_ADDR;
  LOG_PRINTLN("[POWER-IC] found IP5306 (0x75)");

  uint8_t raw[3];
  const uint8_t regs[3] = { 0x70, 0x71, 0x78 };
  for (int i = 0; i < 3; i++) {
    raw[i] = 0xFF;
    ReadByte(regs[i], raw[i]);
  }
  LOG_PRINTF("[POWER-IC] regs 0x70=%02X 0x71=%02X 0x78=%02X\n",
             raw[0], raw[1], raw[2]);

  Refresh();
  LOG_PRINTF("[POWER-IC] charging=%d full=%d level=%d%%\n",
             Charging() ? 1 : 0, ChargeFull() ? 1 : 0, BatteryPercent());

#if POWER_IC_BOOST_ENABLE
  if (WriteByte(REG_SYS_CTL0, 0x37)) {
    LOG_PRINTLN("[POWER-IC] boost keep-on written (reg 0x00 = 0x37)");
  } else {
    LOG_PRINTLN("[POWER-IC] WARN: boost keep-on write failed");
  }
#endif
  return true;
}

bool PowerIC::SetupMax17048() {
  _found = true;
  _max17048 = true;
  _addr = MAX17048_ADDR;
  LOG_PRINTLN("[POWER-IC] found MAX17048 (0x36)");

  RefreshMax17048();
  LOG_PRINTF("[POWER-IC] vbat=%.2fV level=%d%% (gauge only, no charging info)\n",
             BatteryVoltage(), BatteryPercent());
  return true;
}

bool PowerIC::Refresh() {
  if (!_found) return false;
  if (_max17048) return RefreshMax17048();

  uint8_t v = 0;
  _charging   = ReadByte(REG_STATUS0, v) && (v & (1 << 3));
  _chargeFull = ReadByte(REG_STATUS1, v) && (v & (1 << 3));
  _percentValid = ReadByte(REG_BAT_LEVEL, v);
  if (_percentValid) {
    switch (v & 0xF0) {
      case 0xE0: _percent = 25;  break;
      case 0xC0: _percent = 50;  break;
      case 0x80: _percent = 75;  break;
      case 0x00: _percent = 100; break;
      default:   _percentValid = false; _percent = -1; break;
    }
  } else {
    _percent = -1;
  }
  return _percentValid;
}

bool PowerIC::RefreshMax17048() {
  uint16_t raw = 0;
  if (ReadWord(REG_MAX_SOC, raw)) {
    _percent = (int8_t)(raw >> 8); // percent = high byte of 16-bit SOC
    if (_percent < 0) _percent = 0;
    if (_percent > 100) _percent = 100;
    _percentValid = true;
  } else {
    _percentValid = false;
    _percent = -1;
  }
  if (ReadWord(REG_MAX_VCELL, raw)) {
    _batteryV = (float)raw * 0.000078125f; // 78.125 uV/LSB
  }
  _charging = false;   // gauge-only: no charger status available
  _chargeFull = false;
  return _percentValid;
}
