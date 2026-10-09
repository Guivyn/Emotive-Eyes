#pragma once

#include <Arduino.h>

// Power management for the Emotive Eyes robot.
// Backlight PWM + Active/Idle/LightSleep state machine.
enum class PowerState : uint8_t {
  ACTIVE,
  RELAX,
  IDLE,
  SLEEP,
  SOFT_OFF
};

class Face;
class PowerManager;
extern Face* gFace;
extern PowerManager* gPowerManager;

class PowerManager {
public:
  PowerManager(Face& face, uint8_t backlightPin);

  void Begin();
  void Update();

  // Called by Face when the user touches the screen.
  void NotifyInteraction();

  // Called by Face when the user gestures "go to sleep".
  void EnterSleep();
  // Start dimming as soon as the swipe is recognized, while the eyes close.
  void BeginSleepFade();

  // Called by Face to drop into low-power IDLE while keeping the face visible.
  void EnterIdle();
  void EnterSoftOff();

  // User brightness is stored as a perceived 20-100% level. State dimming is
  // applied relative to this level, so manual sleep remains visibly dimmer.
  static constexpr uint8_t MIN_BRIGHTNESS_PERCENT = 20;
  static constexpr uint8_t MAX_BRIGHTNESS_PERCENT = 100;
  uint8_t GetBrightness() const { return _userBrightness; }
  bool SetBrightness(uint8_t percent);

  // Main-loop cadence: fast in ACTIVE, slower in RELAX/IDLE, poll-only in SLEEP.
  uint32_t GetFrameIntervalMs() const;

  PowerState GetState() const { return _state; }

  static const unsigned long ACTIVE_TIMEOUT_MS = 60 * 1000UL;
  static const unsigned long RELAX_TIMEOUT_MS  = 3 * 60 * 1000UL;
  static const unsigned long IDLE_TIMEOUT_MS   = 5 * 60 * 1000UL;
  static const unsigned long LIGHT_SLEEP_PERIOD_MS = 3 * 1000UL;

private:
  Face& _face;
  uint8_t _backlightPin;
  PowerState _state;
  unsigned long _lastInteractionMs;
  uint8_t _userBrightness;
  bool _brightnessPreferenceDirty;
  uint8_t _backlightLevel;      // current perceived brightness (0-100)
  uint8_t _targetBacklightLevel;
  uint8_t _stateBacklightTarget; // brightness set by the current power state
  unsigned long _lastBacklightStepMs;
  bool _sleepFadeActive;
  uint8_t _sleepFadeStartLevel;
  uint8_t _sleepFadeTargetLevel;
  unsigned long _sleepFadeStartMs;
  unsigned long _lastTempSampleMs;
  unsigned long _lastStatsLogMs;
  float _chipTempC;
  bool _overheatRecovery;
  bool _displaySleepCommanded;
  bool _displayOffCommanded;
  uint8_t _gammaLut[101];

  void SetState(PowerState s);
  void SetBacklight(uint8_t percent); // set target, smoothed by Update()
  void BeginBacklightFade(uint8_t targetLevel);
  uint8_t BrightnessForState(PowerState state) const;
  void UpdateBacklight();
  void ApplyBacklight(uint8_t level);
  void SampleTemperature();
  const char* StateName() const;
  void EnterLightSleep();
  void EnterSoftOffLightSleep();
  void EnterSoftOffDeepSleep();
};
