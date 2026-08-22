#include "Face.h"
#include "Log.h"
#include <LittleFS.h>

extern TFT_eSPI tft;

static const EyeConfig Preset_Normal = {0, 0, 50, 50, 0, 0, 10, 10};
static const EyeConfig Preset_Happy = {0, 0, 12, 50, 0, 0, 12, 0};
static const EyeConfig Preset_Glee = {0, 0, 10, 50, 0, 0, 10, 0};
static const EyeConfig Preset_Sad = {0, 0, 19, 50, -0.5f, 0, 1, 12};
static const EyeConfig Preset_Worried = {0, 0, 31, 50, -0.1f, 0, 8, 12};
static const EyeConfig Preset_Worried_Alt = {0, 0, 44, 50, -0.2f, 0, 8, 12};
static const EyeConfig Preset_Focused = {0, 0, 18, 50, 0.2f, 0, 4, 1};
static const EyeConfig Preset_Annoyed = {0, 0, 15, 50, 0, 0, 0, 12};
static const EyeConfig Preset_Annoyed_Alt = {0, 0, 6, 50, 0, 0, 0, 5};
static const EyeConfig Preset_Surprised = {-2, 0, 56, 56, 0, 0, 20, 20};
static const EyeConfig Preset_Skeptic = {0, 0, 50, 50, 0, 0, 12, 12};
static const EyeConfig Preset_Skeptic_Alt = {0, -8, 32, 50, 0.3f, 0, 1, 12};
static const EyeConfig Preset_Frustrated = {4, -6, 15, 50, 0, 0, 0, 12};
static const EyeConfig Preset_Unimpressed = {4, 0, 15, 50, 0, 0, 1, 12};
static const EyeConfig Preset_Unimpressed_Alt = {4, -4, 28, 50, 0, 0, 1, 20};
static const EyeConfig Preset_Sleepy = {0, -2, 18, 50, -0.5f, -0.5f, 4, 4};
static const EyeConfig Preset_Sleepy_Alt = {0, -2, 10, 50, -0.5f, -0.5f, 4, 4};
static const EyeConfig Preset_Suspicious = {0, 0, 28, 50, 0, 0, 10, 4};
static const EyeConfig Preset_Suspicious_Alt = {0, -4, 20, 50, 0.2f, 0, 8, 4};
static const EyeConfig Preset_Squint = {-12, -4, 44, 44, 0, 0, 10, 10};
static const EyeConfig Preset_Squint_Alt = {6, 0, 25, 25, 0, 0, 6, 6};
static const EyeConfig Preset_Angry = {-4, 0, 25, 50, 0.3f, 0, 2, 15};
static const EyeConfig Preset_Furious = {-2, 0, 38, 50, 0.4f, 0, 2, 10};
static const EyeConfig Preset_Scared = {-4, 0, 50, 50, -0.1f, 0, 15, 10};
static const EyeConfig Preset_Awe = {2, 0, 44, 56, -0.1f, 0.1f, 15, 15};
static const EyeConfig Preset_Coquette = {0, -6, 20, 44, 0.25f, -0.25f, 10, 8};

LookAssistant::LookAssistant(Face& face)
  : _face(face), Timer(2500), _minInterval(1500), _maxInterval(3500) {
  Timer.Start();
}

void LookAssistant::LookAt(float x, float y) {
  Transformation t;
  float mx = -20 * x;
  float my = 20 * y;
  float sy = (1.0f - x * 0.2f) * (1.0f - (y > 0 ? y : -y) * 0.4f);
  t.MoveX = mx;
  t.MoveY = my;
  t.ScaleX = 1.0f;
  t.ScaleY = sy;
  _face.RightEye.Transformation.SetDestin(t);
  _face.LeftEye.Transformation.SetDestin(t);
  _face.RightEye.Transformation.Animation.Restart();
  _face.LeftEye.Transformation.Animation.Restart();
}

void LookAssistant::Update() {
  Timer.Update();
  if (!Timer.IsExpired()) return;

  float x = random(-100, 101) / 100.0f;
  float y = random(-100, 101) / 100.0f;
  LookAt(x, y);
  Timer.SetIntervalMillis(_minInterval + random(_maxInterval - _minInterval + 1));
  Timer.Reset();
}

void LookAssistant::SetIntervalRange(unsigned long minMs, unsigned long maxMs) {
  _minInterval = minMs;
  _maxInterval = maxMs;
}

BlinkAssistant::BlinkAssistant(Face& face)
  : _face(face), Timer(3000), _minInterval(2000), _maxInterval(5000) {
  Timer.Start();
}

void BlinkAssistant::Blink() {
  _face.LeftEye.BlinkTransformation.Animation.Restart();
  _face.RightEye.BlinkTransformation.Animation.Restart();
}

void BlinkAssistant::Update() {
  Timer.Update();
  if (!Timer.IsExpired()) return;

  Blink();
  Timer.SetIntervalMillis(_minInterval + random(_maxInterval - _minInterval + 1));
  Timer.Reset();
}

void BlinkAssistant::SetIntervalRange(unsigned long minMs, unsigned long maxMs) {
  _minInterval = minMs;
  _maxInterval = maxMs;
}

FaceExpression::FaceExpression(Face& face) : _face(face) {}

void FaceExpression::ClearVariations() {
  _face.RightEye.Variation1.StartFadeOut();
  _face.RightEye.Variation2.StartFadeOut();
  _face.LeftEye.Variation1.StartFadeOut();
  _face.LeftEye.Variation2.StartFadeOut();
  _face.RightEye.Variation1.Animation.Restart();
  _face.LeftEye.Variation1.Animation.Restart();
}

void FaceExpression::GoTo_Normal() {
  ClearVariations();
  _face.RightEye.Variation1.Values.Height = 3;
  _face.RightEye.Variation2.Values.Width = 1;
  _face.LeftEye.Variation1.Values.Height = 2;
  _face.LeftEye.Variation2.Values.Width = 2;
  _face.RightEye.Variation1.Animation.SetTriangle(1000, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(1000, 0);
  _face.RightEye.TransitionTo(Preset_Normal);
  _face.LeftEye.TransitionTo(Preset_Normal);
}

void FaceExpression::GoTo_Angry() {
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 2;
  _face.LeftEye.Variation1.Values.OffsetY = 2;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Angry);
  _face.LeftEye.TransitionTo(Preset_Angry);
}

void FaceExpression::GoTo_Glee() {
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 5;
  _face.LeftEye.Variation1.Values.OffsetY = 5;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Glee);
  _face.LeftEye.TransitionTo(Preset_Glee);
}

void FaceExpression::GoTo_Happy() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Happy);
  _face.LeftEye.TransitionTo(Preset_Happy);
}

void FaceExpression::GoTo_Sad() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Sad);
  _face.LeftEye.TransitionTo(Preset_Sad);
}

void FaceExpression::GoTo_Worried() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Worried);
  _face.LeftEye.TransitionTo(Preset_Worried_Alt);
}

void FaceExpression::GoTo_Focused() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Focused);
  _face.LeftEye.TransitionTo(Preset_Focused);
}

void FaceExpression::GoTo_Annoyed() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Annoyed);
  _face.LeftEye.TransitionTo(Preset_Annoyed_Alt);
}

void FaceExpression::GoTo_Surprised() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Surprised);
  _face.LeftEye.TransitionTo(Preset_Surprised);
}

void FaceExpression::GoTo_Skeptic() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Skeptic);
  _face.LeftEye.TransitionTo(Preset_Skeptic_Alt);
}

void FaceExpression::GoTo_Frustrated() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Frustrated);
  _face.LeftEye.TransitionTo(Preset_Frustrated);
}

void FaceExpression::GoTo_Unimpressed() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Unimpressed);
  _face.LeftEye.TransitionTo(Preset_Unimpressed_Alt);
}

void FaceExpression::GoTo_Sleepy() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Sleepy);
  _face.LeftEye.TransitionTo(Preset_Sleepy_Alt);
}

void FaceExpression::GoTo_Suspicious() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Suspicious);
  _face.LeftEye.TransitionTo(Preset_Suspicious_Alt);
}

void FaceExpression::GoTo_Squint() {
  ClearVariations();
  _face.LeftEye.Variation1.Values.OffsetX = 6;
  _face.LeftEye.Variation2.Values.OffsetY = 6;
  _face.RightEye.TransitionTo(Preset_Squint);
  _face.LeftEye.TransitionTo(Preset_Squint_Alt);
}

void FaceExpression::GoTo_Furious() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Furious);
  _face.LeftEye.TransitionTo(Preset_Furious);
}

void FaceExpression::GoTo_Scared() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Scared);
  _face.LeftEye.TransitionTo(Preset_Scared);
}

void FaceExpression::GoTo_Awe() {
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Awe);
  _face.LeftEye.TransitionTo(Preset_Awe);
}

void FaceExpression::GoTo_Coquette() {
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 3;
  _face.LeftEye.Variation1.Values.OffsetY = 3;
  _face.RightEye.Variation1.Animation.SetTriangle(400, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(400, 0);
  _face.RightEye.TransitionTo(Preset_Coquette);
  _face.LeftEye.TransitionTo(Preset_Coquette);
}

FaceBehavior::FaceBehavior(Face& face)
  : Timer(4000), _face(face) {
  Timer.Start();
  Clear();
  Emotions[Normal] = 4;
  Emotions[Angry] = 2;
  Emotions[Glee] = 2;
  Emotions[Happy] = 2;
  Emotions[Sad] = 1;
  Emotions[Worried] = 1;
  Emotions[Focused] = 1;
  Emotions[Annoyed] = 1;
  Emotions[Surprised] = 2;
  Emotions[Skeptic] = 1;
  Emotions[Frustrated] = 1;
  Emotions[Unimpressed] = 1;
  Emotions[Sleepy] = 2;
  Emotions[Suspicious] = 1;
  Emotions[Squint] = 1;
  Emotions[Furious] = 1;
  Emotions[Scared] = 1;
  Emotions[Awe] = 1;
  Emotions[Coquette] = 1;
}

void FaceBehavior::Clear() {
  for (int i = 0; i < EMOTIONS_COUNT; i++) Emotions[i] = 0.0f;
  CurrentEmotion = Normal;
}

eEmotions FaceBehavior::GetRandomEmotion() {
  float sum = 0;
  for (int i = 0; i < EMOTIONS_COUNT; i++) sum += Emotions[i];
  if (sum == 0) return Normal;

  float r = random(0, 1000 * sum) / 1000.0f;
  float acc = 0;
  for (int i = 0; i < EMOTIONS_COUNT; i++) {
    if (Emotions[i] == 0) continue;
    acc += Emotions[i];
    if (r <= acc) return (eEmotions)i;
  }
  return Normal;
}

void FaceBehavior::Update() {
  Timer.Update();
  if (!Timer.IsExpired()) return;

  Timer.Reset();
  eEmotions next = GetRandomEmotion();
  if (CurrentEmotion != next) GoToEmotion(next);
}

void FaceBehavior::GoToEmotion(eEmotions e) {
  if (CurrentEmotion != e) {
    static const char* emotionNames[] = {
      "Normal", "Angry", "Glee", "Happy", "Sad", "Worried", "Focused", "Annoyed",
      "Surprised", "Skeptic", "Frustrated", "Unimpressed", "Sleepy", "Suspicious",
      "Squint", "Furious", "Scared", "Awe", "Coquette"
    };
    if ((int)e >= 0 && (int)e < EMOTIONS_COUNT) {
      LOG_PRINT("[EMOTION] ");
      LOG_PRINTLN(emotionNames[(int)e]);
    }
  }

  CurrentEmotion = e;
  switch (e) {
    case Normal: _face.Expression.GoTo_Normal(); break;
    case Angry: _face.Expression.GoTo_Angry(); break;
    case Glee: _face.Expression.GoTo_Glee(); break;
    case Happy: _face.Expression.GoTo_Happy(); break;
    case Sad: _face.Expression.GoTo_Sad(); break;
    case Worried: _face.Expression.GoTo_Worried(); break;
    case Focused: _face.Expression.GoTo_Focused(); break;
    case Annoyed: _face.Expression.GoTo_Annoyed(); break;
    case Surprised: _face.Expression.GoTo_Surprised(); break;
    case Skeptic: _face.Expression.GoTo_Skeptic(); break;
    case Frustrated: _face.Expression.GoTo_Frustrated(); break;
    case Unimpressed: _face.Expression.GoTo_Unimpressed(); break;
    case Sleepy: _face.Expression.GoTo_Sleepy(); break;
    case Suspicious: _face.Expression.GoTo_Suspicious(); break;
    case Squint: _face.Expression.GoTo_Squint(); break;
    case Furious: _face.Expression.GoTo_Furious(); break;
    case Scared: _face.Expression.GoTo_Scared(); break;
    case Awe: _face.Expression.GoTo_Awe(); break;
    case Coquette: _face.Expression.GoTo_Coquette(); break;
    default: break;
  }
}

Face::Face(uint16_t sw, uint16_t sh, uint16_t es)
  : LeftEye(), RightEye(), Blink(*this), Look(*this),
    Behavior(*this), Expression(*this), sprite(&tft) {
  EyeSize = es;
  CenterX = sw / 2;
  CenterY = sh / 2;
  LeftEye.IsMirrored = true;
  RightEye.IsMirrored = false;
  EyeInterDistance = 12;
  Behavior.CurrentEmotion = Normal;
  _touchPressed = false;
  _tapCount = 0;
  _touchStartValid = false;
  _touchStartX = 0;
  _touchStartY = 0;
  _touchLastX = 0;
  _touchLastY = 0;
  _tapWindowUntil = 0;
  _touchPressStartMs = 0;
  _longPressHandled = false;
  _doubleTapPendingUntil = 0;
  _lastTouchPollMs = 0;
  _calOffset = TOUCH_CAL_OFFSET;
  _calRange = TOUCH_CAL_RANGE;
  _yawnState = 0;
  _yawnEndMs = 0;
  _nextYawnMs = 0;
  _sleeping = false;
  _sleepRequested = false;
  _zzzDirty = false;
  _sleepRequestedAt = 0;
  spriteOk = false;

  if (sprite.createSprite(SCREEN_WIDTH, SPRITE_HEIGHT)) {
    sprite.fillSprite(BG_COLOR);
    spriteOk = true;
    LOG_PRINTLN("[OK] Single sprite (320x120, 77KB)");
  } else {
    LOG_PRINTLN("[WARN] Sprite alloc failed; rendering disabled");
  }

  LoadCalibration();
}

void Face::SetCalibration(int32_t offset, int32_t range) {
  if (range <= 0) return;
  _calOffset = offset;
  _calRange = range;
  SaveCalibration();
  LOG_PRINTF("[CAL] offset=%d range=%d saved\n", _calOffset, _calRange);
}

void Face::LoadCalibration() {
  if (!LittleFS.begin(true)) return;
  // exists()/open("r") log a vfs error for missing files; scan the root
  // directory with the v3 File API instead.
  bool found = false;
  File root = LittleFS.open("/");
  File entry;
  while ((entry = root.openNextFile())) {
    String name = entry.name();
    if (name == "touch.cal" || name.endsWith("/touch.cal")) found = true;
    entry.close();
    if (found) break;
  }
  root.close();
  if (!found) return;
  File f = LittleFS.open("/touch.cal", "r");
  if (!f) return;
  int32_t off = 0, rng = 0;
  size_t n = f.readBytes((char*)&off, sizeof(off));
  n += f.readBytes((char*)&rng, sizeof(rng));
  f.close();
  if (n == sizeof(off) + sizeof(rng) && rng > 0) {
    _calOffset = off;
    _calRange = rng;
  }
}

void Face::SaveCalibration() const {
  if (!LittleFS.begin(true)) return;
  File f = LittleFS.open("/touch.cal", "w");
  if (!f) return;
  f.write((const uint8_t*)&_calOffset, sizeof(_calOffset));
  f.write((const uint8_t*)&_calRange, sizeof(_calRange));
  f.close();
}

int16_t Face::RawToScreenX(uint16_t rawX, uint16_t rawY) const {
  int32_t x = (int32_t)rawY - _calOffset;
  x = x * SCREEN_WIDTH / _calRange;
  return (int16_t)(SCREEN_WIDTH - x);
}

int16_t Face::RawToScreenY(uint16_t rawX, uint16_t rawY) const {
  int32_t y = (int32_t)rawX - _calOffset;
  y = y * SCREEN_HEIGHT / _calRange;
  return (int16_t)y;
}

void Face::LookFront() {
  Look.LookAt(0.0f, 0.0f);
}

void Face::GoToSleep() {
  _sleeping = true;
  RandomBehavior = false;
  RandomLook = false;
  RandomBlink = false;
  Behavior.Timer.SetIntervalMillis(1000000UL);
  Behavior.GoToEmotion(Sleepy);
  RightEye.Variation1.Values.Height = 2;
  LeftEye.Variation1.Values.Height = 2;
  RightEye.Variation1.Animation.SetTriangle(2400, 0);
  LeftEye.Variation1.Animation.SetTriangle(2400, 0);
  _sleepRequested = true;
  _sleepRequestedAt = millis();
  _zzzDirty = true;
  LOG_PRINTLN("[TOUCH] Sleep mode on");
}

void Face::WakeFromSleep() {
  _sleeping = false;
  _sleepRequested = false;
  _zzzDirty = true;
  RandomBehavior = true;
  RandomLook = true;
  RandomBlink = true;
  Behavior.Timer.SetIntervalMillis(4000);
  Behavior.Timer.Reset();
  Behavior.GoToEmotion(Normal);
  LOG_PRINTLN("[TOUCH] Wake up");
}

void Face::Begin() {
  LeftEye.Transformation.Sync();
  RightEye.Transformation.Sync();
  _startupPhase = StartupPhase::BOOT;
  _bootTime = millis();
  RandomBehavior = false;
  RandomLook = false;
  RandomBlink = false;
  Expression.GoTo_Sleepy();
}

void Face::Update() {
  uint32_t now = millis();
  uint32_t elapsed = now - _bootTime;
  switch (_startupPhase) {
    case StartupPhase::BOOT:
      if (elapsed >= 2000) {
        Expression.GoTo_Normal();
        _startupPhase = StartupPhase::FACE_READY;
      }
      break;
    case StartupPhase::FACE_READY:
      if (elapsed >= 5000) {
        RandomLook = true;
        _startupPhase = StartupPhase::BEHAVIOR_READY;
      }
      break;
    case StartupPhase::BEHAVIOR_READY:
      if (elapsed >= 8000) {
        RandomBlink = true;
        RandomBehavior = true;
        _startupPhase = StartupPhase::RUNNING;
      }
      break;
    case StartupPhase::RUNNING:
      break;
  }

  if (RandomBehavior) Behavior.Update();
  if (RandomLook) Look.Update();
  if (RandomBlink) Blink.Update();

  if (gPowerManager && gPowerManager->GetState() == PowerState::RELAX) {
    if (_yawnState == 0) {
      if (_nextYawnMs == 0) {
        _nextYawnMs = now + 20000 + random(20000);
      } else if (now >= _nextYawnMs) {
        Expression.GoTo_Sleepy();
        _yawnState = 1;
        _yawnEndMs = now + 800;
        _nextYawnMs = 0;
      }
    } else if (_yawnState == 1 && now >= _yawnEndMs) {
      Expression.GoTo_Normal();
      Expression.ClearVariations();
      _yawnState = 0;
      _nextYawnMs = now + 30000 + random(30000);
    }
  } else {
    _yawnState = 0;
    _nextYawnMs = 0;
  }

  UpdateTouch();
  if (_sleepRequested && now - _sleepRequestedAt >= SLEEP_ENTER_DELAY_MS) {
    _sleepRequested = false;
    if (gPowerManager) gPowerManager->EnterIdle();
  }
  if (NeedsRedraw()) Draw();
}

bool Face::NeedsRedraw() const {
  return LeftEye.NeedsRedraw() || RightEye.NeedsRedraw();
}

void Face::UpdateTouch() {
  unsigned long now = millis();

  if (_tapCount > 0 && now >= _tapWindowUntil) {
    _tapCount = 0;
    _doubleTapPendingUntil = 0;
  }

  if (_doubleTapPendingUntil && now >= _doubleTapPendingUntil) {
    _doubleTapPendingUntil = 0;
    if (_tapCount == 2) {
      _tapCount = 0;
      Behavior.GoToEmotion(Normal);
      LOG_PRINTLN("[TOUCH] Double tap -> Normal");
    }
  }

  // Throttle SPI touch reads to ~30ms; gesture timeouts above stay exact.
  if (now - _lastTouchPollMs < TOUCH_POLL_INTERVAL_MS) return;
  _lastTouchPollMs = now;

  uint16_t z = tft.getTouchRawZ();
  bool pressed = z >= TOUCH_PRESSURE_THRESHOLD;
  if (pressed && !_touchPressed) {
    if (_sleeping) WakeFromSleep();
    uint16_t rx = 0, ry = 0;
    tft.getTouchRaw(&rx, &ry);
    LOG_PRINTF("[TOUCH] press z=%u raw=%u,%u\n", z, rx, ry);
    _touchStartX = _touchLastX = RawToScreenX(rx, ry);
    _touchStartY = _touchLastY = RawToScreenY(rx, ry);
    _touchStartValid = true;
    if (gPowerManager) gPowerManager->NotifyInteraction();
    Blink.Blink();
    _touchPressStartMs = now;
    _longPressHandled = false;
    _touchPressed = true;
  } else if (pressed && _touchPressed) {
    uint16_t rx = 0, ry = 0;
    tft.getTouchRaw(&rx, &ry);
    _touchLastX = RawToScreenX(rx, ry);
    _touchLastY = RawToScreenY(rx, ry);
    _touchStartValid = true;
    if (!_longPressHandled && now - _touchPressStartMs >= LONG_PRESS_MS) {
      _longPressHandled = true;
      _tapCount = 0;
      _doubleTapPendingUntil = 0;
      Behavior.GoToEmotion(Normal);
      LOG_PRINTLN("[TOUCH] Long press -> Normal");
    }
  } else if (!pressed && _touchPressed) {
    _touchPressed = false;
    if (_longPressHandled) {
      _longPressHandled = false;
      return;
    }

    int32_t dx = _touchStartValid ? (int32_t)_touchLastX - _touchStartX : 0;
    int32_t dy = _touchStartValid ? (int32_t)_touchLastY - _touchStartY : 0;
    LOG_PRINTF("[TOUCH] release start=%u,%u last=%u,%u dx=%d dy=%d\n",
               _touchStartX, _touchStartY, _touchLastX, _touchLastY, dx, dy);
    if (abs(dx) >= TOUCH_SWIPE_THRESHOLD ||
        abs(dy) >= TOUCH_SWIPE_THRESHOLD) {
      HandleSwipe(dx, dy);
      _tapCount = 0;
      _doubleTapPendingUntil = 0;
      return;
    }

    _tapCount++;
    LOG_PRINTF("[TOUCH] Tap #%u\n", _tapCount);
    if (_tapCount >= TAP_COUNT_FOR_ANGER) {
      _tapCount = 0;
      _doubleTapPendingUntil = 0;
      Behavior.GoToEmotion(Angry);
      LOG_PRINTLN("[TOUCH] Angry face triggered by taps");
    } else if (_tapCount == 2) {
      _doubleTapPendingUntil = now + DOUBLE_TAP_WINDOW_MS;
    }
    _tapWindowUntil = now + TAP_WINDOW_MS;
  }
}

void Face::HandleSwipe(int32_t dx, int32_t dy) {
  LOG_PRINTF("[TOUCH] Swipe dx=%d dy=%d\n", dx, dy);
  if (abs(dx) > abs(dy)) {
    LOG_PRINTLN("[TOUCH] Horizontal swipe ignored");
    return;
  } else if (dy < 0) {
    Behavior.GoToEmotion(Coquette);
    LOG_PRINTLN("[TOUCH] Swipe up -> Coquette");
  } else {
    GoToSleep();
    LOG_PRINTLN("[TOUCH] Swipe down -> Sleepy");
  }
}

template<typename T>
static void DrawCapsule(T& dpy, float x1, float y1, float x2, float y2,
                        int radius, uint16_t color) {
  int steps = abs(x2 - x1) > abs(y2 - y1)
    ? abs(x2 - x1) : abs(y2 - y1);
  if (steps < 1) steps = 1;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    dpy.fillCircle((int)(x1 + (x2 - x1) * t),
                   (int)(y1 + (y2 - y1) * t), radius, color);
  }
}

template<typename T>
static void DrawZ(T& dpy, int16_t x, int16_t y, uint16_t color, float scale) {
  float w = 14.0f * scale;
  float h = 12.0f * scale;
  float skew = 4.0f * scale;
  int radius = scale >= 1.4f ? 3 : 2;
  DrawCapsule(dpy, x + skew, y + radius, x + w + skew, y + radius,
              radius, color);
  DrawCapsule(dpy, x + skew, y + radius, x + w, y + h - radius,
              radius, color);
  DrawCapsule(dpy, x, y + h - radius, x + w, y + h - radius,
              radius, color);
}

void Face::Draw() {
  if (!spriteOk) return;

  uint32_t drawStartUs = micros();
  LeftEye.CenterX = CenterX - EyeSize / 2 - EyeInterDistance;
  LeftEye.CenterY = CenterY;
  RightEye.CenterX = CenterX + EyeSize / 2 + EyeInterDistance;
  RightEye.CenterY = CenterY;

  LeftEye.Update();
  RightEye.Update();

  sprite.fillSprite(BG_COLOR);
  EyeDrawer::Draw(sprite, LeftEye.CenterX, SPRITE_HEIGHT / 2, LeftEye.FinalConfig);
  EyeDrawer::Draw(sprite, RightEye.CenterX, SPRITE_HEIGHT / 2, RightEye.FinalConfig);

  sprite.pushSprite(0, CenterY - SPRITE_HEIGHT / 2);

  if (_zzzDirty) {
    tft.fillRect(194, 186, 126, 48, BG_COLOR);
    if (_sleeping || Behavior.CurrentEmotion == Sleepy ||
        (gPowerManager && gPowerManager->GetState() == PowerState::SLEEP)) {
      DrawZ(tft, 202, 190, EYE_COLOR, 1.0f);
      DrawZ(tft, 236, 196, EYE_COLOR, 1.4f);
      DrawZ(tft, 270, 204, EYE_COLOR, 1.8f);
    }
    _zzzDirty = false;
  }

  uint32_t drawUs = micros() - drawStartUs;
  static uint32_t perfWindowStartMs = 0;
  static uint32_t perfFrames = 0;
  static uint32_t perfTotalUs = 0;

  uint32_t nowMs = millis();
  if (perfWindowStartMs == 0) perfWindowStartMs = nowMs;
  perfFrames++;
  perfTotalUs += drawUs;
  if (nowMs - perfWindowStartMs >= 5000) {
    uint32_t windowMs = nowMs - perfWindowStartMs;
    float avgDrawUs = (float)perfTotalUs / perfFrames;
    float avgFps = perfFrames * 1000.0f / windowMs;
    LOG_PRINTF("[PERF] avg_draw=%uus avg_fps=%.1f frames=%u\n",
               (uint32_t)avgDrawUs, avgFps, perfFrames);
    perfWindowStartMs = nowMs;
    perfFrames = 0;
    perfTotalUs = 0;
  }
}
