#pragma once

#include <stdint.h>

class OTAUpdates {
public:
  void configureIfNeeded(const char *hostname);
  void handle(bool wifiConnected, const char *hostname);

private:
  bool active = false;
  uint32_t retryAtMs = 0;
};
