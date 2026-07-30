#ifndef NTP_C
#define NTP_C

#pragma once

#include "Config.h"
#include "watchdog.h"

#include <hal/hal.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <string.h>
#include <stdint.h>

#include "MQTTClient.h"
#include "MyHardware.h"

#define NTP_BUFFER 128
enum NTPState {
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
  int getNTPState(void);
  void stateMachine(void);
  const char *getTimeFormatted(void);
  long getTimeNow(void);
  void evaluateTimeCondition();

private:
  Logic& logic;

  MyHardware& hardware();
  MQTTClient& mqtt();

  void reconnect(void);
  void setNTPState(NTPState state);
  void setWatchdogPhase(WatchdogPhase phase);
  static const char* stateName(int state);

  NTPState currentState = STATE_NOT_CONNECTED;
  char buffer[NTP_BUFFER];
  long now_time;
  bool localTimeHasBeenSet = false;
  bool wgStarted = false;
  Watchdog watchdog;

  SmartTimers wifiTimeoutTimer;
  SmartTimers connectingPollTimer;
  SmartTimers ntpTimeoutTimer;
  SmartTimers wgHandshakeTimer;
  SmartTimers ntpReSyncTimer;
  SmartTimers evaluateRelayTimer;
  SmartTimers loopLogTimer;
};


#endif
