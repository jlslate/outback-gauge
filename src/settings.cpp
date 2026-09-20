#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

// Stored as one blob rather than a key per field: the whole struct round-trips
// in two NVS calls, and a version mismatch after a firmware change falls back
// to defaults instead of leaving half the fields stale. Bump on any layout change.
constexpr uint16_t LAYOUT = 1;

struct Stored {
  uint16_t layout;
  Settings s;
};

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// Defaults rather than zeros: anything reading settings() before
// settings_load() runs would otherwise see every threshold at 0.
Settings live = settings_defaults();
Settings staged;
bool hasStaged = false;

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// The form can post anything, so nothing reaches the gauge unchecked. Ranges
// match what each scale can actually draw.
void sanitize(Settings &s) {
  s.backlight = (uint8_t)clampf(s.backlight, 5, 100);
  s.boostWarnPsi = clampf(s.boostWarnPsi, 0, 20);
  s.coolantWarnF = clampf(s.coolantWarnF, 150, 260);
  s.intakeWarnF = clampf(s.intakeWarnF, 60, 200);
  s.voltsLowWarn = clampf(s.voltsLowWarn, 10, 14);
  s.voltsHighWarn = clampf(s.voltsHighWarn, 13, 16);
  s.tiltWarnDeg = clampf(s.tiltWarnDeg, 5, 45);
  s.rollSign = s.rollSign < 0 ? -1 : 1;
  s.pitchSign = s.pitchSign < 0 ? -1 : 1;
  // A low warning above the high one would paint the whole scale; keep them apart.
  if (s.voltsLowWarn > s.voltsHighWarn - 0.5f) s.voltsLowWarn = s.voltsHighWarn - 0.5f;
}

}  // namespace

Settings settings_defaults() {
  Settings s;
  s.backlight = BACKLIGHT_PERCENT;
  s.boostWarnPsi = BOOST_WARN_PSI;
  s.coolantWarnF = COOLANT_WARN_F;
  s.intakeWarnF = INTAKE_WARN_F;
  s.voltsLowWarn = VOLTS_LOW_WARN;
  s.voltsHighWarn = VOLTS_HIGH_WARN;
  s.tiltWarnDeg = TILT_WARN_DEG;
  s.rotate180 = DISPLAY_ROTATE_180;
  s.touchSwapXY = TOUCH_SWAP_XY;
  s.touchInvertX = TOUCH_INVERT_X;
  s.touchInvertY = TOUCH_INVERT_Y;
  s.rollSign = TILT_ROLL_SIGN;
  s.pitchSign = TILT_PITCH_SIGN;
  return s;
}

const Settings &settings() {
  return live;
}

void settings_load() {
  live = settings_defaults();

  Preferences prefs;
  prefs.begin("gauge", true);
  Stored st;
  const size_t n = prefs.getBytes("cfg", &st, sizeof st);
  prefs.end();

  if (n == sizeof st && st.layout == LAYOUT) {
    live = st.s;
    sanitize(live);
    Serial.println("[CFG] loaded saved settings");
  } else if (n) {
    Serial.printf("[CFG] saved settings are layout %u, wanted %u; using defaults\n",
                  n == sizeof st ? st.layout : 0, LAYOUT);
  }
}

void settings_stage(const Settings &s) {
  Settings copy = s;
  sanitize(copy);
  taskENTER_CRITICAL(&mux);
  staged = copy;
  hasStaged = true;
  taskEXIT_CRITICAL(&mux);
}

bool settings_apply() {
  Settings incoming;
  taskENTER_CRITICAL(&mux);
  const bool pending = hasStaged;
  if (pending) {
    incoming = staged;
    hasStaged = false;
  }
  taskEXIT_CRITICAL(&mux);
  if (!pending) return false;

  live = incoming;

  Stored st{LAYOUT, live};
  Preferences prefs;
  prefs.begin("gauge", false);
  prefs.putBytes("cfg", &st, sizeof st);
  prefs.end();
  Serial.println("[CFG] settings saved");
  return true;
}
