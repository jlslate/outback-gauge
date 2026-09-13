#pragma once

#include <stdint.h>

// SPD2010 touch controller (I2C 0x53) with tap / long-press / swipe detection.

enum class TouchEvent : uint8_t { None, Tap, LongPress, SwipeLeft, SwipeRight };

bool touch_init();
TouchEvent touch_update();  // call every ~20 ms from the same task as the other I2C users
