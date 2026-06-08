#ifndef T_CONFIG
#define T_CONFIG

#include <JaszczurHAL.h>

//The maximum watchdog time for the RP2040 is 8388 ms
#define WATCHDOG_TIME (8000)
#define CORE_OPERATION_DELAY 1

#define PING_TIMEOUT_MS   (SECOND * 3) 
#define WIFI_TIMEOUT_MS    (SECOND * 30)
#define NTP_TIMEOUT_MS     (SECOND * 30)
#define PRINT_INTERVAL_MS  (SECOND * 10)
#define HOURS_SYNC_INTERVAL 12

#define EVALUATE_TIME_FOR_RELAY_MS (SECOND)
#define OLED_ACTIVE_WINDOW_MS (SECOND * 15)

#define MAX_AMOUNT_OF_RELAYS 4

#define UDP_PORT 12345
#define DISCOVER_PACKET "AQUA_DISCOVER"
#define RESPONSE_PACKET "AQUA_FOUND"

#define MQTT_MAX_TOPIC_LENGTH 256
#define MQTT_MAX_BUFFER_LENGTH 2048

#define MQTT_RECONNECT_TIME (SECOND * 4)
#define MQTT_TOPIC_STATUS "status-"
#define MQTT_TOPIC_UPDATE "update-"
#define MQTT_TOPIC_TIME_SET "time-"
#define MQTT_TOPIC_SWITCH_SET "switch-"
#define MQTT_TOPIC_DIAGNOSTICS "diagnostics"
#define MQTT_TOPIC_DIAGNOSTICS_BOOT_CAUSE "boot_cause"
#define MQTT_TOPIC_DIAGNOSTICS_WATCHDOG "watchdog"
#define MQTT_TOPIC_DIAGNOSTICS_PING_HEALTH "ping_health"

#define MAX_TIMEOUT (SECOND * 5)
#define MQTT_SOCKET_MAX_TIMEOUT 2
#define MQTT_KEEP_ALIVE 15

#define MAX_FAILED_PINGS 7
#define NEXT_PING_TIME (SECOND * 5)

// Stack guard is currently opt-in. Some RP2040 core/build combinations may
// report false positives and trigger reboot loops when checked each loop.
#define ENABLE_STACK_GUARD 0

// Fault diagnostics hooks are opt-in during stabilization. Disable by default
// to avoid regressions on boards/core builds with backend incompatibilities.
#define ENABLE_FAULT_DIAGNOSTICS 0

#endif
