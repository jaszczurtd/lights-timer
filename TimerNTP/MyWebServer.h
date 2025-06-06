#ifndef WEBSERVER_C
#define WEBSERVER_C

#pragma once

#include <WiFi.h>
#include <time.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>

#include "cJSON.h"

#define HTTP_BUFFER 2048
#define HTTP_TIMEOUT 5000
#define PARAM_LENGTH 20

#define CHECK_METHOD(val) strcmp(method, val) == 0

class NTPMachine;
class MyHardware;
class Logic;

class MyWebServer {
public:

  explicit MyWebServer(Logic& l);
  void start();
  void handleHTTPClient();
  void setTimeRangeForHTTPResponses(long start, long end);
  void updateRelaysStatesForClient(void);

private:
  Logic& logic;
  WiFiServer server;
  WiFiClient currentClient;   
  unsigned int bufferIndex = 0;
  unsigned long lastActivityTime = 0;
  char requestBuffer[HTTP_BUFFER] = "";
  char returnBuffer[HTTP_BUFFER / 2] = "";
  char dateHourStart[PARAM_LENGTH] = "";
  char dateHourEnd[PARAM_LENGTH] = "";
  char isOn1[PARAM_LENGTH] = "";
  char isOn2[PARAM_LENGTH] = "";
  char isOn3[PARAM_LENGTH] = "";
  char isOn4[PARAM_LENGTH] = "";

  NTPMachine& ntp();
  MyHardware& hardware();

  char* extractPostBody(char* http_request);
  long processGETToken(const char* token);
  bool findPOSTParameter(const char *toFind, const char *query, char *parameterValue);
  char *parseGETParameters(const char* query);
  bool parsePOSTParameters(const char* query);

};

#endif
