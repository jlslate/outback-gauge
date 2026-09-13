// Desktop harness for build.sh: drives ui.cpp with sample telemetry and saves
// each page, masked to the round panel, into one contact sheet.

#include <lvgl.h>
#include "ui.h"
#include "obd.h"
#include "imu.h"
#include <vector>

static uint32_t now_ms = 10000;
extern "C" uint32_t millis() { return now_ms; }
void obd_setFocus(Focus) {}
void imu_calibrate() {}

static const int N = 412;
static uint16_t fb[N * N];
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px) {
  int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; y++) { memcpy(fb + y * N + a->x1, px, w * 2); px += w; }
  lv_disp_flush_ready(d);
}

static std::vector<uint8_t> sheet;
static const int COLS = 3, ROWS = 2, GAP = 24, W = COLS * N + (COLS + 1) * GAP, H = ROWS * N + (ROWS + 1) * GAP;
static void capture(int slot) {
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(nullptr);
  int ox = GAP + (slot % COLS) * (N + GAP), oy = GAP + (slot / COLS) * (N + GAP);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    float dx = x - N / 2 + 0.5f, dy = y - N / 2 + 0.5f;
    uint8_t *p = &sheet[((oy + y) * W + ox + x) * 3];
    if (dx * dx + dy * dy > (N / 2.0f) * (N / 2.0f)) continue;  // round panel
    uint16_t c = fb[y * N + x];
    p[0] = ((c >> 11) & 0x1F) * 255 / 31; p[1] = ((c >> 5) & 0x3F) * 255 / 63; p[2] = (c & 0x1F) * 255 / 31;
  }
}

int main() {
  sheet.assign(W * H * 3, 0);
  for (size_t i = 0; i < sheet.size(); i += 3) { sheet[i] = 38; sheet[i+1] = 40; sheet[i+2] = 44; }
  lv_init();
  static lv_color_t buf[N * 40];
  static lv_disp_draw_buf_t db; lv_disp_draw_buf_init(&db, buf, nullptr, N * 40);
  static lv_disp_drv_t drv; lv_disp_drv_init(&drv);
  drv.hor_res = N; drv.ver_res = N; drv.flush_cb = flush; drv.draw_buf = &db;
  lv_disp_drv_register(&drv);
  ui_init();

  Telemetry t; t.state = ObdState::Live;
  uint32_t s = now_ms - 10;
  t.boostPsi = 14.8f; t.boostAt = s; ui_update(t, Tilt{});   // sets the peak
  t.boostPsi = 12.4f; t.coolantF = 203; t.intakeF = 97; t.volts = 14.2f;
  t.coolantAt = t.intakeAt = t.voltsAt = s;
  Tilt tl; tl.ok = true; tl.calibrated = true; tl.roll = 6; tl.pitch = -3;

  for (int page = 0; page < 5; page++) {
    ui_update(t, tl); capture(page); ui_nextPage();
  }
  // Boost page again while still searching for the adapter.
  Telemetry idle; idle.state = ObdState::Scanning;
  ui_update(idle, tl); capture(5);

  FILE *f = fopen("gauges.ppm", "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H); fwrite(sheet.data(), 1, sheet.size(), f); fclose(f);
  return 0;
}
