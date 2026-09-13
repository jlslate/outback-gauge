#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "touch.h"

// Turns a stream of touch reports into tap / long-press / swipe events.
// Hardware-free so it can be exercised on a desktop.
class GestureDetector {
 public:
  static constexpr int TAP_SLOP = 30;          // px a tap or long press may wander
  static constexpr int SWIPE_MIN = 60;         // px of horizontal travel for a swipe
  static constexpr uint32_t TAP_MAX_MS = 700;
  static constexpr uint32_t LONG_MS = 900;
  static constexpr uint32_t LIFT_GAP_MS = 150;  // no reports for this long counts as lifted

  // report: a point report was read this poll; down: finger on the glass
  // (false on the controller's lift report).
  TouchEvent step(bool report, bool down, int x, int y, uint32_t now) {
    if (report && down) {
      if (!held_) {
        held_ = true;
        longFired_ = false;
        startX_ = x, startY_ = y, startAt_ = now;
      }
      lastX_ = x, lastY_ = y, lastSeen_ = now;
    }
    if (!held_) return TouchEvent::None;

    if (!longFired_ && now - startAt_ >= LONG_MS && withinSlop() && now - lastSeen_ < LIFT_GAP_MS) {
      longFired_ = true;
      return TouchEvent::LongPress;
    }

    const bool lifted = (report && !down) || now - lastSeen_ >= LIFT_GAP_MS;
    if (!lifted) return TouchEvent::None;
    held_ = false;
    if (longFired_) return TouchEvent::None;

    const int dx = lastX_ - startX_, dy = lastY_ - startY_;
    if (abs(dx) >= SWIPE_MIN && abs(dx) > abs(dy) * 3 / 2) return dx < 0 ? TouchEvent::SwipeLeft : TouchEvent::SwipeRight;
    if (withinSlop() && lastSeen_ - startAt_ < TAP_MAX_MS) return TouchEvent::Tap;
    return TouchEvent::None;
  }

 private:
  bool withinSlop() const { return abs(lastX_ - startX_) < TAP_SLOP && abs(lastY_ - startY_) < TAP_SLOP; }

  bool held_ = false, longFired_ = false;
  int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
  uint32_t startAt_ = 0, lastSeen_ = 0;
};
