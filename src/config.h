#pragma once

// User-tunable settings.

#define BACKLIGHT_PERCENT 80

// Case-insensitive name fragments that identify the OBD adapter in a BLE scan.
// The strongest-signal match wins.
static const char *const OBD_NAME_HINTS[] = {"obdlink", "obd", "vlink", "vgate", "icar"};

// Set to 1 if the picture is upside down with the board mounted the way you want.
#define DISPLAY_ROTATE_180 0

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
