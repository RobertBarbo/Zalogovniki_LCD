#include "device_restart.h"
#include <Arduino.h>

namespace
{
bool pending = false;
uint32_t requestedAt = 0;
uint8_t blockers = 0;
constexpr uint32_t RESPONSE_GRACE_MS = 750;
}

bool requestDeviceRestart()
{
  if (blockers)
    return false;
  if (!pending)
  {
    pending = true;
    requestedAt = millis();
    Serial.println("Device restart scheduled.");
  }
  return true;
}

bool deviceRestartPending()
{
  return pending;
}

void setDeviceRestartBlocked(RestartBlock reason, bool blocked)
{
  const uint8_t bit = static_cast<uint8_t>(reason);
  if (blocked)
    blockers |= bit;
  else
    blockers &= ~bit;
}

void handleDeviceRestart()
{
  // Finish HTTP replies and never interrupt an active image/firmware write.
  if (pending && !blockers && static_cast<uint32_t>(millis() - requestedAt) >= RESPONSE_GRACE_MS)
    ESP.restart();
}
