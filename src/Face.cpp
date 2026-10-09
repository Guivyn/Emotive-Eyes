#include "Face.h"
#include "CalibrationCodec.h"
#include "Log.h"
#include <math.h>
#include <LittleFS.h>
#if defined(ESP32)
  #include "esp_heap_caps.h"
#endif

extern TFT_eSPI tft;

static const EyeConfig Preset_Normal = {0, 0, 60, 60, 0, 0, 12, 12};
static const EyeConfig Preset_Happy = {0, 0, 14, 60, 0, 0, 14, 0};
static const EyeConfig Preset_Glee = {0, 0, 12, 60, 0, 0, 12, 0};
static const EyeConfig Preset_Sad = {0, 0, 23, 60, -0.5f, 0, 1, 14};
static const EyeConfig Preset_Worried = {0, 0, 37, 60, -0.1f, 0, 10, 14};
static const EyeConfig Preset_Worried_Alt = {0, 0, 53, 60, -0.2f, 0, 10, 14};
static const EyeConfig Preset_Focused = {0, 0, 22, 60, 0.2f, 0, 5, 1};
static const EyeConfig Preset_Annoyed = {0, 0, 18, 60, 0, 0, 0, 14};
static const EyeConfig Preset_Annoyed_Alt = {0, 0, 7, 60, 0, 0, 0, 6};
static const EyeConfig Preset_Surprised = {-2, 0, 67, 67, 0, 0, 24, 24};
static const EyeConfig Preset_Skeptic = {0, 0, 60, 60, 0, 0, 14, 14};
static const EyeConfig Preset_Skeptic_Alt = {0, -8, 38, 60, 0.3f, 0, 1, 14};
static const EyeConfig Preset_Frustrated = {4, -6, 18, 60, 0, 0, 0, 14};
static const EyeConfig Preset_Unimpressed = {4, 0, 18, 60, 0, 0, 1, 14};
static const EyeConfig Preset_Unimpressed_Alt = {4, -4, 34, 60, 0, 0, 1, 24};
static const EyeConfig Preset_Sleepy = {0, -2, 22, 60, -0.5f, -0.5f, 5, 5};
static const EyeConfig Preset_Sleepy_Alt = {0, -2, 12, 60, -0.5f, -0.5f, 5, 5};
static const EyeConfig Preset_Suspicious = {0, 0, 34, 60, 0, 0, 12, 5};
static const EyeConfig Preset_Suspicious_Alt = {0, -4, 24, 60, 0.2f, 0, 10, 5};
static const EyeConfig Preset_Squint = {-12, -4, 53, 53, 0, 0, 12, 12};
static const EyeConfig Preset_Squint_Alt = {6, 0, 30, 30, 0, 0, 7, 7};
static const EyeConfig Preset_Angry = {-4, 0, 30, 60, 0.3f, 0, 2, 18};
static const EyeConfig Preset_Furious = {-2, 0, 46, 60, 0.4f, 0, 2, 12};
static const EyeConfig Preset_Scared = {-4, 0, 60, 60, -0.1f, 0, 18, 12};
static const EyeConfig Preset_Awe = {2, 0, 53, 67, -0.1f, 0.1f, 18, 18};
static const EyeConfig Preset_Coquette = {0, -6, 24, 53, 0.25f, -0.25f, 12, 10};

static const unsigned long DOUBLE_UP_SWIPE_WINDOW_MS = 700;
static const unsigned long CELEBRATION_GLEE_MS = 600;
static const unsigned long CELEBRATION_AWE_MS = 600;
static const unsigned long CELEBRATION_HAPPY_MS = 900;
static const unsigned long RANDOM_EMOTION_INTERVAL_MS = 6000;

namespace {

bool LittleFsHasCalibrationFile(const char* suffix) {
  File root = LittleFS.open("/");
  if (!root) return false;
  bool found = false;
  File entry;
  while ((entry = root.openNextFile())) {
    String name = entry.name();
    if (name == suffix || name.endsWith(suffix)) found = true;
    entry.close();
    if (found) break;
  }
  root.close();
  return found;
}

float RandomCenteredUnit() {
  const long sum = random(-1000L, 1001L) + random(-1000L, 1001L);
  return sum / 2000.0f;
}

bool IsEyeBlinkingOrChangingExpression(const Eye& eye) {
  return eye.Transition.Active ||
      eye.BlinkTransformation.Animation.GetElapsed() <
          eye.BlinkTransformation.Animation.Interval;
}

struct GazeActivityProfile {
  float intervalScale;
  float movementScale;
  uint8_t glanceChancePercent;
};

GazeActivityProfile GetGazeActivityProfile(eEmotions emotion) {
  switch (emotion) {
    case Sleepy:      return {1.80f, 0.45f, 3};
    case Sad:         return {1.45f, 0.70f, 7};
    case Unimpressed: return {1.60f, 0.60f, 5};
    case Focused:
    case Squint:      return {1.25f, 0.75f, 8};
    case Happy:
    case Glee:
    case Coquette:    return {0.70f, 1.10f, 28};
    case Surprised:
    case Scared:
    case Awe:         return {0.62f, 1.05f, 35};
    case Worried:
    case Suspicious:
    case Skeptic:     return {0.82f, 0.95f, 26};
    case Angry:
    case Furious:     return {0.95f, 0.82f, 20};
    case Annoyed:
    case Frustrated:  return {0.90f, 0.68f, 12};
    case Normal:
    default:          return {1.00f, 1.00f, 15};
  }
}

unsigned long ScaleGazeInterval(unsigned long intervalMs, float scale) {
  return static_cast<unsigned long>(intervalMs * scale);
}

} // namespace

LookAssistant::LookAssistant(Face& face)
  : _face(face), Timer(2500), _minInterval(1500), _maxInterval(3500),
    _gazeX(0.0f), _gazeY(0.0f), _returnToCenter(false),
    _burstFollowup(false), _lastEmotion(Normal) {
  Timer.Start();
}

void LookAssistant::LookAt(float x, float y, unsigned long durationMs) {
  Transformation t;
  float mx = -20 * x;
  float my = 20 * y;
  const float absX = x < 0.0f ? -x : x;
  const float absY = y < 0.0f ? -y : y;
  float sy = 1.0f - absX * 0.05f - absY * 0.08f;
  if (sy < 0.90f) sy = 0.90f;
  t.MoveX = mx;
  t.MoveY = my;
  t.ScaleX = 1.0f;
  t.ScaleY = sy;
  _gazeX = x;
  _gazeY = y;
  _face.RightEye.Transformation.SetDestin(t);
  _face.LeftEye.Transformation.SetDestin(t);
  _face.RightEye.Transformation.Animation.Interval = durationMs;
  _face.LeftEye.Transformation.Animation.Interval = durationMs;
  _face.RightEye.Transformation.Animation.Restart();
  _face.LeftEye.Transformation.Animation.Restart();
}

void LookAssistant::Update() {
  const eEmotions emotion = _face.Behavior.CurrentEmotion;
  const GazeActivityProfile activity = GetGazeActivityProfile(emotion);
  if (emotion != _lastEmotion) {
    _lastEmotion = emotion;
    // Keep an in-progress glance/settle pair intact. Otherwise, adopt the
    // new expression's cadence from this point onward.
    if (!_returnToCenter && !_burstFollowup) {
      const unsigned long baseInterval = _minInterval +
          random(_maxInterval - _minInterval + 1);
      Timer.SetIntervalMillis(
          ScaleGazeInterval(baseInterval, activity.intervalScale));
      Timer.Reset();
      return;
    }
  }

  Timer.Update();
  if (!Timer.IsExpired()) return;
  if (IsEyeBlinkingOrChangingExpression(_face.LeftEye) ||
      IsEyeBlinkingOrChangingExpression(_face.RightEye)) {
    // Keep the pending gaze event and retry shortly after the eyes settle.
    Timer.SetIntervalMillis(80);
    Timer.Reset();
    return;
  }

  const bool returningToCenter = _returnToCenter;
  const bool burstFollowup = _burstFollowup;
  _burstFollowup = false;

  // Small, center-biased correlated shifts are the normal motion. A rare
  // wider glance is followed by a deliberate return to center.
  if (returningToCenter) {
    _returnToCenter = false;
    LookAt(0.0f, 0.0f, random(140L, 201L));
  } else if (burstFollowup) {
    const float x = (_gazeX * 0.55f + RandomCenteredUnit() * 0.05f) *
        activity.movementScale;
    const float y = (_gazeY * 0.55f + RandomCenteredUnit() * 0.05f) *
        activity.movementScale;
    LookAt(x, y, random(200L, 281L));
  } else if (random(0L, 100L) < activity.glanceChancePercent) {
    const float angle = random(0L, 6284L) / 1000.0f;
    const float radius = random(350L, 501L) / 1000.0f *
        activity.movementScale;
    // Larger saccade-like glances move decisively, then settle before return.
    LookAt(cosf(angle) * radius, sinf(angle) * radius,
           random(110L, 171L));
    _returnToCenter = true;
  } else {
    const float x = (_gazeX * 0.35f + RandomCenteredUnit() * 0.13f) *
        activity.movementScale;
    const float y = (_gazeY * 0.35f + RandomCenteredUnit() * 0.13f) *
        activity.movementScale;
    // Tiny fixation drift stays slower and less abrupt than a glance.
    LookAt(x, y, random(240L, 361L));
    _burstFollowup = random(0L, 4L) == 0;
  }

  unsigned long nextInterval =
      _minInterval + random(_maxInterval - _minInterval + 1);
  if (_returnToCenter) {
    // Hold a wider glance for a short, varied fixation before returning.
    nextInterval = random(700L, 1601L);
  } else if (_burstFollowup) {
    // Occasionally add one quick micro-correction, then resume a long hold.
    nextInterval = ScaleGazeInterval(random(450L, 1001L),
                                     activity.intervalScale);
  } else {
    nextInterval = ScaleGazeInterval(nextInterval, activity.intervalScale);
  }
  Timer.SetIntervalMillis(nextInterval);
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

void FaceExpression::SetCurrentEmotion(eEmotions emotion) {
  _face.Behavior.CurrentEmotion = emotion;
}

void FaceExpression::ClearVariations() {
  _face.RightEye.Variation1.StartFadeOut();
  _face.RightEye.Variation2.StartFadeOut();
  _face.LeftEye.Variation1.StartFadeOut();
  _face.LeftEye.Variation2.StartFadeOut();
  _face.RightEye.Variation1.Animation.Restart();
  _face.LeftEye.Variation1.Animation.Restart();
}

void FaceExpression::GoTo_Normal() {
  SetCurrentEmotion(Normal);
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
  SetCurrentEmotion(Angry);
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 2;
  _face.LeftEye.Variation1.Values.OffsetY = 2;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Angry);
  _face.LeftEye.TransitionTo(Preset_Angry);
}

void FaceExpression::GoTo_Glee() {
  SetCurrentEmotion(Glee);
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 5;
  _face.LeftEye.Variation1.Values.OffsetY = 5;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Glee);
  _face.LeftEye.TransitionTo(Preset_Glee);
}

void FaceExpression::GoTo_Happy() {
  SetCurrentEmotion(Happy);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Happy);
  _face.LeftEye.TransitionTo(Preset_Happy);
}

void FaceExpression::GoTo_Sad() {
  SetCurrentEmotion(Sad);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Sad);
  _face.LeftEye.TransitionTo(Preset_Sad);
}

void FaceExpression::GoTo_Worried() {
  SetCurrentEmotion(Worried);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Worried);
  _face.LeftEye.TransitionTo(Preset_Worried_Alt);
}

void FaceExpression::GoTo_Focused() {
  SetCurrentEmotion(Focused);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Focused);
  _face.LeftEye.TransitionTo(Preset_Focused);
}

void FaceExpression::GoTo_Annoyed() {
  SetCurrentEmotion(Annoyed);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Annoyed);
  _face.LeftEye.TransitionTo(Preset_Annoyed_Alt);
}

void FaceExpression::GoTo_Surprised() {
  SetCurrentEmotion(Surprised);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Surprised);
  _face.LeftEye.TransitionTo(Preset_Surprised);
}

void FaceExpression::GoTo_Skeptic() {
  SetCurrentEmotion(Skeptic);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Skeptic);
  _face.LeftEye.TransitionTo(Preset_Skeptic_Alt);
}

void FaceExpression::GoTo_Frustrated() {
  SetCurrentEmotion(Frustrated);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Frustrated);
  _face.LeftEye.TransitionTo(Preset_Frustrated);
}

void FaceExpression::GoTo_Unimpressed() {
  SetCurrentEmotion(Unimpressed);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Unimpressed);
  _face.LeftEye.TransitionTo(Preset_Unimpressed_Alt);
}

void FaceExpression::GoTo_Sleepy() {
  SetCurrentEmotion(Sleepy);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Sleepy);
  _face.LeftEye.TransitionTo(Preset_Sleepy_Alt);
}

void FaceExpression::GoTo_Suspicious() {
  SetCurrentEmotion(Suspicious);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Suspicious);
  _face.LeftEye.TransitionTo(Preset_Suspicious_Alt);
}

void FaceExpression::GoTo_Squint() {
  SetCurrentEmotion(Squint);
  ClearVariations();
  _face.LeftEye.Variation1.Values.OffsetX = 6;
  _face.LeftEye.Variation2.Values.OffsetY = 6;
  _face.RightEye.TransitionTo(Preset_Squint);
  _face.LeftEye.TransitionTo(Preset_Squint_Alt);
}

void FaceExpression::GoTo_Furious() {
  SetCurrentEmotion(Furious);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Furious);
  _face.LeftEye.TransitionTo(Preset_Furious);
}

void FaceExpression::GoTo_Scared() {
  SetCurrentEmotion(Scared);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Scared);
  _face.LeftEye.TransitionTo(Preset_Scared);
}

void FaceExpression::GoTo_Awe() {
  SetCurrentEmotion(Awe);
  ClearVariations();
  _face.RightEye.TransitionTo(Preset_Awe);
  _face.LeftEye.TransitionTo(Preset_Awe);
}

void FaceExpression::GoTo_Coquette() {
  SetCurrentEmotion(Coquette);
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 3;
  _face.LeftEye.Variation1.Values.OffsetY = 3;
  _face.RightEye.Variation1.Animation.SetTriangle(400, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(400, 0);
  _face.RightEye.TransitionTo(Preset_Coquette);
  _face.LeftEye.TransitionTo(Preset_Coquette);
}

FaceBehavior::FaceBehavior(Face& face)
  : Emotions{}, CurrentEmotion(Normal), Timer(RANDOM_EMOTION_INTERVAL_MS), _face(face) {
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
  : EyeSize(es),
    CenterX(sw / 2),
    CenterY(sh / 2),
    EyeInterDistance(15),
    LeftEye(),
    RightEye(),
    Blink(*this),
    Look(*this),
    Behavior(*this),
    Expression(*this),
    RandomBehavior(false),
    RandomLook(false),
    RandomBlink(false),
    _touchPressed(false),
    _touchPressSamples(0),
    _touchReleaseSamples(0),
    _touchCandidateX(0),
    _touchCandidateY(0),
    _sleeping(false),
    _sleepRequested(false),
    _sleepIndicatorDirty(false),
    _hasPresentedFrame(false),
    _lastDrawBoundsValid(false),
    _lastDrawLeft(0),
    _lastDrawTop(0),
    _lastDrawRight(0),
    _lastDrawBottom(0),
    _sleepRequestedAt(0),
    _tapCount(0),
    _touchStartValid(false),
    _touchStartX(0),
    _touchStartY(0),
    _touchLastX(0),
    _touchLastY(0),
    _tapWindowUntil(0),
    _touchPressStartMs(0),
    _longPressHandled(false),
    _doubleTapPendingUntil(0),
    _lastTouchPollMs(0),
    _calOffset(TOUCH_CAL_OFFSET),
    _calRange(TOUCH_CAL_RANGE),
    _yawnState(0),
    _yawnEndMs(0),
    _nextYawnMs(0),
    _upSwipePending(false),
    _lastUpSwipeMs(0),
    _celebrationActive(false),
    _celebrationStage(0),
    _celebrationStageStartedAt(0),
    _celebrationReturnEmotion(Normal),
    sprite(&tft),
    spriteOk(false),
    _renderMode(RenderMode::NONE),
    _startupPhase(StartupPhase::BOOT),
    _bootTime(0) {
  LeftEye.IsMirrored = true;
  RightEye.IsMirrored = false;

  const size_t fullSpriteBytes =
      (1UL * FRAME_SPRITE_WIDTH * FRAME_SPRITE_HEIGHT + 1UL) *
      sizeof(uint16_t);
  bool fullFramePossible = true;
#if defined(ESP32)
  const size_t largestFreeBlock =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  fullFramePossible = largestFreeBlock >= fullSpriteBytes;
  if (!fullFramePossible) {
    LOG_PRINTF("[INFO] Full-frame sprite skipped (%uKB needed, "
               "largest block=%uKB)\n",
               (unsigned)(fullSpriteBytes / 1024U),
               (unsigned)(largestFreeBlock / 1024U));
  }
#endif

  if (fullFramePossible &&
      sprite.createSprite(FRAME_SPRITE_WIDTH, FRAME_SPRITE_HEIGHT)) {
    sprite.fillSprite(BG_COLOR);
    spriteOk = true;
    _renderMode = RenderMode::FULL_FRAME;
    LOG_PRINTF("[OK] Full-frame sprite (%ux%u, %uKB)\n",
               FRAME_SPRITE_WIDTH, FRAME_SPRITE_HEIGHT,
               (unsigned)((1UL * FRAME_SPRITE_WIDTH * FRAME_SPRITE_HEIGHT *
                           sizeof(uint16_t)) / 1024UL));
  } else if (sprite.createSprite(FRAME_TILE_WIDTH, FRAME_SPRITE_HEIGHT)) {
    sprite.fillSprite(BG_COLOR);
    spriteOk = true;
    _renderMode = RenderMode::HALF_TILED;
    LOG_PRINTF("[WARN] Full-frame sprite unavailable; using 2 tiled passes "
               "with one buffer (%ux%u, %uKB)\n",
               FRAME_TILE_WIDTH, FRAME_SPRITE_HEIGHT,
               (unsigned)((1UL * FRAME_TILE_WIDTH * FRAME_SPRITE_HEIGHT *
                           sizeof(uint16_t)) / 1024UL));
  } else {
    // This is deliberately slow, but guarantees a visible frame even if a
    // future board has less contiguous RAM than the tiled fallback requires.
    spriteOk = true;
    _renderMode = RenderMode::DIRECT_TFT;
    LOG_PRINTLN("[WARN] Sprite alloc failed; using direct TFT fallback");
  }
}

void Face::LoadDeferredState() {
  // LittleFS is not needed to draw the first frame, so mount it after the
  // face is already visible during boot.
  LoadCalibration();
}

bool Face::InitializeFilesystem() {
  if (LittleFS.begin(false)) return true;
  LOG_PRINTLN("[CAL] filesystem mount failed; formatting by explicit request");
  if (!LittleFS.format()) return false;
  return LittleFS.begin(false);
}

bool Face::SetCalibration(int32_t offset, int32_t range) {
  if (range <= 0) return false;
  _calOffset = offset;
  _calRange = range;
  bool saved = SaveCalibration();
  if (saved) {
    LOG_PRINTF("[CAL] offset=%d range=%d saved\n", _calOffset, _calRange);
  } else {
    LOG_PRINTF("[CAL] offset=%d range=%d applied, NOT saved\n",
               _calOffset, _calRange);
  }
  return saved;
}

void Face::LoadCalibration() {
  // Never format flash implicitly while loading calibration. A missing or
  // damaged filesystem should not erase user data during boot.
  if (!LittleFS.begin(false)) {
    LOG_PRINTLN("[CAL] filesystem unavailable; using built-in calibration");
    return;
  }
  // exists()/open("r") log a vfs error for missing files; scan the root
  // directory with the v3 File API instead.
  if (!LittleFsHasCalibrationFile("touch.cal")) return;
  File f = LittleFS.open("/touch.cal", "r");
  if (!f) {
    LOG_PRINTLN("[CAL] calibration file could not be opened");
    return;
  }

  // Accept the old 8-byte format so existing devices migrate on the next
  // successful save, while new writes are versioned and checksummed.
  if (f.size() == sizeof(int32_t) * 2) {
    int32_t off = 0, rng = 0;
    size_t n = f.readBytes((char*)&off, sizeof(off));
    n += f.readBytes((char*)&rng, sizeof(rng));
    f.close();
    if (n == sizeof(off) + sizeof(rng) && rng > 0) {
      _calOffset = off;
      _calRange = rng;
      LOG_PRINTLN("[CAL] loaded legacy calibration");
    } else {
      LOG_PRINTLN("[CAL] invalid legacy calibration; using defaults");
    }
    return;
  }

  CalibrationCodec::Record record{};
  size_t n = f.read(reinterpret_cast<uint8_t*>(&record), sizeof(record));
  f.close();
  if (n == sizeof(record) && CalibrationCodec::IsValid(record)) {
    _calOffset = record.offset;
    _calRange = record.range;
    LOG_PRINTLN("[CAL] loaded persisted calibration");
  } else {
    LOG_PRINTLN("[CAL] invalid calibration record; using defaults");
  }
}

bool Face::SaveCalibration() const {
  if (!LittleFS.begin(false)) {
    LOG_PRINTLN("[CAL] filesystem unavailable; calibration not saved");
    return false;
  }

  CalibrationCodec::Record record =
      CalibrationCodec::Make(_calOffset, _calRange);

  // Write a complete record first. This prevents a power loss from leaving a
  // partially written calibration file at the canonical path.
  if (LittleFsHasCalibrationFile("touch.cal.tmp")) {
    LittleFS.remove("/touch.cal.tmp");
  }
  File f = LittleFS.open("/touch.cal.tmp", "w");
  if (!f) {
    LOG_PRINTLN("[CAL] temporary calibration file could not be opened");
    return false;
  }
  size_t written = f.write(reinterpret_cast<const uint8_t*>(&record),
                           sizeof(record));
  f.flush();
  f.close();
  if (written != sizeof(record)) {
    LittleFS.remove("/touch.cal.tmp");
    LOG_PRINTLN("[CAL] calibration write incomplete");
    return false;
  }

  // LittleFS rename is atomic and replaces the destination, so the previous
  // valid record remains available if power is lost before this point.
  if (!LittleFS.rename("/touch.cal.tmp", "/touch.cal")) {
    if (LittleFsHasCalibrationFile("touch.cal.tmp")) {
      LittleFS.remove("/touch.cal.tmp");
    }
    LOG_PRINTLN("[CAL] calibration rename failed");
    return false;
  }
  return true;
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
  CancelCelebration();
  _upSwipePending = false;
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
  if (gPowerManager) gPowerManager->BeginSleepFade();
  _sleepIndicatorDirty = true;
  LOG_PRINTLN("[TOUCH] Sleep mode on");
}

void Face::WakeFromSleep() {
  CancelCelebration();
  _upSwipePending = false;
  _sleeping = false;
  _sleepRequested = false;
  _sleepIndicatorDirty = true;
  RandomBehavior = true;
  RandomLook = true;
  RandomBlink = true;
  Behavior.Timer.SetIntervalMillis(RANDOM_EMOTION_INTERVAL_MS);
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
#if defined(PERF_CONTINUOUS_ANIMATION)
  // The continuous benchmark supplies its own deterministic emotion cycle.
  // Skip startup's automatic random-animation enable so the test stays repeatable.
  RandomBehavior = false;
  RandomLook = false;
  RandomBlink = false;
#else
  uint32_t elapsed = now - _bootTime;
  switch (_startupPhase) {
    case StartupPhase::BOOT:
      if (elapsed >= STARTUP_FACE_WAKE_MS) {
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
#endif

  if (RandomBehavior && !_celebrationActive) Behavior.Update();
  if (_celebrationActive) UpdateCelebration(now);
  // Start any due blink first so the gaze assistant can defer this frame.
  if (RandomBlink) Blink.Update();
  if (RandomLook) Look.Update();

  if (!_celebrationActive && gPowerManager &&
      gPowerManager->GetState() == PowerState::RELAX) {
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
  if (_sleepIndicatorDirty) return true;
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
  if (!_touchPressed) {
    if (z < TOUCH_PRESSURE_THRESHOLD) {
      _touchPressSamples = 0;
      return;
    }

    uint16_t rx = 0, ry = 0;
    tft.getTouchRaw(&rx, &ry);
    if (_touchPressSamples == 0) {
      _touchCandidateX = (uint16_t)RawToScreenX(rx, ry);
      _touchCandidateY = (uint16_t)RawToScreenY(rx, ry);
      _touchPressSamples = 1;
      return;
    }
    if (_touchPressSamples < TOUCH_STABLE_SAMPLES) {
      _touchPressSamples++;
    }
    if (_touchPressSamples < TOUCH_STABLE_SAMPLES) return;

    _touchPressSamples = 0;
    if (_celebrationActive) CancelCelebration();
    if (_sleeping) WakeFromSleep();
    LOG_PRINTF("[TOUCH] press z=%u raw=%u,%u\n", z, rx, ry);
    _touchStartX = _touchCandidateX;
    _touchStartY = _touchCandidateY;
    _touchLastX = (uint16_t)RawToScreenX(rx, ry);
    _touchLastY = (uint16_t)RawToScreenY(rx, ry);
    _touchStartValid = true;
    if (gPowerManager) gPowerManager->NotifyInteraction();
    _touchPressStartMs = now;
    _longPressHandled = false;
    _touchPressed = true;
  } else if (z <= TOUCH_PRESSURE_RELEASE_THRESHOLD) {
    if (_touchReleaseSamples < TOUCH_STABLE_SAMPLES) {
      _touchReleaseSamples++;
    }
    if (_touchReleaseSamples < TOUCH_STABLE_SAMPLES) return;
    _touchReleaseSamples = 0;
    _touchPressed = false;
    if (!_longPressHandled && now - _touchPressStartMs >= LONG_PRESS_MS) {
      _longPressHandled = true;
      _upSwipePending = false;
      _tapCount = 0;
      _doubleTapPendingUntil = 0;
      Behavior.GoToEmotion(Normal);
      LOG_PRINTLN("[TOUCH] Long press -> Normal");
    }
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

    _upSwipePending = false;
    // Wait until release confirms this is a tap. Starting touch feedback on
    // press also animated horizontal brightness swipes.
    Blink.Blink();
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
  } else {
    _touchReleaseSamples = 0;
    if (z >= TOUCH_PRESSURE_THRESHOLD) {
      uint16_t rx = 0, ry = 0;
      tft.getTouchRaw(&rx, &ry);
      _touchLastX = (uint16_t)RawToScreenX(rx, ry);
      _touchLastY = (uint16_t)RawToScreenY(rx, ry);
      _touchStartValid = true;
    }
    if (!_longPressHandled && now - _touchPressStartMs >= LONG_PRESS_MS) {
      _longPressHandled = true;
      _upSwipePending = false;
      _tapCount = 0;
      _doubleTapPendingUntil = 0;
      Behavior.GoToEmotion(Normal);
      LOG_PRINTLN("[TOUCH] Long press -> Normal");
    }
  }
}

void Face::HandleSwipe(int32_t dx, int32_t dy) {
  LOG_PRINTF("[TOUCH] Swipe dx=%d dy=%d\n", dx, dy);
  if (abs(dx) > abs(dy)) {
    _upSwipePending = false;
    if (!gPowerManager) return;
    int16_t next = (int16_t)gPowerManager->GetBrightness() +
                   (dx > 0 ? 10 : -10);
    if (next < PowerManager::MIN_BRIGHTNESS_PERCENT) {
      next = PowerManager::MIN_BRIGHTNESS_PERCENT;
    } else if (next > PowerManager::MAX_BRIGHTNESS_PERCENT) {
      next = PowerManager::MAX_BRIGHTNESS_PERCENT;
    }
    bool saved = gPowerManager->SetBrightness((uint8_t)next);
    LOG_PRINTF("[TOUCH] Swipe %s -> brightness %u%% (%s)\n",
               dx > 0 ? "right" : "left", (unsigned)next,
               saved ? "saved" : "save failed");
    return;
  } else if (dy < 0) {
    unsigned long now = millis();
    if (_upSwipePending && now - _lastUpSwipeMs <= DOUBLE_UP_SWIPE_WINDOW_MS) {
      _upSwipePending = false;
      StartCelebration();
      LOG_PRINTLN("[TOUCH] Double swipe up -> celebration");
    } else {
      _upSwipePending = true;
      _lastUpSwipeMs = now;
      Behavior.GoToEmotion(Coquette);
      Behavior.Timer.Reset();
      LOG_PRINTLN("[TOUCH] Swipe up -> Coquette; repeat within 700ms to celebrate");
    }
  } else {
    _upSwipePending = false;
    GoToSleep();
    LOG_PRINTLN("[TOUCH] Swipe down -> Sleepy");
  }
}

void Face::StartCelebration() {
  _celebrationReturnEmotion = Behavior.CurrentEmotion;
  _celebrationActive = true;
  _celebrationStage = 0;
  _celebrationStageStartedAt = millis();
  Behavior.Timer.Reset();
  Behavior.GoToEmotion(Glee);
}

void Face::UpdateCelebration(unsigned long now) {
  unsigned long stageDuration = 0;
  switch (_celebrationStage) {
    case 0: stageDuration = CELEBRATION_GLEE_MS; break;
    case 1: stageDuration = CELEBRATION_AWE_MS; break;
    case 2: stageDuration = CELEBRATION_HAPPY_MS; break;
    default:
      _celebrationActive = false;
      Behavior.GoToEmotion(_celebrationReturnEmotion);
      Behavior.Timer.Reset();
      return;
  }

  if (now - _celebrationStageStartedAt < stageDuration) return;
  _celebrationStageStartedAt = now;
  if (_celebrationStage == 0) {
    _celebrationStage = 1;
    Behavior.GoToEmotion(Awe);
  } else if (_celebrationStage == 1) {
    _celebrationStage = 2;
    Behavior.GoToEmotion(Happy);
  } else {
    _celebrationActive = false;
    Behavior.GoToEmotion(_celebrationReturnEmotion);
    Behavior.Timer.Reset();
    LOG_PRINTLN("[TOUCH] Celebration complete");
  }
}

void Face::CancelCelebration() {
  _upSwipePending = false;
  if (!_celebrationActive) return;
  _celebrationActive = false;
  Behavior.GoToEmotion(_celebrationReturnEmotion);
  Behavior.Timer.Reset();
  LOG_PRINTLN("[TOUCH] Celebration interrupted");
}

template<typename T>
static void DrawSleepSparkle(T& dpy, int16_t centerX, int16_t centerY,
                             int16_t verticalRadius,
                             int16_t horizontalRadius, uint16_t color) {
  if (verticalRadius < 2 || horizontalRadius < 2) return;

  dpy.fillTriangle(centerX, centerY - verticalRadius,
                   centerX - 1, centerY,
                   centerX + 1, centerY, color);
  dpy.fillTriangle(centerX + horizontalRadius, centerY,
                   centerX, centerY - 1,
                   centerX, centerY + 1, color);
  dpy.fillTriangle(centerX, centerY + verticalRadius,
                   centerX + 1, centerY,
                   centerX - 1, centerY, color);
  dpy.fillTriangle(centerX - horizontalRadius, centerY,
                   centerX, centerY + 1,
                   centerX, centerY - 1, color);
  dpy.fillRect(centerX - 1, centerY - 1, 3, 3, color);
}

// Use a thicker crescent and tapered four-point sparkles so the sleep mark
// stays recognizable on the low-resolution panel without dominating the face.
template<typename T>
static void DrawSleepIndicator(T& dpy, int16_t offsetX = 0) {
  dpy.fillCircle(269 - offsetX, 196, 13, EYE_COLOR);
  dpy.fillCircle(278 - offsetX, 196, 10, BG_COLOR);
  DrawSleepSparkle(dpy, 285 - offsetX, 184, 4, 3, EYE_COLOR);
  DrawSleepSparkle(dpy, 288 - offsetX, 207, 3, 2, EYE_COLOR);
}

static bool XRangeIntersectsRegion(float centerX, float offsetX, float width,
                                   int16_t originX, uint16_t regionWidth) {
  const float halfWidth = width * 0.5f;
  const float left = centerX + offsetX - halfWidth - 1.0f;
  const float right = centerX + offsetX + halfWidth + 1.0f;
  const float regionRight = originX + regionWidth - 1.0f;
  return left <= regionRight && right >= originX;
}

struct DrawBounds {
  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;
  bool valid;
};

static void IncludeDrawBounds(DrawBounds& bounds, int32_t left, int32_t top,
                              int32_t right, int32_t bottom) {
  if (right < 0 || bottom < 0 || left >= SCREEN_WIDTH ||
      top >= SCREEN_HEIGHT) return;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right >= SCREEN_WIDTH) right = SCREEN_WIDTH - 1;
  if (bottom >= SCREEN_HEIGHT) bottom = SCREEN_HEIGHT - 1;
  if (left > right || top > bottom) return;

  if (!bounds.valid) {
    bounds.left = left;
    bounds.top = top;
    bounds.right = right;
    bounds.bottom = bottom;
    bounds.valid = true;
    return;
  }
  if (left < bounds.left) bounds.left = left;
  if (top < bounds.top) bounds.top = top;
  if (right > bounds.right) bounds.right = right;
  if (bottom > bounds.bottom) bounds.bottom = bottom;
}

static void IncludeEyeBounds(DrawBounds& bounds, const Eye& eye) {
  const EyeConfig* cfg = eye.FinalConfig;
  if (!cfg || cfg->Width <= 0 || cfg->Height <= 0) return;

  const float centerX = eye.CenterX + cfg->OffsetX;
  const float centerY = eye.CenterY + cfg->OffsetY;
  const float halfWidth = cfg->Width * 0.5f + 2.0f;
  const float topSlope = fabsf(cfg->Slope_Top) * cfg->Height * 0.5f;
  const float bottomSlope = fabsf(cfg->Slope_Bottom) * cfg->Height * 0.5f;
  const float maxSlope = topSlope > bottomSlope ? topSlope : bottomSlope;
  const float halfHeight = cfg->Height * 0.5f + maxSlope + 2.0f;
  IncludeDrawBounds(bounds,
      (int32_t)floorf(centerX - halfWidth),
      (int32_t)floorf(centerY - halfHeight),
      (int32_t)ceilf(centerX + halfWidth),
      (int32_t)ceilf(centerY + halfHeight));
}

static DrawBounds CurrentDrawBounds(const Eye& leftEye, const Eye& rightEye,
                                   bool showSleepIndicator) {
  DrawBounds bounds = {0, 0, -1, -1, false};
  IncludeEyeBounds(bounds, leftEye);
  IncludeEyeBounds(bounds, rightEye);
  if (showSleepIndicator) {
    // Encloses the crescent and both four-point sparkles, with one pixel of
    // margin for drawing-library edge rounding.
    IncludeDrawBounds(bounds, 255, 180, 291, 210);
  }
  return bounds;
}

static bool BoundsIntersectX(const DrawBounds& bounds, int16_t originX,
                             uint16_t regionWidth) {
  if (!bounds.valid || regionWidth == 0) return false;
  const int16_t regionRight = originX + regionWidth - 1;
  return bounds.left <= regionRight && bounds.right >= originX;
}

static bool EyeIntersectsRegion(const Eye& eye, int16_t originX,
                                uint16_t regionWidth) {
  if (!eye.FinalConfig) return false;
  return XRangeIntersectsRegion(eye.CenterX, eye.FinalConfig->OffsetX,
                                eye.FinalConfig->Width, originX,
                                regionWidth);
}

static bool SleepIndicatorIntersectsRegion(int16_t originX,
                                           uint16_t regionWidth) {
  return XRangeIntersectsRegion(272, 0.0f, 36.0f, originX, regionWidth);
}

// Push only the dirty rectangle from the sprite. TFT_eSPI accepts one pixel
// span per row inside the same address window, so the sprite's wider row stride
// does not force a full-width or full-height transfer.
static void PushSpriteRect(TFT_eSprite& source, int16_t originX,
                           const DrawBounds& bounds) {
  if (!bounds.valid) return;
  const int32_t sourceWidth = source.width();
  const int32_t sourceHeight = source.height();
  const int32_t sourceLeft = bounds.left - originX;
  const int32_t sourceRight = bounds.right - originX;
  const int32_t left = sourceLeft < 0 ? 0 : sourceLeft;
  const int32_t right = sourceRight >= sourceWidth
      ? sourceWidth - 1 : sourceRight;
  const int32_t top = bounds.top < 0 ? 0 : bounds.top;
  const int32_t bottom = bounds.bottom >= sourceHeight
      ? sourceHeight - 1 : bounds.bottom;
  if (left > right || top > bottom) return;

  const int32_t width = right - left + 1;
  const int32_t height = bottom - top + 1;
  uint16_t* pixels = static_cast<uint16_t*>(source.getPointer());
  tft.setWindow(originX + left, top, originX + right, bottom);
  if (left == 0 && width == sourceWidth) {
    tft.pushPixels(pixels + top * sourceWidth,
                   (uint32_t)width * height);
    return;
  }
  for (int32_t y = top; y <= bottom; y++) {
    tft.pushPixels(pixels + y * sourceWidth + left, width);
  }
}

void Face::Draw() {
  if (!spriteOk || _renderMode == RenderMode::NONE) return;

  uint32_t drawStartUs = micros();
  LeftEye.CenterX = CenterX - EyeSize / 2 - EyeInterDistance;
  LeftEye.CenterY = CenterY;
  RightEye.CenterX = CenterX + EyeSize / 2 + EyeInterDistance;
  RightEye.CenterY = CenterY;

  LeftEye.Update();
  RightEye.Update();

  const bool showSleepIndicator = _sleeping ||
      (gPowerManager && gPowerManager->GetState() == PowerState::SLEEP);
  const DrawBounds currentBounds = CurrentDrawBounds(
      LeftEye, RightEye, showSleepIndicator);
  DrawBounds dirtyBounds = currentBounds;
  if (!_hasPresentedFrame) {
    dirtyBounds.left = 0;
    dirtyBounds.top = 0;
    dirtyBounds.right = SCREEN_WIDTH - 1;
    dirtyBounds.bottom = SCREEN_HEIGHT - 1;
    dirtyBounds.valid = true;
  } else if (_lastDrawBoundsValid) {
    IncludeDrawBounds(dirtyBounds, _lastDrawLeft, _lastDrawTop,
                      _lastDrawRight, _lastDrawBottom);
  }

  if (_renderMode == RenderMode::DIRECT_TFT) {
    if (dirtyBounds.valid) {
      tft.fillRect(dirtyBounds.left, dirtyBounds.top,
                   dirtyBounds.right - dirtyBounds.left + 1,
                   dirtyBounds.bottom - dirtyBounds.top + 1, BG_COLOR);
      EyeDrawer::Draw(tft, LeftEye.CenterX, CenterY, LeftEye.FinalConfig);
      EyeDrawer::Draw(tft, RightEye.CenterX, CenterY, RightEye.FinalConfig);
      if (showSleepIndicator) DrawSleepIndicator(tft);
    }
  } else {
    auto drawFrameRegion = [&](int16_t originX, uint16_t regionWidth) {
      sprite.fillSprite(BG_COLOR);
      if (EyeIntersectsRegion(LeftEye, originX, regionWidth)) {
        EyeDrawer::Draw(sprite, LeftEye.CenterX - originX, CenterY,
                        LeftEye.FinalConfig);
      }
      if (EyeIntersectsRegion(RightEye, originX, regionWidth)) {
        EyeDrawer::Draw(sprite, RightEye.CenterX - originX, CenterY,
                        RightEye.FinalConfig);
      }

      if (showSleepIndicator &&
          SleepIndicatorIntersectsRegion(originX, regionWidth)) {
        DrawSleepIndicator(sprite, originX);
      }
    };

    if (_renderMode == RenderMode::HALF_TILED) {
      // Reuse one full-height tile. Each object is rasterized only in the
      // tile it intersects; objects crossing x=FRAME_TILE_WIDTH are drawn in
      // both tiles and clipped by TFT_eSPI.
      if (dirtyBounds.valid) {
        const bool drawLeft = BoundsIntersectX(dirtyBounds, 0,
                                               FRAME_TILE_WIDTH);
        const bool drawRight = BoundsIntersectX(dirtyBounds,
            FRAME_TILE_WIDTH, FRAME_TILE_WIDTH);
        if (drawLeft) drawFrameRegion(0, FRAME_TILE_WIDTH);
        const bool oldSwapBytes = tft.getSwapBytes();
        tft.setSwapBytes(false);
        tft.startWrite();
        if (drawLeft) PushSpriteRect(sprite, 0, dirtyBounds);
        if (drawRight) {
          drawFrameRegion(FRAME_TILE_WIDTH, FRAME_TILE_WIDTH);
          PushSpriteRect(sprite, FRAME_TILE_WIDTH, dirtyBounds);
        }
        tft.endWrite();
        tft.setSwapBytes(oldSwapBytes);
      }
    } else {
      if (dirtyBounds.valid) {
        drawFrameRegion(0, FRAME_SPRITE_WIDTH);
        const bool oldSwapBytes = tft.getSwapBytes();
        tft.setSwapBytes(false);
        tft.startWrite();
        PushSpriteRect(sprite, 0, dirtyBounds);
        tft.endWrite();
        tft.setSwapBytes(oldSwapBytes);
      }
    }
  }
  _hasPresentedFrame = true;
  _lastDrawBoundsValid = currentBounds.valid;
  if (currentBounds.valid) {
    _lastDrawLeft = currentBounds.left;
    _lastDrawTop = currentBounds.top;
    _lastDrawRight = currentBounds.right;
    _lastDrawBottom = currentBounds.bottom;
  }
  _sleepIndicatorDirty = false;

  uint32_t drawUs = micros() - drawStartUs;
  static uint32_t perfWindowStartMs = 0;
  static uint32_t perfFrames = 0;
  static uint32_t perfTotalUs = 0;
  static uint32_t perfMaxDrawUs = 0;
  static uint32_t perfOverBudgetFrames = 0;

  uint32_t nowMs = millis();
  if (perfWindowStartMs == 0) perfWindowStartMs = nowMs;
  perfFrames++;
  perfTotalUs += drawUs;
  if (drawUs > perfMaxDrawUs) perfMaxDrawUs = drawUs;
  if (drawUs > FRAME_INTERVAL_ACTIVE_MS * 1000UL) perfOverBudgetFrames++;
  if (nowMs - perfWindowStartMs >= 5000) {
    uint32_t windowMs = nowMs - perfWindowStartMs;
    float avgDrawUs = (float)perfTotalUs / perfFrames;
    float avgFps = perfFrames * 1000.0f / windowMs;
    LOG_PRINTF("[PERF] avg_draw=%uus max_draw=%uus over_budget=%u avg_fps=%.1f frames=%u\n",
               (uint32_t)avgDrawUs, perfMaxDrawUs, perfOverBudgetFrames,
               avgFps, perfFrames);
    perfWindowStartMs = nowMs;
    perfFrames = 0;
    perfTotalUs = 0;
    perfMaxDrawUs = 0;
    perfOverBudgetFrames = 0;
  }
}
