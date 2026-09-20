#include "ui.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "board.h"
#include "config.h"
#include "settings.h"
#include "webconfig.h"

namespace {

constexpr lv_coord_t C = LCD_SIZE / 2;  // screen center
constexpr uint32_t WHITE = 0xFFFFFF, GREY = 0x9E9E9E, RED = 0xE53935, AMBER = 0xFB8C00, BLUE = 0x1E88E5;
#define DEG "\xC2\xB0"

// A coloured arc on the scale. One endpoint can be driven by a setting, so the
// warning band moves when the threshold is edited; the other stays put.
struct Band {
  float from, to;
  uint32_t color = 0;  // 0 means this gauge has no second band
  float Settings::*live = nullptr;
  bool liveIsFrom = true;
};

struct GaugeCfg {
  const char *title, *unit, *fmt;
  int min, max, mul;      // scale in display units; mul lets the integer-only meter show tenths
  int ticks, majorEvery;  // tick count across the whole scale, label every Nth tick
  Band bands[2];
  Focus focus;
};

const GaugeCfg BOOST = {"BOOST", "psi", "%.1f", -15, 20, 1, 36, 5, {{0, 20, RED, &Settings::boostWarnPsi}, {}}, Focus::Boost};
const GaugeCfg COOLANT = {"COOLANT", DEG "F", "%.0f", 100, 260, 1, 17, 2, {{100, 140, BLUE}, {0, 260, RED, &Settings::coolantWarnF}}, Focus::Coolant};
const GaugeCfg INTAKE = {"INTAKE AIR", DEG "F", "%.0f", 0, 200, 1, 21, 5, {{0, 200, RED, &Settings::intakeWarnF}, {}}, Focus::Intake};
const GaugeCfg VOLTS = {"BATTERY", "V", "%.1f", 10, 16, 10, 13, 2, {{10, 0, AMBER, &Settings::voltsLowWarn, false}, {0, 16, RED, &Settings::voltsHighWarn}}, Focus::Volts};

struct Gauge {
  const GaugeCfg *cfg = nullptr;
  lv_obj_t *screen = nullptr, *meter = nullptr, *value = nullptr, *status = nullptr;
  lv_meter_indicator_t *needle = nullptr;
  lv_meter_indicator_t *arcs[2] = {nullptr, nullptr};
  int32_t needleAt = INT32_MIN;
};

struct TiltPage {
  lv_obj_t *screen, *horizon, *roll, *pitch, *hint;
  lv_point_t pts[2];
  int shownRoll = INT_MIN, shownPitch = INT_MIN;
};

struct SettingsPage {
  lv_obj_t *screen, *caption, *headline, *caption2, *detail, *url, *foot;
};

enum : uint8_t { PAGE_BOOST, PAGE_COOLANT, PAGE_INTAKE, PAGE_VOLTS, PAGE_TILT, PAGE_SETTINGS, PAGE_COUNT };

Gauge gauges[PAGE_TILT];
TiltPage tiltPage;
SettingsPage settingsPage;
uint8_t page = PAGE_BOOST;
float peakBoost = NAN;

// ---- helpers ---------------------------------------------------------------

lv_obj_t *newScreen() {
  lv_obj_t *s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, lv_color_black(), 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, lv_coord_t x, lv_coord_t y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, x, y);
  lv_label_set_text(l, "");
  return l;
}

// Skips the redraw when nothing changed.
void setText(lv_obj_t *l, const char *text) {
  if (strcmp(lv_label_get_text(l), text) != 0) lv_label_set_text(l, text);
}

void setColor(lv_obj_t *l, uint32_t color) {
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
}

// Row of dots along the bottom edge showing which page this is.
void addPageDots(lv_obj_t *screen, uint8_t index, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    lv_obj_t *dot = lv_obj_create(screen);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(i == index ? WHITE : 0x424242), 0);
    lv_obj_align(dot, LV_ALIGN_CENTER, (i - (count - 1) / 2.0f) * 16, 190);
  }
}

bool fresh(uint32_t stamp) {
  return stamp && millis() - stamp < 3000;
}

const char *statusFor(const Telemetry &t, char *buf, size_t n) {
  switch (t.state) {
    case ObdState::Scanning: return "Searching for OBD adapter";
    case ObdState::Connecting: snprintf(buf, n, "Connecting to %s", t.adapter); return buf;
    case ObdState::Initializing: return "Starting adapter";
    case ObdState::NoEcu: return "Adapter ready - start the car";
    case ObdState::Simulated: return "SIMULATED DATA";
    case ObdState::Live: break;
  }
  return "";
}

// ---- round gauges ----------------------------------------------------------

// The meter only knows integers, so scales with mul > 1 relabel their ticks.
// LVGL 8.3 leaves text_length at 0 for meter ticks, so point the label at our
// own buffer instead of writing into theirs; it's drawn before the next tick.
void scaledTickLabels(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  auto *cfg = (const GaugeCfg *)lv_event_get_user_data(e);
  if (dsc->class_p != &lv_meter_class || dsc->type != LV_METER_DRAW_PART_TICK || !dsc->text) return;
  static char buf[8];
  lv_snprintf(buf, sizeof buf, "%d", (int)(dsc->value / cfg->mul));
  dsc->text = buf;
}

// Moves each band's arc to where the current thresholds put it.
void applyBands(Gauge &g) {
  const Settings &s = settings();
  for (int i = 0; i < 2; i++) {
    if (!g.arcs[i]) continue;
    const Band &b = g.cfg->bands[i];
    const float from = b.live && b.liveIsFrom ? s.*(b.live) : b.from;
    const float to = b.live && !b.liveIsFrom ? s.*(b.live) : b.to;
    lv_meter_set_indicator_start_value(g.meter, g.arcs[i], lroundf(from * g.cfg->mul));
    lv_meter_set_indicator_end_value(g.meter, g.arcs[i], lroundf(to * g.cfg->mul));
  }
}

Gauge makeGauge(const GaugeCfg &cfg) {
  Gauge g;
  g.cfg = &cfg;
  g.screen = newScreen();

  // Created before the meter so the needle sweeps over it.
  lv_label_set_text(label(g.screen, &lv_font_montserrat_20, GREY, 0, -72), cfg.title);

  g.meter = lv_meter_create(g.screen);
  lv_obj_set_size(g.meter, LCD_SIZE - 12, LCD_SIZE - 12);
  lv_obj_center(g.meter);
  lv_obj_set_style_bg_opa(g.meter, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g.meter, 0, 0);
  lv_obj_set_style_pad_all(g.meter, 10, 0);
  lv_obj_set_style_text_color(g.meter, lv_color_white(), LV_PART_TICKS);
  lv_obj_set_style_text_font(g.meter, &lv_font_montserrat_20, LV_PART_TICKS);
  lv_obj_set_style_bg_color(g.meter, lv_color_hex(0x424242), LV_PART_INDICATOR);  // needle hub
  lv_obj_set_style_size(g.meter, 24, LV_PART_INDICATOR);

  // 270 degree sweep starting at the lower left; the gap at the bottom holds the readout.
  lv_meter_scale_t *scale = lv_meter_add_scale(g.meter);
  lv_meter_set_scale_ticks(g.meter, scale, cfg.ticks, 2, 12, lv_color_hex(0x757575));
  lv_meter_set_scale_major_ticks(g.meter, scale, cfg.majorEvery, 4, 22, lv_color_white(), 14);
  lv_meter_set_scale_range(g.meter, scale, cfg.min * cfg.mul, cfg.max * cfg.mul, 270, 135);

  for (int i = 0; i < 2; i++) {
    if (!cfg.bands[i].color) continue;
    g.arcs[i] = lv_meter_add_arc(g.meter, scale, 8, lv_color_hex(cfg.bands[i].color), 0);
  }
  applyBands(g);

  g.needle = lv_meter_add_needle_line(g.meter, scale, 6, lv_color_hex(0xFF6D00), -24);
  lv_meter_set_indicator_value(g.meter, g.needle, cfg.min * cfg.mul);
  if (cfg.mul > 1) lv_obj_add_event_cb(g.meter, scaledTickLabels, LV_EVENT_DRAW_PART_BEGIN, (void *)&cfg);

  g.value = label(g.screen, &lv_font_montserrat_48, WHITE, 0, 92);
  lv_label_set_text(label(g.screen, &lv_font_montserrat_20, GREY, 0, 136), cfg.unit);
  g.status = label(g.screen, &lv_font_montserrat_14, GREY, 0, 166);
  return g;
}

void setGauge(Gauge &g, bool isFresh, float v, bool warn, const char *status) {
  if (isFresh) {
    const int32_t at = lroundf(constrain(v, (float)g.cfg->min, (float)g.cfg->max) * g.cfg->mul);
    if (at != g.needleAt) {
      lv_meter_set_indicator_value(g.meter, g.needle, at);
      g.needleAt = at;
    }
    char buf[16];
    snprintf(buf, sizeof buf, g.cfg->fmt, v);
    setText(g.value, buf);
  } else {
    setText(g.value, "--");
  }
  setColor(g.value, isFresh && warn ? RED : WHITE);
  setText(g.status, status);
}

// ---- tilt page -------------------------------------------------------------

void makeTilt() {
  TiltPage &p = tiltPage;
  p.screen = newScreen();

  p.horizon = lv_line_create(p.screen);
  lv_obj_set_size(p.horizon, LCD_SIZE, LCD_SIZE);
  lv_obj_set_pos(p.horizon, 0, 0);
  lv_obj_set_style_line_width(p.horizon, 6, 0);
  lv_obj_set_style_line_color(p.horizon, lv_color_hex(0x29B6F6), 0);
  lv_obj_set_style_line_rounded(p.horizon, true, 0);

  // Fixed "vehicle" marker the horizon moves against.
  static lv_point_t marker[] = {{C - 70, C}, {C - 24, C}, {C - 12, C + 12}, {C, C}, {C + 12, C + 12}, {C + 24, C}, {C + 70, C}};
  lv_obj_t *m = lv_line_create(p.screen);
  lv_obj_set_size(m, LCD_SIZE, LCD_SIZE);
  lv_obj_set_pos(m, 0, 0);
  lv_line_set_points(m, marker, sizeof marker / sizeof marker[0]);
  lv_obj_set_style_line_width(m, 5, 0);
  lv_obj_set_style_line_color(m, lv_color_hex(0xFFB300), 0);
  lv_obj_set_style_line_rounded(m, true, 0);

  lv_label_set_text(label(p.screen, &lv_font_montserrat_14, GREY, -95, 100), "ROLL");
  lv_label_set_text(label(p.screen, &lv_font_montserrat_14, GREY, 95, 100), "PITCH");
  p.roll = label(p.screen, &lv_font_montserrat_28, WHITE, -95, 128);
  p.pitch = label(p.screen, &lv_font_montserrat_28, WHITE, 95, 128);
  p.hint = label(p.screen, &lv_font_montserrat_14, GREY, 0, -120);
}

void updateTilt(const Tilt &t) {
  TiltPage &p = tiltPage;
  if (!t.ok) {
    setText(p.roll, "--");
    setText(p.pitch, "--");
    setText(p.hint, "Motion sensor not found");
    return;
  }
  setText(p.hint, t.calibrated ? "" : "Park level, press and hold to zero");

  const int roll = lroundf(t.roll), pitch = lroundf(t.pitch);
  if (roll == p.shownRoll && pitch == p.shownPitch) return;
  p.shownRoll = roll;
  p.shownPitch = pitch;

  // Attitude-indicator style: the horizon counter-rotates against the body
  // roll and drops as the nose rises.
  const float a = -t.roll * DEG_TO_RAD;
  const float dx = cosf(a), dy = sinf(a);
  const float shift = constrain(t.pitch, -30.0f, 30.0f) * 4.0f;
  const float cx = C - dy * shift, cy = C + dx * shift;
  constexpr float half = 170;
  p.pts[0] = {(lv_coord_t)(cx - dx * half), (lv_coord_t)(cy - dy * half)};
  p.pts[1] = {(lv_coord_t)(cx + dx * half), (lv_coord_t)(cy + dy * half)};
  lv_line_set_points(p.horizon, p.pts, 2);

  const float warn = settings().tiltWarnDeg;
  char buf[12];
  snprintf(buf, sizeof buf, "%+d" DEG, roll);
  setText(p.roll, buf);
  setColor(p.roll, abs(roll) >= warn ? RED : WHITE);
  snprintf(buf, sizeof buf, "%+d" DEG, pitch);
  setText(p.pitch, buf);
  setColor(p.pitch, abs(pitch) >= warn ? RED : WHITE);
}

// ---- settings page ---------------------------------------------------------

void makeSettings() {
  SettingsPage &p = settingsPage;
  p.screen = newScreen();
  lv_label_set_text(label(p.screen, &lv_font_montserrat_20, GREY, 0, -120), "SETTINGS");
  p.caption = label(p.screen, &lv_font_montserrat_14, GREY, 0, -74);
  p.headline = label(p.screen, &lv_font_montserrat_28, WHITE, 0, -40);
  p.caption2 = label(p.screen, &lv_font_montserrat_14, GREY, 0, 4);
  p.detail = label(p.screen, &lv_font_montserrat_28, WHITE, 0, 36);
  p.url = label(p.screen, &lv_font_montserrat_20, BLUE, 0, 82);
  p.foot = label(p.screen, &lv_font_montserrat_14, GREY, 0, 122);
}

void updateSettings() {
  SettingsPage &p = settingsPage;
  if (!webconfig_active()) {
    setText(p.caption, "");
    setText(p.headline, "Wi-Fi off");
    setText(p.caption2, "");
    setText(p.detail, "");
    setText(p.url, "");
    setText(p.foot, "Press and hold to start");
    return;
  }
  const uint32_t left = webconfig_secondsLeft();
  char buf[40];
  snprintf(buf, sizeof buf, "%u:%02u left - hold to stop", (unsigned)(left / 60), (unsigned)(left % 60));
  setText(p.caption, "JOIN THIS NETWORK");
  setText(p.headline, webconfig_ssid());
  setText(p.caption2, "PASSWORD");
  setText(p.detail, webconfig_password());
  setText(p.url, webconfig_url());
  setText(p.foot, buf);
}

// ---- paging ----------------------------------------------------------------

lv_obj_t *screenFor(uint8_t p) {
  if (p == PAGE_TILT) return tiltPage.screen;
  if (p == PAGE_SETTINGS) return settingsPage.screen;
  return gauges[p].screen;
}

void showPage(uint8_t p) {
  page = p;
  lv_scr_load(screenFor(p));
  obd_setFocus(p < PAGE_TILT ? gauges[p].cfg->focus : Focus::None);

  Preferences prefs;
  prefs.begin("gauge", false);
  prefs.putUChar("page", p);
  prefs.end();
}

}  // namespace

void ui_init() {
  gauges[PAGE_BOOST] = makeGauge(BOOST);
  gauges[PAGE_COOLANT] = makeGauge(COOLANT);
  gauges[PAGE_INTAKE] = makeGauge(INTAKE);
  gauges[PAGE_VOLTS] = makeGauge(VOLTS);
  makeTilt();
  makeSettings();
  for (uint8_t i = 0; i < PAGE_TILT; i++) addPageDots(gauges[i].screen, i, PAGE_COUNT);
  addPageDots(tiltPage.screen, PAGE_TILT, PAGE_COUNT);
  addPageDots(settingsPage.screen, PAGE_SETTINGS, PAGE_COUNT);

  Preferences prefs;
  prefs.begin("gauge", true);
  const uint8_t saved = prefs.getUChar("page", PAGE_BOOST);
  prefs.end();
  showPage(saved < PAGE_COUNT ? saved : PAGE_BOOST);
}

void ui_update(const Telemetry &t, const Tilt &tilt) {
  const Settings &s = settings();
  const bool live = t.state == ObdState::Live || t.state == ObdState::Simulated;
  if (live && fresh(t.boostAt) && !(t.boostPsi <= peakBoost)) peakBoost = t.boostPsi;

  char buf[40];
  const char *status = statusFor(t, buf, sizeof buf);

  switch (page) {
    case PAGE_BOOST: {
      char peak[24];
      if (live && !isnan(peakBoost)) {
        snprintf(peak, sizeof peak, t.state == ObdState::Simulated ? "PEAK %.1f  (SIM)" : "PEAK %.1f", peakBoost);
        status = peak;
      }
      setGauge(gauges[PAGE_BOOST], fresh(t.boostAt), t.boostPsi, t.boostPsi >= s.boostWarnPsi, status);
      break;
    }
    case PAGE_COOLANT:
      setGauge(gauges[PAGE_COOLANT], fresh(t.coolantAt), t.coolantF, t.coolantF >= s.coolantWarnF, status);
      break;
    case PAGE_INTAKE:
      setGauge(gauges[PAGE_INTAKE], fresh(t.intakeAt), t.intakeF, t.intakeF >= s.intakeWarnF, status);
      break;
    case PAGE_VOLTS:
      setGauge(gauges[PAGE_VOLTS], fresh(t.voltsAt), t.volts, t.volts < s.voltsLowWarn || t.volts > s.voltsHighWarn, status);
      break;
    case PAGE_TILT:
      updateTilt(tilt);
      break;
    case PAGE_SETTINGS:
      updateSettings();
      break;
  }
}

void ui_applySettings() {
  for (uint8_t i = 0; i < PAGE_TILT; i++) applyBands(gauges[i]);
  tiltPage.shownRoll = INT_MIN;  // re-evaluates the tilt colours at the new threshold
}

void ui_nextPage() {
  showPage((page + 1) % PAGE_COUNT);
}

void ui_prevPage() {
  showPage((page + PAGE_COUNT - 1) % PAGE_COUNT);
}

void ui_longPress() {
  if (page == PAGE_BOOST) peakBoost = NAN;
  if (page == PAGE_TILT) {
    imu_calibrate();
    tiltPage.shownRoll = INT_MIN;  // force a redraw at the new zero
  }
  if (page == PAGE_SETTINGS) {
    if (webconfig_active()) webconfig_stop();
    else webconfig_start();
  }
}
