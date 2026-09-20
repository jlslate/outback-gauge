#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifdef __cplusplus
#include <algorithm>
using std::min; using std::max;
extern "C" uint32_t millis();

// settings.cpp logs and guards its staging buffer; neither means anything here.
struct SerialShim {
  void println(const char *) {}
  template <typename... A> void printf(const char *, A...) {}
};
inline SerialShim Serial;
// webpage.cpp builds its markup with Arduino's String.
#include <string>
#define F(x) (x)
struct String : std::string {
  String() {}
  String(const char *s) : std::string(s) {}
  String(int v) : std::string(std::to_string(v)) {}
  String &operator+=(const char *s) { std::string::operator+=(s); return *this; }
  String &operator+=(const String &s) { std::string::operator+=(s); return *this; }
};

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(m) ((void)0)
#define taskEXIT_CRITICAL(m) ((void)0)
#else
uint32_t millis();
#endif
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
