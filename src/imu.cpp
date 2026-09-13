#include "imu.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <math.h>

#include "config.h"

namespace {

constexpr uint8_t REG_WHO_AM_I = 0x00;
constexpr uint8_t REG_CTRL1 = 0x02;
constexpr uint8_t REG_CTRL2 = 0x03;
constexpr uint8_t REG_CTRL7 = 0x08;
constexpr uint8_t REG_AX_L = 0x35;
constexpr uint8_t WHO_AM_I_QMI8658 = 0x05;

int addr = -1;
float fx = 0, fy = 0, fz = 0;  // low-passed raw accel counts
bool primed = false;
float refRoll = 0, refPitch = 0;
bool calibrated = false;

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)addr, n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// Roll is rotation within the screen plane and pitch is tilt toward or away
// from the screen. Both are differences against the calibrated reference, so
// how the chip's x/y axes are rotated on the board doesn't matter.
float rawRoll() { return atan2f(fx, fy) * RAD_TO_DEG; }
float rawPitch() { return atan2f(fz, sqrtf(fx * fx + fy * fy)) * RAD_TO_DEG; }

float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

}  // namespace

bool imu_init() {
  static const uint8_t candidates[] = {0x6B, 0x6A};
  for (uint8_t a : candidates) {
    addr = a;
    uint8_t who = 0;
    if (readRegs(REG_WHO_AM_I, &who, 1) && who == WHO_AM_I_QMI8658) break;
    addr = -1;
  }
  if (addr < 0) {
    Serial.println("[IMU] QMI8658 not found");
    return false;
  }

  writeReg(REG_CTRL1, 0x40);  // register address auto-increment
  writeReg(REG_CTRL2, 0x16);  // accel +/-4 g, ~117 Hz
  writeReg(REG_CTRL7, 0x01);  // accel on, gyro off

  Preferences prefs;
  prefs.begin("gauge", true);
  calibrated = prefs.getBool("tiltCal", false);
  refRoll = prefs.getFloat("refRoll", 0);
  refPitch = prefs.getFloat("refPitch", 0);
  prefs.end();

  Serial.printf("[IMU] QMI8658 at 0x%02X, calibrated=%d\n", addr, calibrated);
  return true;
}

void imu_update() {
  if (addr < 0) return;
  uint8_t b[6];
  if (!readRegs(REG_AX_L, b, sizeof b)) return;
  const float ax = (int16_t)(b[1] << 8 | b[0]);
  const float ay = (int16_t)(b[3] << 8 | b[2]);
  const float az = (int16_t)(b[5] << 8 | b[4]);
  if (!primed) {
    fx = ax, fy = ay, fz = az;
    primed = true;
    return;
  }
  constexpr float k = 0.1f;  // smooths road vibration at 50 Hz
  fx += k * (ax - fx);
  fy += k * (ay - fy);
  fz += k * (az - fz);
}

Tilt imu_tilt() {
  Tilt t;
  t.ok = addr >= 0 && primed;
  t.calibrated = calibrated;
  if (!t.ok) return t;
  t.roll = wrap180(rawRoll() - refRoll) * TILT_ROLL_SIGN;
  t.pitch = (rawPitch() - refPitch) * TILT_PITCH_SIGN;
  return t;
}

void imu_calibrate() {
  if (addr < 0 || !primed) return;
  refRoll = rawRoll();
  refPitch = rawPitch();
  calibrated = true;

  Preferences prefs;
  prefs.begin("gauge", false);
  prefs.putBool("tiltCal", true);
  prefs.putFloat("refRoll", refRoll);
  prefs.putFloat("refPitch", refPitch);
  prefs.end();
  Serial.printf("[IMU] level set: roll %.1f, pitch %.1f\n", refRoll, refPitch);
}
