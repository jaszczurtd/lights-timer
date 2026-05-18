#ifndef DISCOVERME_C
#define DISCOVERME_C

#pragma once

#include "Config.h"

#include <Credentials.h>
#include <tools.h>
#include <string.h>

class NTPMachine;
class MyHardware;
class Logic;

class DiscoverMe {
public:

  explicit DiscoverMe(Logic& l) : logic(l) {}
  void start(void);
  void handleDiscoveryRequests();

private:
  Logic& logic;
  char packetBuffer[255] = {0};
  bool multicastInitialized = false;
  bool pendingResponse = false;
  unsigned long scheduledResponseTime = 0;
  char remoteIp[HAL_UDP_IP_STR_LEN] = "0.0.0.0";
  uint16_t remotePort = 0;

  MyHardware& hardware();
};

#endif
