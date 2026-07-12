#pragma once

// Copy this file to CredentialsData.local.h, replace every placeholder and
// change the marker below to 1. The local file is ignored by Git.
#define CREDENTIALS_LOCAL_CONFIGURED 0

static const char *const CREDENTIAL_VALUES[CR_LAST] = {
    "YOUR_WIFI_SSID",                       // CR_WIFI_SSID
    "YOUR_WIFI_PASSWORD",                   // CR_WIFI_PASSWORD
    "YOUR_MQTT_USER",                       // CR_MQTT_USER
    "YOUR_MQTT_PASSWORD",                   // CR_MQTT_PASSWORD
    "mqtt.example.com",                     // CR_MQTT_BROKER
    "1883",                                 // CR_MQTT_BROKER_PORT
    "10.8.0.1",                             // CR_MQTT_BROKER_WIREGUARD
    "mqtt.example.com",                     // CR_MQTT_BROKER_SECURE
    "8883",                                 // CR_MQTT_BROKER_SECURE_PORT
    "203.0.113.10",                         // CR_MQTT_BROKER_IP
    "8266",                                 // CR_OTA_UPDATE_PORT
    "YOUR_OTA_PASSWORD",                    // CR_OTA_UPDATE_PASSWORD
    "YOUR_WIREGUARD_SERVER_PUBLIC_KEY",     // CR_WG_SERVER_PUBLIC_KEY
    "vpn.example.com",                      // CR_WG_ENDPOINT
    "51820",                                // CR_WG_ENDPOINT_PORT
    "10.8.0.0",                             // CR_WG_ALLOWED_IP
    "255.255.255.0",                        // CR_WG_ALLOWED_MASK
    "pool.ntp.org",                         // CR_NTPSERVER0
    "time.google.com",                      // CR_NTPSERVER1
    "europe.pool.ntp.org",                  // CR_NTPSERVER2
    "YOUR_CA_CERTIFICATE_PEM"               // CR_CA_CERT
};
