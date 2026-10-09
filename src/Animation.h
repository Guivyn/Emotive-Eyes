#pragma once

#include <Arduino.h>

// Time-based animation wave shapes. All values are derived from millis(), so
// the animation stays frame-rate independent.
class AnimationBase {
public:
  explicit AnimationBase(unsigned long interval)
    : Interval(interval), _startTime(millis()) {}

  virtual ~AnimationBase() = default;

  void Restart() { _startTime = millis(); }
  float GetValue() { return Calculate(GetElapsed()); }
  unsigned long GetElapsed() const {
    return static_cast<unsigned long>(millis() - _startTime);
  }

  unsigned long Interval;

protected:
  virtual float Calculate(unsigned long elapsedMillis) = 0;

private:
  unsigned long _startTime;
};

class RampAnimation : public AnimationBase {
public:
  explicit RampAnimation(unsigned long interval) : AnimationBase(interval) {}

protected:
  float Calculate(unsigned long elapsedMillis) override {
    if (elapsedMillis >= Interval) return 1.0f;
    // Smoothstep eases in and out while keeping a monotonic, frame-rate
    // independent transition without overshoot.
    float t = static_cast<float>(elapsedMillis) / Interval;
    return t * t * (3.0f - 2.0f * t);
  }
};

class TrapeziumAnimation : public AnimationBase {
public:
  explicit TrapeziumAnimation(unsigned long t) : AnimationBase(t) {
    _t0 = t / 3;
    _t1 = _t0;
    _t2 = t - _t0 - _t1;
  }

  TrapeziumAnimation(unsigned long t0, unsigned long t1, unsigned long t2)
    : AnimationBase(t0 + t1 + t2), _t0(t0), _t1(t1), _t2(t2) {}

protected:
  float Calculate(unsigned long elapsedMillis) override {
    if (elapsedMillis >= Interval) return 0.0f;
    if (elapsedMillis < _t0) return Divide(elapsedMillis, _t0);
    if (elapsedMillis < _t0 + _t1) return 1.0f;
    return 1.0f - Divide(elapsedMillis - _t0 - _t1, _t2);
  }

private:
  unsigned long _t0, _t1, _t2;

  static float Divide(float value, unsigned long divisor) {
    return divisor == 0 ? 0.0f : value / divisor;
  }
};

class TrapeziumPulseAnimation : public AnimationBase {
public:
  TrapeziumPulseAnimation(unsigned long t0, unsigned long t1,
                          unsigned long t2, unsigned long t3,
                          unsigned long t4)
    : AnimationBase(t0 + t1 + t2 + t3 + t4),
      _t0(t0), _t1(t1), _t2(t2), _t3(t3), _t4(t4) {}

  void SetShape(unsigned long t0, unsigned long t1, unsigned long t2,
                unsigned long t3, unsigned long t4) {
    _t0 = t0; _t1 = t1; _t2 = t2; _t3 = t3; _t4 = t4;
    Interval = t0 + t1 + t2 + t3 + t4;
  }

  void SetTriangle(uint16_t t, uint16_t delay) {
    SetShape(0, t / 2, 0, t - t / 2, delay);
  }

protected:
  float Calculate(unsigned long elapsedMillis) override {
    if (Interval == 0) return 0.0f;
    unsigned long e = elapsedMillis % Interval;
    if (e < _t0) return 0.0f;
    if (e < _t0 + _t1) return Divide(e - _t0, _t1);
    if (e < _t0 + _t1 + _t2) return 1.0f;
    if (e < _t0 + _t1 + _t2 + _t3)
      return 1.0f - Divide(e - _t0 - _t1 - _t2, _t3);
    return 0.0f;
  }

private:
  unsigned long _t0, _t1, _t2, _t3, _t4;

  static float Divide(float value, unsigned long divisor) {
    return divisor == 0 ? 0.0f : value / divisor;
  }
};

class AsyncTimer {
public:
  explicit AsyncTimer(unsigned long ms)
    : Interval(ms), _active(false), _done(false), _start(0) {}

  void Start() {
    _start = millis();
    _active = true;
    _done = false;
  }

  void Reset() { Start(); }

  bool Update() {
    if (!_active) return false;
    if (_done) return true;
    if (millis() - _start >= Interval) {
      _done = true;
      _active = false;
      return true;
    }
    return false;
  }

  void SetIntervalMillis(unsigned long ms) { Interval = ms; }
  bool IsExpired() const { return _done; }

  unsigned long Interval;

private:
  bool _active;
  bool _done;
  unsigned long _start;
};
