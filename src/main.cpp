#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "display.h"
#include "imu.h"
#include "obd.h"
#include "touch.h"
#include "ui.h"

namespace {

void handleTouch() {
  switch (touch_update()) {
    case TouchEvent::Tap:
    case TouchEvent::SwipeLeft: ui_nextPage(); break;
    case TouchEvent::SwipeRight: ui_prevPage(); break;
    case TouchEvent::LongPress: ui_longPress(); break;
    case TouchEvent::None: break;
  }
}

// BOOT button: short press = next page, hold = page action.
void pollButton() {
  static bool down = false, held = false;
  static uint32_t since = 0;
  const bool pressed = digitalRead(PIN_BOOT_BUTTON) == LOW;
  const uint32_t now = millis();
  if (pressed && !down) {
    down = true;
    held = false;
    since = now;
  } else if (pressed && !held && now - since > 900) {
    held = true;
    ui_longPress();
  } else if (!pressed && down) {
    down = false;
    if (!held && now - since > 30) ui_nextPage();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  board_init();
  display_init();
  imu_init();
  touch_init();
  ui_init();
  display_refreshNow();  // first frame is drawn before the backlight comes on
  board_setBacklight(BACKLIGHT_PERCENT);
  obd_start();
}

void loop() {
  static uint32_t lastImu = 0, lastTouch = 0, lastUi = 0, lastBat = 0;
  const uint32_t now = millis();

  pollButton();
  if (now - lastTouch >= 20) {
    lastTouch = now;
    handleTouch();
  }
  if (now - lastImu >= 20) {
    lastImu = now;
    imu_update();
  }
  if (now - lastUi >= 50) {
    lastUi = now;
    ui_update(obd_snapshot(), imu_tilt());
  }
  if (now - lastBat >= 10000) {
    lastBat = now;
    Serial.printf("[BAT] %.2f V\n", board_batteryVolts());
  }

  display_loop();
  delay(2);
}
