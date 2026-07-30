#pragma once

#include <stdint.h>

class OTAUpdates {
public:
  void configureIfNeeded(const char *hostname);
  void handle(bool wifiConnected, bool startupHealthy, const char *hostname);

private:
  void confirmBootIfHealthy(bool startupHealthy);

  bool active = false;
  bool littlefsRecoveryAttempted = false;
  bool bootStateChecked = false;
  uint32_t retryAtMs = 0;
  uint32_t bootConfirmRetryAtMs = 0;
};
