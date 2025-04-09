#ifndef TIMER_NTP_C
#define TIMER_NTP_C

#include <WiFi.h>
#include <time.h>
#include <Credentials.h>
#include <tools.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <string.h>


#define WIFI_CONNECTED     (WiFi.status() == WL_CONNECTED)
#define WIFI_TIMEOUT_MS    30000
#define NTP_TIMEOUT_MS     30000
#define PRINT_INTERVAL_MS  5000
#define HOURS_SYNC_INTERVAL 12

#define HTTP_BUFFER 2048
#define HTTP_TIMEOUT 10000

enum {
  STATE_NOT_CONNECTED = 0, 
  STATE_CONNECTING, 
  STATE_NTP_SYNCHRO, 
  STATE_CONNECTED
};

#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "europe.pool.ntp.org"


#endif
