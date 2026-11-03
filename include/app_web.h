#pragma once
#include <WebServer.h>
#include "sensor_diagnostics.h"

struct DeviceStatus
{
  float temperatures[6];
  bool sampled;
  uint32_t sampledAt;
  bool mqttConnected;
};
using StatusProvider = DeviceStatus (*)();
using ImageChanged = void (*)();
void setupAppWeb(WebServer &server, StatusProvider status, ImageChanged imageChanged,
                 bool filesystemReady, SensorDiagnosticsProvider sensorDiagnostics);
