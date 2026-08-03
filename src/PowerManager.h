#pragma once

#include <Arduino.h>

// Power management for the Emotive Eyes robot.
// Backlight PWM + Active/Idle/LightSleep state machine + battery monitor.
enum class PowerState : uint8_t {
  ACTIVE,
  RELAX,
  IDLE,
  SLEEP
};

class Face;
extern Face* gFace;
extern class PowerManager* gPowerManager;

class PowerManager {
public:
  PowerManager(Face& face, uint8_t backlightPin);

  void Begin();
  void Update();

  // Called by Face when the user touches the screen.
  void NotifyInteraction();

  // Main-loop cadence: fast in ACTIVE, slower in RELAX/IDLE, poll-only in SLEEP.
  uint32_t GetFrameIntervalMs() const;

  float GetBatteryVolts() const;
  uint8_t GetBatteryPercent() const;
  PowerState GetState() const { return _state; }

  static const unsigned long ACTIVE_TIMEOUT_MS = 60 * 1000UL;
  static const unsigned long RELAX_TIMEOUT_MS  = 3 * 60 * 1000UL;
  static const unsigned long IDLE_TIMEOUT_MS   = 5 * 60 * 1000UL;
  static const unsigned long LIGHT_SLEEP_PERIOD_MS = 3 * 1000UL;
  static const float LOW_BATTERY_V;    // enter forced idle + dim
  static const float CRITICAL_BATTERY_V; // forced sleep
  static const float BATTERY_FULL_V;   // 4.2V for 1S LiPo
  static const float BATTERY_EMPTY_V;  // 3.0V cutoff

private:
  Face& _face;
  uint8_t _backlightPin;
  PowerState _state;
  unsigned long _lastInteractionMs;
  unsigned long _lastBatterySampleMs;
  float _batteryV;
  uint8_t _backlightLevel;      // current perceived brightness (0-100)
  uint8_t _targetBacklightLevel;
  uint8_t _stateBacklightTarget; // brightness set by the current power state
  uint8_t _ambientLevel;         // 0-100, from the GPIO34 light sensor
  unsigned long _lastBacklightStepMs;
  unsigned long _lastLightSampleMs;
  unsigned long _lastLightLogMs;
  unsigned long _lastTempSampleMs;
  unsigned long _lastStatsLogMs;
  float _chipTempC;
  uint8_t _gammaLut[101];

  void SetState(PowerState s);
  void SetBacklight(uint8_t percent); // set target, smoothed by Update()
  void UpdateBacklight();
  void ApplyBacklight(uint8_t level);
  void SampleLight();
  float GetLightFactor() const;
  void SampleTemperature();
  const char* StateName() const;
  void SampleBattery();
  void EnterLightSleep();
};
