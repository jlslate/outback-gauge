#pragma once

#include <stdint.h>

// Everything the Wi-Fi settings page can change. The compile-time values in
// config.h are the defaults; what's here is what's actually in force, loaded
// from NVS at boot.
//
// Reads happen on the UI task, writes on the web server's task, so the web
// side stages a whole struct and the UI task swaps it in at a safe point
// (settings_apply) rather than editing fields underneath a running frame.
constexpr int GAUGE_PAGES = 8;             // keep in step with the page list in ui.cpp and webpage.cpp

struct Settings {
  uint8_t backlight;  // percent

  float boostWarnPsi;
  float coolantWarnF;
  float oilWarnF;
  float intakeWarnF;
  float voltsLowWarn;
  float voltsHighWarn;

  bool autoRotate;      // step through the gauges on a timer
  float autoRotateSecs;  // seconds each gauge stays up

  uint8_t pageMask;   // bit per gauge page, in the order of the pages in ui.cpp: set = shown
  uint8_t startPage;  // the page to come up on at power-up, or START_LAST for the last one tapped to

  // Added after the rest, so a blob saved without them still loads (see settings_load).
  float speedWarnMph;
  float rpmWarn;
  uint8_t order[GAUGE_PAGES];  // the gauges in the order the screen steps through them; a permutation of 0..GAUGE_PAGES-1
};

constexpr uint8_t START_LAST = 255;

Settings settings_defaults();
const Settings &settings();  // current values; safe to read every frame

void settings_load();               // call once, before anything reads settings()
Settings settings_clean(const Settings &s);  // a copy with every value clamped, as settings_stage would keep it
void settings_stage(const Settings &s);  // from the web task; clamped here
bool settings_apply();              // UI task: adopts and persists staged values
