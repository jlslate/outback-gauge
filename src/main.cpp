#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "display.h"
#include "imu.h"
#include "obd.h"
#include "settings.h"
#include "touch.h"
#include "ui.h"

namespace {

void handleTouch() {
  switch (touch_update()) {
    case TouchEvent::Tap: ui_nextPage(); break;
    case TouchEvent::DoubleTap: ui_doubleTap(); break;
    case TouchEvent::Hold: ui_hold(); break;
    case TouchEvent::SwipeUp: ui_brightness(+1); break;
    case TouchEvent::SwipeDown: ui_brightness(-1); break;
    case TouchEvent::LongPress: ui_longPress(); break;
    case TouchEvent::None: break;
  }
}

// BOOT button: short press = next page, hold = Wi-Fi page.
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
  settings_load();  // before anything reads a threshold or an orientation flag
  board_init();
  display_init();
  touch_init();
  imu_init();
  ui_init();
  display_refreshNow();  // first frame is drawn before the backlight comes on
  board_setBacklight(settings().backlight);
  obd_start();
}

void loop() {
  static uint32_t lastImu = 0, lastTouch = 0, lastUi = 0, lastBat = 0;
  const uint32_t now = millis();

  pollButton();
  // Settings edited over Wi-Fi land on the web task; adopt them here, where
  // nothing is mid-frame.
  static const bool bootLight = settings().lightTheme;
  static uint32_t restartAt = 0;
  if (settings_apply()) {
    board_setBacklight(settings().backlight);
    ui_applySettings();
    // The theme is baked into the screens when they are built, so it takes a
    // restart; the pause lets the page's reply go out first.
    if (settings().lightTheme != bootLight && !restartAt) restartAt = millis() + 1500;
  }
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
  if (now - lastImu >= 20) {
    lastImu = now;
    imu_update();
  }
  if (now - lastTouch >= 20) {
    lastTouch = now;
    handleTouch();
  }
  if (now - lastUi >= 50) {
    lastUi = now;
    ui_update(obd_snapshot());
  }
  if (now - lastBat >= 10000) {
    lastBat = now;
    Serial.printf("[BAT] %.2f V\n", board_batteryVolts());
  }

  display_loop();
  delay(2);
}
