#pragma once

#include "obd.h"

void ui_init();
void ui_update(const Telemetry &t);
void ui_applySettings();  // thresholds changed: move the warning bands
void ui_nextPage();   // tap or BOOT short press
void ui_longPress();  // hold: open the Wi-Fi page and start it; hold there to stop and go back
