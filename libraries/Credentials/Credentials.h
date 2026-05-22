#ifndef CREDENTIALS_C
#define CREDENTIALS_C

#pragma once

#include "MacHostMapping.h"
#include "ca_cert.h"

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define MQTT_USER "YOUR_MQTT_USER"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#define MQTT_BROKER "192.168.2.100"
#define MQTT_BROKER_PORT 1883
#define MQTT_BROKER_WIREGUARD "10.8.0.1"

#define MQTT_BROKER_SECURE "mqtt.example.com"
#define MQTT_BROKER_SECURE_PORT 8883
#define MQTT_BROKER_IP "203.0.113.10"

#define OTA_UPDATE_PORT 8266
#define OTA_UPDATE_PASSWORD "YOUR_OTA_PASSWORD"

#define WG_SERVER_PUBLIC_KEY "REPLACE_WITH_SERVER_PUBLIC_KEY_BASE64"
#define WG_ENDPOINT "vpn.example.com"

// Server listen port (as configured on the WireGuard server).
#define WG_ENDPOINT_PORT 51820

#define WG_ALLOWED_IP "10.8.0.0"
#define WG_ALLOWED_MASK "255.255.255.0"

#define ntpServer0 "pool.ntp.org"
#define ntpServer1 "time.google.com"
#define ntpServer2 "europe.pool.ntp.org"

#ifdef __cplusplus
extern "C" {
#endif

const char* getFriendlyHostname(const char* mac);
const char* getWireguardPrivateKey(const char* mac);
const char* getWireguardLocalIP(const char* mac);
int getSwitchesNumber(const char* mac);

#ifdef __cplusplus
}
#endif

#endif
