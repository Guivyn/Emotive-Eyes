#include "PowerManager.h"
#include "FacePower.h"

#ifndef POWER_BATTERY_SENSE_ENABLED
  #define POWER_BATTERY_SENSE_ENABLED 0
#endif
#ifndef POWER_LIGHT_SLEEP_ENABLED
  #define POWER_LIGHT_SLEEP_ENABLED 0
#endif
#ifndef POWER_LIGHT_SENSOR_ENABLED
  #define POWER_LIGHT_SENSOR_ENABLED 0
#endif
#ifndef POWER_TEMP_PROTECT_ENABLED
  #define POWER_TEMP_PROTECT_ENABLED 1
#endif

const float PowerManager::LOW_BATTERY_V    = 3.50f;
const float PowerManager::CRITICAL_BATTERY_V = 3.30f;
const float PowerManager::BATTERY_FULL_V   = 4.20f;
const float PowerManager::BATTERY_EMPTY_V  = 3.00f;

// Board-specific battery sense: 100k/100k divider on GPIO34 -> Vbat = adc * 2.
static const uint8_t BATTERY_ADC_PIN = 34;
static const float   BATTERY_DIVIDER = 2.0f;
static const uint8_t LIGHT_ADC_PIN   = 35; // actual LDR pin on this board

PowerManager::PowerManager(Face& face, uint8_t backlightPin)
  : _face(face), _backlightPin(backlightPin) {
  _state = PowerState::ACTIVE;
  _lastInteractionMs = 0;
  _lastBatterySampleMs = 0;
  _batteryV = BATTERY_FULL_V;
  _backlightLevel = 0;
  _targetBacklightLevel = 100;
  _stateBacklightTarget = 100;
  _ambientLevel = 100;
  _lastBacklightStepMs = 0;
  _lastLightSampleMs = 0;
  _lastLightLogMs = 0;
  _lastTempSampleMs = 0;
  _lastStatsLogMs = 0;
  _chipTempC = 30.0f;
  for (int i = 0; i <= 100; i++) {
    _gammaLut[i] = (uint8_t)(255.0f * powf(i / 100.0f, 2.2f) + 0.5f);
  }
}

void PowerManager::Begin() {
  pinMode(_backlightPin, OUTPUT);
  ApplyBacklight(0); // boot dark, then ramp up in loop()
#if POWER_LIGHT_SENSOR_ENABLED
  analogSetPinAttenuation(LIGHT_ADC_PIN, ADC_11db);
  analogSetPinAttenuation(35, ADC_11db); // diagnostic probe for the LDR
  SampleLight();
#endif
#if POWER_BATTERY_SENSE_ENABLED
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  analogRead(BATTERY_ADC_PIN); // settle ADC once
  SampleBattery();
  Serial.printf("[POWER] Battery %.2fV (%u%%), state=ACTIVE\n",
    _batteryV, GetBatteryPercent());
#else
  Serial.println("[POWER] Battery sense disabled (no divider on GPIO34 yet)");
#endif
}

void PowerManager::Update() {
  unsigned long now = millis();
#if POWER_LIGHT_SENSOR_ENABLED
  if (now - _lastLightSampleMs >= 1000) {
    _lastLightSampleMs = now;
    SampleLight();
  }
#endif
  UpdateBacklight();

#if POWER_TEMP_PROTECT_ENABLED
  if (now - _lastTempSampleMs >= 5000) {
    _lastTempSampleMs = now;
    SampleTemperature();
    if (_chipTempC >= 80.0f) {
      if (_state != PowerState::SLEEP) {
        Serial.printf("[POWER] overheat %.1fC, forced SLEEP\n", _chipTempC);
      }
      SetState(PowerState::SLEEP);
    } else if (_chipTempC >= 70.0f &&
               (_state == PowerState::ACTIVE || _state == PowerState::RELAX)) {
      Serial.printf("[POWER] warm %.1fC, forced IDLE\n", _chipTempC);
      SetState(PowerState::IDLE);
    }
  }
#endif

  if (now - _lastStatsLogMs >= 30000) {
    _lastStatsLogMs = now;
    Serial.printf("[STATS] state=%s bl=%u interval=%ums temp=%.1fC\n",
      StateName(), _backlightLevel, GetFrameIntervalMs(), _chipTempC);
  }

#if POWER_BATTERY_SENSE_ENABLED
  // Sample battery once per second; continuous ADC polling wastes power.
  if (now - _lastBatterySampleMs >= 1000) {
    _lastBatterySampleMs = now;
    SampleBattery();
  }

  if (_batteryV < CRITICAL_BATTERY_V) {
    SetState(PowerState::SLEEP);
  } else if (_batteryV < LOW_BATTERY_V && _state == PowerState::ACTIVE) {
    SetState(PowerState::IDLE);
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

uint32_t PowerManager::GetFrameIntervalMs() const {
  switch (_state) {
    case PowerState::ACTIVE: return 33;  // 30 FPS
    case PowerState::RELAX:  return 41;  // ~24 FPS
    case PowerState::IDLE:   return 66;  // ~15 FPS
    case PowerState::SLEEP:  return 200; // touch-poll cadence, no render
  }
  return 33;
}

float PowerManager::GetBatteryVolts() const { return _batteryV; }

uint8_t PowerManager::GetBatteryPercent() const {
  float p = (_batteryV - BATTERY_EMPTY_V) /
            (BATTERY_FULL_V - BATTERY_EMPTY_V);
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return (uint8_t)(p * 100.0f);
}

void PowerManager::SetState(PowerState s) {
  if (_state == s) return;
  _state = s;
  switch (s) {
    case PowerState::ACTIVE:
      SetBacklight(100);
      FacePower_Wake(_face);
      Serial.println("[POWER] state=ACTIVE");
      break;
    case PowerState::RELAX:
      SetBacklight(50);
      FacePower_EnterRelax(_face);
      Serial.println("[POWER] state=RELAX");
      break;
    case PowerState::IDLE:
      SetBacklight(25);
      FacePower_EnterIdle(_face);
      Serial.println("[POWER] state=IDLE");
      break;
    case PowerState::SLEEP:
      FacePower_EnterSleep(_face);
      SetBacklight(0);
      Serial.printf("[POWER] state=SLEEP battery=%.2fV\n", _batteryV);
      break;
  }
}

void PowerManager::SetBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  _stateBacklightTarget = percent;
}

void PowerManager::UpdateBacklight() {
  uint8_t target = _stateBacklightTarget;
#if POWER_LIGHT_SENSOR_ENABLED
  target = (uint8_t)(_stateBacklightTarget * GetLightFactor());
  if (_stateBacklightTarget > 0 && target < 5) target = 5; // min visible
#endif
  _targetBacklightLevel = target;
  if (_backlightLevel == _targetBacklightLevel) return;
  unsigned long now = millis();
  if (now - _lastBacklightStepMs < 20) return;
  _lastBacklightStepMs = now;
  if (_backlightLevel < _targetBacklightLevel) _backlightLevel++;
  else _backlightLevel--;
  ApplyBacklight(_backlightLevel);
}

void PowerManager::ApplyBacklight(uint8_t level) {
  if (level > 100) level = 100;
  analogWrite(_backlightPin, _gammaLut[level]);
}

void PowerManager::SampleLight() {
  uint32_t sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(LIGHT_ADC_PIN);
    delayMicroseconds(200);
  }
  float mv = (float)sum / samples;
  float ambient = (mv - 120.0f) * 100.0f / 380.0f; // 120..500mV -> 0..100
  if (ambient < 0.0f) ambient = 0.0f;
  if (ambient > 100.0f) ambient = 100.0f;
  if (fabsf(ambient - _ambientLevel) >= 2.0f) _ambientLevel = (uint8_t)ambient;
}

float PowerManager::GetLightFactor() const {
  return 0.10f + 0.90f * (_ambientLevel / 100.0f);
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
  uint32_t sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delayMicroseconds(200);
  }
  float mv = (float)sum / samples;
  _batteryV = mv * BATTERY_DIVIDER / 1000.0f;
}

void PowerManager::EnterLightSleep() {
  // Wake on touch IRQ (GPIO36, active low) with a timer fallback so a
  // floating IRQ line cannot lock the device asleep.
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(36), 0);
  esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_PERIOD_MS * 1000ULL);
  esp_light_sleep_start();
  _lastInteractionMs = millis(); // treat every wake as a check point
}
