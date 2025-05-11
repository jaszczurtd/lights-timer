#ifndef DISCOVERME_C
#define DISCOVERME_C

#pragma once

#include <WiFi.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <string.h>

#include "NTPMachine.h"

#define udpPort 12345

class NTPMachine;

class DiscoverMe {
public:

  DiscoverMe();
  void start(NTPMachine *ntp);
  void handleDiscoveryRequests();

private:
  NTPMachine *ntp; 
  char packetBuffer[255] = {0};
  WiFiUDP udp;
  bool multicastInitialized = false;
  bool pendingResponse = false;
  unsigned long scheduledResponseTime = 0;
  IPAddress remoteIp;
  uint16_t remotePort;  
};

#endif
