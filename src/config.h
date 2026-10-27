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

// Which gauge the screen comes up on at power-up (0 boost, 1 engine load, 2 coolant,
// 3 oil temperature, 4 intake air, 5 battery; 255 = the last one you tapped to),
// and which gauges are shown at all (one bit each, in that order; 0x3F is all six).
#define START_PAGE 0
#define PAGE_MASK 0x3F

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

// Settings access point. Compile-time only: it has to be reachable before you
// can change anything. How long it stays up with no requests before shutting
// the radio back off.
#define WEBCONFIG_SSID "OutbackGauge"
#define WEBCONFIG_MINUTES 10
