#pragma once

// QMI8658 accelerometer, used for the tilt page. Assumes the screen is mounted
// roughly upright (vent or dash face), not lying flat.

struct Tilt {
  float roll = 0;   // degrees, positive = leaning right
  float pitch = 0;  // degrees, positive = nose up
  bool ok = false;
  bool calibrated = false;
};

bool imu_init();
void imu_update();     // call at ~50 Hz
Tilt imu_tilt();
void imu_calibrate();  // current attitude becomes level; saved across reboots
