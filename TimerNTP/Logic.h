#ifndef C_LOGIC_H
#define C_LOGIC_H

#pragma once

#include "Config.h"

#include <Arduino.h>
#include <tools.h>

#include "NTPMachine.h"
#include "MQTTClient.h"
#include "MyHardware.h"
#include "DiscoverMe.h"

class NTPMachine;
class MQTTClient;
class MyHardware;
class DiscoverMe;

class Logic {
public:
  Logic() : ntp(*this), mqtt(*this), hardware(*this), discover(*this) { }
  void logicSetup(void);
  void logicLoop(void);

  MyHardware& hardwareObj() { return hardware; }
  NTPMachine& ntpObj()   { return ntp; }
  DiscoverMe& discoverObj() { return discover; }
  MQTTClient& mqttObj()   { return mqtt; }

private:
  NTPMachine ntp;
  MQTTClient mqtt;
  MyHardware hardware;
  DiscoverMe discover;

  bool initialized;
};

#endif
