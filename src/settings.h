#pragma once

#include <stdint.h>

// Everything the Wi-Fi settings page can change. The compile-time values in
// config.h are the defaults; what's here is what's actually in force, loaded
// from NVS at boot.
//
// Reads happen on the UI task, writes on the web server's task, so the web
// side stages a whole struct and the UI task swaps it in at a safe point
// (settings_apply) rather than editing fields underneath a running frame.
struct Settings {
  uint8_t backlight;  // percent

  float boostWarnPsi;
  float coolantWarnF;
  float intakeWarnF;
  float voltsLowWarn;
  float voltsHighWarn;
  float tiltWarnDeg;

  bool rotate180;  // needs a reboot to take effect
  bool touchSwapXY;
  bool touchInvertX;
  bool touchInvertY;

  int8_t rollSign;   // +1 or -1
  int8_t pitchSign;
};

Settings settings_defaults();
const Settings &settings();  // current values; safe to read every frame

void settings_load();               // call once, before anything reads settings()
void settings_stage(const Settings &s);  // from the web task; clamped here
bool settings_apply();              // UI task: adopts and persists staged values
