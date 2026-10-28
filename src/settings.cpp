#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

// Stored as one blob rather than a key per field: the whole struct round-trips
// in two NVS calls, and a version mismatch after a firmware change falls back
// to defaults instead of leaving half the fields stale. Bump on any layout change.
constexpr uint16_t LAYOUT = 7;
constexpr uint16_t LAYOUT_SIX_GAUGES = 4;   // saved before speed and rpm existed
constexpr uint16_t LAYOUT_NO_LIMITS = 5;    // saved before they had warning limits
constexpr uint16_t LAYOUT_NO_ORDER = 6;     // saved before the gauges could be rearranged

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
  s.oilWarnF = clampf(s.oilWarnF, 180, 300);
  s.intakeWarnF = clampf(s.intakeWarnF, 60, 200);
  s.voltsLowWarn = clampf(s.voltsLowWarn, 10, 14);
  s.voltsHighWarn = clampf(s.voltsHighWarn, 13, 16);
  s.autoRotateSecs = clampf(s.autoRotateSecs, 1, 60);
  s.speedWarnMph = clampf(s.speedWarnMph, 20, 160);
  s.rpmWarn = clampf(s.rpmWarn, 3000, 7000);
  bool seen[GAUGE_PAGES] = {};
  bool perm = true;
  for (uint8_t o : s.order) {
    if (o >= GAUGE_PAGES || seen[o]) perm = false;
    else seen[o] = true;
  }
  if (!perm) memcpy(s.order, settings_defaults().order, sizeof s.order);
  s.pageMask &= (1u << GAUGE_PAGES) - 1;
  if (!s.pageMask) s.pageMask = 1;                       // something has to be showing
  if (s.startPage != START_LAST && s.startPage >= GAUGE_PAGES) s.startPage = 0;
  if (s.startPage != START_LAST && !(s.pageMask >> s.startPage & 1)) {      // can't start on a hidden gauge
    int i = 0;
    while (!(s.pageMask >> s.order[i] & 1)) i++;   // the first one shown, in the chosen order
    s.startPage = s.order[i];
  }
  // A low warning above the high one would paint the whole scale; keep them apart.
  if (s.voltsLowWarn > s.voltsHighWarn - 0.5f) s.voltsLowWarn = s.voltsHighWarn - 0.5f;
}

}  // namespace

Settings settings_defaults() {
  Settings s;
  s.backlight = BACKLIGHT_PERCENT;
  s.boostWarnPsi = BOOST_WARN_PSI;
  s.coolantWarnF = COOLANT_WARN_F;
  s.oilWarnF = OIL_WARN_F;
  s.intakeWarnF = INTAKE_WARN_F;
  s.voltsLowWarn = VOLTS_LOW_WARN;
  s.voltsHighWarn = VOLTS_HIGH_WARN;
  s.autoRotate = AUTO_ROTATE;
  s.autoRotateSecs = AUTO_ROTATE_SECS;
  s.pageMask = PAGE_MASK;
  s.startPage = START_PAGE;
  s.speedWarnMph = SPEED_WARN_MPH;
  s.rpmWarn = RPM_WARN;
  // Alphabetical by the gauge's name: battery, boost, coolant, engine load,
  // intake air, oil temperature, speed, tach (the numbers are the pages in ui.cpp).
  static const uint8_t alphabetical[GAUGE_PAGES] = {5, 0, 2, 1, 4, 3, 6, 7};
  memcpy(s.order, alphabetical, sizeof alphabetical);
  return s;
}

const Settings &settings() {
  return live;
}

void settings_load() {
  live = settings_defaults();

  Preferences prefs;
  prefs.begin("gauge", true);
  Stored st{LAYOUT, live};   // anything an older blob doesn't carry keeps its default
  const size_t n = prefs.getBytes("cfg", &st, sizeof st);
  prefs.end();

  // Older blobs end where the fields added since begin.
  const size_t noLimitsBytes = offsetof(Stored, s) + offsetof(Settings, speedWarnMph);
  const size_t noOrderBytes = offsetof(Stored, s) + offsetof(Settings, order);
  const bool known = (n == sizeof st && st.layout == LAYOUT) ||
                     (n == noOrderBytes && st.layout == LAYOUT_NO_ORDER) ||
                     (n == noLimitsBytes && (st.layout == LAYOUT_NO_LIMITS || st.layout == LAYOUT_SIX_GAUGES));
  if (known) {
    live = st.s;
    if (st.layout == LAYOUT_SIX_GAUGES) live.pageMask |= 0xC0;   // the new gauges start out shown
    sanitize(live);
    Serial.println("[CFG] loaded saved settings");
  } else if (n) {
    Serial.printf("[CFG] saved settings are layout %u, wanted %u; using defaults\n",
                  n >= sizeof st.layout ? st.layout : 0, LAYOUT);
  }
}

Settings settings_clean(const Settings &s) {
  Settings copy = s;
  sanitize(copy);
  return copy;
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
