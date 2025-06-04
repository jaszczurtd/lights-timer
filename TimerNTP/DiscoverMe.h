#ifndef DISCOVERME_C
#define DISCOVERME_C

#pragma once

#include "Config.h"

#include <WiFi.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <string.h>
#include <WiFiUdp.h>

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
  WiFiUDP udp;
  bool multicastInitialized = false;
  bool pendingResponse = false;
  unsigned long scheduledResponseTime = 0;
  IPAddress remoteIp;
  uint16_t remotePort;  

  MyHardware& hardware();
};

#endif
