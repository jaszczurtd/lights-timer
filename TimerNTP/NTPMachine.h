#ifndef NTP_C
#define NTP_C

#pragma once

#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>

#include "MQTTClient.h"
#include "MyHardware.h"

#define WIFI_CONNECTED     (WiFi.status() == WL_CONNECTED)
#define WIFI_TIMEOUT_MS    30000
#define NTP_TIMEOUT_MS     30000
#define PRINT_INTERVAL_MS  10000
#define HOURS_SYNC_INTERVAL 12

#define NTP_BUFFER 128
enum {
  STATE_NOT_CONNECTED = 0, 
  STATE_CONNECTING, 
  STATE_NTP_SYNCHRO, 
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

private:
  Logic& logic;

  MyHardware& hardware();
  MQTTClient& mqtt();

  int currentState;
  unsigned long connectionStartTime;
  char buffer[NTP_BUFFER];
  long now_time;
};


#endif
