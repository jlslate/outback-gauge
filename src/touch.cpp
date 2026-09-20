#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board.h"
#include "config.h"
#include "gesture.h"
#include "settings.h"

// Protocol ported from Espressif's esp_lcd_touch_spd2010 (esp-iot-solution,
// Apache-2.0). The controller boots into a BIOS state and has to be walked
// into point-reporting mode; every poll reads its status word and does
// whatever the current state needs. Registers are 16-bit, sent low byte first,
// as a write of their own followed by a separate read.

namespace {

constexpr uint8_t ADDR = 0x53;
bool present = false;

bool tx(const uint8_t *d, size_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(d, n);
  const bool ok = Wire.endTransmission() == 0;
  delayMicroseconds(200);
  return ok;
}

bool rx(uint8_t *d, size_t n) {
  if (n && Wire.requestFrom(ADDR, n) != n) return false;
  for (size_t i = 0; i < n; i++) d[i] = Wire.read();
  delayMicroseconds(200);
  return true;
}

bool command(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  const uint8_t d[] = {b0, b1, b2, b3};
  return tx(d, sizeof d);
}

bool readReg(uint8_t lo, uint8_t hi, uint8_t *d, size_t n) {
  const uint8_t reg[] = {lo, hi};
  return tx(reg, sizeof reg) && rx(d, n);
}

bool clearInt() { return command(0x02, 0x00, 0x01, 0x00); }
bool cpuStart() { return command(0x04, 0x00, 0x01, 0x00); }
bool pointMode() { return command(0x50, 0x00, 0x00, 0x00); }
bool touchStart() { return command(0x46, 0x00, 0x00, 0x00); }

enum class Report { None, Point, Error };

// One pass of the controller state machine. Returns Point with the first
// finger's position when a report was read; down is false on the lift report.
Report poll(uint16_t &x, uint16_t &y, bool &down) {
  uint8_t s[4];
  if (!readReg(0x20, 0x00, s, sizeof s)) return Report::Error;
  const bool ptExist = s[0] & 0x01, gesture = s[0] & 0x02, aux = s[0] & 0x08;
  const bool inBios = s[1] & 0x40, inCpu = s[1] & 0x20, cpuRun = s[1] & 0x08;
  uint16_t len = s[3] << 8 | s[2];

  if (inBios) {
    clearInt();
    cpuStart();
    return Report::None;
  }
  if (inCpu) {
    pointMode();
    touchStart();
    clearInt();
    return Report::None;
  }
  if (cpuRun && len == 0) {
    clearInt();
    return Report::None;
  }
  if (cpuRun && aux && !ptExist && !gesture) {
    clearInt();
    return Report::None;
  }
  if (!ptExist && !gesture) return Report::None;

  // 4-byte header, then 6 bytes per finger: id, x lo, y lo, x/y high nibbles, weight.
  uint8_t d[4 + 10 * 6];
  len = min<uint16_t>(len, sizeof d);
  if (!readReg(0x00, 0x03, d, len)) return Report::Error;
  Report r = Report::None;
  if (ptExist && len >= 10 && d[4] <= 0x0A) {
    x = ((d[7] & 0xF0) << 4) | d[5];
    y = ((d[7] & 0x0F) << 8) | d[6];
    down = d[8] != 0;
    r = Report::Point;
  }

  // Drain anything left in the packet until the controller says it's done.
  for (int i = 0; i < 4; i++) {
    uint8_t h[8];
    if (!readReg(0xFC, 0x02, h, sizeof h)) return Report::Error;
    if (h[5] == 0x82) {
      clearInt();
      break;
    }
    if (h[5] != 0x00) break;
    uint8_t rest[32];
    readReg(0x00, 0x03, rest, min<uint16_t>(h[2] | h[3] << 8, sizeof rest));
  }
  return r;
}

GestureDetector gestures;

void orient(uint16_t &x, uint16_t &y) {
  const Settings &s = settings();
  if (s.touchSwapXY) {
    const uint16_t t = x;
    x = y;
    y = t;
  }
  // Rotating the picture already flips both axes, so an inversion on top of it
  // cancels out.
  if (s.touchInvertX != s.rotate180) x = LCD_SIZE - 1 - x;
  if (s.touchInvertY != s.rotate180) y = LCD_SIZE - 1 - y;
}

}  // namespace

bool touch_init() {
  Wire.beginTransmission(ADDR);
  present = Wire.endTransmission() == 0;
  if (!present) {
    Serial.println("[TOUCH] SPD2010 touch not found");
    return false;
  }
  uint8_t v[18];
  if (readReg(0x26, 0x00, v, sizeof v))
    Serial.printf("[TOUCH] %.3s%.4s, firmware %u\n", (const char *)&v[14], (const char *)&v[10], v[5] << 8 | v[4]);
  return true;
}

TouchEvent touch_update() {
  if (!present) return TouchEvent::None;
  uint16_t x = 0, y = 0;
  bool down = false;
  const bool report = poll(x, y, down) == Report::Point;
  if (report) orient(x, y);
  return gestures.step(report, down, x, y, millis());
}
