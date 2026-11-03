#pragma once
#include <Arduino.h>

struct SensorDiagnostics
{
  uint8_t devicesAtBoot = 0;
  uint8_t index[6] = {};
  bool indexFoundAtBoot[6] = {};
  uint8_t address[6][8] = {};
  bool addressConfigured[6] = {};
};
using SensorDiagnosticsProvider = SensorDiagnostics (*)();
