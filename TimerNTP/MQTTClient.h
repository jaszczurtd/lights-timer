#ifndef MQTT_C
#define MQTT_C

#pragma once

#include "Config.h"
#include "MQTTDiagnostics.h"

#include <time.h>
#include <tools.h>
#include <string.h>
#include <stdint.h>

class NTPMachine;
class MyHardware;
class Logic;

class MQTTClient {
public:

  explicit MQTTClient(Logic& l);
  void start(const char *brokerIP, const int port);
  void stop(); 
  void handleMQTTClient();
  void handleDiagnosticsPingHealth();
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

  char msg[MQTT_MAX_BUFFER_LENGTH];
  char topic[MQTT_MAX_TOPIC_LENGTH];
  char response[MQTT_MAX_BUFFER_LENGTH];

  MQTTDiagnostics diagnostics;

  bool reconnect();
};

#endif
