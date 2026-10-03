#pragma once

#include <stdint.h>

// SPD2010 touch controller (I2C 0x53) with tap / long-press detection.

enum class TouchEvent : uint8_t { None, Tap, DoubleTap, LongPress, SwipeUp, SwipeDown };

bool touch_init();
TouchEvent touch_update();  // call every ~20 ms from the same task as the other I2C users
