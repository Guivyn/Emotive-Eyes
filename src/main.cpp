#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "Log.h"
#include "Face.h"
#include "PowerManager.h"
#include "Version.h"

TFT_eSPI tft = TFT_eSPI();
Face* gFace = nullptr;
PowerManager* gPowerManager = nullptr;

void setup() {
  Serial.begin(115200);
#if SERIAL_DEBUG_ENABLED
  Serial.setDebugOutput(true);
#endif
  delay(200);
  LOG_PRINTLN("\n=== ESP32 TFT Eyes - Pure Procedural ===");
  LOG_PRINTF("[BOOT] version=%s\n", FIRMWARE_VERSION);
  LOG_PRINTF("[BOOT] Reset reason: %d\n", esp_reset_reason());

  // Keep the backlight off during display bring-up; PowerManager takes over.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  delay(300);
  tft.begin();
  delay(200);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  LOG_PRINTF("[OK] TFT ready (SPI=%uMHz)\n", SPI_FREQUENCY / 1000000);

  tft.setTextColor(EYE_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ESP32 TFT Eyes", SCREEN_WIDTH / 2,
                 SCREEN_HEIGHT / 2 - 10, 2);
  tft.drawString("Procedural Engine", SCREEN_WIDTH / 2,
                 SCREEN_HEIGHT / 2 + 20, 2);
  delay(1000);
  tft.fillScreen(BG_COLOR);

  LOG_PRINTLN("[BOOT] Creating Face system...");
  uint32_t freeHeap = ESP.getFreeHeap();
  LOG_PRINTF("[BOOT] Free heap: %u bytes\n", freeHeap);

  gFace = new Face(SCREEN_WIDTH, SCREEN_HEIGHT, 70);
  gFace->Behavior.GoToEmotion(Normal);
  gFace->Begin();
  gFace->RandomBehavior = false;
  gFace->RandomLook = true;
  gFace->RandomBlink = false;

  static PowerManager power(*gFace, TFT_BL);
  gPowerManager = &power;
  power.Begin();

  LOG_PRINTF("[OK] Touch ready (XPT2046 CS=33, shared SPI, threshold=%u)\n",
             TOUCH_PRESSURE_THRESHOLD);
  LOG_PRINTF("[TOUCH] Tap the screen %u times quickly to trigger Angry\n",
             TAP_COUNT_FOR_ANGER);

  LOG_PRINTF("[BOOT] Free heap after Face: %u bytes\n", ESP.getFreeHeap());
  LOG_PRINTLN("=== Ready ===");
}

void loop() {
  static uint32_t lastFrameMs = 0;
  static uint32_t lastLoopLogMs = 0;

  uint32_t now = millis();
  uint32_t dt = now - lastFrameMs;

  // Power state machine runs every iteration so timers stay responsive.
  if (gPowerManager) gPowerManager->Update();

  uint32_t frameInterval = gPowerManager
    ? gPowerManager->GetFrameIntervalMs() : EYES_FRAME_MS_MIN;
  if (dt < frameInterval) {
    delay(1);
    yield();
    return;
  }
  if (dt > frameInterval * 4) {
    lastFrameMs = now;
    yield();
    return;
  }

  lastFrameMs = now;

  if (gFace) {
    if (gPowerManager && gPowerManager->GetState() == PowerState::SLEEP) {
      gFace->UpdateTouch(); // sleep: touch-poll only, no render
    } else {
      gFace->Update();
    }
  }

  if (now - lastLoopLogMs >= 5000) {
    lastLoopLogMs = now;
    LOG_PRINTF("[LOOP] frame=%ums | heap=%u\n", dt, ESP.getFreeHeap());
  }

  delay(1);
  yield();
}
