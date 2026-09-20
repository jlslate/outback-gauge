#pragma once
#include <stdint.h>
#include <stddef.h>
// Always empty, so the preview renders a gauge that has never been configured.
struct Preferences {
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  uint8_t getUChar(const char*, uint8_t d) { return d; }
  void putUChar(const char*, uint8_t) {}
  size_t getBytes(const char*, void*, size_t) { return 0; }
  size_t putBytes(const char*, const void*, size_t n) { return n; }
};
