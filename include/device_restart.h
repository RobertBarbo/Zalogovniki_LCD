#pragma once
#include <stdint.h>

enum class RestartBlock : uint8_t
{
  Ota = 1,
  ImageUpload = 2
};

bool requestDeviceRestart();
bool deviceRestartPending();
void setDeviceRestartBlocked(RestartBlock reason, bool blocked);
void handleDeviceRestart();
