#ifndef C_LOGIC_H
#define C_LOGIC_H

#pragma once

#include "Config.h"

#include <Arduino.h>
#include <tools.h>

#include "NTPMachine.h"
#include "MyWebServer.h"
#include "MyHardware.h"
#include "DiscoverMe.h"

class NTPMachine;
class MyWebServer;
class MyHardware;
class DiscoverMe;

class Logic {
public:
  Logic() : ntp(*this), web(*this), hardware(*this), discover(*this) { }
  void logicSetup(void);
  void logicLoop(void);

  MyHardware& hardwareObj() { return hardware; }
  NTPMachine& ntpObj()   { return ntp; }
  DiscoverMe& discoverObj() { return discover; }
  MyWebServer& webObj()   { return web; }

private:
  NTPMachine ntp;
  MyWebServer web;
  MyHardware hardware;
  DiscoverMe discover;

  bool initialized;
};

#endif
