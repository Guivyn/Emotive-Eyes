#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include "Config.h"
#include "Log.h"
#include "Face.h"
#include "PowerManager.h"
#include "Version.h"
#include "PowerButton.h"
#include "PowerDiagnostics.h"
#include <string.h>

TFT_eSPI tft = TFT_eSPI();
Face* gFace = nullptr;
PowerManager* gPowerManager = nullptr;

void setup() {
  Serial.begin(115200);
#if SERIAL_DEBUG_ENABLED
  Serial.setDebugOutput(true);
#endif

  // BOOT is an input: a long press while running requests SOFT_OFF. While
  // already in SOFT_OFF it is ignored; hardware RST/EN is the wake path.
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  LOG_PRINTLN("\n=== ESP32 TFT Eyes - Pure Procedural ===");
  LOG_PRINTF("[BOOT] version=%s\n", FIRMWARE_VERSION);
  LOG_PRINTF("[BOOT] Reset reason: %d\n", esp_reset_reason());

  // Keep the backlight off during display bring-up; PowerManager takes over.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  tft.begin();
  delay(STARTUP_TFT_SETTLE_MS);
  tft.setRotation(1);
  LOG_PRINTF("[OK] TFT ready (SPI=%uMHz)\n", SPI_FREQUENCY / 1000000);

  LOG_PRINTLN("[BOOT] Creating Face system...");
  uint32_t freeHeap = ESP.getFreeHeap();
  LOG_PRINTF("[BOOT] Free heap: %u bytes\n", freeHeap);

  gFace = new Face(SCREEN_WIDTH, SCREEN_HEIGHT, 70);
  gFace->Behavior.GoToEmotion(Normal);
  gFace->Begin();
  gFace->RandomBehavior = false;
  // Face::Begin owns the staged startup phases; look movement starts after
  // the initial wake frame rather than being enabled here prematurely.
  gFace->RandomBlink = false;

  static PowerManager power(*gFace, TFT_BL);
  gPowerManager = &power;
  // Draw while the backlight is still off. The first frame covers the whole
  // screen, so a separate startup fillScreen transfer is unnecessary.
  gFace->Draw();
  power.Begin();
  // Back RGB LEDs are unused; drive the pins high (active-LOW) so the
  // LEDs stay dark instead of floating.
  pinMode(PIN_LED_R, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH);
  pinMode(PIN_LED_G, OUTPUT);
  digitalWrite(PIN_LED_G, HIGH);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_B, HIGH);

  LOG_PRINTF("[OK] Touch ready (XPT2046 CS=33, shared SPI, threshold=%u)\n",
             TOUCH_PRESSURE_THRESHOLD);
  LOG_PRINTF("[TOUCH] Tap the screen %u times quickly to trigger Angry\n",
             TAP_COUNT_FOR_ANGER);

  LOG_PRINTF("[BOOT] Free heap after Face: %u bytes\n", ESP.getFreeHeap());
  LOG_PRINTLN("=== Ready ===");

}

void loop() {
  static uint32_t lastFrameMs = 0;
#if SERIAL_DEBUG_ENABLED
  static uint32_t loopPerfWindowStartMs = 0;
  static uint32_t loopPerfFrames = 0;
  static uint32_t loopPerfTotalDt = 0;
  static uint32_t loopPerfMaxDt = 0;
  static uint32_t loopPerfLateFrames = 0;
#endif
  static bool deferredStateLoaded = false;
  static char cmdLine[64] = {};
  static size_t cmdLength = 0;
  static bool cmdOverflow = false;

  if (!deferredStateLoaded && gFace) {
    // Calibration is non-critical for first-frame display, so mount the
    // filesystem on the first loop rather than extending the boot sequence.
    gFace->LoadDeferredState();
    deferredStateLoaded = true;
  }

  // Serial command handling (cal / help) must stay responsive even when the
  // frame loop is throttling.
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      cmdLine[cmdLength] = '\0';
      if (cmdOverflow) {
        Serial.println("[CAL] command too long");
      } else if (cmdLength > 0) {
        if (strcmp(cmdLine, "cal") == 0) {
          if (gFace) {
            Serial.printf("[CAL] offset=%d range=%d\n",
                          gFace->GetCalOffset(), gFace->GetCalRange());
          }
        } else if (strncmp(cmdLine, "cal ", 4) == 0) {
          int off = 0, rng = 0;
          if (gFace &&
              sscanf(cmdLine + 4, "%d %d", &off, &rng) == 2 &&
              rng > 0) {
            bool saved = gFace->SetCalibration(off, rng);
#if !SERIAL_DEBUG_ENABLED
            Serial.printf("[CAL] offset=%d range=%d %s\n", off, rng,
                          saved ? "saved" : "applied, NOT saved");
#endif
          } else {
            Serial.println("[CAL] usage: cal <offset> <range>");
          }
        } else if (strcmp(cmdLine, "brightness") == 0) {
          if (gPowerManager) {
            Serial.printf("[BRIGHTNESS] %u%% (range %u-%u)\n",
                          (unsigned)gPowerManager->GetBrightness(),
                          (unsigned)PowerManager::MIN_BRIGHTNESS_PERCENT,
                          (unsigned)PowerManager::MAX_BRIGHTNESS_PERCENT);
          }
        } else if (strncmp(cmdLine, "brightness ", 11) == 0) {
          int brightness = -1;
          char trailing = '\0';
          if (gPowerManager &&
              sscanf(cmdLine + 11, "%d %c", &brightness, &trailing) == 1 &&
              brightness >= PowerManager::MIN_BRIGHTNESS_PERCENT &&
              brightness <= PowerManager::MAX_BRIGHTNESS_PERCENT) {
            bool saved = gPowerManager->SetBrightness((uint8_t)brightness);
            Serial.printf("[BRIGHTNESS] set=%d%% saved=%s\n", brightness,
                          saved ? "yes" : "no");
          } else {
            Serial.printf("[BRIGHTNESS] usage: brightness <%u-%u>\n",
                          (unsigned)PowerManager::MIN_BRIGHTNESS_PERCENT,
                          (unsigned)PowerManager::MAX_BRIGHTNESS_PERCENT);
          }
        } else if (strcmp(cmdLine, "help") == 0) {
          Serial.println("[LOG] commands: cal [offset range] | brightness [20-100] | fs-init | power | power-scan | off | wake | help");
        } else if (strcmp(cmdLine, "power") == 0 || strcmp(cmdLine, "power-scan") == 0) {
          PrintPowerDiagnostics(strcmp(cmdLine, "power-scan") == 0);
          Serial.printf("[POWER] state=%s\n", gPowerManager && gPowerManager->GetState() == PowerState::SOFT_OFF ? "SOFT_OFF" : "RUNNING");
        } else if (strcmp(cmdLine, "off") == 0) {
          if (gPowerManager) gPowerManager->EnterSoftOff();
        } else if (strcmp(cmdLine, "wake") == 0) {
          if (gPowerManager) gPowerManager->NotifyInteraction();
        } else if (strcmp(cmdLine, "fs-init") == 0) {
          bool initialized = gFace && gFace->InitializeFilesystem();
          Serial.printf("[FS] %s\n", initialized ? "ready" : "init failed");
        } else {
          Serial.printf("[LOG] unknown command: %s ('help' for list)\n",
                        cmdLine);
        }
      }
      cmdLength = 0;
      cmdOverflow = false;
      cmdLine[0] = '\0';
    } else if (c != '\r') {
      if (cmdLength < sizeof(cmdLine) - 1) {
        cmdLine[cmdLength++] = c;
      } else {
        cmdOverflow = true;
      }
    }
  }

  uint32_t now = millis();
  uint32_t dt = now - lastFrameMs;

  static PowerButton powerButton;
  const auto buttonEvent = powerButton.Update(digitalRead(PIN_BTN_POWER) == LOW, now);
  if (gPowerManager && buttonEvent != PowerButton::Event::NONE) {
    if (gPowerManager->GetState() == PowerState::SOFT_OFF) {
      // BOOT is intentionally ignored in SOFT_OFF. Press RST/EN to reboot.
    } else if (buttonEvent == PowerButton::Event::LONG_PRESS) {
      // While running, a long press is the explicit software-off gesture.
      gPowerManager->EnterSoftOff();
    }
  }

  // Power state machine runs every iteration so timers stay responsive.
  if (gPowerManager) gPowerManager->Update();

  if (gPowerManager && gPowerManager->GetState() == PowerState::SOFT_OFF) {
    // SOFT_OFF normally never returns here: PowerManager enters Deep Sleep.
    return;
  }

  // SLEEP has no animation to render. Avoid the former 1ms busy loop while
  // retaining responsive touch/BOOT wake-up checks. Use a faster poll only
  // during the short backlight fade; normal sleep returns to the 50ms cadence.
  if (gPowerManager && gPowerManager->GetState() == PowerState::SLEEP) {
    if (gFace) gFace->UpdateTouch();
    delay(gPowerManager->GetFrameIntervalMs());
    return;
  }

  uint32_t frameInterval = gPowerManager
    ? gPowerManager->GetFrameIntervalMs() : FRAME_INTERVAL_ACTIVE_MS;
  if (dt < frameInterval) {
    delay(1);
    return;
  }
  if (dt > frameInterval * 4) {
    lastFrameMs = now;
    delay(1);
    return;
  }

  lastFrameMs = now;

  if (gFace) {
#if defined(PERF_CONTINUOUS_ANIMATION)
    static const eEmotions benchmarkEmotions[] = {
      Normal, Happy, Worried, Surprised, Glee, Sleepy, Coquette, Skeptic
    };
    static uint8_t benchmarkEmotionIndex = 0;
    static uint32_t nextBenchmarkEmotionMs = 0;
    if (nextBenchmarkEmotionMs == 0) {
      nextBenchmarkEmotionMs = now + 450;
    } else if ((int32_t)(now - nextBenchmarkEmotionMs) >= 0) {
      benchmarkEmotionIndex = (benchmarkEmotionIndex + 1) %
          (sizeof(benchmarkEmotions) / sizeof(benchmarkEmotions[0]));
      gFace->Behavior.GoToEmotion(benchmarkEmotions[benchmarkEmotionIndex]);
      nextBenchmarkEmotionMs = now + 450;
    }
#endif
    gFace->Update();
  }

#if SERIAL_DEBUG_ENABLED
  if (loopPerfWindowStartMs == 0) loopPerfWindowStartMs = now;
  loopPerfFrames++;
  loopPerfTotalDt += dt;
  if (dt > loopPerfMaxDt) loopPerfMaxDt = dt;
  if (dt > frameInterval + 2) loopPerfLateFrames++;
  if (now - loopPerfWindowStartMs >= 5000) {
    uint32_t windowMs = now - loopPerfWindowStartMs;
    float avgFps = loopPerfFrames * 1000.0f / windowMs;
    uint32_t avgDt = loopPerfTotalDt / loopPerfFrames;
    LOG_PRINTF("[LOOP] target=%ums avg_fps=%.1f avg_dt=%ums max_dt=%ums late=%u frames=%u heap=%u\n",
               frameInterval, avgFps, avgDt, loopPerfMaxDt,
               loopPerfLateFrames, loopPerfFrames, ESP.getFreeHeap());
    loopPerfWindowStartMs = now;
    loopPerfFrames = 0;
    loopPerfTotalDt = 0;
    loopPerfMaxDt = 0;
    loopPerfLateFrames = 0;
  }
#endif

  delay(1);
}
