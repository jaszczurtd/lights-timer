#ifndef HARDWARE_C
#define HARDWARE_C

#include <EEPROM.h>
#include <Credentials.h>
#include "NTPMachine.h"

#pragma once

class NTPMachine;
class MyWebServer;

class MyHardware {
public:
  MyHardware();
  void start(NTPMachine *m, MyWebServer *w);
  void restartWiFi(void);
  int getWifiStrength(void);
  const char *getMyIP(void);
  const char *getMyMAC(void);
  const char *getMyHostname(void);
  const char *getAmountOfSwitches(void);
  void updateBuildInLed(void);
  void extractTime(long start, long end);
  void setTimeRange(long start, long end);
  void saveStartEnd(long start, long end);
  void loadStartEnd(long *start, long *end);
  void checkConditionsForStartEnAction(long timeNow);
  void setLightsTo(bool state);
  void setRelayTo(int index, bool state);
  bool *getSwitchesStates(void);

private:
  NTPMachine *ntp;
  MyWebServer *web;

  char ip_str[sizeof("255.255.255.255") + 1];
  char mac_str[sizeof("FF:FF:FF:FF:FF:FF") + 1];
  char hostname_str[32];

  int startHour = 0;
  int startMinute = 0;
  int endHour = 0;
  int endMinute = 0;

  bool flagLights = false;
  bool lastStateFlagLights = false;

  char switches_str[8];

  bool switches[4];
};


#endif
