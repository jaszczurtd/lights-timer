#ifndef MQTT_C
#define MQTT_C

#pragma once

#include "Config.h"

#include <time.h>
#include <tools.h>
#include <string.h>
#include <stdint.h>

#include <utils/cJSON.h>

class NTPMachine;
class MyHardware;
class Logic;

class MQTTClient {
public:

  explicit MQTTClient(Logic& l);
  void start(const char *brokerIP, const int port);
  void stop(); 
  void handleMQTTClient();
  void publish();
  void requestPublish() { publishPending = true; }
  void handleMessage(const char* topic, const uint8_t* payload, uint16_t length);

private:
  Logic& logic;

  NTPMachine& ntp();
  MyHardware& hardware();

  SmartTimers reconnectTimer;
  bool clientInitialized = false;
  bool publishPending = false;

  char msg[512];
  char topic[128];
  char response[512];

  bool reconnect();
};

#endif
