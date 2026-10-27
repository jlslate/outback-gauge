#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "touch.h"

// Turns a stream of touch reports into tap / double-tap / long-press / tap-and-hold / vertical swipe events.
// Hardware-free so it can be exercised on a desktop.
class GestureDetector {
 public:
  static constexpr int TAP_SLOP = 30;          // px a tap or long press may wander
  static constexpr int SWIPE_MIN = 60;         // px of vertical travel for a swipe up or down
  static constexpr uint32_t TAP_MAX_MS = 700;
  static constexpr uint32_t LONG_MS = 900;
  static constexpr uint32_t TAP_HOLD_MS = 600;  // a second press, soon after a tap, held this long
  static constexpr uint32_t DOUBLE_MS = 500;    // second tap this soon after the first is a double tap
  static constexpr uint32_t LIFT_GAP_MS = 150;  // no reports for this long counts as lifted

  // report: a point report was read this poll; down: finger on the glass
  // (false on the controller's lift report).
  TouchEvent step(bool report, bool down, int x, int y, uint32_t now) {
    if (report && down) {
      if (!held_) {
        held_ = true;
        longFired_ = false;
        secondPress_ = tapAt_ && now - tapAt_ <= DOUBLE_MS;   // this press follows a tap
        startX_ = x, startY_ = y, startAt_ = now;
      }
      lastX_ = x, lastY_ = y, lastSeen_ = now;
    }
    if (!held_) return TouchEvent::None;

    // Tap, then press again and keep holding: reported before the ordinary long
    // press could be, and it takes this press out of being a double tap.
    if (!longFired_ && secondPress_ && now - startAt_ >= TAP_HOLD_MS && withinSlop() && now - lastSeen_ < LIFT_GAP_MS) {
      longFired_ = true;
      tapAt_ = 0;
      return TouchEvent::TapHold;
    }
    if (!longFired_ && now - startAt_ >= LONG_MS && withinSlop() && now - lastSeen_ < LIFT_GAP_MS) {
      longFired_ = true;
      return TouchEvent::LongPress;
    }

    const bool lifted = (report && !down) || now - lastSeen_ >= LIFT_GAP_MS;
    if (!lifted) return TouchEvent::None;
    held_ = false;
    if (longFired_) return TouchEvent::None;

    const int dx = lastX_ - startX_, dy = lastY_ - startY_;
    if (abs(dy) >= SWIPE_MIN && abs(dy) > abs(dx) * 3 / 2) return dy < 0 ? TouchEvent::SwipeUp : TouchEvent::SwipeDown;
    if (!withinSlop() || lastSeen_ - startAt_ >= TAP_MAX_MS) return TouchEvent::None;
    // The first tap is reported straight away, so the second one is reported
    // as a double tap instead of a second Tap; the caller undoes the first.
    if (tapAt_ && now - tapAt_ <= DOUBLE_MS) {
      tapAt_ = 0;
      return TouchEvent::DoubleTap;
    }
    tapAt_ = now ? now : 1;
    return TouchEvent::Tap;
  }

 private:
  bool withinSlop() const { return abs(lastX_ - startX_) < TAP_SLOP && abs(lastY_ - startY_) < TAP_SLOP; }

  bool held_ = false, longFired_ = false, secondPress_ = false;
  int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
  uint32_t startAt_ = 0, lastSeen_ = 0, tapAt_ = 0;
};
