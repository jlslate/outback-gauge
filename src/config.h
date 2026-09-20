#pragma once

// Defaults for the settings on the Wi-Fi page. Everything below is the value a
// factory-fresh gauge starts with; once you save from the settings page, the
// saved copy in NVS wins and editing this file stops having any effect until
// you hit "Restore defaults". See src/settings.h.

#define BACKLIGHT_PERCENT 80

// Case-insensitive name fragments that identify the OBD adapter in a BLE scan.
// The strongest-signal match wins. Compile-time only.
static const char *const OBD_NAME_HINTS[] = {"obdlink", "obd", "vlink", "vgate", "icar"};

// Set to 1 if the picture is upside down with the board mounted the way you want.
#define DISPLAY_ROTATE_180 0

// Touch axes relative to the picture. Only swipe direction depends on these;
// if swiping left goes to the previous page, flip TOUCH_INVERT_X.
#define TOUCH_SWAP_XY 0
#define TOUCH_INVERT_X 0
#define TOUCH_INVERT_Y 0

// Flip these to -1 if the tilt page leans the wrong way.
#define TILT_ROLL_SIGN 1
#define TILT_PITCH_SIGN 1

// Value turns red at or beyond these.
#define BOOST_WARN_PSI 17.0f
#define COOLANT_WARN_F 230.0f
#define INTAKE_WARN_F 140.0f
#define VOLTS_LOW_WARN 12.0f
#define VOLTS_HIGH_WARN 15.2f
#define TILT_WARN_DEG 25.0f

// Settings access point. Compile-time only: it has to be reachable before you
// can change anything. How long it stays up with no requests before shutting
// the radio back off.
#define WEBCONFIG_SSID "OutbackGauge"
#define WEBCONFIG_MINUTES 10
