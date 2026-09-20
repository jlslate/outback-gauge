// Bench build for the settings page: the settings store and the web server,
// with no display, touch, IMU or OBD. None of that code touches board-specific
// hardware, so this runs on any ESP32-S3 with Wi-Fi -- a SenseCAP Indicator,
// a bare devkit -- and lets the form be used and debugged before the gauge
// board exists.
//
//   pio run -e webconfig_probe -t upload && pio device monitor
//
// Join the network it prints, open http://192.168.4.1, and every save comes
// back out over serial. The access point is brought straight back up when it
// times out so the board stays reachable on the bench.

#include <Arduino.h>

#include "settings.h"
#include "webconfig.h"

namespace {

void dump() {
  const Settings &s = settings();
  Serial.printf("[PROBE] boost %.1f psi  coolant %.0fF  intake %.0fF  volts %.1f-%.1f  tilt %.0fdeg  backlight %u%%\n",
                s.boostWarnPsi, s.coolantWarnF, s.intakeWarnF, s.voltsLowWarn, s.voltsHighWarn,
                s.tiltWarnDeg, s.backlight);
  Serial.printf("[PROBE] rotate180=%d swapXY=%d invertX=%d invertY=%d rollSign=%d pitchSign=%d\n",
                s.rotate180, s.touchSwapXY, s.touchInvertX, s.touchInvertY, s.rollSign, s.pitchSign);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);  // the native USB CDC port needs a moment before it's listening
  Serial.println("\n[PROBE] settings page bench build");
  settings_load();
  dump();
  webconfig_start();
}

void loop() {
  if (settings_apply()) dump();
  if (!webconfig_active()) {
    Serial.println("[PROBE] window expired; restarting the access point");
    webconfig_start();
  }
  delay(50);
}
