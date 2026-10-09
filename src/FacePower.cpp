#include "FacePower.h"
#include "Face.h"

void FacePower_EnterRelax(Face& face) {
  face.CancelCelebration();
  face.RandomBehavior = false;
  face.RandomLook = true;
  face.RandomBlink = true;
  face.Expression.ClearVariations();
  face.Behavior.Timer.SetIntervalMillis(1000000UL); // freeze emotion timer
  face.Blink.SetIntervalRange(4000, 7000);
  face.Look.SetIntervalRange(3000, 5000);
}

void FacePower_EnterIdle(Face& face) {
  face.CancelCelebration();
  face.RandomBehavior = false;
  face.RandomLook = false;
  face.RandomBlink = false;
  if (!face.IsSleeping()) {
    face.Expression.GoTo_Normal();
    face.Expression.ClearVariations();
    face.LookFront();
  }
  face.Behavior.Timer.SetIntervalMillis(1000000UL);
  face.Blink.SetIntervalRange(6000, 10000);
  face.Look.SetIntervalRange(8000, 12000);
}

void FacePower_EnterSleep(Face& face) {
  face.CancelCelebration();
  face.RandomBehavior = false;
  face.RandomLook = false;
  face.RandomBlink = false;
  face.Expression.GoTo_Sleepy();
  face.Draw(); // show the closed-eye frame before the backlight goes off
}

void FacePower_Wake(Face& face) {
  face.WakeFromSleep();
  face.Blink.SetIntervalRange(2000, 5000);
  face.Look.SetIntervalRange(1500, 3500);
  face.Draw();
}
