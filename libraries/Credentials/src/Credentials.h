#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#pragma once

#define CREDENTIALS_API_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Cred {
    CR_WIFI_SSID = 0,
    CR_WIFI_PASSWORD,
    CR_MQTT_USER,
    CR_MQTT_PASSWORD,
    CR_MQTT_BROKER,
    CR_MQTT_BROKER_PORT,
    CR_MQTT_BROKER_WIREGUARD,
    CR_MQTT_BROKER_SECURE,
    CR_MQTT_BROKER_SECURE_PORT,
    CR_MQTT_BROKER_IP,
    CR_OTA_UPDATE_PORT,
    CR_OTA_UPDATE_PASSWORD,
    CR_WG_SERVER_PUBLIC_KEY,
    CR_WG_ENDPOINT,
    CR_WG_ENDPOINT_PORT,
    CR_WG_ALLOWED_IP,
    CR_WG_ALLOWED_MASK,
    CR_NTPSERVER0,
    CR_NTPSERVER1,
    CR_NTPSERVER2,
    CR_CA_CERT,
    CR_LAST
} Cred;

const char *getFriendlyHostname(const char *mac);
const char *getWireguardPrivateKey(const char *mac);
const char *getWireguardLocalIP(const char *mac);
int getSwitchesNumber(const char *mac);

void initCredentials(void);
char *getCredential(Cred cred);
int getCredentialInt(Cred cred);

#ifdef __cplusplus
}
#endif

#endif
