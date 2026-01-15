#ifndef P_PROVIDER_H
#define P_PROVIDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <sys/stat.h>

#include <cjson/cJSON.h>
#include <mosquitto.h>

#define DISCOVER_PORT 12345
#define DISCOVER_MSG "AQUA_DISCOVER"
#define AQUA_FOUND "AQUA_FOUND|"
#define BROADCAST_IP "255.255.255.255"
#define MAX_BUFFER_SIZE 256
#define MAX_INPUT_SIZE 64
#define MQTT_HOST "10.8.0.1"
#define BROADCAST_SECONDS_DELAY 5
#define MQTT_TOPIC "discovered/devices"
#define AUTH_PATH ".mqtt_provider_auth"

#define MQTT_PORT 1883
#define MQTT_KEEPALIVE 60
#define MQTT_CLIENT_ID "aqua-provider"

#define VERSION "1.0.1"

// Struktura przechowująca informacje o urządzeniu
typedef struct {
    char mac[18];
    char ip[16];
    char hostName[256];
    int switches;
    void *next;
} DeviceInfo;

void *discovery_thread(void *arg);
void send_broadcast_message(int sock);
void handle_device_response(char *message);
void send_json_mqtt(const char *json_str);
int is_device_alive_tcp(const char *ip, int port);
void notify_mqtt(void);
bool mosquitto_init(void);
void mosquitto_quit(void);

#endif

