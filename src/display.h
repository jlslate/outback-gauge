#pragma once

void display_init();  // SPD2010 panel + LVGL display driver
void display_loop();  // run LVGL timers and redraw; call often from loop()
void display_refreshNow();
