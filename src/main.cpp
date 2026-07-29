// =============================================================================
// ESP32 + ST7796 TFT (320x240) Emotive Eyes — pure procedural rendering
// Ported from SH1106 OLED original by Alastair Aitchison / playfultechnology
// =============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

// --------------------------- Display constants ---------------------------
static const uint16_t EYE_COLOR = TFT_WHITE;
static const uint16_t BG_COLOR  = TFT_BLACK;
static const uint16_t EYES_FRAME_MS_MIN = 24; // cap max fps ~41, floor ≥24 fps

// Global TFT instance
TFT_eSPI tft = TFT_eSPI();

// ===================== Animation engine (from legacy) =====================
class AnimationBase {
public:
  AnimationBase(unsigned long interval) : Interval(interval), StarTime(millis()) {}
  unsigned long Interval, StarTime;
  void Restart() { StarTime = millis(); }
  float GetValue() { return GetValue(GetElapsed()); }
  float GetValue(unsigned long elapsedMillis) { return Calculate(elapsedMillis); }
  unsigned long GetElapsed() const { return static_cast<unsigned long>(millis() - StarTime); }
protected:
  virtual float Calculate(unsigned long elapsedMillis) { return 0.0; }
};

class RampAnimation : public AnimationBase {
public:
  bool IsActive = true;
  RampAnimation(unsigned long interval) : AnimationBase(interval) {}
  float Calculate(unsigned long elapsedMillis) {
    if (elapsedMillis < Interval) return static_cast<float>(elapsedMillis) / Interval;
    return 1.0f;
  }
};

class TriangleAnimation : public AnimationBase {
public:
  unsigned long _t0, _t1;
  TriangleAnimation(unsigned long interval) : AnimationBase(interval) {
    _t0 = interval / 2; _t1 = interval - _t0; }
  TriangleAnimation(unsigned long t0, unsigned long t1) : AnimationBase(t0 + t1) {
    _t0 = t0; _t1 = t1; }
  float Calculate(unsigned long elapsedMillis) {
    unsigned long e = elapsedMillis % Interval;
    if (e < _t0) return static_cast<float>(e) / _t0;
    return 1.0f - (static_cast<float>(e) - _t0) / _t1;
  }
};

class TrapeziumAnimation : public AnimationBase {
public:
  unsigned long _t0, _t1, _t2;
  TrapeziumAnimation(unsigned long t) : AnimationBase(t) {
    _t0 = t / 3; _t1 = _t0; _t2 = t - _t0 - _t1; }
  TrapeziumAnimation(unsigned long t0, unsigned long t1, unsigned long t2)
    : AnimationBase(t0 + t1 + t2) { _t0 = t0; _t1 = t1; _t2 = t2; }
  float Calculate(unsigned long elapsedMillis) {
    if (elapsedMillis > Interval) return 0.0;
    if (elapsedMillis < _t0) return static_cast<float>(elapsedMillis) / _t0;
    else if (elapsedMillis < _t0 + _t1) return 1.0f;
    else return 1.0f - (static_cast<float>(elapsedMillis) - _t1 - _t0) / _t2;
  }
};

class TrapeziumPulseAnimation : public AnimationBase {
public:
  unsigned long _t0, _t1, _t2, _t3, _t4;
  TrapeziumPulseAnimation(unsigned long t0, unsigned long t1,
    unsigned long t2, unsigned long t3, unsigned long t4)
    : AnimationBase(t0 + t1 + t2 + t3 + t4) {
    _t0 = t0; _t1 = t1; _t2 = t2; _t3 = t3; _t4 = t4; }
  float Calculate(unsigned long elapsedMillis) {
    unsigned long e = elapsedMillis % Interval;
    if (e < _t0) return 0.0;
    if (e < _t0 + _t1) return static_cast<float>(e - _t0) / _t1;
    else if (e < _t0 + _t1 + _t2) return 1.0f;
    else if (e < _t0 + _t1 + _t2 + _t3)
      return 1.0f - (static_cast<float>(e) - _t2 - _t1 - _t0) / _t3;
    return 0.0;
  }
  void SetTriangle(uint16_t t, uint16_t delay) {
    _t0 = 0; _t1 = t / 2; _t2 = 0; _t3 = _t1; _t4 = delay;
    Interval = _t0 + _t1 + _t2 + _t3 + _t4; }
};

// ============================ EyeConfig ====================================
struct EyeConfig {
  int16_t OffsetX, OffsetY, Height, Width;
  float Slope_Top, Slope_Bottom;
  int16_t Radius_Top, Radius_Bottom;
  int16_t Inverse_Radius_Top, Inverse_Radius_Bottom;
  int16_t Inverse_Offset_Top, Inverse_Offset_Bottom;
};

// ========================== EyeTransition ==================================
class EyeTransition {
public:
  EyeTransition();
  EyeConfig Output, Snapshot, Destin;
  bool Active;
  RampAnimation Animation;
  void Update(); void Start(); void Apply(float t);
};

EyeTransition::EyeTransition() : Animation(500) { Active = false; memset(&Output, 0, sizeof(EyeConfig)); }
void EyeTransition::Start() { Animation.Restart(); Active = true; }
void EyeTransition::Update() {
  if (!Active) return;
  float t = Animation.GetValue();
  if (t >= 1.0f) { Active = false; Output = Destin; return; }
  Apply(t);
}
void EyeTransition::Apply(float t) {
  Output.OffsetX = Snapshot.OffsetX * (1.0f - t) + Destin.OffsetX * t;
  Output.OffsetY = Snapshot.OffsetY * (1.0f - t) + Destin.OffsetY * t;
  Output.Height  = Snapshot.Height  * (1.0f - t) + Destin.Height  * t;
  Output.Width   = Snapshot.Width   * (1.0f - t) + Destin.Width   * t;
  Output.Slope_Top    = Snapshot.Slope_Top    * (1.0f - t) + Destin.Slope_Top    * t;
  Output.Slope_Bottom = Snapshot.Slope_Bottom * (1.0f - t) + Destin.Slope_Bottom * t;
  Output.Radius_Top    = Snapshot.Radius_Top    * (1.0f - t) + Destin.Radius_Top    * t;
  Output.Radius_Bottom = Snapshot.Radius_Bottom * (1.0f - t) + Destin.Radius_Bottom * t;
  Output.Inverse_Radius_Top    = Snapshot.Inverse_Radius_Top    * (1.0f - t) + Destin.Inverse_Radius_Top * t;
  Output.Inverse_Radius_Bottom = Snapshot.Inverse_Radius_Bottom * (1.0f - t) + Destin.Inverse_Radius_Bottom * t;
  Output.Inverse_Offset_Top    = Snapshot.Inverse_Offset_Top    * (1.0f - t) + Destin.Inverse_Offset_Top * t;
  Output.Inverse_Offset_Bottom = Snapshot.Inverse_Offset_Bottom * (1.0f - t) + Destin.Inverse_Offset_Bottom * t;
}

// ======================== EyeTransformation ================================
struct Transformation {
  float MoveX = 0.0, MoveY = 0.0, ScaleX = 1.0, ScaleY = 1.0;
};

class EyeTransformation {
public:
  EyeTransformation();
  EyeConfig* Input; EyeConfig Output;
  Transformation Origin, Current, Destin;
  RampAnimation Animation;
  void Update(); void Apply(); void SetDestin(Transformation t);
  void Sync();
};

EyeTransformation::EyeTransformation() : Animation(200) { Input = nullptr; }
void EyeTransformation::Update() { if (!Input) return; Apply(); }
void EyeTransformation::Apply() {
  if (!Input) return;
  float t = Animation.GetValue();
  Output = *Input;
  Output.OffsetX = (int16_t)(Origin.MoveX + (Destin.MoveX - Origin.MoveX) * t);
  Output.OffsetY = (int16_t)(Origin.MoveY + (Destin.MoveY - Origin.MoveY) * t);
  float sX = Origin.ScaleX + (Destin.ScaleX - Origin.ScaleX) * t;
  float sY = Origin.ScaleY + (Destin.ScaleY - Origin.ScaleY) * t;
  Output.Width  = (int16_t)(Input->Width * sX);
  Output.Height = (int16_t)(Input->Height * sY);
}
void EyeTransformation::SetDestin(Transformation t) {
  Origin.MoveX = Output.OffsetX;
  Origin.MoveY = Output.OffsetY;
  if (Input && Input->Width > 0) Origin.ScaleX = (float)Output.Width / Input->Width;
  else Origin.ScaleX = 1.0f;
  if (Input && Input->Height > 0) Origin.ScaleY = (float)Output.Height / Input->Height;
  else Origin.ScaleY = 1.0f;
  Destin = t;
}
void EyeTransformation::Sync() {
  if (!Input) return;
  Output = *Input;
  Origin.MoveX = Input->OffsetX; Origin.MoveY = Input->OffsetY;
  Origin.ScaleX = 1.0f; Origin.ScaleY = 1.0f;
  Destin.MoveX = Input->OffsetX; Destin.MoveY = Input->OffsetY;
  Destin.ScaleX = 1.0f; Destin.ScaleY = 1.0f;
}

// ========================== EyeVariation ===================================
class EyeVariation {
public:
  EyeVariation();
  EyeConfig* Input; EyeConfig Output;
  TrapeziumPulseAnimation Animation;
  EyeConfig Values;
  EyeConfig FadeValues;
  unsigned long FadeStart;
  bool IsFading;
  void Clear(); void Update(); void Apply(float t);
  void StartFadeOut();
};

EyeVariation::EyeVariation()
  : Animation(200, 200, 200, 200, 0) {
  Input = nullptr;
  IsFading = false;
  FadeStart = 0;
  memset(&FadeValues, 0, sizeof(EyeConfig));
  Clear();
}
void EyeVariation::Clear() { Values = {0,0,0,0,0,0,0,0,0,0,0,0}; }
void EyeVariation::StartFadeOut() {
  float t = Animation.GetValue();
  FadeValues.OffsetX = Values.OffsetX * t;
  FadeValues.OffsetY = Values.OffsetY * t;
  FadeValues.Height = Values.Height * t;
  FadeValues.Width = Values.Width * t;
  FadeValues.Slope_Top = Values.Slope_Top * t;
  FadeValues.Slope_Bottom = Values.Slope_Bottom * t;
  FadeValues.Radius_Top = Values.Radius_Top * t;
  FadeValues.Radius_Bottom = Values.Radius_Bottom * t;
  FadeValues.Inverse_Radius_Top = Values.Inverse_Radius_Top * t;
  FadeValues.Inverse_Radius_Bottom = Values.Inverse_Radius_Bottom * t;
  FadeValues.Inverse_Offset_Top = Values.Inverse_Offset_Top * t;
  FadeValues.Inverse_Offset_Bottom = Values.Inverse_Offset_Bottom * t;
  FadeStart = millis();
  IsFading = true;
  Clear();
}
void EyeVariation::Update() {
  if (!Input) return;
  Apply(Animation.GetValue());
  if (IsFading) {
    unsigned long elapsed = millis() - FadeStart;
    if (elapsed >= 150) {
      IsFading = false;
    } else {
      float fadeFactor = 1.0f - (float)elapsed / 150.0f;
      Output.OffsetX += FadeValues.OffsetX * fadeFactor;
      Output.OffsetY += FadeValues.OffsetY * fadeFactor;
      Output.Height += FadeValues.Height * fadeFactor;
      Output.Width += FadeValues.Width * fadeFactor;
      Output.Slope_Top += FadeValues.Slope_Top * fadeFactor;
      Output.Slope_Bottom += FadeValues.Slope_Bottom * fadeFactor;
      Output.Radius_Top += FadeValues.Radius_Top * fadeFactor;
      Output.Radius_Bottom += FadeValues.Radius_Bottom * fadeFactor;
      Output.Inverse_Radius_Top += FadeValues.Inverse_Radius_Top * fadeFactor;
      Output.Inverse_Radius_Bottom += FadeValues.Inverse_Radius_Bottom * fadeFactor;
      Output.Inverse_Offset_Top += FadeValues.Inverse_Offset_Top * fadeFactor;
      Output.Inverse_Offset_Bottom += FadeValues.Inverse_Offset_Bottom * fadeFactor;
    }
  }
}
void EyeVariation::Apply(float t) {
  if (!Input) return; Output = *Input;
  Output.OffsetX = Input->OffsetX + Values.OffsetX * t;
  Output.OffsetY = Input->OffsetY + Values.OffsetY * t;
  Output.Height = Input->Height + Values.Height * t;
  Output.Width = Input->Width + Values.Width * t;
  Output.Slope_Top = Input->Slope_Top + Values.Slope_Top * t;
  Output.Slope_Bottom = Input->Slope_Bottom + Values.Slope_Bottom * t;
  Output.Radius_Top = Input->Radius_Top + Values.Radius_Top * t;
  Output.Radius_Bottom = Input->Radius_Bottom + Values.Radius_Bottom * t;
  Output.Inverse_Radius_Top = Input->Inverse_Radius_Top + Values.Inverse_Radius_Top * t;
  Output.Inverse_Radius_Bottom = Input->Inverse_Radius_Bottom + Values.Inverse_Radius_Bottom * t;
  Output.Inverse_Offset_Top = Input->Inverse_Offset_Top + Values.Inverse_Offset_Top * t;
  Output.Inverse_Offset_Bottom = Input->Inverse_Offset_Bottom + Values.Inverse_Offset_Bottom * t;
}

// ============================= EyeBlink ====================================
class EyeBlink {
public:
  EyeBlink();
  EyeConfig* Input; EyeConfig Output;
  TrapeziumAnimation Animation;
  int32_t BlinkWidth = 60, BlinkHeight = 2;
  void Update(); void Apply(float t);
};

EyeBlink::EyeBlink() : Animation(100, 50, 100) { Input = nullptr; }
void EyeBlink::Update() { if (!Input) return; Apply(Animation.GetValue()); }
void EyeBlink::Apply(float t) {
  if (!Input) return; Output = *Input;
  Output.Height = Input->Height * (1.0 - t);
  Output.Width = Input->Width + (BlinkWidth - Input->Width) * t;
  Output.Radius_Top = Input->Radius_Top * (1.0 - t);
  Output.Radius_Bottom = Input->Radius_Bottom * (1.0 - t);
  Output.Inverse_Radius_Top = Input->Inverse_Radius_Top * (1.0 - t);
  Output.Inverse_Radius_Bottom = Input->Inverse_Radius_Bottom * (1.0 - t);
}

// =============== EyeDrawer — Hybrid Vector Rasterizer ======================
// (1) GenerateBoundary: Bezier curves + float coords + adaptive sampling
// (2) RasterizePolygon:  float-intersection scanline fill

 struct PolygonPoint {
     float x, y;
 };
static const int MAX_POLY_PTS = 64;

class EyeDrawer {
public:
    // ---------- Boundary Generator ----------
    static int GenerateBoundary(const EyeConfig* cfg, float cx, float cy,
                                PolygonPoint* pts, int maxPts) {
        if (!cfg || maxPts < 3) return 0;

        float rT = (float)cfg->Radius_Top;
        float rB = (float)cfg->Radius_Bottom;
        float dYT = cfg->Height * cfg->Slope_Top * 0.5f;
        float dYB = cfg->Height * cfg->Slope_Bottom * 0.5f;
        float tH = cfg->Height + dYT - dYB;

        if (rB > 0 && rT > 0 && tH > 1.0f && tH - 1.0f < (float)(rB + rT)) {
            float sR = (float)(rB + rT);
            if (sR > 0.0f) {
                rT = (float)cfg->Radius_Top * (tH - 1.0f) / sR;
                rB = (float)cfg->Radius_Bottom * (tH - 1.0f) / sR;
            }
        }

        float TLx = cx + cfg->OffsetX - cfg->Width / 2.0f + rT;
        float TLy = cy + cfg->OffsetY - cfg->Height / 2.0f + rT - dYT;
        float TRx = cx + cfg->OffsetX + cfg->Width / 2.0f - rT;
        float TRy = cy + cfg->OffsetY - cfg->Height / 2.0f + rT + dYT;
        float BLx = cx + cfg->OffsetX - cfg->Width / 2.0f + rB;
        float BLy = cy + cfg->OffsetY + cfg->Height / 2.0f - rB - dYB;
        float BRx = cx + cfg->OffsetX + cfg->Width / 2.0f - rB;
        float BRy = cy + cfg->OffsetY + cfg->Height / 2.0f - rB + dYB;

        const float DEG2RAD = PI / 180.0f;
        int cnt = 0;
        auto add = [&](float x, float y) {
            if (cnt < maxPts) { pts[cnt].x = x; pts[cnt].y = y; cnt++; }
        };

        // TL corner arc: left-tangent (180 deg) -> top-tangent (270 deg)
        if (rT > 0) {
            for (int a = 180; a <= 270; a += 15) {
                float r = a * DEG2RAD;
                add(TLx + rT * cosf(r), TLy + rT * sinf(r));
            }
        } else {
            add(TLx, TLy);
        }

        // TR corner arc: top-tangent (270 deg) -> right-tangent (0/360 deg)
        if (rT > 0) {
            for (int a = 285; a <= 360; a += 15) {
                float r = (a == 360) ? 0.0f : a * DEG2RAD;
                add(TRx + rT * cosf(r), TRy + rT * sinf(r));
            }
        } else {
            add(TRx, TRy);
        }

        // BR corner arc: right-tangent (0 deg) -> bottom-tangent (90 deg)
        if (rB > 0) {
            for (int a = 15; a <= 90; a += 15) {
                float r = a * DEG2RAD;
                add(BRx + rB * cosf(r), BRy + rB * sinf(r));
            }
        } else {
            add(BRx, BRy);
        }

        // BL corner arc: bottom-tangent (90 deg) -> left-tangent (180 deg)
        if (rB > 0) {
            for (int a = 105; a <= 180; a += 15) {
                float r = a * DEG2RAD;
                add(BLx + rB * cosf(r), BLy + rB * sinf(r));
            }
        } else {
            add(BLx, BLy);
        }

        return cnt;
    }

    // ---------- Scanline Rasterizer ----------
    template<typename T>
    static void RasterizePolygon(T& d, const PolygonPoint* pts, int count,
                                 uint16_t color) {
        if (count < 3) return;

        float minY = pts[0].y, maxY = pts[0].y;
        for (int i = 1; i < count; i++) {
            if (pts[i].y < minY) minY = pts[i].y;
            if (pts[i].y > maxY) maxY = pts[i].y;
        }
        if (minY < 0.0f) minY = 0.0f;
        if (maxY >= 240.0f) maxY = 239.0f;

        float xs[64];

        for (int y = (int)minY; y <= (int)maxY; y++) {
            float fy = (float)y;
            int n = 0;
            for (int i = 0; i < count; i++) {
                int j = (i + 1) % count;
                float x1 = pts[i].x, y1 = pts[i].y;
                float x2 = pts[j].x, y2 = pts[j].y;
                if ((y1 <= fy && y2 > fy) || (y2 <= fy && y1 > fy)) {
                    float dx = x2 - x1;
                    float dy = y2 - y1;
                    float x = x1 + dx * (fy - y1) / dy;
                    if (n < 64) xs[n++] = x;
                }
            }
            if (n == 0) continue;
            for (int i = 1; i < n; i++) {
                float key = xs[i];
                int j = i - 1;
                while (j >= 0 && xs[j] > key) { xs[j+1] = xs[j]; j--; }
                xs[j+1] = key;
            }
            for (int i = 0; i + 1 < n; i += 2) {
                int x0 = (int)roundf(xs[i]);
                int x1 = (int)roundf(xs[i+1]);
                if (x1 > x0) d.drawFastHLine(x0, y, x1 - x0, color);
            }
        }
    }

    // ---------- Public Draw ----------
    template<typename T>
    static void Draw(T& dpy, float cx, float cy, const EyeConfig *cfg) {
        PolygonPoint boundary[MAX_POLY_PTS];
        int n = GenerateBoundary(cfg, cx, cy, boundary, MAX_POLY_PTS);
        if (n >= 3) RasterizePolygon(dpy, boundary, n, EYE_COLOR);
    }

    // ----- Legacy helpers -----
    template<typename T>
    static void FillCorner(T& d, int16_t x0, int16_t y0,
        int32_t rx, int32_t ry, uint16_t clr) { }
    template<typename T>
    static void FillRect(T& d, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) {
        d.fillRect(min(x0,x1), min(y0,y1), max(x0,x1)-min(x0,x1), max(y0,y1)-min(y0,y1), c); }
    template<typename T>
    static void FillTri(T& d, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) {
        d.fillTriangle(x0, y0, x1, y1, x1, y0, c); }
};



// ============================ Emotions =====================================
enum eEmotions {
  Normal, Angry, Glee, Happy, Sad, Worried, Focused, Annoyed,
  Surprised, Skeptic, Frustrated, Unimpressed, Sleepy, Suspicious,
  Squint, Furious, Scared, Awe, EMOTIONS_COUNT
};

// ============================ EyePresets ===================================
static const EyeConfig Preset_Normal        = {0, 0, 50, 50, 0, 0, 10, 10, 0, 0, 0, 0};
static const EyeConfig Preset_Happy         = {0, 0, 12, 50, 0, 0, 12, 0, 0, 0, 0, 0};
static const EyeConfig Preset_Glee          = {0, 0, 10, 50, 0, 0, 10, 0, 0, 5, 0, 0};
static const EyeConfig Preset_Sad           = {0, 0, 19, 50, -0.5f, 0, 1, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Worried       = {0, 0, 31, 50, -0.1f, 0, 8, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Worried_Alt   = {0, 0, 44, 50, -0.2f, 0, 8, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Focused       = {0, 0, 18, 50, 0.2f, 0, 4, 1, 0, 0, 0, 0};
static const EyeConfig Preset_Annoyed       = {0, 0, 15, 50, 0, 0, 0, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Annoyed_Alt   = {0, 0, 6, 50, 0, 0, 0, 5, 0, 0, 0, 0};
static const EyeConfig Preset_Surprised     = {-2, 0, 56, 56, 0, 0, 20, 20, 0, 0, 0, 0};
static const EyeConfig Preset_Skeptic       = {0, 0, 50, 50, 0, 0, 12, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Skeptic_Alt   = {0, -8, 32, 50, 0.3f, 0, 1, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Frustrated    = {4, -6, 15, 50, 0, 0, 0, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Unimpressed   = {4, 0, 15, 50, 0, 0, 1, 12, 0, 0, 0, 0};
static const EyeConfig Preset_Unimpressed_Alt = {4, -4, 28, 50, 0, 0, 1, 20, 0, 0, 0, 0};
static const EyeConfig Preset_Sleepy        = {0, -2, 18, 50, -0.5f, -0.5f, 4, 4, 0, 0, 0, 0};
static const EyeConfig Preset_Sleepy_Alt    = {0, -2, 10, 50, -0.5f, -0.5f, 4, 4, 0, 0, 0, 0};
static const EyeConfig Preset_Suspicious    = {0, 0, 28, 50, 0, 0, 10, 4, 0, 0, 0, 0};
static const EyeConfig Preset_Suspicious_Alt= {0, -4, 20, 50, 0.2f, 0, 8, 4, 0, 0, 0, 0};
static const EyeConfig Preset_Squint        = {-12, -4, 44, 44, 0, 0, 10, 10, 0, 0, 0, 0};
static const EyeConfig Preset_Squint_Alt    = {6, 0, 25, 25, 0, 0, 6, 6, 0, 0, 0, 0};
static const EyeConfig Preset_Angry         = {-4, 0, 25, 50, 0.3f, 0, 2, 15, 0, 0, 0, 0};
static const EyeConfig Preset_Furious       = {-2, 0, 38, 50, 0.4f, 0, 2, 10, 0, 0, 0, 0};
static const EyeConfig Preset_Scared        = {-4, 0, 50, 50, -0.1f, 0, 15, 10, 0, 0, 0, 0};
static const EyeConfig Preset_Awe           = {2, 0, 44, 56, -0.1f, 0.1f, 15, 15, 0, 0, 0, 0};

// ===================== Forward declarations ================================
class Face;
class AsyncTimer {
public:
  AsyncTimer(unsigned long ms) : Interval(ms), _active(false), _done(false), _start(0) {}
  void Start() { _start = millis(); _active = true; _done = false; }
  void Reset() { Start(); }
  bool Update() {
    if (!_active) return false; if (_done) return true;
    if (millis() - _start >= Interval) { _done = true; _active = false; return true; }
    return false;
  }
  void SetIntervalMillis(unsigned long ms) { Interval = ms; }
  bool IsExpired() const { return _done; }
  unsigned long Interval;
private:
  bool _active, _done; unsigned long _start;
};

// =========================== LookAssistant ================================
class LookAssistant {
public:
  LookAssistant(Face& face);
  void LookAt(float x, float y);
  void Update();
private:
  Face& _face;
  AsyncTimer Timer;
};

// =========================== BlinkAssistant ===============================
class BlinkAssistant {
public:
  BlinkAssistant(Face& face);
  void Blink();
  void Update();
private:
  Face& _face;
  AsyncTimer Timer;
};

// ============================== Eye =======================================
class Eye {
public:
  Eye(Face& face);
  int16_t CenterX, CenterY;
  bool IsMirrored;
  EyeConfig Config;
  EyeTransition Transition;
  EyeTransformation Transformation;
  EyeVariation Variation1, Variation2;
  EyeBlink BlinkTransformation;
  EyeConfig* FinalConfig;
  void ChainOperators();
  bool NeedsRedraw() const;
  void Update();
  template<typename T> void Draw(T& dpy);
  template<typename T> void Draw(T& dpy, int16_t cx, int16_t cy);
  void ApplyPreset(const EyeConfig config);
  void TransitionTo(const EyeConfig config);
private:
  Face& _face;
};

// ========================== FaceExpression ================================
class FaceExpression {
public:
  FaceExpression(Face& face);
  void ClearVariations();
  void GoTo_Normal(); void GoTo_Angry(); void GoTo_Glee();
  void GoTo_Happy(); void GoTo_Sad(); void GoTo_Worried();
  void GoTo_Focused(); void GoTo_Annoyed(); void GoTo_Surprised();
  void GoTo_Skeptic(); void GoTo_Frustrated(); void GoTo_Unimpressed();
  void GoTo_Sleepy(); void GoTo_Suspicious(); void GoTo_Squint();
  void GoTo_Furious(); void GoTo_Scared(); void GoTo_Awe();
private:
  Face& _face;
};

// =========================== FaceBehavior =================================
class FaceBehavior {
public:
  FaceBehavior(Face& face);
  float Emotions[EMOTIONS_COUNT];
  eEmotions CurrentEmotion;
  AsyncTimer Timer;
  void SetEmotion(eEmotions e, float v);
  float GetEmotion(eEmotions e);
  void Clear(); void Update(); void GoToEmotion(eEmotions e);
private:
  Face& _face;
  eEmotions GetRandomEmotion();
};

// ============================== Face ======================================
enum class StartupPhase : uint8_t { BOOT, FACE_READY, BEHAVIOR_READY, RUNNING };

class Face {
public:
  Face(uint16_t sw, uint16_t sh, uint16_t es);
  uint16_t Width, Height, EyeSize;
  int16_t CenterX, CenterY, EyeInterDistance;
  Eye LeftEye, RightEye;
  BlinkAssistant Blink; LookAssistant Look;
  FaceBehavior Behavior; FaceExpression Expression;
  bool RandomBehavior, RandomLook, RandomBlink;
  void LookFront(); void LookRight(); void LookLeft();
  void LookTop(); void LookBottom();
  void Wait(unsigned long ms); void DoBlink();
  void Begin(); void Update(); void Draw();
private:
  TFT_eSprite sprite;
  bool spriteOk;
  StartupPhase _startupPhase; uint32_t _bootTime;
};

// ====================== Implementation Section ============================

// ---- LookAssistant ----
LookAssistant::LookAssistant(Face& face) : _face(face), Timer(2500) { Timer.Start(); }
void LookAssistant::LookAt(float x, float y) {
  Transformation t;
  float mx = -20 * x, my = 20 * y;
  float sy = (1.0 - x*0.2) * (1.0 - (y>0?y:-y)*0.4);
  t.MoveX = mx; t.MoveY = my; t.ScaleX = 1.0; t.ScaleY = sy;
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
  Timer.SetIntervalMillis(1500 + random(2000));
  Timer.Reset();
}

// ---- BlinkAssistant ----
BlinkAssistant::BlinkAssistant(Face& face) : _face(face), Timer(3000) { Timer.Start(); }
void BlinkAssistant::Blink() {
  _face.LeftEye.BlinkTransformation.Animation.Restart();
  _face.RightEye.BlinkTransformation.Animation.Restart();
}
void BlinkAssistant::Update() {
  Timer.Update();
  if (!Timer.IsExpired()) return;
  Blink();
  Timer.SetIntervalMillis(2000 + random(3000));
  Timer.Reset();
}

// ---- Eye ----
Eye::Eye(Face& face) : _face(face) {
  IsMirrored = false; ChainOperators();
  Variation1.Animation._t0 = 200; Variation1.Animation._t1 = 200;
  Variation1.Animation._t2 = 200; Variation1.Animation._t3 = 200;
  Variation1.Animation._t4 = 0;   Variation1.Animation.Interval = 800;
  Variation2.Animation._t0 = 0;   Variation2.Animation._t1 = 200;
  Variation2.Animation._t2 = 200; Variation2.Animation._t3 = 200;
  Variation2.Animation._t4 = 200; Variation2.Animation.Interval = 800;
}
void Eye::ChainOperators() {
  Transformation.Input = &Transition.Output;
  Variation1.Input = &(Transformation.Output);
  Variation2.Input = &(Variation1.Output);
  BlinkTransformation.Input = &(Variation2.Output);
  FinalConfig = &(BlinkTransformation.Output);
}
bool Eye::NeedsRedraw() const {
  // Only redraw when a shape-changing animation is actually in progress
  if (Transition.Active) return true;
  if (BlinkTransformation.Animation.GetElapsed() < BlinkTransformation.Animation.Interval) return true;
  if (Transformation.Animation.GetElapsed() < Transformation.Animation.Interval) return true;
  if (Variation1.Animation.GetElapsed() < Variation1.Animation.Interval) return true;
  if (Variation2.Animation.GetElapsed() < Variation2.Animation.Interval) return true;
  return false;
}
void Eye::Update() {
  Transition.Update(); Transformation.Update();
  Variation1.Update(); Variation2.Update(); BlinkTransformation.Update();
}
template<typename T> void Eye::Draw(T& dpy) { Update(); EyeDrawer::Draw(dpy, CenterX, CenterY, FinalConfig); }
template<typename T> void Eye::Draw(T& dpy, int16_t cx, int16_t cy) { Update(); EyeDrawer::Draw(dpy, cx, cy, FinalConfig); }

void Eye::ApplyPreset(const EyeConfig config) {
  Config.OffsetX = IsMirrored ? -config.OffsetX : config.OffsetX;
  Config.OffsetY = -config.OffsetY; Config.Height = config.Height; Config.Width = config.Width;
  Config.Slope_Top = IsMirrored ? config.Slope_Top : -config.Slope_Top;
  Config.Slope_Bottom = IsMirrored ? config.Slope_Bottom : -config.Slope_Bottom;
  Config.Radius_Top = config.Radius_Top; Config.Radius_Bottom = config.Radius_Bottom;
  Config.Inverse_Radius_Top = config.Inverse_Radius_Top;
  Config.Inverse_Radius_Bottom = config.Inverse_Radius_Bottom;
}
void Eye::TransitionTo(const EyeConfig config) {
  Transition.Snapshot = Transition.Output;
  Transition.Destin.OffsetX = IsMirrored ? -config.OffsetX : config.OffsetX;
  Transition.Destin.OffsetY = -config.OffsetY;
  Transition.Destin.Height = config.Height; Transition.Destin.Width = config.Width;
  Transition.Destin.Slope_Top = IsMirrored ? config.Slope_Top : -config.Slope_Top;
  Transition.Destin.Slope_Bottom = IsMirrored ? config.Slope_Bottom : -config.Slope_Bottom;
  Transition.Destin.Radius_Top = config.Radius_Top;
  Transition.Destin.Radius_Bottom = config.Radius_Bottom;
  Transition.Destin.Inverse_Radius_Top = config.Inverse_Radius_Top;
  Transition.Destin.Inverse_Radius_Bottom = config.Inverse_Radius_Bottom;
  Transition.Destin.Inverse_Offset_Top = config.Inverse_Offset_Top;
  Transition.Destin.Inverse_Offset_Bottom = config.Inverse_Offset_Bottom;
  Transition.Start();
}

// ---- FaceExpression ----
FaceExpression::FaceExpression(Face& face) : _face(face) {}
void FaceExpression::ClearVariations() {
  _face.RightEye.Variation1.StartFadeOut(); _face.RightEye.Variation2.StartFadeOut();
  _face.LeftEye.Variation1.StartFadeOut();  _face.LeftEye.Variation2.StartFadeOut();
  _face.RightEye.Variation1.Animation.Restart();
  _face.LeftEye.Variation1.Animation.Restart();
}
void FaceExpression::GoTo_Normal() {
  ClearVariations();
  _face.RightEye.Variation1.Values.Height = 3; _face.RightEye.Variation2.Values.Width = 1;
  _face.LeftEye.Variation1.Values.Height = 2;  _face.LeftEye.Variation2.Values.Width = 2;
  _face.RightEye.Variation1.Animation.SetTriangle(1000, 0);
  _face.LeftEye.Variation1.Animation.SetTriangle(1000, 0);
  _face.RightEye.TransitionTo(Preset_Normal); _face.LeftEye.TransitionTo(Preset_Normal);
}
void FaceExpression::GoTo_Angry() {
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 2; _face.LeftEye.Variation1.Values.OffsetY = 2;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0); _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Angry); _face.LeftEye.TransitionTo(Preset_Angry);
}
void FaceExpression::GoTo_Glee() {
  ClearVariations();
  _face.RightEye.Variation1.Values.OffsetY = 5; _face.LeftEye.Variation1.Values.OffsetY = 5;
  _face.RightEye.Variation1.Animation.SetTriangle(300, 0); _face.LeftEye.Variation1.Animation.SetTriangle(300, 0);
  _face.RightEye.TransitionTo(Preset_Glee); _face.LeftEye.TransitionTo(Preset_Glee);
}
void FaceExpression::GoTo_Happy()     { ClearVariations(); _face.RightEye.TransitionTo(Preset_Happy); _face.LeftEye.TransitionTo(Preset_Happy); }
void FaceExpression::GoTo_Sad()       { ClearVariations(); _face.RightEye.TransitionTo(Preset_Sad);   _face.LeftEye.TransitionTo(Preset_Sad); }
void FaceExpression::GoTo_Worried()   { ClearVariations(); _face.RightEye.TransitionTo(Preset_Worried); _face.LeftEye.TransitionTo(Preset_Worried_Alt); }
void FaceExpression::GoTo_Focused()   { ClearVariations(); _face.RightEye.TransitionTo(Preset_Focused); _face.LeftEye.TransitionTo(Preset_Focused); }
void FaceExpression::GoTo_Annoyed()   { ClearVariations(); _face.RightEye.TransitionTo(Preset_Annoyed); _face.LeftEye.TransitionTo(Preset_Annoyed_Alt); }
void FaceExpression::GoTo_Surprised() { ClearVariations(); _face.RightEye.TransitionTo(Preset_Surprised); _face.LeftEye.TransitionTo(Preset_Surprised); }
void FaceExpression::GoTo_Skeptic()   { ClearVariations(); _face.RightEye.TransitionTo(Preset_Skeptic); _face.LeftEye.TransitionTo(Preset_Skeptic_Alt); }
void FaceExpression::GoTo_Frustrated(){ ClearVariations(); _face.RightEye.TransitionTo(Preset_Frustrated); _face.LeftEye.TransitionTo(Preset_Frustrated); }
void FaceExpression::GoTo_Unimpressed(){ ClearVariations(); _face.RightEye.TransitionTo(Preset_Unimpressed); _face.LeftEye.TransitionTo(Preset_Unimpressed_Alt); }
void FaceExpression::GoTo_Sleepy()    { ClearVariations(); _face.RightEye.TransitionTo(Preset_Sleepy); _face.LeftEye.TransitionTo(Preset_Sleepy_Alt); }
void FaceExpression::GoTo_Suspicious(){ ClearVariations(); _face.RightEye.TransitionTo(Preset_Suspicious); _face.LeftEye.TransitionTo(Preset_Suspicious_Alt); }
void FaceExpression::GoTo_Squint() {
  ClearVariations();
  _face.LeftEye.Variation1.Values.OffsetX = 6; _face.LeftEye.Variation2.Values.OffsetY = 6;
  _face.RightEye.TransitionTo(Preset_Squint); _face.LeftEye.TransitionTo(Preset_Squint_Alt);
}
void FaceExpression::GoTo_Furious()   { ClearVariations(); _face.RightEye.TransitionTo(Preset_Furious); _face.LeftEye.TransitionTo(Preset_Furious); }
void FaceExpression::GoTo_Scared()    { ClearVariations(); _face.RightEye.TransitionTo(Preset_Scared);  _face.LeftEye.TransitionTo(Preset_Scared); }
void FaceExpression::GoTo_Awe()       { ClearVariations(); _face.RightEye.TransitionTo(Preset_Awe);     _face.LeftEye.TransitionTo(Preset_Awe); }

// ---- FaceBehavior ----
FaceBehavior::FaceBehavior(Face& face) : _face(face), Timer(4000) {
    Timer.Start();
    Clear();
    Emotions[Normal]      = 4;
    Emotions[Angry]       = 2;
    Emotions[Glee]        = 2;
    Emotions[Happy]       = 2;
    Emotions[Sad]         = 1;
    Emotions[Worried]     = 1;
    Emotions[Focused]     = 1;
    Emotions[Annoyed]     = 1;
    Emotions[Surprised]   = 2;
    Emotions[Skeptic]     = 1;
    Emotions[Frustrated]  = 1;
    Emotions[Unimpressed] = 1;
    Emotions[Sleepy]      = 2;
    Emotions[Suspicious]  = 1;
    Emotions[Squint]      = 1;
    Emotions[Furious]     = 1;
    Emotions[Scared]      = 1;
    Emotions[Awe]         = 1;
}
void FaceBehavior::SetEmotion(eEmotions e, float v) { Emotions[e] = v; }
float FaceBehavior::GetEmotion(eEmotions e) { return Emotions[e]; }
void FaceBehavior::Clear() {
  for (int i = 0; i < EMOTIONS_COUNT; i++) Emotions[i] = 0.0;
  CurrentEmotion = Normal;
}
eEmotions FaceBehavior::GetRandomEmotion() {
  float sum = 0; for (int i = 0; i < EMOTIONS_COUNT; i++) sum += Emotions[i];
  if (sum == 0) return Normal;
  float r = random(0, 1000 * sum) / 1000.0; float acc = 0;
  for (int i = 0; i < EMOTIONS_COUNT; i++) {
    if (Emotions[i] == 0) continue; acc += Emotions[i];
    if (r <= acc) return (eEmotions)i;
  }
  return Normal;
}
void FaceBehavior::Update() {
  Timer.Update();
  if (!Timer.IsExpired()) return;
  Timer.Reset();
  eEmotions ne = GetRandomEmotion();
  if (CurrentEmotion != ne) GoToEmotion(ne);
}
void FaceBehavior::GoToEmotion(eEmotions e) {
  if (CurrentEmotion != e) {
    static const char* emotionNames[] = {
      "Normal", "Angry", "Glee", "Happy", "Sad", "Worried", "Focused", "Annoyed",
      "Surprised", "Skeptic", "Frustrated", "Unimpressed", "Sleepy", "Suspicious",
      "Squint", "Furious", "Scared", "Awe"
    };
    if ((int)e >= 0 && (int)e < EMOTIONS_COUNT) {
      Serial.print("[EMOTION] ");
      Serial.println(emotionNames[(int)e]);
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
    default: break;
  }
}

// ---- Face ----
Face::Face(uint16_t sw, uint16_t sh, uint16_t es)
  : LeftEye(*this), RightEye(*this), Blink(*this), Look(*this),
    Behavior(*this), Expression(*this),
    sprite(&tft) {
  Width = sw; Height = sh; EyeSize = es;
  CenterX = Width/2; CenterY = Height/2;
  LeftEye.IsMirrored = true; RightEye.IsMirrored = false;
  EyeInterDistance = 12; Behavior.CurrentEmotion = Normal;
  spriteOk = false;
  if (sprite.createSprite(320, 120)) {
    sprite.fillSprite(TFT_BLACK);
    spriteOk = true;
    Serial.println("[OK] Single sprite (320x120, 77KB)");
  } else {
    Serial.println("[WARN] Sprite alloc failed");
  }
}
void Face::LookFront()  { Look.LookAt(0.0, 0.0); }
void Face::LookRight()  { Look.LookAt(-1.0, 0.0); }
void Face::LookLeft()   { Look.LookAt(1.0, 0.0); }
void Face::LookTop()    { Look.LookAt(0.0, 1.0); }
void Face::LookBottom() { Look.LookAt(0.0, -1.0); }
void Face::DoBlink() { Blink.Blink(); }
void Face::Begin() {
  LeftEye.Transformation.Sync(); RightEye.Transformation.Sync();
  _startupPhase = StartupPhase::BOOT; _bootTime = millis();
  RandomBehavior = false; RandomLook = false; RandomBlink = false;
}
void Face::Update() {
  uint32_t now = millis(), elapsed = now - _bootTime;
  switch (_startupPhase) {
    case StartupPhase::BOOT:
      if (elapsed >= 2000) { Expression.GoTo_Normal(); _startupPhase = StartupPhase::FACE_READY; } break;
    case StartupPhase::FACE_READY:
      if (elapsed >= 5000) { RandomLook = true; _startupPhase = StartupPhase::BEHAVIOR_READY; } break;
    case StartupPhase::BEHAVIOR_READY:
      if (elapsed >= 8000) { RandomBlink = true; RandomBehavior = true; _startupPhase = StartupPhase::RUNNING; } break;
    case StartupPhase::RUNNING: break;
  }
  if (RandomBehavior) Behavior.Update();
  if (RandomLook)     Look.Update();
  if (RandomBlink)    Blink.Update();
  Draw();
}
void Face::Draw() {
  LeftEye.CenterX  = CenterX - EyeSize/2 - EyeInterDistance;
  LeftEye.CenterY  = CenterY;
  RightEye.CenterX = CenterX + EyeSize/2 + EyeInterDistance;
  RightEye.CenterY = CenterY;

  // Advance animation state machine
  LeftEye.Update();
  RightEye.Update();

  // Clear sprite
  sprite.fillSprite(BG_COLOR);

  // Render both eyes into the single sprite
  EyeDrawer::Draw(sprite, LeftEye.CenterX, 60, LeftEye.FinalConfig);
  EyeDrawer::Draw(sprite, RightEye.CenterX, 60, RightEye.FinalConfig);

  // Push to TFT (blocking SPI transfer)
  sprite.pushSprite(0, CenterY - 60);
}

// ========================= Global state ===================================
static Face* gFace = nullptr;

// ============================== setup() ====================================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(200);
  Serial.println("\n=== ESP32 TFT Eyes — Pure Procedural ===");
  Serial.printf("[BOOT] Reset reason: %d\n", esp_reset_reason());

  pinMode(27, OUTPUT);
  digitalWrite(27, LOW);

  delay(300);
  tft.begin();
  delay(200);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  Serial.printf("[OK] TFT ready (SPI=%uMHz)\n", SPI_FREQUENCY/1000000);
  digitalWrite(27, HIGH);

  tft.setTextColor(EYE_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ESP32 TFT Eyes", 160, 110, 2);
  tft.drawString("Procedural Engine", 160, 140, 2);
  delay(1000);
  tft.fillScreen(BG_COLOR);

  Serial.println("[BOOT] Creating Face system...");
  uint32_t freeHeap = ESP.getFreeHeap();
  Serial.printf("[BOOT] Free heap: %u bytes\n", freeHeap);

  gFace = new Face(320, 240, 70);
  gFace->Behavior.GoToEmotion(Normal);
  gFace->Begin();
  gFace->RandomBehavior = false;
  gFace->RandomLook = true;
  gFace->RandomBlink = false;

  Serial.printf("[BOOT] Free heap after Face: %u bytes\n", ESP.getFreeHeap());
  Serial.println("=== Ready ===");
}

// ============================== loop() =====================================
void loop() {
  static uint32_t lastFrameMs = 0, loopCount = 0;
  loopCount++;

  uint32_t now = millis();
  uint32_t dt = now - lastFrameMs;

  if (dt < EYES_FRAME_MS_MIN) {
    delay(1); yield(); return;
  }
  if (dt > 80) {
    lastFrameMs = now; yield(); return;
  }

  lastFrameMs = now;

  if (gFace) gFace->Update();

  if (loopCount % 100 == 0) {
    Serial.printf("[LOOP] #%u | frame=%ums | heap=%u\n",
      loopCount, dt, ESP.getFreeHeap());
  }

  delay(1); yield();
}
