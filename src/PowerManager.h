#pragma once

#include <Arduino.h>
#include "PowerIC.h"

// Power management for the Emotive Eyes robot.
// Backlight PWM + Active/Idle/LightSleep state machine + battery monitor.
enum class PowerState : uint8_t {
  ACTIVE,
  RELAX,
  IDLE,
  SLEEP
};

class Face;
class PowerManager;
extern Face* gFace;
extern PowerManager* gPowerManager;

class PowerManager {
public:
  PowerManager(Face& face, uint8_t backlightPin, float bootBatteryV = -1.0f);

  void Begin();
  void Update();

  // Called by Face when the user touches the screen.
  void NotifyInteraction();

  // Called by Face when the user gestures "go to sleep".
  void EnterSleep();

  // Called by Face to drop into low-power IDLE while keeping the face visible.
  void EnterIdle();

  // Main-loop cadence: fast in ACTIVE, slower in RELAX/IDLE, poll-only in SLEEP.
  uint32_t GetFrameIntervalMs() const;

  uint8_t GetBatteryPercent() const;
  float GetBatteryVoltage() const { return _batteryV; }
  bool HasBatteryReading() const { return _batteryValid; }
  bool IsCharging() const;
  bool IsChargeFull() const;
  // True when the charging state came from the I2C power IC rather than the
  // voltage-trend heuristic (useful for serial diagnostics).
  bool ChargingSourceIsI2C() const {
#if POWER_IC_ENABLED
    return _powerIc.Found();
#else
    return false;
#endif
  }
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
  unsigned long _lastIcSampleMs;
  bool _lastIcCharging;
  bool _lastIcFull;
  int8_t _lastIcPercent;
  float _batteryV;
  float _bootBatteryV;
  float _lastLoggedV;
  float _lastRawMv;
  unsigned long _lastInvalidLogMs;
  float _trendSampleV;
  unsigned long _lastTrendSampleMs;
  bool _trendCharging;
  bool _batteryValid;
  PowerIC _powerIc;
  uint8_t _backlightLevel;      // current perceived brightness (0-100)
  uint8_t _targetBacklightLevel;
  uint8_t _stateBacklightTarget; // brightness set by the current power state
  unsigned long _lastBacklightStepMs;
  unsigned long _lastTempSampleMs;
  unsigned long _lastStatsLogMs;
  float _chipTempC;
  bool _overheatRecovery;
  uint8_t _gammaLut[101];

  void SetState(PowerState s);
  void SetBacklight(uint8_t percent); // set target, smoothed by Update()
  void UpdateBacklight();
  void ApplyBacklight(uint8_t level);
  void SampleTemperature();
  const char* StateName() const;
  void SampleBattery();
  void EnterLightSleep();
};
