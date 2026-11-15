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

struct Vec {
  float x, y, z;
};

Vec operator-(Vec a) { return {-a.x, -a.y, -a.z}; }
Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec operator*(float k, Vec a) { return {k * a.x, k * a.y, k * a.z}; }
float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec cross(Vec a, Vec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float len(Vec a) { return sqrtf(dot(a, a)); }
Vec unit(Vec a) { return (1.0f / len(a)) * a; }

int addr = -1;
Vec filt = {0, 0, 0};  // low-passed raw accel counts; at rest it points up
bool primed = false;
volatile bool levelRequested = false;

bool calibrated = false;
Vec fwd = {0, 0, 0}, left = {0, 0, 0}, upAxis = {0, 0, 0};  // the car's forward, left and up axes, in the chip's frame

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

// With the car level, an accelerometer at rest reads straight up. The screen
// faces back and toward the driver by TILT_YAW_DEG, so the screen's normal,
// flattened onto the ground plane, fixes where forward and left are.
bool deriveAxes(Vec up) {
  const Vec normal = {0, 0, TILT_Z_OUT_OF_SCREEN > 0 ? 1.0f : -1.0f};
  const Vec flat = normal + (-dot(normal, up)) * up;
  if (len(flat) < 0.2f) return false;  // lying flat: no way to tell which way the screen faces
  upAxis = up;
  const Vec nh = unit(flat);
  const Vec p = cross(up, nh);
  const float a = TILT_YAW_DEG * DEG_TO_RAD;
  fwd = (-cosf(a)) * nh + (-sinf(a)) * p;
  left = sinf(a) * nh + (-cosf(a)) * p;
  return true;
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
  if (prefs.getBool("tiltCal", false)) {
    const Vec up = {prefs.getFloat("lvlX", 0), prefs.getFloat("lvlY", 0), prefs.getFloat("lvlZ", 0)};
    if (len(up) > 0.5f) calibrated = deriveAxes(unit(up));
  }
  prefs.end();

  Serial.printf("[IMU] QMI8658 at 0x%02X, calibrated=%d\n", addr, calibrated);
  return true;
}

void imu_update() {
  if (addr < 0) return;
  uint8_t b[6];
  if (!readRegs(REG_AX_L, b, sizeof b)) return;
  const Vec a = {(float)(int16_t)(b[1] << 8 | b[0]), (float)(int16_t)(b[3] << 8 | b[2]), (float)(int16_t)(b[5] << 8 | b[4])};
  if (!primed) {
    filt = a;
    primed = true;
    return;
  }
  constexpr float k = 0.1f;  // smooths road vibration at 50 Hz
  filt = filt + k * (a + (-filt));

  if (levelRequested) {
    levelRequested = false;
    const Vec up = unit(filt);
    if (deriveAxes(up)) {
      calibrated = true;
      Preferences prefs;
      prefs.begin("gauge", false);
      prefs.putBool("tiltCal", true);
      prefs.putFloat("lvlX", up.x);
      prefs.putFloat("lvlY", up.y);
      prefs.putFloat("lvlZ", up.z);
      prefs.end();
      Serial.printf("[IMU] level set: up = (%.3f, %.3f, %.3f)\n", up.x, up.y, up.z);
    } else {
      Serial.println("[IMU] level not set: the screen is lying flat");
    }
  }
}

Tilt imu_tilt() {
  Tilt t;
  t.ok = addr >= 0 && primed;
  t.calibrated = calibrated;
  if (!t.ok || !calibrated) return t;
  const Vec up = unit(filt);
  t.pitch = asinf(constrain(dot(up, fwd), -1.0f, 1.0f)) * RAD_TO_DEG;   // forward axis rising = nose up
  // Roll goes the whole way round: how far gravity has swung from the car's up toward its left, so
  // leaning right is positive and upside down reads 180 rather than folding back to 0.
  t.roll = atan2f(dot(up, left), dot(up, upAxis)) * RAD_TO_DEG;
  return t;
}

void imu_requestLevel() {
  levelRequested = true;
}
