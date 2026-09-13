#pragma once

#include "imu.h"
#include "obd.h"

void ui_init();
void ui_update(const Telemetry &t, const Tilt &tilt);
void ui_nextPage();   // BOOT short press
void ui_longPress();  // BOOT hold: reset peak on the boost page, set level on the tilt page
