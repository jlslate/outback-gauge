#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

// Stored as one blob rather than a key per field: the whole struct round-trips
// in two NVS calls, and a version mismatch after a firmware change falls back
// to defaults instead of leaving half the fields stale. Bump on any layout change.
constexpr uint16_t LAYOUT = 11;

struct Stored {
  uint16_t layout;
  Settings s;
};

// Layout 7 as it was before the fuel gauge, kept so settings saved then (or in
// 4 to 6, which are shorter prefixes of it) carry over instead of resetting.
struct SettingsV7 {
  uint8_t backlight;
  float boostWarnPsi, coolantWarnF, oilWarnF, intakeWarnF, voltsLowWarn, voltsHighWarn;
  bool autoRotate;
  float autoRotateSecs;
  uint8_t pageMask, startPage;
  float speedWarnMph, rpmWarn;
  uint8_t order[8];
};

struct StoredV7 {
  uint16_t layout;
  SettingsV7 s;
};

constexpr uint8_t V7_ORDER[8] = {5, 0, 2, 1, 4, 3, 6, 7};

SettingsV7 v7From(const Settings &d) {
  SettingsV7 o{};
  o.backlight = d.backlight;
  o.boostWarnPsi = d.boostWarnPsi, o.coolantWarnF = d.coolantWarnF, o.oilWarnF = d.oilWarnF;
  o.intakeWarnF = d.intakeWarnF, o.voltsLowWarn = d.voltsLowWarn, o.voltsHighWarn = d.voltsHighWarn;
  o.autoRotate = d.autoRotate, o.autoRotateSecs = d.autoRotateSecs;
  o.pageMask = d.pageMask & 0xFF, o.startPage = d.startPage;
  o.speedWarnMph = d.speedWarnMph, o.rpmWarn = d.rpmWarn;
  memcpy(o.order, V7_ORDER, sizeof o.order);
  return o;
}

// Layout 8, before the clinometer joined the gauges.
struct SettingsV8 {
  uint8_t backlight;
  float boostWarnPsi, coolantWarnF, oilWarnF, intakeWarnF, voltsLowWarn, voltsHighWarn;
  bool autoRotate;
  float autoRotateSecs;
  uint16_t pageMask;
  uint8_t startPage;
  float speedWarnMph, rpmWarn, fuelWarnPct;
  uint8_t order[9];
};

struct StoredV8 {
  uint16_t layout;
  SettingsV8 s;
};

// Fuel is shown and goes last; everything else keeps its place.
SettingsV8 v8FromV7(const SettingsV7 &o, uint16_t layout) {
  SettingsV8 s{};
  s.backlight = o.backlight;
  s.boostWarnPsi = o.boostWarnPsi, s.coolantWarnF = o.coolantWarnF, s.oilWarnF = o.oilWarnF;
  s.intakeWarnF = o.intakeWarnF, s.voltsLowWarn = o.voltsLowWarn, s.voltsHighWarn = o.voltsHighWarn;
  s.autoRotate = o.autoRotate, s.autoRotateSecs = o.autoRotateSecs;
  s.pageMask = o.pageMask | 0x100 | (layout == 4 ? 0xC0 : 0);   // layout 4 predates speed and rpm too
  s.startPage = o.startPage;
  s.speedWarnMph = o.speedWarnMph, s.rpmWarn = o.rpmWarn, s.fuelWarnPct = FUEL_WARN_PCT;
  memcpy(s.order, o.order, sizeof o.order);
  s.order[8] = 8;
  return s;
}

// Layout 9, when the clinometer was a single page.
struct SettingsV9 {
  uint8_t backlight;
  float boostWarnPsi, coolantWarnF, oilWarnF, intakeWarnF, voltsLowWarn, voltsHighWarn;
  bool autoRotate;
  float autoRotateSecs;
  uint16_t pageMask;
  uint8_t startPage;
  float speedWarnMph, rpmWarn, fuelWarnPct, rollWarnDeg, pitchWarnDeg;
  bool warnHidden;
  uint8_t order[10];
};

struct StoredV9 {
  uint16_t layout;
  SettingsV9 s;
};

// Layout 10, before alerts could be switched off per gauge.
struct SettingsV10 {
  uint8_t backlight;
  float boostWarnPsi, coolantWarnF, oilWarnF, intakeWarnF, voltsLowWarn, voltsHighWarn;
  bool autoRotate;
  float autoRotateSecs;
  uint16_t pageMask;
  uint8_t startPage;
  float speedWarnMph, rpmWarn, fuelWarnPct, rollWarnDeg, pitchWarnDeg;
  bool warnHidden;
  uint8_t order[MAX_GAUGES];
};

struct StoredV10 {
  uint16_t layout;
  SettingsV10 s;
};

// Everything carries over; the new options start at their defaults.
Settings fromV10(const SettingsV10 &o) {
  Settings s = settings_defaults();
  s.backlight = o.backlight;
  s.boostWarnPsi = o.boostWarnPsi, s.coolantWarnF = o.coolantWarnF, s.oilWarnF = o.oilWarnF;
  s.intakeWarnF = o.intakeWarnF, s.voltsLowWarn = o.voltsLowWarn, s.voltsHighWarn = o.voltsHighWarn;
  s.autoRotate = o.autoRotate, s.autoRotateSecs = o.autoRotateSecs;
  s.pageMask = o.pageMask;
  s.startPage = o.startPage;
  s.speedWarnMph = o.speedWarnMph, s.rpmWarn = o.rpmWarn, s.fuelWarnPct = o.fuelWarnPct;
  s.rollWarnDeg = o.rollWarnDeg, s.pitchWarnDeg = o.pitchWarnDeg, s.warnHidden = o.warnHidden;
  memcpy(s.order, o.order, sizeof s.order);
  return s;
}

// The clinometer is shown and goes last.
SettingsV9 v9FromV8(const SettingsV8 &o) {
  SettingsV9 s{};
  s.backlight = o.backlight;
  s.boostWarnPsi = o.boostWarnPsi, s.coolantWarnF = o.coolantWarnF, s.oilWarnF = o.oilWarnF;
  s.intakeWarnF = o.intakeWarnF, s.voltsLowWarn = o.voltsLowWarn, s.voltsHighWarn = o.voltsHighWarn;
  s.autoRotate = o.autoRotate, s.autoRotateSecs = o.autoRotateSecs;
  s.pageMask = o.pageMask | 0x200;
  s.startPage = o.startPage;
  s.speedWarnMph = o.speedWarnMph, s.rpmWarn = o.rpmWarn, s.fuelWarnPct = o.fuelWarnPct;
  s.rollWarnDeg = ROLL_WARN_DEG, s.pitchWarnDeg = PITCH_WARN_DEG, s.warnHidden = WARN_HIDDEN;
  memcpy(s.order, o.order, sizeof o.order);
  s.order[9] = 9;
  return s;
}

// Layout 9 had the same pages and a shorter order array, so it copies straight across.
Settings fromV9(const SettingsV9 &o) {
  Settings s = settings_defaults();
  s.backlight = o.backlight;
  s.boostWarnPsi = o.boostWarnPsi, s.coolantWarnF = o.coolantWarnF, s.oilWarnF = o.oilWarnF;
  s.intakeWarnF = o.intakeWarnF, s.voltsLowWarn = o.voltsLowWarn, s.voltsHighWarn = o.voltsHighWarn;
  s.autoRotate = o.autoRotate, s.autoRotateSecs = o.autoRotateSecs;
  s.pageMask = o.pageMask;
  s.startPage = o.startPage;
  s.speedWarnMph = o.speedWarnMph, s.rpmWarn = o.rpmWarn, s.fuelWarnPct = o.fuelWarnPct;
  s.rollWarnDeg = o.rollWarnDeg, s.pitchWarnDeg = o.pitchWarnDeg, s.warnHidden = o.warnHidden;
  memcpy(s.order, o.order, sizeof o.order);
  return s;
}

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
  s.fuelWarnPct = clampf(s.fuelWarnPct, 5, 50);
  s.rollWarnDeg = clampf(s.rollWarnDeg, 5, 45);
  s.pitchWarnDeg = clampf(s.pitchWarnDeg, 5, 45);
  s.speedWarnMph = clampf(s.speedWarnMph, 20, 160);
  s.rpmWarn = clampf(s.rpmWarn, 3000, 7000);
  bool seen[GAUGE_PAGES] = {};
  bool perm = true;
  for (int i = 0; i < GAUGE_PAGES; i++) {
    const uint8_t o = s.order[i];
    if (o >= GAUGE_PAGES || seen[o]) perm = false;
    else seen[o] = true;
  }
  if (!perm) memcpy(s.order, settings_defaults().order, sizeof s.order);
  s.alertMask &= (1u << GAUGE_PAGES) - 1;
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
  s.fuelWarnPct = FUEL_WARN_PCT;
  s.rollWarnDeg = ROLL_WARN_DEG;
  s.pitchWarnDeg = PITCH_WARN_DEG;
  s.warnHidden = WARN_HIDDEN;
  s.alertMask = ALERT_MASK;
  s.stayOnWarning = STAY_ON_WARNING;
  // Alphabetical by the gauge's name: battery, boost, clinometer, coolant, engine
  // load, fuel, intake air, oil temperature, speed, tach (the numbers are the
  // pages in ui.cpp).
  static const uint8_t alphabetical[GAUGE_PAGES] = {5, 0, 9, 2, 1, 8, 4, 3, 6, 7};
  memset(s.order, 0, sizeof s.order);
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
  uint8_t raw[sizeof(Stored) + sizeof(StoredV10) + sizeof(StoredV9) + sizeof(StoredV8) + sizeof(StoredV7)] = {};   // roomy: getBytes refuses a short buffer
  const size_t n = prefs.getBytes("cfg", raw, sizeof raw);
  prefs.end();
  uint16_t layout = 0;
  if (n >= sizeof layout) memcpy(&layout, raw, sizeof layout);

  if (n == sizeof(Stored) && layout == LAYOUT) {
    Stored st;
    memcpy(&st, raw, sizeof st);
    live = st.s;
    sanitize(live);
    Serial.println("[CFG] loaded saved settings");
    return;
  }
  if (n == sizeof(StoredV10) && layout == 10) {
    StoredV10 old;
    memcpy(&old, raw, sizeof old);
    live = fromV10(old.s);
    sanitize(live);
    Serial.println("[CFG] carried over layout 10 settings");
    return;
  }
  if (n == sizeof(StoredV9) && layout == 9) {
    StoredV9 old;
    memcpy(&old, raw, sizeof old);
    live = fromV9(old.s);
    sanitize(live);
    Serial.println("[CFG] carried over layout 9 settings");
    return;
  }
  if (n == sizeof(StoredV8) && layout == 8) {
    StoredV8 old;
    memcpy(&old, raw, sizeof old);
    live = fromV9(v9FromV8(old.s));
    sanitize(live);
    Serial.println("[CFG] carried over layout 8 settings");
    return;
  }
  // Older blobs end where the fields added since begin.
  StoredV7 old{7, v7From(live)};
  const size_t noLimitsBytes = offsetof(StoredV7, s) + offsetof(SettingsV7, speedWarnMph);
  const size_t noOrderBytes = offsetof(StoredV7, s) + offsetof(SettingsV7, order);
  if ((layout == 7 && n == sizeof old) || (layout == 6 && n == noOrderBytes) ||
      ((layout == 5 || layout == 4) && n == noLimitsBytes)) {
    memcpy(&old, raw, n);
    live = fromV9(v9FromV8(v8FromV7(old.s, layout)));
    sanitize(live);
    Serial.printf("[CFG] carried over layout %u settings\n", layout);
  } else if (n) {
    Serial.printf("[CFG] saved settings are layout %u, wanted %u; using defaults\n", layout, LAYOUT);
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
