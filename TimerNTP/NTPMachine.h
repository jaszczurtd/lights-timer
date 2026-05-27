#ifndef NTP_C
#define NTP_C

#pragma once

#include "Config.h"

#include <hal/hal.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <string.h>
#include <stdint.h>

#include "MQTTClient.h"
#include "MyHardware.h"

#define NTP_BUFFER 128
enum {
  STATE_NOT_CONNECTED = 0, 
  STATE_CONNECTING, 
  STATE_NTP_SYNCHRO, 
  STATE_WIREGUARD_CONNECT,
  STATE_WIREGUARD_CONNECTED,
  STATE_CONNECTED
};

#define MAX_TIME (24 * 60)

class MQTTClient;
class MyHardware;
class Logic;

class NTPMachine {
public:
  explicit NTPMachine(Logic& l) : logic(l) {}
  void start();
  int getCurrentState(void);
  void stateMachine(void);
  const char *getTimeFormatted(void);
  long getTimeNow(void);
  void evaluateTimeCondition();
  bool isBrokerAvailable(void);
  unsigned long lastBrokerRespoinsePingTime(void);
  bool wasWatchdogResetOnBoot() const;
  int getLastStateBeforeReset() const;
  uint32_t getLastUptimeBeforeResetMs() const;
  uint32_t getWdtBootCount() const;
  const char* getStateName(int state) const;

private:
  Logic& logic;

  MyHardware& hardware();
  MQTTClient& mqtt();

  void reconnect(void);
  void saveResetBreadcrumb(void);

  int currentState;
  char buffer[NTP_BUFFER];
  long now_time;
  bool localTimeHasBeenSet = false;
  bool wgStarted = false;
  bool watchdogResetOnBoot = false;
  int lastStateBeforeReset = -1;
  uint32_t lastUptimeBeforeResetMs = 0;
  uint32_t wdtBootCount = 0;

  bool isBAvailable = false;
  unsigned long dt1 = 0;
  int failedPingsCNT = 0;

  SmartTimers wifiTimeoutTimer;
  SmartTimers connectingPollTimer;
  SmartTimers ntpTimeoutTimer;
  SmartTimers wgHandshakeTimer;
  SmartTimers pingTimer;
  SmartTimers ntpReSyncTimer;
  SmartTimers evaluateRelayTimer;
  SmartTimers loopLogTimer;
};


#endif
