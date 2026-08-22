#include "board_config.h"
#include "PowerManager.h"
#include "FacePower.h"
#include "Config.h"
#include "Log.h"

extern TFT_eSPI tft;

#ifndef POWER_BATTERY_SENSE_ENABLED
  #define POWER_BATTERY_SENSE_ENABLED 0
#endif
#ifndef POWER_LIGHT_SLEEP_ENABLED
  #define POWER_LIGHT_SLEEP_ENABLED 0
#endif
#ifndef POWER_TEMP_PROTECT_ENABLED
  #define POWER_TEMP_PROTECT_ENABLED 1
#endif

const float PowerManager::LOW_BATTERY_V    = 3.50f;
const float PowerManager::CRITICAL_BATTERY_V = 3.30f;
const float PowerManager::BATTERY_FULL_V   = 4.20f;
const float PowerManager::BATTERY_EMPTY_V  = 3.00f;

// Board-specific battery sense: 100k/100k divider on GPIO39 -> Vbat = adc * 2.
// Overridable via platformio.ini (POWER_BATTERY_ADC_PIN / POWER_BATTERY_DIVIDER).
#ifndef POWER_BATTERY_ADC_PIN
  #define POWER_BATTERY_ADC_PIN 39
#endif
#ifndef POWER_BATTERY_DIVIDER
  #define POWER_BATTERY_DIVIDER 2.0f
#endif
static const uint8_t BATTERY_ADC_PIN = POWER_BATTERY_ADC_PIN;
static const float   BATTERY_DIVIDER = POWER_BATTERY_DIVIDER;
// Plausible 1S LiPo voltage range. Readings outside this band mean the sense
// divider is not wired (e.g. USB-only power) and must not trigger protection.
static const float BATTERY_SENSE_MIN_V = 2.80f;
static const float BATTERY_SENSE_MAX_V = 4.35f;

static bool BatteryReadingPlausible(float v) {
  return v >= BATTERY_SENSE_MIN_V && v <= BATTERY_SENSE_MAX_V;
}

static const uint8_t CPU_MHZ_ACTIVE = 240;
static const uint8_t CPU_MHZ_RELAX  = 160;
static const uint8_t CPU_MHZ_IDLE   = 80;
static const uint8_t CPU_MHZ_SLEEP  = 80;

PowerManager::PowerManager(Face& face, uint8_t backlightPin, float bootBatteryV)
  : _face(face), _backlightPin(backlightPin) {
  _state = PowerState::ACTIVE;
  _lastInteractionMs = 0;
  _lastBatterySampleMs = 0;
  _lastIcSampleMs = 0;
  _lastIcCharging = false;
  _lastIcFull = false;
  _lastIcPercent = -1;
  _batteryV = -1.0f;
  _bootBatteryV = bootBatteryV;
  _batteryValid = false;
  _lastLoggedV = -1.0f;
  _lastRawMv = -1.0f;
  _lastInvalidLogMs = 0;
  _trendSampleV = -1.0f;
  _lastTrendSampleMs = 0;
  _trendCharging = false;
  _backlightLevel = 0;
  _targetBacklightLevel = 100;
  _stateBacklightTarget = 100;
  _lastBacklightStepMs = 0;
  _lastTempSampleMs = 0;
  _lastStatsLogMs = 0;
  _chipTempC = 30.0f;
  _overheatRecovery = false;
  for (int i = 0; i <= 100; i++) {
    _gammaLut[i] = (uint8_t)(255.0f * powf(i / 100.0f, 2.2f) + 0.5f);
  }
}

void PowerManager::Begin() {
  pinMode(_backlightPin, OUTPUT);
  ApplyBacklight(0); // boot dark, then ramp up in loop()
#if POWER_BATTERY_SENSE_ENABLED
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  analogRead(BATTERY_ADC_PIN); // settle ADC once
  SampleBattery();
  if (_batteryValid) {
    LOG_PRINTF("[POWER] Battery %.2fV (%u%%), state=ACTIVE\n",
      _batteryV, GetBatteryPercent());
  } else {
    LOG_PRINTF("[POWER] No usable battery reading: GPIO%u ADC=%.0fmV "
               "-> %.2fV (wire BAT+ ->100k-> GPIO%u ->100k-> GND)\n",
               BATTERY_ADC_PIN, _lastRawMv, _batteryV, BATTERY_ADC_PIN);
#if SERIAL_DEBUG_ENABLED
    static const uint8_t kScanPins[] = {32, 34, 35, 36, 39};
    for (uint8_t pin : kScanPins) {
      analogSetPinAttenuation(pin, ADC_11db);
      LOG_PRINTF("[POWER]   ADC pin %u: %umV\n", pin,
                 analogReadMilliVolts(pin));
    }
#endif
  }
#else
  LOG_PRINTLN("[POWER] Battery sense disabled (no divider on GPIO34 yet)");
#endif

#if POWER_IC_ENABLED
  if (_powerIc.Probe()) {
    LOG_PRINTF("[POWER] power IC: charging=%d full=%d level=%d%%\n",
               _powerIc.Charging() ? 1 : 0,
               _powerIc.ChargeFull() ? 1 : 0,
               _powerIc.BatteryPercent());
  } else {
    LOG_PRINTLN("[POWER] power IC not found "
                "(IP5306@0x75 / MAX17048@0x36)");
    LOG_PRINTLN("[POWER] charging is automatic while USB is connected; "
                "watch the bat= voltage climb to confirm");
  }
#else
  LOG_PRINTLN("[POWER] power IC I2C disabled (POWER_IC_ENABLED=0)");
#endif
}

void PowerManager::Update() {
  unsigned long now = millis();
  UpdateBacklight();

#if POWER_TEMP_PROTECT_ENABLED
  if (now - _lastTempSampleMs >= 5000) {
    _lastTempSampleMs = now;
    SampleTemperature();
    if (_chipTempC >= 80.0f) {
      _overheatRecovery = true;
      if (_state != PowerState::SLEEP) {
        LOG_PRINTF("[POWER] overheat %.1fC, forced SLEEP\n", _chipTempC);
      }
      SetState(PowerState::SLEEP);
    } else if (_chipTempC >= 70.0f &&
               (_state == PowerState::ACTIVE || _state == PowerState::RELAX)) {
      _overheatRecovery = true;
      LOG_PRINTF("[POWER] warm %.1fC, forced IDLE\n", _chipTempC);
      SetState(PowerState::IDLE);
    } else if (_chipTempC < 70.0f && _overheatRecovery) {
      _overheatRecovery = false;
      if (_state == PowerState::IDLE || _state == PowerState::SLEEP) {
        LOG_PRINTF("[POWER] overheat cleared (%.1fC), resumed ACTIVE\n",
                   _chipTempC);
        _lastInteractionMs = now;
        SetState(PowerState::ACTIVE);
      }
    }
  }
#endif

  if (now - _lastStatsLogMs >= 30000) {
    _lastStatsLogMs = now;
    LOG_PRINTF("[STATS] state=%s bl=%u interval=%ums temp=%.1fC "
               "bat=%.2fV %u%% chg=%d full=%d\n",
      StateName(), _backlightLevel, GetFrameIntervalMs(), _chipTempC,
      _batteryV, GetBatteryPercent(), IsCharging() ? 1 : 0,
      IsChargeFull() ? 1 : 0);
  }

#if POWER_BATTERY_SENSE_ENABLED
  // Sample battery once per second; continuous ADC polling wastes power.
  if (now - _lastBatterySampleMs >= 1000) {
    _lastBatterySampleMs = now;
    SampleBattery();
    if (_batteryValid && fabsf(_batteryV - _lastLoggedV) >= 0.03f) {
      _lastLoggedV = _batteryV;
      LOG_PRINTF("[POWER] bat=%.2fV %u%%\n",
                 _batteryV, GetBatteryPercent());
      if (_batteryV >= BATTERY_FULL_V - 0.05f) _trendCharging = false;
    } else if (!_batteryValid &&
               (now - _lastInvalidLogMs >= 15000 || _lastInvalidLogMs == 0)) {
      _lastInvalidLogMs = now;
      LOG_PRINTF("[POWER] no battery source (GPIO%u ADC=%.0fmV -> %.2fV)\n",
                 BATTERY_ADC_PIN, _lastRawMv, _batteryV);
    }

    // Infer "charging" from the voltage trend when the I2C power IC is not
    // reachable: a battery being charged climbs steadily, a resting or
    // discharging one does not.
    if (_batteryValid && now - _lastTrendSampleMs >= 30000) {
      if (_lastTrendSampleMs != 0 && _trendSampleV > 0.0f) {
        if (_batteryV > _trendSampleV + 0.012f) {
          _trendCharging = true;
        } else if (_batteryV <= _trendSampleV) {
          _trendCharging = false;
        }
      }
      _trendSampleV = _batteryV;
      _lastTrendSampleMs = now;
    }

  }

  // Only enforce protection on plausible 1S LiPo readings. A floating or
  // unwired divider (~0V) or a noisy spike must not lock the device into
  // SLEEP while it is powered from USB.
  // While charging (USB plugged) never force the device to sleep for a low
  // battery; it is being topped up.
  if (BatteryReadingPlausible(_batteryV) && !IsCharging()) {
    if (_batteryV < CRITICAL_BATTERY_V) {
      _overheatRecovery = false;
      SetState(PowerState::SLEEP);
    } else if (_batteryV < LOW_BATTERY_V && _state == PowerState::ACTIVE) {
      SetState(PowerState::IDLE);
    }
  }
#endif

#if POWER_IC_ENABLED
  // Poll the power IC every 5 s and log state changes (charging / full / %).
  if (_powerIc.Found() && now - _lastIcSampleMs >= 5000) {
    _lastIcSampleMs = now;
    _powerIc.Refresh();
    if (_powerIc.Charging() != _lastIcCharging ||
        _powerIc.ChargeFull() != _lastIcFull ||
        _powerIc.BatteryPercent() != _lastIcPercent) {
      LOG_PRINTF("[POWER-IC] charging=%d full=%d level=%d%%\n",
                 _powerIc.Charging() ? 1 : 0,
                 _powerIc.ChargeFull() ? 1 : 0,
                 _powerIc.BatteryPercent());
      _lastIcCharging = _powerIc.Charging();
      _lastIcFull = _powerIc.ChargeFull();
      _lastIcPercent = _powerIc.BatteryPercent();
    }
  }
#endif

  unsigned long idleFor = now - _lastInteractionMs;
  if (_state == PowerState::ACTIVE && idleFor >= ACTIVE_TIMEOUT_MS) {
    SetState(PowerState::RELAX);
  } else if (_state == PowerState::RELAX && idleFor >= RELAX_TIMEOUT_MS) {
    SetState(PowerState::IDLE);
  } else if (_state == PowerState::IDLE && idleFor >= IDLE_TIMEOUT_MS) {
    SetState(PowerState::SLEEP);
  }

  if (_state == PowerState::SLEEP) {
#if POWER_LIGHT_SLEEP_ENABLED
    EnterLightSleep();
#else
    // Light sleep disabled: loop() handles the 200ms touch-poll cadence.
#endif
  }
}

void PowerManager::NotifyInteraction() {
  _lastInteractionMs = millis();
  if (_state != PowerState::ACTIVE) SetState(PowerState::ACTIVE);
}

void PowerManager::EnterSleep() {
  SetState(PowerState::SLEEP);
}

void PowerManager::EnterIdle() {
  SetState(PowerState::IDLE);
}

uint32_t PowerManager::GetFrameIntervalMs() const {
  switch (_state) {
    case PowerState::ACTIVE: return 25;  // 40 FPS
    case PowerState::RELAX:  return 33;  // 30 FPS
    case PowerState::IDLE:   return 41;  // ~24 FPS
    case PowerState::SLEEP:  return 200; // touch-poll cadence, no render
  }
  return 25;
}

uint8_t PowerManager::GetBatteryPercent() const {
#if POWER_IC_ENABLED
  if (_powerIc.Found() && _powerIc.PercentValid()) {
    return (uint8_t)_powerIc.BatteryPercent();
  }
#endif
  if (!_batteryValid) return 0;
  float p = (_batteryV - BATTERY_EMPTY_V) /
            (BATTERY_FULL_V - BATTERY_EMPTY_V);
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return (uint8_t)(p * 100.0f);
}

bool PowerManager::IsCharging() const {
#if POWER_IC_ENABLED
  if (_powerIc.Found()) return _powerIc.Charging();
#endif
  return _trendCharging;
}

bool PowerManager::IsChargeFull() const {
#if POWER_IC_ENABLED
  if (_powerIc.Found()) return _powerIc.ChargeFull();
#endif
  return false;
}

static void ApplyCpuFrequency(PowerState state) {
  uint8_t mhz = CPU_MHZ_ACTIVE;
  switch (state) {
    case PowerState::ACTIVE: mhz = CPU_MHZ_ACTIVE; break;
    case PowerState::RELAX:  mhz = CPU_MHZ_RELAX;  break;
    case PowerState::IDLE:   mhz = CPU_MHZ_IDLE;   break;
    case PowerState::SLEEP:  mhz = CPU_MHZ_SLEEP;  break;
  }
  setCpuFrequencyMhz(mhz);
  LOG_PRINTF("[POWER] cpu=%uMHz\n", getCpuFrequencyMhz());
}

void PowerManager::SetState(PowerState s) {
  if (_state == s) return;
  _state = s;
  switch (s) {
    case PowerState::ACTIVE:
      ApplyCpuFrequency(s);
      tft.writecommand(0x29); // DISPON
      SetBacklight(100);
      FacePower_Wake(_face);
      LOG_PRINTLN("[POWER] state=ACTIVE");
      break;
    case PowerState::RELAX:
      ApplyCpuFrequency(s);
      SetBacklight(50);
      FacePower_EnterRelax(_face);
      LOG_PRINTLN("[POWER] state=RELAX");
      break;
    case PowerState::IDLE:
      ApplyCpuFrequency(s);
      SetBacklight(25);
      FacePower_EnterIdle(_face);
      LOG_PRINTLN("[POWER] state=IDLE");
      break;
    case PowerState::SLEEP:
      FacePower_EnterSleep(_face);
      SetBacklight(0);
      ApplyCpuFrequency(s);
      tft.writecommand(0x28); // DISPOFF
      LOG_PRINTF("[POWER] state=SLEEP battery=%.2fV\n", _batteryV);
      break;
  }
}

void PowerManager::SetBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  _stateBacklightTarget = percent;
}

void PowerManager::UpdateBacklight() {
  uint8_t target = _stateBacklightTarget;
  _targetBacklightLevel = target;
  if (_backlightLevel == _targetBacklightLevel) return;
  unsigned long now = millis();
  if (now - _lastBacklightStepMs < BACKLIGHT_STEP_MS) return;
  _lastBacklightStepMs = now;
  int32_t distance = abs((int32_t)_targetBacklightLevel - _backlightLevel);
  int32_t step = distance / 6;
  if (step < 1) step = 1;
  if (step > 4) step = 4;
  if (_backlightLevel < _targetBacklightLevel) _backlightLevel += step;
  else _backlightLevel -= step;
  ApplyBacklight(_backlightLevel);
}

void PowerManager::ApplyBacklight(uint8_t level) {
  if (level > 100) level = 100;
  analogWrite(_backlightPin, _gammaLut[level]);
}

void PowerManager::SampleTemperature() {
  _chipTempC = temperatureRead();
}

const char* PowerManager::StateName() const {
  switch (_state) {
    case PowerState::ACTIVE: return "ACTIVE";
    case PowerState::RELAX:  return "RELAX";
    case PowerState::IDLE:   return "IDLE";
    case PowerState::SLEEP:  return "SLEEP";
  }
  return "?";
}

void PowerManager::SampleBattery() {
  float bestV = -1.0f;
  float rawMv = -1.0f;
#if POWER_BATTERY_SENSE_ENABLED
  uint32_t sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delayMicroseconds(200);
  }
  rawMv = (float)sum / samples;
  float adcV = rawMv * BATTERY_DIVIDER / 1000.0f;
  if (BatteryReadingPlausible(adcV)) bestV = adcV;
#endif
  // Fall back to the one-shot XPT2046 VBAT sample taken before tft.begin().
  if (bestV < 0.0f && BatteryReadingPlausible(_bootBatteryV)) {
    bestV = _bootBatteryV;
  }
  if (bestV >= 0.0f) {
    _batteryV = bestV;
    _batteryValid = true;
  } else {
    _batteryV = -1.0f;
    _batteryValid = false;
  }

  _lastRawMv = rawMv;
}

void PowerManager::EnterLightSleep() {
  // Wake on touch IRQ (GPIO36, active low) with a timer fallback so a
  // floating IRQ line cannot lock the device asleep.
  static const gpio_num_t TOUCH_WAKE_GPIO = static_cast<gpio_num_t>(36);
  esp_sleep_enable_ext0_wakeup(TOUCH_WAKE_GPIO, 0);
  esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_PERIOD_MS * 1000ULL);
  esp_light_sleep_start();
}
