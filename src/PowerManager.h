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

  // Called by Face to drop into low-power IDLE while keeping the face visible.
  void EnterIdle();
  void EnterSoftOff();

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
  uint8_t _backlightLevel;      // current perceived brightness (0-100)
  uint8_t _targetBacklightLevel;
  uint8_t _stateBacklightTarget; // brightness set by the current power state
  unsigned long _lastBacklightStepMs;
  unsigned long _lastTempSampleMs;
  unsigned long _lastStatsLogMs;
  float _chipTempC;
  bool _overheatRecovery;
  bool _displaySleepCommanded;
  uint8_t _gammaLut[101];

  void SetState(PowerState s);
  void SetBacklight(uint8_t percent); // set target, smoothed by Update()
  void UpdateBacklight();
  void ApplyBacklight(uint8_t level);
  void SampleTemperature();
  const char* StateName() const;
  void EnterLightSleep();
  void EnterSoftOffLightSleep();
  void EnterSoftOffDeepSleep();
};
