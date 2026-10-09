#pragma once
#include <stdint.h>

// Release events only; unsigned subtraction survives millis rollover.
class PowerButton {
public:
  enum class Event { NONE, PRESS, SHORT_PRESS, LONG_PRESS };
  static constexpr uint32_t LONG_PRESS_DURATION_MS = 1000;
  Event Update(bool down, uint32_t now) {
    if (down != _raw) { _raw = down; _changed = now; }
    if (down == _stable || uint32_t(now - _changed) < 40) return Event::NONE;
    _stable = down;
    if (down) { _pressed = now; return Event::PRESS; }
    return uint32_t(now - _pressed) >= LONG_PRESS_DURATION_MS
        ? Event::LONG_PRESS : Event::SHORT_PRESS;
  }
private:
  bool _raw = false, _stable = false;
  uint32_t _changed = 0, _pressed = 0;
};
