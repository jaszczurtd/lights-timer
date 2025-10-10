#ifndef T_CONFIG
#define T_CONFIG

#define WATCHDOG_TIME 7000
#define CORE_OPERATION_DELAY 1

#define WIFI_TIMEOUT_MS    30000
#define NTP_TIMEOUT_MS     30000
#define PRINT_INTERVAL_MS  10000
#define HOURS_SYNC_INTERVAL 12

#define EVALUATE_TIME_FOR_RELAY_MS 1000

#define MAX_AMOUNT_OF_RELAYS 4

#define UDP_PORT 12345
#define DISCOVER_PACKET "AQUA_DISCOVER"
#define RESPONSE_PACKET "AQUA_FOUND"
#define MQTT_PORT 1883
#define MQTT_RECONNECT_TIME 5000
#define MQTT_TOPIC_STATUS "status-"
#define MQTT_TOPIC_UPDATE "update-"
#define MQTT_TOPIC_TIME_SET "time-"
#define MQTT_TOPIC_SWITCH_SET "switch-"

#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "europe.pool.ntp.org"

#define MAX_TIMEOUT 3000
#define MQTT_SOCKET_MAX_TIMEOUT 2
#define MQTT_KEEP_ALIVE 15

#define PING_ONE "8.8.8.8"
#define MAX_FAILED_PINGS 7

#endif
