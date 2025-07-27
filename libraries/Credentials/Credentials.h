#ifndef CREDENTIALS_C
#define CREDENTIALS_C

#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "MacHostMapping.h"
#include "ca_cert.h"

#define WIFI_SSID "Your wifi name"
#define WIFI_PASSWORD "Your wifi password"

#define MQTT_USER "Your mosquitto user"
#define MQTT_PASSWORD "Your mosquitto password"

#define MQTT_BROKER_SECURE "Your external domain/IP"
#define MQTT_BROKER_SECURE_PORT 8883

#define OTA_UPDATE_PORT 8266
#define OTA_UPDATE_PASSWORD "Your OTA update password"

#ifdef __cplusplus
extern "C" {
#endif

const char* getFriendlyHostname(const char* mac);
int getSwitchesNumber(const char* mac);

#ifdef __cplusplus
}
#endif

#endif
