#include "ui.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "board.h"
#include "config.h"
#include "settings.h"
#include "webconfig.h"

// Digits only, generated at a size the bundled fonts stop short of; see
// tools/gen_big_font.py.
extern "C" const lv_font_t font_big;

namespace {

constexpr lv_coord_t C = LCD_SIZE / 2;  // screen center
constexpr uint32_t WHITE = 0xFFFFFF, GREY = 0x9E9E9E, RED = 0xE53935, AMBER = 0xFB8C00, BLUE = 0x1E88E5;
constexpr uint32_t TEXT_GREEN = 0x4CAF50, TEXT_ORANGE = 0xFF9800, TEXT_BLUE = 0x42A5F5, TEXT_PINK = 0xFF4FA3,
                   TEXT_YELLOW = 0xFFEB3B, TEXT_PURPLE = 0xBA68C8;
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
  uint32_t color;  // the readout's colour; red is kept for a warning
  int8_t track;    // +1: a thin needle holds the highest reading; -1: the lowest; 0: none
};

const GaugeCfg BOOST = {"BOOST", "psi", "%.1f", -15, 20, 1, 36, 5, {{0, 20, RED, &Settings::boostWarnPsi}, {}}, Focus::Boost, TEXT_ORANGE, 1};
const GaugeCfg LOAD = {"ENGINE\nLOAD", "%", "%.0f", 0, 100, 1, 11, 2, {{}, {}}, Focus::Load, TEXT_PURPLE, 0};
const GaugeCfg COOLANT = {"COOLANT", DEG "F", "%.0f", 100, 260, 1, 17, 2, {{100, 140, BLUE}, {0, 260, RED, &Settings::coolantWarnF}}, Focus::Coolant, TEXT_BLUE, 0};
const GaugeCfg OIL = {"OIL\nTEMP", DEG "F", "%.0f", 100, 300, 1, 21, 5, {{100, 140, BLUE}, {0, 300, RED, &Settings::oilWarnF}}, Focus::Oil, TEXT_PINK, 1};
const GaugeCfg INTAKE = {"INTAKE\nAIR", DEG "F", "%.0f", 0, 200, 1, 21, 5, {{0, 200, RED, &Settings::intakeWarnF}, {}}, Focus::Intake, TEXT_YELLOW, 1};
const GaugeCfg VOLTS = {"BATTERY", "V", "%.1f", 10, 16, 10, 13, 2, {{10, 0, AMBER, &Settings::voltsLowWarn, false}, {0, 16, RED, &Settings::voltsHighWarn}}, Focus::Volts, TEXT_GREEN, -1};

struct Gauge {
  const GaugeCfg *cfg = nullptr;
  lv_obj_t *screen = nullptr, *meter = nullptr, *value = nullptr, *status = nullptr;
  lv_meter_indicator_t *needle = nullptr, *peakNeedle = nullptr;   // peakNeedle: the highest (or lowest) reading, where cfg->track asks for one
  lv_meter_indicator_t *arcs[2] = {nullptr, nullptr};
  int32_t needleAt = INT32_MIN, peakAt = INT32_MIN;
};

struct SettingsPage {
  lv_obj_t *screen, *caption, *headline, *caption2, *detail, *url, *foot;
};

enum : uint8_t { PAGE_BOOST, PAGE_LOAD, PAGE_COOLANT, PAGE_OIL, PAGE_INTAKE, PAGE_VOLTS, PAGE_COUNT, PAGE_SETTINGS = PAGE_COUNT };

Gauge gauges[PAGE_COUNT];
SettingsPage settingsPage;
uint8_t page = PAGE_BOOST;
uint32_t shownAt = 0;  // when the current page came up, for auto-rotate
uint8_t cameFrom = PAGE_BOOST;    // the page before the last tap, for undoing it on a double tap
uint8_t returnPage = PAGE_BOOST;  // where the settings page goes back to
float extreme[PAGE_COUNT];   // highest or lowest reading since the last reset, per gauge that tracks one; NAN until there is one
uint32_t toastUntil = 0;
const char *toastText = "";
uint32_t toastColor = GREY;

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
    case ObdState::Scanning: return "Searching for adapter";
    case ObdState::Connecting: snprintf(buf, n, "Connecting to %s", t.adapter); return buf;
    case ObdState::Initializing: return "Starting adapter";
    case ObdState::NoEcu: return "Adapter ready: start car";
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
  lv_obj_t *title = label(g.screen, &lv_font_montserrat_36, cfg.color, 0, -72);
  lv_obj_set_style_text_line_space(title, -4, 0);
  lv_label_set_text(title, cfg.title);

  g.meter = lv_meter_create(g.screen);
  lv_obj_set_size(g.meter, LCD_SIZE - 12, LCD_SIZE - 12);
  lv_obj_center(g.meter);
  lv_obj_set_style_bg_opa(g.meter, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g.meter, 0, 0);
  lv_obj_set_style_pad_all(g.meter, 10, 0);
  lv_obj_set_style_text_color(g.meter, lv_color_white(), LV_PART_TICKS);
  lv_obj_set_style_text_font(g.meter, &lv_font_montserrat_24, LV_PART_TICKS);
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

  // A thin white needle at the highest (or lowest) reading since the last
  // reset; added first so the live needle is drawn over it.
  if (cfg.track) {
    g.peakNeedle = lv_meter_add_needle_line(g.meter, scale, 3, lv_color_white(), -24);
    lv_meter_set_indicator_value(g.meter, g.peakNeedle, cfg.min * cfg.mul);
  }
  g.needle = lv_meter_add_needle_line(g.meter, scale, 6, lv_color_hex(0xFF6D00), -24);
  lv_meter_set_indicator_value(g.meter, g.needle, cfg.min * cfg.mul);
  if (cfg.mul > 1) lv_obj_add_event_cb(g.meter, scaledTickLabels, LV_EVENT_DRAW_PART_BEGIN, (void *)&cfg);

  g.value = label(g.screen, &font_big, WHITE, 0, 94);
  lv_label_set_text(label(g.screen, &lv_font_montserrat_24, GREY, 0, 140), cfg.unit);
  g.status = label(g.screen, &lv_font_montserrat_18, GREY, 0, 165);
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
  setColor(g.value, isFresh && warn ? RED : g.cfg->color);
  const bool toast = millis() < toastUntil;
  setText(g.status, toast ? toastText : status);
  setColor(g.status, toast ? toastColor : GREY);
}

// ---- settings page ---------------------------------------------------------

void makeSettings() {
  SettingsPage &p = settingsPage;
  p.screen = newScreen();
  lv_label_set_text(label(p.screen, &lv_font_montserrat_24, GREY, 0, -120), "SETTINGS");
  p.caption = label(p.screen, &lv_font_montserrat_18, GREY, 0, -74);
  p.headline = label(p.screen, &lv_font_montserrat_32, WHITE, 0, -40);
  p.caption2 = label(p.screen, &lv_font_montserrat_18, GREY, 0, 4);
  p.detail = label(p.screen, &lv_font_montserrat_32, WHITE, 0, 36);
  p.url = label(p.screen, &lv_font_montserrat_24, BLUE, 0, 82);
  p.foot = label(p.screen, &lv_font_montserrat_18, GREY, 0, 122);
}

void updateSettings() {
  SettingsPage &p = settingsPage;
  if (!webconfig_active()) {
    setText(p.caption, "");
    setText(p.headline, "Wi-Fi off");
    setText(p.caption2, "");
    setText(p.detail, "");
    setText(p.url, "");
    setText(p.foot, "Wi-Fi starting...");
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
  if (p == PAGE_SETTINGS) return settingsPage.screen;
  return gauges[p].screen;
}

bool shown(uint8_t p) {
  return p < PAGE_COUNT && (settings().pageMask >> p & 1);
}

// The first page at or after `from` that is switched on, going round; PAGE_BOOST
// if the mask somehow has none.
uint8_t visibleFrom(uint8_t from) {
  for (uint8_t i = 0; i < PAGE_COUNT; i++)
    if (shown((from + i) % PAGE_COUNT)) return (from + i) % PAGE_COUNT;
  return PAGE_BOOST;
}

// `remember` is for a page the person chose by tapping. The rotation and a
// warning's jump don't count: that would write flash every few seconds, and
// leave the gauge starting on whatever last warned.
void showPage(uint8_t p, bool remember = false) {
  page = p;
  shownAt = millis();
  lv_scr_load(screenFor(p));
  obd_setFocus(p < PAGE_COUNT ? gauges[p].cfg->focus : Focus::None);

  if (!remember || p == PAGE_SETTINGS || settings().startPage != START_LAST) return;
  Preferences prefs;
  prefs.begin("gauge", false);
  prefs.putUChar("page", p);
  prefs.end();
}

}  // namespace

void ui_init() {
  for (float &e : extreme) e = NAN;
  gauges[PAGE_BOOST] = makeGauge(BOOST);
  gauges[PAGE_LOAD] = makeGauge(LOAD);
  gauges[PAGE_COOLANT] = makeGauge(COOLANT);
  gauges[PAGE_OIL] = makeGauge(OIL);
  gauges[PAGE_INTAKE] = makeGauge(INTAKE);
  gauges[PAGE_VOLTS] = makeGauge(VOLTS);
  makeSettings();
  for (uint8_t i = 0; i < PAGE_COUNT; i++) addPageDots(gauges[i].screen, i, PAGE_COUNT);

  uint8_t start = settings().startPage;
  if (start == START_LAST) {
    Preferences prefs;
    prefs.begin("gauge", true);
    start = prefs.getUChar("page", PAGE_BOOST);
    prefs.end();
  }
  showPage(visibleFrom(start < PAGE_COUNT ? start : PAGE_BOOST));
}

// Whether a gauge's reading is fresh and past its warning threshold.
static bool warning(uint8_t p, const Telemetry &t, const Settings &s) {
  switch (p) {
    case PAGE_BOOST: return fresh(t.boostAt) && t.boostPsi >= s.boostWarnPsi;
    case PAGE_COOLANT: return fresh(t.coolantAt) && t.coolantF >= s.coolantWarnF;
    case PAGE_OIL: return fresh(t.oilAt) && t.oilF >= s.oilWarnF;
    case PAGE_INTAKE: return fresh(t.intakeAt) && t.intakeF >= s.intakeWarnF;
    case PAGE_VOLTS: return fresh(t.voltsAt) && (t.volts < s.voltsLowWarn || t.volts > s.voltsHighWarn);
  }
  return false;  // engine load has no warning
}

// A gauge that goes into warning comes up on screen, and while any is in
// warning the auto-rotate waits. A warning counts as over only once it has
// stayed clear for ALARM_CLEAR_MS, so a reading hovering at its threshold
// doesn't keep pulling the screen back. Returns whether any warning is live.
static constexpr uint32_t ALARM_CLEAR_MS = 3000;
static constexpr uint32_t STARTUP_GRACE_MS = 20000;
static bool warned[PAGE_COUNT];
static uint32_t clearSince[PAGE_COUNT];

static bool checkAlarms(const Telemetry &t, const Settings &s) {
  bool any = false;
  for (uint8_t i = 0; i < PAGE_COUNT; i++) {
    // A gauge that is switched off stays off, and the first few seconds are
    // skipped: starting the engine drags the battery down and would pull the
    // screen to the battery gauge every time.
    if (shown(i) && millis() > STARTUP_GRACE_MS && warning(i, t, s)) {
      clearSince[i] = 0;
      if (!warned[i]) {
        warned[i] = true;
        if (page != PAGE_SETTINGS && page != i) showPage(i);
      }
    } else if (warned[i]) {
      if (!clearSince[i]) clearSince[i] = millis();
      else if (millis() - clearSince[i] >= ALARM_CLEAR_MS) warned[i] = false;
    }
    any |= warned[i];
  }
  return any;
}

// Keeps each tracking gauge's needle where its extreme is; with none yet it rests at the start of the scale.
static void moveExtremeNeedles() {
  for (uint8_t i = 0; i < PAGE_COUNT; i++) {
    Gauge &g = gauges[i];
    if (!g.cfg->track) continue;
    const float v = isnan(extreme[i]) ? (float)g.cfg->min : constrain(extreme[i], (float)g.cfg->min, (float)g.cfg->max);
    const int32_t at = lroundf(v * g.cfg->mul);
    if (at == g.peakAt) continue;
    lv_meter_set_indicator_value(g.meter, g.peakNeedle, at);
    g.peakAt = at;
  }
}

// A gauge's reading, and whether it is fresh.
static bool reading(uint8_t p, const Telemetry &t, float &v) {
  uint32_t at = 0;
  switch (p) {
    case PAGE_BOOST: v = t.boostPsi, at = t.boostAt; break;
    case PAGE_LOAD: v = t.loadPct, at = t.loadAt; break;
    case PAGE_COOLANT: v = t.coolantF, at = t.coolantAt; break;
    case PAGE_OIL: v = t.oilF, at = t.oilAt; break;
    case PAGE_INTAKE: v = t.intakeF, at = t.intakeAt; break;
    case PAGE_VOLTS: v = t.volts, at = t.voltsAt; break;
  }
  return fresh(at);
}

void ui_update(const Telemetry &t) {
  const Settings &s = settings();
  // Wi-Fi went off by itself (web page button or idle timeout): back to the gauges.
  if (page == PAGE_SETTINGS && !webconfig_active()) showPage(returnPage);
  const bool alarmed = checkAlarms(t, s);
  if (s.autoRotate && !alarmed && page != PAGE_SETTINGS && millis() - shownAt >= (uint32_t)(s.autoRotateSecs * 1000)) showPage(visibleFrom((page + 1) % PAGE_COUNT));
  const bool live = t.state == ObdState::Live || t.state == ObdState::Simulated;
  for (uint8_t i = 0; i < PAGE_COUNT; i++) {
    float v;
    // Battery voltage is read by the adapter itself, so it counts without the engine answering.
    if (!gauges[i].cfg->track || !(live || i == PAGE_VOLTS) || !reading(i, t, v)) continue;
    const int8_t dir = gauges[i].cfg->track;
    if (isnan(extreme[i]) || (dir > 0 ? v > extreme[i] : v < extreme[i])) extreme[i] = v;
  }
  moveExtremeNeedles();

  if (page == PAGE_SETTINGS) {
    updateSettings();
    return;
  }

  char buf[40];
  const char *status = statusFor(t, buf, sizeof buf);
  char held[32];
  const Gauge &g = gauges[page];
  if (g.cfg->track && !isnan(extreme[page]) && (live || page == PAGE_VOLTS)) {
    char num[16];
    snprintf(num, sizeof num, g.cfg->fmt, extreme[page]);
    snprintf(held, sizeof held, "%s %s%s", g.cfg->track > 0 ? "PEAK" : "LOW", num, t.state == ObdState::Simulated ? "  (SIM)" : "");
    status = held;
  }
  float v = 0;
  const bool ok = reading(page, t, v);
  setGauge(gauges[page], ok, v, warning(page, t, s), status);
}

void ui_applySettings() {
  for (uint8_t i = 0; i < PAGE_COUNT; i++) applyBands(gauges[i]);
  if (page != PAGE_SETTINGS && !shown(page)) showPage(visibleFrom(page));   // the page on screen was just switched off
}

void ui_nextPage() {
  if (page == PAGE_SETTINGS) return;
  cameFrom = page;
  showPage(visibleFrom((page + 1) % PAGE_COUNT), true);
}

// Two quick taps: the first already moved on a page, so go back to where it
// started, then flip auto-rotate and say so on the status line for a moment.
void ui_doubleTap() {
  if (page == PAGE_SETTINGS) return;
  showPage(cameFrom);
  Settings s = settings();
  s.autoRotate = !s.autoRotate;
  settings_stage(s);
  toastText = s.autoRotate ? "Auto-rotate on" : "Auto-rotate off";
  toastColor = s.autoRotate ? TEXT_GREEN : RED;
  toastUntil = millis() + 3000;
}

// Swipe up for brighter, down for dimmer, one preset at a time. It goes through
// the same staging as the web page, so the level is saved and the page shows it.
void ui_brightness(int dir) {
  if (page == PAGE_SETTINGS) return;
  static const uint8_t steps[] = {5, 20, 40, 60, 80, 100};
  Settings s = settings();
  int pick = -1;
  for (int i = 0; i < (int)(sizeof steps); i++) {
    if (dir > 0 && steps[i] > s.backlight) { pick = i; break; }
    if (dir < 0 && steps[i] < s.backlight) pick = i;
  }
  if (pick >= 0) {
    s.backlight = steps[pick];
    settings_stage(s);
  }
  static char text[24];
  snprintf(text, sizeof text, "Brightness %u%%", pick >= 0 ? s.backlight : settings().backlight);
  toastText = text;
  toastColor = WHITE;
  toastUntil = millis() + 3000;
}

// A tap, then a press held on a gauge that tracks a peak or low: the first tap
// already moved on, so go back, and start that reading over.
void ui_tapHold() {
  if (page == PAGE_SETTINGS) return;
  showPage(cameFrom);
  const int8_t dir = gauges[page].cfg->track;
  if (!dir) return;
  extreme[page] = NAN;
  toastText = dir > 0 ? "Peak reset" : "Low reset";
  toastColor = WHITE;
  toastUntil = millis() + 3000;
}

// Hold on any gauge opens the Wi-Fi page and starts the access point; hold on
// the Wi-Fi page stops it and goes back.
void ui_longPress() {
  if (page == PAGE_SETTINGS) {
    webconfig_stop();
    showPage(returnPage);
    return;
  }
  webconfig_start();
  if (!webconfig_active()) return;  // the radio didn't come up; stay on the gauge
  returnPage = page;
  showPage(PAGE_SETTINGS);
}
