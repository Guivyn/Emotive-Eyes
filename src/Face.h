#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Animation.h"
#include "Config.h"
#include "Eye.h"
#include "PowerManager.h"

enum eEmotions {
  Normal, Angry, Glee, Happy, Sad, Worried, Focused, Annoyed,
  Surprised, Skeptic, Frustrated, Unimpressed, Sleepy, Suspicious,
  Squint, Furious, Scared, Awe, Coquette, EMOTIONS_COUNT
};

class Face;

class LookAssistant {
public:
  explicit LookAssistant(Face& face);

  void LookAt(float x, float y);
  void Update();
  void SetIntervalRange(unsigned long minMs, unsigned long maxMs);

private:
  Face& _face;
  AsyncTimer Timer;
  unsigned long _minInterval;
  unsigned long _maxInterval;
};

class BlinkAssistant {
public:
  explicit BlinkAssistant(Face& face);

  void Blink();
  void Update();
  void SetIntervalRange(unsigned long minMs, unsigned long maxMs);

private:
  Face& _face;
  AsyncTimer Timer;
  unsigned long _minInterval;
  unsigned long _maxInterval;
};

class FaceExpression {
public:
  explicit FaceExpression(Face& face);

  void ClearVariations();
  void GoTo_Normal();
  void GoTo_Angry();
  void GoTo_Glee();
  void GoTo_Happy();
  void GoTo_Sad();
  void GoTo_Worried();
  void GoTo_Focused();
  void GoTo_Annoyed();
  void GoTo_Surprised();
  void GoTo_Skeptic();
  void GoTo_Frustrated();
  void GoTo_Unimpressed();
  void GoTo_Sleepy();
  void GoTo_Suspicious();
  void GoTo_Squint();
  void GoTo_Furious();
  void GoTo_Scared();
  void GoTo_Awe();
  void GoTo_Coquette();

private:
  Face& _face;
};

class FaceBehavior {
public:
  explicit FaceBehavior(Face& face);

  float Emotions[EMOTIONS_COUNT];
  eEmotions CurrentEmotion;
  AsyncTimer Timer;

  void Clear();
  void Update();
  void GoToEmotion(eEmotions e);

private:
  eEmotions GetRandomEmotion();
  Face& _face;
};

enum class StartupPhase : uint8_t {
  BOOT,
  FACE_READY,
  BEHAVIOR_READY,
  RUNNING
};

class Face {
public:
  Face(uint16_t sw, uint16_t sh, uint16_t es);

  uint16_t EyeSize;
  int16_t CenterX, CenterY, EyeInterDistance;
  Eye LeftEye, RightEye;
  BlinkAssistant Blink;
  LookAssistant Look;
  FaceBehavior Behavior;
  FaceExpression Expression;
  bool RandomBehavior, RandomLook, RandomBlink;

  void LookFront();
  void Begin();
  void Update();
  void Draw();
  bool NeedsRedraw() const;
  void UpdateTouch();
  void GoToSleep();
  void WakeFromSleep();
  bool IsSleeping() const { return _sleeping; }

private:
  void HandleSwipe(int32_t dx, int32_t dy);

  bool _touchPressed;
  bool _sleeping;
  bool _sleepRequested;
  bool _zzzDirty;
  unsigned long _sleepRequestedAt;
  uint8_t _tapCount;
  bool _touchStartValid;
  uint16_t _touchStartX;
  uint16_t _touchStartY;
  uint16_t _touchLastX;
  uint16_t _touchLastY;
  unsigned long _tapWindowUntil;
  unsigned long _touchPressStartMs;
  bool _longPressHandled;
  unsigned long _doubleTapPendingUntil;
  uint8_t _yawnState;
  unsigned long _yawnEndMs;
  unsigned long _nextYawnMs;
  TFT_eSprite sprite;
  bool spriteOk;
  StartupPhase _startupPhase;
  uint32_t _bootTime;
};
