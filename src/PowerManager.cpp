#include "PowerManager.h"
#include "FacePower.h"
#include "Config.h"
#include "Log.h"
#include "board_config.h"
#include <Preferences.h>

extern TFT_eSPI tft;

#ifndef POWER_LIGHT_SLEEP_ENABLED
  #define POWER_LIGHT_SLEEP_ENABLED 0
#endif
#ifndef POWER_TEMP_PROTECT_ENABLED
  #define POWER_TEMP_PROTECT_ENABLED 1
#endif
#ifndef POWER_PROFILE_ALWAYS_ON
  #define POWER_PROFILE_ALWAYS_ON 0
#endif
#ifndef POWER_ACTIVE_CPU_MHZ
  #define POWER_ACTIVE_CPU_MHZ 80
#endif

static constexpr uint8_t POWER_DEFAULT_BRIGHTNESS = 100;
static constexpr unsigned long SLEEP_BACKLIGHT_FADE_MS = SLEEP_ENTER_DELAY_MS;
static constexpr uint32_t SLEEP_BACKLIGHT_UPDATE_MS = 10UL;
static constexpr char POWER_PREFERENCES_NAMESPACE[] = "facePower";
static constexpr char POWER_BRIGHTNESS_KEY[] = "brightness";

static uint8_t LoadBrightnessPreference() {
  Preferences preferences;
  if (!preferences.begin(POWER_PREFERENCES_NAMESPACE, true)) {
    return POWER_DEFAULT_BRIGHTNESS;
  }
  uint8_t brightness = preferences.getUChar(
      POWER_BRIGHTNESS_KEY, POWER_DEFAULT_BRIGHTNESS);
  preferences.end();
  if (brightness < PowerManager::MIN_BRIGHTNESS_PERCENT ||
      brightness > PowerManager::MAX_BRIGHTNESS_PERCENT) {
    return POWER_DEFAULT_BRIGHTNESS;
  }
  return brightness;
}

static bool SaveBrightnessPreference(uint8_t brightness) {
  Preferences preferences;
  if (!preferences.begin(POWER_PREFERENCES_NAMESPACE, false)) return false;
  bool saved = preferences.putUChar(POWER_BRIGHTNESS_KEY, brightness) > 0;
  preferences.end();
  return saved;
}

static const uint8_t CPU_MHZ_ACTIVE = POWER_ACTIVE_CPU_MHZ;
static const uint8_t CPU_MHZ_RELAX  = 80;
static const uint8_t CPU_MHZ_IDLE   = 40;
static const uint8_t CPU_MHZ_SLEEP  = 80;
static void ApplyCpuFrequency(PowerState state);
#if defined(POWER_TEST_FAST_TIMEOUTS)
static constexpr unsigned long POWER_ACTIVE_TIMEOUT_MS_EFFECTIVE = 8 * 1000UL;
static constexpr unsigned long POWER_RELAX_TIMEOUT_MS_EFFECTIVE = 18 * 1000UL;
static constexpr unsigned long POWER_IDLE_TIMEOUT_MS_EFFECTIVE = 45 * 1000UL;
#else
static constexpr unsigned long POWER_ACTIVE_TIMEOUT_MS_EFFECTIVE =
    PowerManager::ACTIVE_TIMEOUT_MS;
static constexpr unsigned long POWER_RELAX_TIMEOUT_MS_EFFECTIVE =
    PowerManager::RELAX_TIMEOUT_MS;
static constexpr unsigned long POWER_IDLE_TIMEOUT_MS_EFFECTIVE =
    PowerManager::IDLE_TIMEOUT_MS;
#endif
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
    _userBrightness(POWER_DEFAULT_BRIGHTNESS),
    _brightnessPreferenceDirty(false),
    _backlightLevel(0),
    _targetBacklightLevel(100),
    _stateBacklightTarget(100),
    _lastBacklightStepMs(0),
    _sleepFadeActive(false),
    _sleepFadeStartLevel(0),
    _sleepFadeTargetLevel(0),
    _sleepFadeStartMs(0),
    _lastTempSampleMs(0),
    _lastStatsLogMs(0),
    _chipTempC(30.0f),
    _overheatRecovery(false),
    _displaySleepCommanded(false),
    _displayOffCommanded(false),
    _gammaLut{} {
  for (int i = 0; i <= 100; i++) {
    _gammaLut[i] = (uint8_t)(255.0f * powf(i / 100.0f, 2.2f) + 0.5f);
  }
  // Apply the selected active clock before the first face frame is rendered.
  ApplyCpuFrequency(_state);
}

void PowerManager::Begin() {
  pinMode(_backlightPin, OUTPUT);
  _userBrightness = LoadBrightnessPreference();
  _stateBacklightTarget = BrightnessForState(_state);
  // Show the first face frame immediately. State transitions after boot still
  // use the smoothed backlight ramp.
  _backlightLevel = _stateBacklightTarget;
  _targetBacklightLevel = _stateBacklightTarget;
  ApplyBacklight(_backlightLevel);
}

void PowerManager::Update() {
  unsigned long now = millis();
  if (_state == PowerState::SOFT_OFF) {
    // RST/EN is the only wake path. This call does not return until hardware
    // reset; no timer, UART polling, or temperature sampling remains active.
    EnterSoftOffDeepSleep();
    return;
  }
  UpdateBacklight();
  if (_state == PowerState::SLEEP && !_sleepFadeActive &&
      _backlightLevel == 0 && !_displayOffCommanded) {
    // Keep the panel enabled while the visible backlight fade is in progress.
    tft.writecommand(0x28); // DISPOFF after the backlight has reached zero
    _displayOffCommanded = true;
  }

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

#if !POWER_PROFILE_ALWAYS_ON
  unsigned long idleFor = now - _lastInteractionMs;
  if (_state == PowerState::ACTIVE &&
      idleFor >= POWER_ACTIVE_TIMEOUT_MS_EFFECTIVE) {
    SetState(PowerState::RELAX);
  } else if (_state == PowerState::RELAX &&
             idleFor >= POWER_RELAX_TIMEOUT_MS_EFFECTIVE) {
    SetState(PowerState::IDLE);
  } else if (_state == PowerState::IDLE &&
             idleFor >= POWER_IDLE_TIMEOUT_MS_EFFECTIVE) {
    SetState(PowerState::SLEEP);
  }
#endif

  if (_state == PowerState::SLEEP) {
#if POWER_LIGHT_SLEEP_ENABLED
    if (!_sleepFadeActive) EnterLightSleep();
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
  if (_state == PowerState::ACTIVE && _sleepFadeActive) {
    _sleepFadeActive = false;
    SetBacklight(BrightnessForState(_state));
  }
  if (_state != PowerState::ACTIVE) SetState(PowerState::ACTIVE);
}

void PowerManager::EnterSleep() {
  SetState(PowerState::SLEEP);
}

void PowerManager::EnterIdle() {
  SetState(PowerState::IDLE);
}

void PowerManager::EnterSoftOff() {
  SetState(PowerState::SOFT_OFF);
}

uint32_t PowerManager::GetFrameIntervalMs() const {
  switch (_state) {
    case PowerState::ACTIVE: return FRAME_INTERVAL_ACTIVE_MS;
    case PowerState::RELAX:  return FRAME_INTERVAL_RELAX_MS;
    case PowerState::IDLE:   return FRAME_INTERVAL_IDLE_MS;
    case PowerState::SLEEP:
      return _sleepFadeActive ? SLEEP_BACKLIGHT_UPDATE_MS
                              : SLEEP_POLL_INTERVAL_MS;
    case PowerState::SOFT_OFF: return SLEEP_POLL_INTERVAL_MS;
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
    case PowerState::SOFT_OFF: mhz = CPU_MHZ_SLEEP;  break;
  }
  if (getCpuFrequencyMhz() != mhz) {
    // The ESP32 APB clock drops to 40MHz at a 40MHz CPU clock. Drain pending
    // bytes before that switch, then restore the console baud against the new
    // APB clock so diagnostics remain readable in IDLE.
    Serial.flush();
    setCpuFrequencyMhz(mhz);
    Serial.updateBaudRate(115200);
  }
  LOG_PRINTF("[POWER] cpu=%uMHz\n", getCpuFrequencyMhz());
}

void PowerManager::SetState(PowerState s) {
  if (_state == s) return;
  _state = s;
  switch (s) {
    case PowerState::ACTIVE:
      ApplyCpuFrequency(s);
      _sleepFadeActive = false;
      if (_displaySleepCommanded) {
        tft.writecommand(0x11); // SLPOUT
        delay(120);
        _displaySleepCommanded = false;
      }
      tft.writecommand(0x29); // DISPON
      _displayOffCommanded = false;
      SetBacklight(BrightnessForState(s));
      FacePower_Wake(_face);
      LOG_PRINTLN("[POWER] state=ACTIVE");
      break;
    case PowerState::RELAX:
      ApplyCpuFrequency(s);
      SetBacklight(BrightnessForState(s));
      FacePower_EnterRelax(_face);
      LOG_PRINTLN("[POWER] state=RELAX");
      break;
    case PowerState::IDLE:
      ApplyCpuFrequency(s);
      SetBacklight(BrightnessForState(s));
      FacePower_EnterIdle(_face);
      LOG_PRINTLN("[POWER] state=IDLE");
      break;
    case PowerState::SLEEP:
      FacePower_EnterSleep(_face);
      _displayOffCommanded = false;
      BeginBacklightFade(0);
      ApplyCpuFrequency(s);
      LOG_PRINTLN("[POWER] state=SLEEP");
      break;
    case PowerState::SOFT_OFF:
      _sleepFadeActive = false;
      _backlightLevel = _stateBacklightTarget = _targetBacklightLevel = 0;
      ApplyBacklight(0);
      tft.writecommand(0x28); // display off
      _displayOffCommanded = true;
      tft.writecommand(0x10); // panel sleep; lower controller current
      _displaySleepCommanded = true;
      pinMode(PIN_TFT_CS, OUTPUT); digitalWrite(PIN_TFT_CS, HIGH);
      pinMode(PIN_TOUCH_CS, OUTPUT); digitalWrite(PIN_TOUCH_CS, HIGH);
      pinMode(PIN_TFT_SCLK, OUTPUT); digitalWrite(PIN_TFT_SCLK, LOW);
      pinMode(PIN_TFT_MOSI, OUTPUT); digitalWrite(PIN_TFT_MOSI, LOW);
      pinMode(PIN_TFT_DC, OUTPUT); digitalWrite(PIN_TFT_DC, LOW);
      pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);
      pinMode(PIN_AUDIO, OUTPUT); digitalWrite(PIN_AUDIO, LOW);
      pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);
      pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, HIGH);
      pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);
      setCpuFrequencyMhz(80);
      Serial.println("[POWER] state=SOFT_OFF deep_sleep=1 wake=RST/EN");
      break;
  }
}

void PowerManager::SetBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  _stateBacklightTarget = percent;
}

void PowerManager::BeginSleepFade() {
  // A manual down-swipe settles in IDLE, whose target is 25% of the user's
  // selected brightness. Fade to that level while the sleepy face animates.
  BeginBacklightFade(BrightnessForState(PowerState::IDLE));
}

void PowerManager::BeginBacklightFade(uint8_t targetLevel) {
  if (_sleepFadeActive && _sleepFadeTargetLevel == targetLevel) return;
  _sleepFadeStartLevel = _backlightLevel;
  _sleepFadeTargetLevel = targetLevel;
  SetBacklight(targetLevel);
  if (_sleepFadeStartLevel == targetLevel) {
    _sleepFadeActive = false;
    return;
  }
  _sleepFadeStartMs = millis();
  _lastBacklightStepMs = _sleepFadeStartMs;
  _sleepFadeActive = true;
}

uint8_t PowerManager::BrightnessForState(PowerState state) const {
  switch (state) {
    case PowerState::ACTIVE:
      return _userBrightness;
    case PowerState::RELAX: {
      uint8_t level = (uint8_t)((_userBrightness * 50U + 50U) / 100U);
      return level < MIN_BRIGHTNESS_PERCENT ? MIN_BRIGHTNESS_PERCENT : level;
    }
    case PowerState::IDLE: {
      uint8_t level = (uint8_t)((_userBrightness * 25U + 50U) / 100U);
      return level < MIN_BRIGHTNESS_PERCENT ? MIN_BRIGHTNESS_PERCENT : level;
    }
    case PowerState::SLEEP:
    case PowerState::SOFT_OFF:
      return 0;
  }
  return _userBrightness;
}

bool PowerManager::SetBrightness(uint8_t percent) {
  if (percent < MIN_BRIGHTNESS_PERCENT ||
      percent > MAX_BRIGHTNESS_PERCENT) {
    return false;
  }
  if (_userBrightness == percent && !_brightnessPreferenceDirty) return true;
  _userBrightness = percent;
  SetBacklight(BrightnessForState(_state));
  _brightnessPreferenceDirty = !SaveBrightnessPreference(percent);
  return !_brightnessPreferenceDirty;
}

void PowerManager::UpdateBacklight() {
  uint8_t target = _stateBacklightTarget;
  _targetBacklightLevel = target;
  if (_sleepFadeActive) {
    const unsigned long now = millis();
    const unsigned long elapsed = now - _sleepFadeStartMs;
    if (elapsed >= SLEEP_BACKLIGHT_FADE_MS) {
      _sleepFadeActive = false;
      if (_backlightLevel != _sleepFadeTargetLevel) {
        _backlightLevel = _sleepFadeTargetLevel;
        ApplyBacklight(_backlightLevel);
      }
    } else {
      if (now - _lastBacklightStepMs < SLEEP_BACKLIGHT_UPDATE_MS) return;
      _lastBacklightStepMs = now;
      const float progress = (float)elapsed / SLEEP_BACKLIGHT_FADE_MS;
      const float progress2 = progress * progress;
      const float progress3 = progress2 * progress;
      const float eased = progress3 *
          (progress * (progress * 6.0f - 15.0f) + 10.0f);
      const float level = _sleepFadeStartLevel +
          (_sleepFadeTargetLevel - _sleepFadeStartLevel) * eased;
      const uint8_t nextLevel = (uint8_t)(level + 0.5f);
      if (nextLevel != _backlightLevel) {
        _backlightLevel = nextLevel;
        ApplyBacklight(_backlightLevel);
      }
    }
    return;
  }
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
    case PowerState::SOFT_OFF: return "SOFT_OFF";
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

void PowerManager::EnterSoftOffLightSleep() {
  esp_light_sleep_start();
}

void PowerManager::EnterSoftOffDeepSleep() {
  // No GPIO/timer wake source is registered. The board's hardware RST/EN
  // line resets the ESP32 and starts setup() again while supply remains on.
  Serial.flush();
  delay(10);
  esp_deep_sleep_start();
}
