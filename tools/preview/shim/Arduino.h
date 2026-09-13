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
#else
uint32_t millis();
#endif
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
