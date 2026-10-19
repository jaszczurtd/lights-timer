#pragma once

// Reserved documentation/test values only. This file intentionally contains
// no deployable credentials and is safe for public CI builds.
#define CREDENTIALS_LOCAL_CONFIGURED 1

static const char *const CREDENTIAL_VALUES[CR_LAST] = {
    "ci-example-ssid",                      // CR_WIFI_SSID
    "ci-example-password",                  // CR_WIFI_PASSWORD
    "ci-example-user",                      // CR_MQTT_USER
    "ci-example-password",                  // CR_MQTT_PASSWORD
    "mqtt.example.invalid",                 // CR_MQTT_BROKER
    "1883",                                 // CR_MQTT_BROKER_PORT
    "192.0.2.1",                            // CR_MQTT_BROKER_WIREGUARD
    "mqtt.example.invalid",                 // CR_MQTT_BROKER_SECURE
    "8883",                                 // CR_MQTT_BROKER_SECURE_PORT
    "192.0.2.2",                            // CR_MQTT_BROKER_IP
    "8266",                                 // CR_OTA_UPDATE_PORT
    "ci-example-ota-password",              // CR_OTA_UPDATE_PASSWORD
    "ci-example-wireguard-public-key",      // CR_WG_SERVER_PUBLIC_KEY
    "vpn.example.invalid",                  // CR_WG_ENDPOINT
    "51820",                                // CR_WG_ENDPOINT_PORT
    "192.0.2.0",                            // CR_WG_ALLOWED_IP
    "255.255.255.0",                        // CR_WG_ALLOWED_MASK
    "time.example.invalid",                 // CR_NTPSERVER0
    "time.example.invalid",                 // CR_NTPSERVER1
    "time.example.invalid",                 // CR_NTPSERVER2
    "ci-example-ca-certificate",            // CR_CA_CERT
    "0000"                                  // CR_SIM_PIN
};
