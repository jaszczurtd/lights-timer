#ifndef NTP_C
#define NTP_C

#pragma once

#include <WiFi.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>

#include "MyWebServer.h"
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

#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "europe.pool.ntp.org"

#define MAX_TIME (24 * 60)

class MyWebServer;
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
  MyWebServer& web();

  int currentState;
  unsigned long connectionStartTime;
  char buffer[NTP_BUFFER];
  long now_time;
};


#endif
