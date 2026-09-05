#include "PowerManager.h"
#include "FacePower.h"
#include "Config.h"
#include "Log.h"

extern TFT_eSPI tft;

#ifndef POWER_LIGHT_SLEEP_ENABLED
  #define POWER_LIGHT_SLEEP_ENABLED 0
#endif
#ifndef POWER_TEMP_PROTECT_ENABLED
  #define POWER_TEMP_PROTECT_ENABLED 1
#endif

static const uint8_t CPU_MHZ_ACTIVE = 240;
static const uint8_t CPU_MHZ_RELAX  = 160;
static const uint8_t CPU_MHZ_IDLE   = 80;
static const uint8_t CPU_MHZ_SLEEP  = 80;
static constexpr float POWER_WARM_ENTER_TEMP_C = 70.0f;
static constexpr float POWER_SLEEP_ENTER_TEMP_C = 80.0f;
// Hysteresis prevents a hot board from oscillating between ACTIVE and IDLE/
// SLEEP as the sensor value moves around an entry threshold.
static constexpr float POWER_RECOVERY_TEMP_C = 65.0f;

PowerManager::PowerManager(Face& face, uint8_t backlightPin)
  : _face(face),
    _backlightPin(backlightPin),
    _state(PowerState::ACTIVE),
    _lastInteractionMs(0),
    _backlightLevel(0),
    _targetBacklightLevel(100),
    _stateBacklightTarget(100),
    _lastBacklightStepMs(0),
    _lastTempSampleMs(0),
    _lastStatsLogMs(0),
    _chipTempC(30.0f),
    _overheatRecovery(false),
    _gammaLut{} {
  for (int i = 0; i <= 100; i++) {
    _gammaLut[i] = (uint8_t)(255.0f * powf(i / 100.0f, 2.2f) + 0.5f);
  }
}

void PowerManager::Begin() {
  pinMode(_backlightPin, OUTPUT);
  // Show the first face frame immediately. State transitions after boot still
  // use the smoothed backlight ramp.
  _backlightLevel = _stateBacklightTarget;
  _targetBacklightLevel = _stateBacklightTarget;
  ApplyBacklight(_backlightLevel);
}

void PowerManager::Update() {
  unsigned long now = millis();
  UpdateBacklight();

#if POWER_TEMP_PROTECT_ENABLED
  if (now - _lastTempSampleMs >= 5000) {
    _lastTempSampleMs = now;
    SampleTemperature();
    if (_chipTempC >= POWER_SLEEP_ENTER_TEMP_C) {
      _overheatRecovery = true;
      if (_state != PowerState::SLEEP) {
        LOG_PRINTF("[POWER] overheat %.1fC, forced SLEEP\n", _chipTempC);
      }
      SetState(PowerState::SLEEP);
    } else if (_chipTempC >= POWER_WARM_ENTER_TEMP_C &&
               (_state == PowerState::ACTIVE || _state == PowerState::RELAX)) {
      _overheatRecovery = true;
      LOG_PRINTF("[POWER] warm %.1fC, forced IDLE\n", _chipTempC);
      SetState(PowerState::IDLE);
    } else if (_chipTempC <= POWER_RECOVERY_TEMP_C && _overheatRecovery) {
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
    LOG_PRINTF("[STATS] state=%s bl=%u interval=%ums temp=%.1fC\n",
      StateName(), _backlightLevel, GetFrameIntervalMs(), _chipTempC);
  }

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
    // Light sleep disabled: loop() handles the 50ms touch-poll cadence.
#endif
  }
}

void PowerManager::NotifyInteraction() {
  _lastInteractionMs = millis();
  if (_overheatRecovery) {
    LOG_PRINTLN("[POWER] interaction ignored until temperature recovers");
    return;
  }
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
    case PowerState::ACTIVE: return FRAME_INTERVAL_ACTIVE_MS;
    case PowerState::RELAX:  return FRAME_INTERVAL_RELAX_MS;
    case PowerState::IDLE:   return FRAME_INTERVAL_IDLE_MS;
    case PowerState::SLEEP:  return SLEEP_POLL_INTERVAL_MS;
  }
  return FRAME_INTERVAL_ACTIVE_MS;
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
      LOG_PRINTLN("[POWER] state=SLEEP");
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

void PowerManager::EnterLightSleep() {
  // Wake on touch IRQ (GPIO36, active low) with a timer fallback so a
  // floating IRQ line cannot lock the device asleep.
  static const gpio_num_t TOUCH_WAKE_GPIO = static_cast<gpio_num_t>(36);
  esp_sleep_enable_ext0_wakeup(TOUCH_WAKE_GPIO, 0);
  esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_PERIOD_MS * 1000ULL);
  esp_light_sleep_start();
}
