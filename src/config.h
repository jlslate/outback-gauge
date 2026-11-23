#pragma once

// Defaults for the settings on the Wi-Fi page. Everything below is the value a
// factory-fresh gauge starts with; once you save from the settings page, the
// saved copy in NVS wins and editing this file stops having any effect until
// you hit "Restore defaults". See src/settings.h.

#define BACKLIGHT_PERCENT 80

// Case-insensitive name fragments that identify the OBD adapter in a BLE scan.
// The strongest-signal match wins. Compile-time only.
static const char *const OBD_NAME_HINTS[] = {"obdlink", "obd", "vlink", "vgate", "icar"};

// Bond with the adapter before using it. The OBDLink CX refuses to enable
// notifications on an unencrypted link, so it never answers without this. Turn
// it off for an adapter that works without pairing.
#define OBD_BONDING 1

// How the board is turned in the case, in degrees: 0, 90, 180 or 270. The
// screen is polarized, and so are sunglasses, so the board sits a quarter turn
// clockwise (USB-C port on the viewer's left) and the picture is
// turned to match. This is the mount, not a preference; it stays fixed. If the
// picture is sideways the wrong way, try 270.
#define DISPLAY_MOUNT_DEG 90

// Which gauge the screen comes up on at power-up (0 boost, 1 coolant,
// 2 oil temperature, 3 intake air, 4 battery, 5 speed, 6 rpm, 7 fuel, 8 clinometer, 9 altitude; 255 = the last one you tapped to),
// and which gauges are shown at all (one bit each, in that order; 0x3FF is all ten).
#define START_PAGE 255
#define PAGE_MASK 0x3FF

// Clinometer. The pod is turned this many degrees toward the driver (left-hand
// drive), and the chip's z axis points out of the screen (-1 if it points in).
// Flip TILT_Z_OUT_OF_SCREEN if the roll and pitch come out swapped or backwards.
#define TILT_YAW_DEG 30.0f
#define TILT_Z_OUT_OF_SCREEN 1

// Step through the gauges on a timer. A tap still moves on early.
#define AUTO_ROTATE 1
#define AUTO_ROTATE_SECS 3.0f

// Value turns red at or beyond these.
#define BOOST_WARN_PSI 17.0f
#define COOLANT_WARN_F 230.0f
#define OIL_WARN_F 250.0f
#define INTAKE_WARN_F 140.0f
#define VOLTS_LOW_WARN 12.0f
#define VOLTS_HIGH_WARN 15.2f
#define SPEED_WARN_MPH 85.0f
#define RPM_WARN 6000.0f
#define FUEL_WARN_PCT 10.0f
#define ROLL_WARN_DEG 25.0f
#define PITCH_WARN_DEG 25.0f

// Whether a gauge that is hidden on the settings page still comes up when it warns.
#define WARN_HIDDEN 0

// Which gauges' warnings pull the screen over (one bit each, in the page order above),
// and whether auto-rotate waits on a gauge that has warned or carries on.
#define ALERT_MASK 0x3FF
#define STAY_ON_WARNING 1

// White background with black numbers and needles instead of black with colour.
#ifndef LIGHT_THEME
#define LIGHT_THEME 0
#endif

// Settings access point. Compile-time only: it has to be reachable before you
// can change anything. How long it stays up with no requests before shutting
// the radio back off.
#define WEBCONFIG_SSID "OutbackGauge"
#define WEBCONFIG_MINUTES 10
