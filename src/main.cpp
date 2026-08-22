#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include "Config.h"
#include "Log.h"
#include "Face.h"
#include "PowerManager.h"
#include "PowerIC.h"
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

  // BOOT button (GPIO0) doubles as the power key: hold ~2s to deep sleep.
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  LOG_PRINTLN("\n=== ESP32 TFT Eyes - Pure Procedural ===");
  LOG_PRINTF("[BOOT] version=%s\n", FIRMWARE_VERSION);
  LOG_PRINTF("[BOOT] Reset reason: %d\n", esp_reset_reason());

  // Keep the backlight off during display bring-up; PowerManager takes over.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  delay(300);

  // Sample the XPT2046 VBAT channel before TFT_eSPI claims the shared SPI
  // bus. Many 024 boards route the battery into this ADC input; the reading
  // is used as a fallback when no I2C power IC / divider is available.
  float bootBatteryV = -1.0f;
#if POWER_XPT_BATTERY_ENABLED
  bootBatteryV = PowerIC::ReadXpt2046BatteryVoltage();
#endif

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

  static PowerManager power(*gFace, TFT_BL, bootBatteryV);
  gPowerManager = &power;
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
  static uint32_t lastLoopLogMs = 0;
  static PowerState lastPowerState = PowerState::ACTIVE;
  static String cmdLine;

  // Serial command handling (cal / help) must stay responsive even when the
  // frame loop is throttling.
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      cmdLine.trim();
      if (cmdLine.length() > 0) {
        if (cmdLine == "cal") {
          if (gFace) {
            Serial.printf("[CAL] offset=%d range=%d\n",
                          gFace->GetCalOffset(), gFace->GetCalRange());
          }
        } else if (cmdLine.startsWith("cal ")) {
          int off = 0, rng = 0;
          if (gFace &&
              sscanf(cmdLine.c_str() + 4, "%d %d", &off, &rng) == 2 &&
              rng > 0) {
            gFace->SetCalibration(off, rng);
          } else {
            Serial.println("[CAL] usage: cal <offset> <range>");
          }
        } else if (cmdLine == "help") {
          Serial.println("[LOG] commands: cal [offset range] | help");
        } else {
          Serial.printf("[LOG] unknown command: %s ('help' for list)\n",
                        cmdLine.c_str());
        }
      }
      cmdLine = "";
    } else if (c != '\r') {
      cmdLine += c;
    }
  }

  uint32_t now = millis();
  uint32_t dt = now - lastFrameMs;

  // Power key: BOOT (GPIO0) held for 2s -> deep sleep. Wake via BOOT
  // (EXT0 level wake) or the EN/reset button (hardware).
  static bool btnWasDown = false;
  static unsigned long btnDownMs = 0;
  bool btnDown = digitalRead(PIN_BTN_POWER) == LOW;
  if (btnDown && !btnWasDown) {
    btnWasDown = true;
    btnDownMs = now;
  } else if (!btnDown) {
    btnWasDown = false;
  }
  if (btnWasDown && now - btnDownMs >= 2000UL) {
    LOG_PRINTLN("[POWER] BOOT held 2s -> shutdown (release BOOT to confirm)");
    Serial.flush();
    // Require a confirmed release before sleeping: entering deep sleep while
    // the button is still held would trigger the EXT0 level wake instantly,
    // which looks like "black screen then auto reboot".
    unsigned long releaseDeadline = now + 10000UL;
    bool released = false;
    while (millis() < releaseDeadline) {
      if (digitalRead(PIN_BTN_POWER) == HIGH) { released = true; break; }
      delay(25);
    }
    if (!released) {
      LOG_PRINTLN("[POWER] shutdown cancelled (button held too long)");
      btnWasDown = false;
      btnDownMs = now; // require a fresh 2s hold next time
    } else {
      delay(150); // debounce after release
      LOG_PRINTLN("[POWER] entering deep sleep (press BOOT or EN to wake)");
      tft.fillScreen(TFT_BLACK);
      digitalWrite(TFT_BL, LOW);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_POWER, LOW);
      esp_deep_sleep_start();
    }
  }

  // Power state machine runs every iteration so timers stay responsive.
  if (gPowerManager) gPowerManager->Update();

  if (gPowerManager) {
    PowerState st = gPowerManager->GetState();
    if (st != lastPowerState) {
      lastPowerState = st;
    }
  }

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
