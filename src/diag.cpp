#include <Arduino.h>
#include <esp_system.h>

#include "fw_version.h"

// Why the chip last started, so a crash can be told apart from a page change.
const char *fw_lastRestart() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "power dip";
    default: return "other";
  }
}
