#include "display.h"

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "board.h"
#include "config.h"

namespace {

// Pin order and panel setup match Arduino_GFX's own WAVESHARE_ESP32_S3_LCD_1_46
// device definition. The SPD2010 wants x-aligned partial writes, so LVGL
// renders into a full-frame canvas and the whole frame is pushed at once.
Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
Arduino_GFX *panel = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);  // reset is on the TCA9554
Arduino_Canvas *canvas = new Arduino_Canvas(LCD_SIZE, LCD_SIZE, panel, 0, 0, 0);

constexpr int BUF_LINES = 40;
lv_disp_draw_buf_t drawBuf;
lv_disp_drv_t drv;

void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px) {
  uint16_t *fb = canvas->getFramebuffer();
  const int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; y++) {
    memcpy(fb + y * LCD_SIZE + a->x1, px, w * sizeof(uint16_t));
    px += w;
  }
  if (lv_disp_flush_is_last(d)) canvas->flush();
  lv_disp_flush_ready(d);
}

}  // namespace

void display_init() {
  if (!canvas->begin()) {
    Serial.println("[LCD] canvas allocation failed");
    return;
  }
  canvas->fillScreen(0x0000);
  canvas->flush();

  lv_init();
  auto *buf = (lv_color_t *)heap_caps_malloc(LCD_SIZE * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  lv_disp_draw_buf_init(&drawBuf, buf, nullptr, LCD_SIZE * BUF_LINES);

  lv_disp_drv_init(&drv);
  drv.hor_res = LCD_SIZE;
  drv.ver_res = LCD_SIZE;
  drv.flush_cb = flush;
  drv.draw_buf = &drawBuf;
#if DISPLAY_ROTATE_180
  drv.sw_rotate = 1;
  drv.rotated = LV_DISP_ROT_180;
#endif
  lv_disp_drv_register(&drv);
}

void display_loop() {
  lv_timer_handler();
}

void display_refreshNow() {
  lv_refr_now(nullptr);
}
