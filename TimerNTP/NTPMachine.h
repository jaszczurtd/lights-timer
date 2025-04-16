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
#define PRINT_INTERVAL_MS  5000
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

class MyWebServer;
class MyHardware;

class NTPMachine {
public:
  NTPMachine();
  void start(MyHardware *h, MyWebServer *w);
  int getCurrentState(void);
  void stateMachine(void);
  const char *getTimeFormatted(void);
  int getWifiStrength(void);
  const char *getMyIP(void);
  const char *getMyMAC(void);

private:
  MyWebServer *web;
  MyHardware *hardware;
  int currentState;
  unsigned long connectionStartTime;
  char buffer[NTP_BUFFER];
  char ip_str[sizeof("255.255.255.255") + 1];
  char mac_str[sizeof("FF:FF:FF:FF:FF:FF") + 1];
};


#endif
