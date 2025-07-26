#ifndef MQTT_C
#define MQTT_C

#pragma once

#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <tools.h>
#include <WiFiClientSecure.h>
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
  void handleMessage(char* topic, uint8_t* payload, unsigned int length);

private:
  Logic& logic;
  WiFiClientSecure currentClient;   
  PubSubClient mqttClient;

  NTPMachine& ntp();
  MyHardware& hardware();

  unsigned long lastReconnectAttempt = 0;
  bool clientInitialized = false;
  bool publishPending = false;

  char msg[512];
  char topic[128];
  char response[512];

  bool reconnect();
};

#endif
