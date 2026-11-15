#pragma once

// "<git revision> <build date and time>", written at build time by tools/ota/version.py.
extern const char *const FW_VERSION;

// Why the chip last started: power on, software restart, crash, watchdog, power dip.
const char *fw_lastRestart();
