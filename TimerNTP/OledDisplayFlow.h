#ifndef OLED_DISPLAY_FLOW_H
#define OLED_DISPLAY_FLOW_H

#pragma once

#include <stdint.h>
#include <time.h>
#include "Config.h"

class OledDisplayFlow {
public:
  explicit OledDisplayFlow(uint32_t activeWindowMs);

  void begin(uint32_t nowMs);
  void setActiveWindowMs(uint32_t activeWindowMs);
  void wake(uint32_t nowMs);
  bool onButtonRelease(uint32_t nowMs);
  void update(uint32_t nowMs, const tm* localTime);

  bool isAwake() const;
  bool consumeForceRedraw();
  bool consumeJustWentToSleep();

private:
  void keepAwakeUntil(uint32_t nowMs);

  uint32_t activeWindowMs = OLED_ACTIVE_WINDOW_MS;
  uint32_t sleepAtMs = 0;
  int lastFullHourKey = -1;

  bool awake = true;
  bool forceRedraw = true;
  bool justWentToSleep = false;
};

#endif
