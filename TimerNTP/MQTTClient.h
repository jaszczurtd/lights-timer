#ifndef MQTT_C
#define MQTT_C

#pragma once

#include <WiFi.h>
#include <time.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>
#include <PubSubClient.h>

#include "cJSON.h"

class NTPMachine;
class MyHardware;
class Logic;

class MQTTClient {
public:

  explicit MQTTClient(Logic& l);
  void start();
  void stop(); 
  void handleMQTTClient();
  void publish();

private:
  Logic& logic;
  WiFiClient currentClient;   
  PubSubClient mqttClient;

  NTPMachine& ntp();
  MyHardware& hardware();

  unsigned long lastReconnectAttempt = 0;
  bool clientInitialized = false;
  bool publishPending = false;

  bool reconnect();

};

#endif
