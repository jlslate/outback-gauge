#pragma once

// QMI8658 accelerometer, used for the clinometer page. The pod sits on the dash
// turned toward the driver and tipped up, so "level" is learned once while the
// car is parked on flat ground and roll and pitch are measured along the car's
// own axes from there.

struct Tilt {
  float roll = 0;   // degrees, positive = leaning right
  float pitch = 0;  // degrees, positive = nose up
  bool ok = false;
  bool calibrated = false;
};

bool imu_init();
void imu_update();         // call at ~50 Hz
Tilt imu_tilt();
void imu_requestLevel();   // from any task: the next imu_update takes the current attitude as level and saves it
