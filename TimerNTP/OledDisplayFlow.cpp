#include "OledDisplayFlow.h"

OledDisplayFlow::OledDisplayFlow(uint32_t activeWindowMsIn) : activeWindowMs(activeWindowMsIn) {}

void OledDisplayFlow::begin(uint32_t nowMs) {
  awake = true;
  forceRedraw = true;
  justWentToSleep = false;
  lastFullHourKey = -1;
  keepAwakeUntil(nowMs);
}

void OledDisplayFlow::setActiveWindowMs(uint32_t activeWindowMsIn) {
  if (activeWindowMsIn == 0) {
    activeWindowMs = 1;
    return;
  }
  activeWindowMs = activeWindowMsIn;
}

void OledDisplayFlow::keepAwakeUntil(uint32_t nowMs) {
  sleepAtMs = nowMs + activeWindowMs;
}

void OledDisplayFlow::wake(uint32_t nowMs) {
  awake = true;
  forceRedraw = true;
  justWentToSleep = false;
  keepAwakeUntil(nowMs);
}

bool OledDisplayFlow::onButtonRelease(uint32_t nowMs) {
  if (!awake) {
    wake(nowMs);
    return false;
  }

  keepAwakeUntil(nowMs);
  return true;
}

void OledDisplayFlow::update(uint32_t nowMs, const tm* localTime) {
  if (localTime && localTime->tm_min == 0) {
    const int fullHourKey = (localTime->tm_yday * 24) + localTime->tm_hour;
    if (fullHourKey != lastFullHourKey) {
      lastFullHourKey = fullHourKey;
      wake(nowMs);
    }
  }

  if (awake && (int32_t)(nowMs - sleepAtMs) >= 0) {
    awake = false;
    justWentToSleep = true;
  }
}

bool OledDisplayFlow::isAwake() const {
  return awake;
}

bool OledDisplayFlow::consumeForceRedraw() {
  const bool value = forceRedraw;
  forceRedraw = false;
  return value;
}

bool OledDisplayFlow::consumeJustWentToSleep() {
  const bool value = justWentToSleep;
  justWentToSleep = false;
  return value;
}
