#ifndef WEBSERVER_C
#define WEBSERVER_C

#pragma once

#include <WiFi.h>
#include <time.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>

#include "NTPMachine.h"

#define HTTP_BUFFER 2048
#define HTTP_TIMEOUT 10000
#define PARAM_LENGTH 20

#define CHECK_METHOD(val) strcmp(method, val) == 0

class NTPMachine;
class MyHardware;

class MyWebServer {
public:

  MyWebServer();
  void start(NTPMachine *ntp, MyHardware *h);
  bool findParameter(const char *toFind, const char *query, char *parameterValue);
  bool parseParameters(const char* query);
  void handleHTTPClient();

private:
  NTPMachine *ntp; 
  MyHardware *hardware;
  WiFiServer server;
  WiFiClient currentClient;   
  unsigned int bufferIndex = 0;
  unsigned long lastActivityTime = 0;
  char requestBuffer[HTTP_BUFFER];
  char dateHourStart[PARAM_LENGTH] = "";
  char dateHourEnd[PARAM_LENGTH] = "";
  char isOn[PARAM_LENGTH] = "";

};

#endif
