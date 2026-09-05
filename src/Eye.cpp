#include "Eye.h"

static const float COS_15[] = {
  1.0f, 0.9659258263f, 0.8660254038f, 0.7071067812f, 0.5f, 0.2588190451f,
  0.0f, -0.2588190451f, -0.5f, -0.7071067812f, -0.8660254038f, -0.9659258263f,
  -1.0f, -0.9659258263f, -0.8660254038f, -0.7071067812f, -0.5f, -0.2588190451f,
  0.0f, 0.2588190451f, 0.5f, 0.7071067812f, 0.8660254038f, 0.9659258263f
};

static const float SIN_15[] = {
  0.0f, 0.2588190451f, 0.5f, 0.7071067812f, 0.8660254038f, 0.9659258263f,
  1.0f, 0.9659258263f, 0.8660254038f, 0.7071067812f, 0.5f, 0.2588190451f,
  0.0f, -0.2588190451f, -0.5f, -0.7071067812f, -0.8660254038f, -0.9659258263f,
  -1.0f, -0.9659258263f, -0.8660254038f, -0.7071067812f, -0.5f, -0.2588190451f
};

EyeTransition::EyeTransition()
  : Output{}, Snapshot{}, Destin{}, Active(false), Animation(500) {}

void EyeTransition::Start() {
  Animation.Restart();
  Active = true;
}

void EyeTransition::Update() {
  if (!Active) return;
  float t = Animation.GetValue();
  if (t >= 1.0f) {
    Active = false;
    Output = Destin;
    return;
  }
  Apply(t);
}

void EyeTransition::Apply(float t) {
  Output.OffsetX = Snapshot.OffsetX * (1.0f - t) + Destin.OffsetX * t;
  Output.OffsetY = Snapshot.OffsetY * (1.0f - t) + Destin.OffsetY * t;
  Output.Height = Snapshot.Height * (1.0f - t) + Destin.Height * t;
  Output.Width = Snapshot.Width * (1.0f - t) + Destin.Width * t;
  Output.Slope_Top = Snapshot.Slope_Top * (1.0f - t) + Destin.Slope_Top * t;
  Output.Slope_Bottom = Snapshot.Slope_Bottom * (1.0f - t) + Destin.Slope_Bottom * t;
  Output.Radius_Top = Snapshot.Radius_Top * (1.0f - t) + Destin.Radius_Top * t;
  Output.Radius_Bottom = Snapshot.Radius_Bottom * (1.0f - t) + Destin.Radius_Bottom * t;
}

EyeTransformation::EyeTransformation()
  : Input(nullptr), Output{}, Origin{}, Destin{}, Animation(200) {}

void EyeTransformation::Update() {
  if (!Input) return;
  Apply();
}

void EyeTransformation::Apply() {
  if (!Input) return;
  float t = Animation.GetValue();
  Output = *Input;
  Output.OffsetX = (int16_t)(Origin.MoveX + (Destin.MoveX - Origin.MoveX) * t);
  Output.OffsetY = (int16_t)(Origin.MoveY + (Destin.MoveY - Origin.MoveY) * t);
  float sX = Origin.ScaleX + (Destin.ScaleX - Origin.ScaleX) * t;
  float sY = Origin.ScaleY + (Destin.ScaleY - Origin.ScaleY) * t;
  Output.Width = (int16_t)(Input->Width * sX);
  Output.Height = (int16_t)(Input->Height * sY);
}

void EyeTransformation::SetDestin(Transformation t) {
  Origin.MoveX = Output.OffsetX;
  Origin.MoveY = Output.OffsetY;
  if (Input && Input->Width > 0) {
    Origin.ScaleX = (float)Output.Width / Input->Width;
  } else {
    Origin.ScaleX = 1.0f;
  }
  if (Input && Input->Height > 0) {
    Origin.ScaleY = (float)Output.Height / Input->Height;
  } else {
    Origin.ScaleY = 1.0f;
  }
  Destin = t;
}

void EyeTransformation::Sync() {
  if (!Input) return;
  Output = *Input;
  Origin.MoveX = Input->OffsetX;
  Origin.MoveY = Input->OffsetY;
  Origin.ScaleX = 1.0f;
  Origin.ScaleY = 1.0f;
  Destin = Origin;
}

EyeVariation::EyeVariation()
  : Input(nullptr), Output{}, Animation(200, 200, 200, 200, 0),
    Values{}, FadeValues{}, FadeStart(0), IsFading(false) {}

void EyeVariation::Clear() {
  Values = EyeConfig{};
}

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
  FadeStart = millis();
  IsFading = true;
  Clear();
}

void EyeVariation::Update() {
  if (!Input) return;
  Apply(Animation.GetValue());
  if (!IsFading) return;

  unsigned long elapsed = millis() - FadeStart;
  if (elapsed >= 150) {
    IsFading = false;
    return;
  }

  float fadeFactor = 1.0f - (float)elapsed / 150.0f;
  Output.OffsetX += FadeValues.OffsetX * fadeFactor;
  Output.OffsetY += FadeValues.OffsetY * fadeFactor;
  Output.Height += FadeValues.Height * fadeFactor;
  Output.Width += FadeValues.Width * fadeFactor;
  Output.Slope_Top += FadeValues.Slope_Top * fadeFactor;
  Output.Slope_Bottom += FadeValues.Slope_Bottom * fadeFactor;
  Output.Radius_Top += FadeValues.Radius_Top * fadeFactor;
  Output.Radius_Bottom += FadeValues.Radius_Bottom * fadeFactor;
}

void EyeVariation::Apply(float t) {
  if (!Input) return;
  Output = *Input;
  Output.OffsetX = Input->OffsetX + Values.OffsetX * t;
  Output.OffsetY = Input->OffsetY + Values.OffsetY * t;
  Output.Height = Input->Height + Values.Height * t;
  Output.Width = Input->Width + Values.Width * t;
  Output.Slope_Top = Input->Slope_Top + Values.Slope_Top * t;
  Output.Slope_Bottom = Input->Slope_Bottom + Values.Slope_Bottom * t;
  Output.Radius_Top = Input->Radius_Top + Values.Radius_Top * t;
  Output.Radius_Bottom = Input->Radius_Bottom + Values.Radius_Bottom * t;
}

bool EyeVariation::IsActive() const {
  if (IsFading) return true;
  return Values.OffsetX || Values.OffsetY || Values.Height || Values.Width ||
         Values.Slope_Top || Values.Slope_Bottom || Values.Radius_Top ||
         Values.Radius_Bottom;
}

EyeBlink::EyeBlink()
  : Input(nullptr), Output{}, Animation(100, 50, 100), BlinkWidth(60) {}

void EyeBlink::Update() {
  if (!Input) return;
  Apply(Animation.GetValue());
}

void EyeBlink::Apply(float t) {
  if (!Input) return;
  Output = *Input;
  Output.Height = Input->Height * (1.0f - t);
  Output.Width = Input->Width + (BlinkWidth - Input->Width) * t;
  Output.Radius_Top = Input->Radius_Top * (1.0f - t);
  Output.Radius_Bottom = Input->Radius_Bottom * (1.0f - t);
}

int EyeDrawer::GenerateBoundary(const EyeConfig* cfg, float cx, float cy,
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

  int cnt = 0;
  auto add = [&](float x, float y) {
    if (cnt < maxPts) {
      pts[cnt].x = x;
      pts[cnt].y = y;
      cnt++;
    }
  };
  auto cosDeg = [](int a) { return COS_15[(a / 15) % 24]; };
  auto sinDeg = [](int a) { return SIN_15[(a / 15) % 24]; };

  if (rT > 0) {
    for (int a = 180; a <= 270; a += 15) {
      add(TLx + rT * cosDeg(a), TLy + rT * sinDeg(a));
    }
  } else {
    add(TLx, TLy);
  }

  if (rT > 0) {
    for (int a = 285; a <= 360; a += 15) {
      add(TRx + rT * cosDeg(a), TRy + rT * sinDeg(a));
    }
  } else {
    add(TRx, TRy);
  }

  if (rB > 0) {
    for (int a = 15; a <= 90; a += 15) {
      add(BRx + rB * cosDeg(a), BRy + rB * sinDeg(a));
    }
  } else {
    add(BRx, BRy);
  }

  if (rB > 0) {
    for (int a = 105; a <= 180; a += 15) {
      add(BLx + rB * cosDeg(a), BLy + rB * sinDeg(a));
    }
  } else {
    add(BLx, BLy);
  }

  return cnt;
}

Eye::Eye()
  : IsMirrored(false), Transition(), Transformation(), Variation1(),
    Variation2(), BlinkTransformation(), FinalConfig(nullptr) {
  ChainOperators();
  Variation1.Animation.SetShape(200, 200, 200, 200, 0);
  Variation2.Animation.SetShape(0, 200, 200, 200, 200);
}

void Eye::ChainOperators() {
  Transformation.Input = &Transition.Output;
  Variation1.Input = &(Transformation.Output);
  Variation2.Input = &(Variation1.Output);
  BlinkTransformation.Input = &(Variation2.Output);
  FinalConfig = &(BlinkTransformation.Output);
}

bool Eye::NeedsRedraw() const {
  if (Transition.Active) return true;
  if (BlinkTransformation.Animation.GetElapsed() <
      BlinkTransformation.Animation.Interval) {
    return true;
  }
  if (Transformation.Animation.GetElapsed() <
      Transformation.Animation.Interval) {
    return true;
  }
  if (Variation1.IsActive()) return true;
  if (Variation2.IsActive()) return true;
  return false;
}

void Eye::Update() {
  Transition.Update();
  Transformation.Update();
  Variation1.Update();
  Variation2.Update();
  BlinkTransformation.Update();
}

void Eye::TransitionTo(const EyeConfig config) {
  Transition.Snapshot = Transition.Output;
  Transition.Destin.OffsetX = IsMirrored ? -config.OffsetX : config.OffsetX;
  Transition.Destin.OffsetY = -config.OffsetY;
  Transition.Destin.Height = config.Height;
  Transition.Destin.Width = config.Width;
  Transition.Destin.Slope_Top = IsMirrored ? config.Slope_Top : -config.Slope_Top;
  Transition.Destin.Slope_Bottom = IsMirrored ? config.Slope_Bottom : -config.Slope_Bottom;
  Transition.Destin.Radius_Top = config.Radius_Top;
  Transition.Destin.Radius_Bottom = config.Radius_Bottom;
  Transition.Start();
}
