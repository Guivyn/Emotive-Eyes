#pragma once

// =====================================================================
// HW-950 board pin map (ESP32-2432S024R / Sunton CYD family)
//
// Single header to edit when moving to a sibling board. TFT_eSPI aliases
// (TFT_MOSI etc.) are defined here so they exist before any TFT_eSPI.h
// include; platformio.ini force-includes this file with `-include`.
// =====================================================================

#define BOARD_HW950 1

// ---- TFT: ST7796 240x320, HSPI ---------------------------------------
#define PIN_TFT_MISO  12
#define PIN_TFT_MOSI  13
#define PIN_TFT_SCLK  14
#define PIN_TFT_CS    15
#define PIN_TFT_DC    2
// CYD-family displays tie RESET to the board RESET line (confirmed by both
// Marauder 024R configs), so no GPIO is needed; GPIO4 is the red LED.
#define PIN_TFT_RST   -1
#define PIN_TFT_BL    27

// ---- Touch: XPT2046, shared SPI --------------------------------------
#define PIN_TOUCH_CS  33

// ---- Buttons ----------------------------------------------------------
// BOOT button: hold ~2s to power off (deep sleep). EN is the hardware
// power-on/reset; GPIO0 also wakes from deep sleep.
#define PIN_BTN_POWER 0

// ---- Ambient light sensor (LDR) --------------------------------------
#define PIN_LDR       34

// ---- Back RGB LED (discrete, active-LOW) -----------------------------
// Hardware pins only; the firmware keeps these LEDs off. HW-950 wiring:
// R=17, G=4, B=16 (standard CYD boards use R=4/G=16/B=17).
#define PIN_LED_R     17
#define PIN_LED_G     4
#define PIN_LED_B     16

// ---- microSD (VSPI) ---------------------------------------------------
#define PIN_SD_CS     5
#define PIN_SD_MOSI   23
#define PIN_SD_CLK    18
#define PIN_SD_MISO   19

// ---- Audio amplifier (FM8002A / PAM8002A) -----------------------------
#define PIN_AUDIO     26

// ---- I2C --------------------------------------------------------------
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// ---- Battery sense (2:1 divider on GPIO39) ----------------------------
#define PIN_BATTERY_ADC     39
#define PIN_BATTERY_DIVIDER 2.0f

// XPT2046 VBAT channel is not routed to the battery on the 024R; skip the
// boot-time SPI read unless a board actually wires VBAT.
#ifndef POWER_XPT_BATTERY_ENABLED
  #define POWER_XPT_BATTERY_ENABLED 0
#endif

// ---------------------------------------------------------------------
// TFT_eSPI aliases (must be visible before any TFT_eSPI.h include)
// ---------------------------------------------------------------------
#ifndef TFT_MISO
  #define TFT_MISO  PIN_TFT_MISO
#endif
#ifndef TFT_MOSI
  #define TFT_MOSI  PIN_TFT_MOSI
#endif
#ifndef TFT_SCLK
  #define TFT_SCLK  PIN_TFT_SCLK
#endif
#ifndef TFT_CS
  #define TFT_CS    PIN_TFT_CS
#endif
#ifndef TFT_DC
  #define TFT_DC    PIN_TFT_DC
#endif
#ifndef TFT_RST
  #define TFT_RST   PIN_TFT_RST
#endif
#ifndef TFT_BL
  #define TFT_BL    PIN_TFT_BL
#endif
#ifndef TOUCH_CS
  #define TOUCH_CS  PIN_TOUCH_CS
#endif

// Power subsystem aliases (used by PowerManager / PowerIC)
#ifndef POWER_BATTERY_ADC_PIN
  #define POWER_BATTERY_ADC_PIN PIN_BATTERY_ADC
#endif
#ifndef POWER_BATTERY_DIVIDER
  #define POWER_BATTERY_DIVIDER PIN_BATTERY_DIVIDER
#endif
#ifndef POWER_IC_I2C_SDA
  #define POWER_IC_I2C_SDA PIN_I2C_SDA
#endif
#ifndef POWER_IC_I2C_SCL
  #define POWER_IC_I2C_SCL PIN_I2C_SCL
#endif
