#pragma once

#include <stdint.h>

// On-demand Wi-Fi access point serving the settings form. Held off until asked
// for: Wi-Fi and BLE share one 2.4 GHz radio, so running the AP while driving
// would cost the OBD link latency for no benefit.

void webconfig_start();
void webconfig_stop();
bool webconfig_active();

uint32_t webconfig_secondsLeft();
const char *webconfig_ssid();
const char *webconfig_password();
const char *webconfig_url();
