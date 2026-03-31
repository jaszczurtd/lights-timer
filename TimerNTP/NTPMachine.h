#ifndef NTP_C
#define NTP_C

#pragma once

#include "Config.h"

#include <hal/hal.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>
#include "arduino-wireguard-pico-w.h"
#include <SmartTimers.h>

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

private:
  Logic& logic;

  MyHardware& hardware();
  MQTTClient& mqtt();

  WireGuard wg;

  void reconnect(void);

  int currentState;
  char buffer[NTP_BUFFER];
  long now_time;
  bool localTimeHasBeenSet = false;

  IPAddress ping1Target;
  String srv1;
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
