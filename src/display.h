#pragma once

void display_init();  // SPD2010 panel + LVGL display driver
void display_loop();  // run LVGL timers and redraw; call often from loop()
void display_refreshNow();

// How far the picture is turned on the panel: 0, 90, 180 or 270. Touch has to
// be turned by the same amount, and only takes effect after a reboot.
int display_rotation();
