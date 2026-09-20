#pragma once

#include "imu.h"
#include "obd.h"

void ui_init();
void ui_update(const Telemetry &t, const Tilt &tilt);
void ui_applySettings();  // thresholds changed: move the warning bands
void ui_nextPage();   // tap, swipe left, or BOOT short press
void ui_prevPage();   // swipe right
void ui_longPress();  // hold: reset peak, set level, or start the settings Wi-Fi
