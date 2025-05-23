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
class MyHardware;

class DiscoverMe {
public:

  DiscoverMe();
  void start(NTPMachine *ntp, MyHardware *h);
  void handleDiscoveryRequests();

private:
  NTPMachine *ntp; 
  MyHardware *hardware;
  char packetBuffer[255] = {0};
  WiFiUDP udp;
  bool multicastInitialized = false;
  bool pendingResponse = false;
  unsigned long scheduledResponseTime = 0;
  IPAddress remoteIp;
  uint16_t remotePort;  
};

#endif
