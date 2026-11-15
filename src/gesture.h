#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "touch.h"

// Turns a stream of touch reports into tap / double-tap / hold / long-press / vertical swipe events.
// Hardware-free so it can be exercised on a desktop.
class GestureDetector {
 public:
  static constexpr int TAP_SLOP = 30;          // px a tap or hold may wander
  static constexpr int SWIPE_MIN = 60;         // px of vertical travel for a swipe up or down
  static constexpr uint32_t TAP_MAX_MS = 700;
  static constexpr uint32_t HOLD_MS = 600;      // held this long and then let go: reset a peak or low
  static constexpr uint32_t LONG_MS = 2000;     // held this long: Wi-Fi page (reported while still held)
  static constexpr uint32_t DOUBLE_MS = 600;    // a second tap starting this soon after the first lifted is a double tap
  static constexpr uint32_t LIFT_GAP_MS = 150;  // no reports for this long counts as lifted

  // report: a point report was read this poll; down: finger on the glass
  // (false on the controller's lift report).
  TouchEvent step(bool report, bool down, int x, int y, uint32_t now) {
    if (report && down) {
      if (!held_) {
        held_ = true;
        longFired_ = false;
        secondPress_ = tapAt_ && now - tapAt_ <= DOUBLE_MS;   // this press follows a tap closely
        startX_ = x, startY_ = y, startAt_ = now;
      }
      lastX_ = x, lastY_ = y, lastSeen_ = now;
    }
    if (!held_) return TouchEvent::None;

    // A long press is reported while the finger is still down. A hold is only
    // reported when it ends, so reaching the long press doesn't reset anything on the way.
    const bool still = withinSlop() && now - lastSeen_ < LIFT_GAP_MS;
    if (!longFired_ && still && now - startAt_ >= LONG_MS) {
      longFired_ = true;
      return TouchEvent::LongPress;
    }

    const bool lifted = (report && !down) || now - lastSeen_ >= LIFT_GAP_MS;
    if (!lifted) return TouchEvent::None;
    held_ = false;
    if (longFired_) return TouchEvent::None;
    if (withinSlop() && lastSeen_ - startAt_ >= HOLD_MS) return TouchEvent::Hold;

    const int dx = lastX_ - startX_, dy = lastY_ - startY_;
    if (abs(dy) >= SWIPE_MIN && abs(dy) > abs(dx) * 3 / 2) return dy < 0 ? TouchEvent::SwipeUp : TouchEvent::SwipeDown;
    if (!withinSlop() || lastSeen_ - startAt_ >= TAP_MAX_MS) return TouchEvent::None;
    // The first tap is reported straight away, so the second one is reported
    // as a double tap instead of a second Tap; the caller undoes the first.
    // Timed from the first finger lifting to the second one touching down, not
    // to when the lift was noticed, which can be a poll or several later.
    if (secondPress_) {
      tapAt_ = 0;
      return TouchEvent::DoubleTap;
    }
    tapAt_ = lastSeen_ ? lastSeen_ : 1;
    return TouchEvent::Tap;
  }

 private:
  bool withinSlop() const { return abs(lastX_ - startX_) < TAP_SLOP && abs(lastY_ - startY_) < TAP_SLOP; }

  bool held_ = false, longFired_ = false, secondPress_ = false;
  int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
  uint32_t startAt_ = 0, lastSeen_ = 0, tapAt_ = 0;
};
