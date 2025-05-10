#ifndef HARDWARE_C
#define HARDWARE_C

#include <EEPROM.h>
#include "NTPMachine.h"

#pragma once

class NTPMachine;

class MyHardware {
public:
  MyHardware();
  void start(NTPMachine *m);
  void updateBuildInLed(void);

private:
  NTPMachine *ntp;
};


#endif
