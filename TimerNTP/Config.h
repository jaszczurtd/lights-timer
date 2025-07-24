#ifndef T_CONFIG
#define T_CONFIG

#define WATCHDOG_TIME 6000
#define CORE_OPERATION_DELAY 1

#define MAX_AMOUNT_OF_RELAYS 4

#define UDP_PORT 12345
#define DISCOVER_PACKET "AQUA_DISCOVER"
#define RESPONSE_PACKET "AQUA_FOUND"
#define MQTT_PORT 1883
#define MQTT_RECONNECT_TIME 5000
#define MQTT_TOPIC_STATUS "status-"
#define MQTT_TOPIC_UPDATE "update-"

#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "europe.pool.ntp.org"

#endif
