#pragma once
#include <math.h>

inline bool validTemperature(float value)
{
  return isfinite(value) && value >= -55.0f && value <= 125.0f;
}
