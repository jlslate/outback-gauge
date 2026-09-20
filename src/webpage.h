#pragma once

#include <Arduino.h>

#include "settings.h"

// The settings form as one self-contained page. Split out from the server so
// tools/webpreview can render it on the Mac without an ESP32.
// `note` is an optional banner across the top, or nullptr for none.
String webpage_render(const Settings &s, const char *note);
