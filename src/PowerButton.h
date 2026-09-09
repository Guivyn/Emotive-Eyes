#pragma once
#include <stdint.h>

// Release events only; unsigned subtraction survives millis rollover.
class PowerButton {
public:
  enum class Event { NONE, PRESS, SHORT_PRESS, LONG_PRESS };
  Event Update(bool down, uint32_t now) {
    if (down != _raw) { _raw = down; _changed = now; }
    if (down == _stable || uint32_t(now - _changed) < 40) return Event::NONE;
    _stable = down;
    if (down) { _pressed = now; return Event::PRESS; }
    return uint32_t(now - _pressed) >= 2000 ? Event::LONG_PRESS : Event::SHORT_PRESS;
  }
private:
  bool _raw = false, _stable = false;
  uint32_t _changed = 0, _pressed = 0;
};
