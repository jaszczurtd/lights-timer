
#include "TimerNTP.h"

static int currentState = STATE_NOT_CONNECTED;
static unsigned long connectionStartTime;
static char buffer[NTP_BUFFER];
static bool initialized = false;

static char requestBuffer[HTTP_BUFFER];
static unsigned int bufferIndex = 0;
static unsigned long lastActivityTime = 0;

static WiFiServer server(80);
static WiFiClient currentClient; 

int getWifiStrength(int32_t rssi) {
  if (rssi >= 0) return 0; 
  if (rssi >= -50) return 5;
  if (rssi >= -60) return 4;
  if (rssi >= -70) return 3;
  if (rssi >= -80) return 2;
  if (rssi >= -90) return 1;
  return 0; 
}

void update_build_in_led(void) {
  static unsigned long last_blink;
  static int prevState = -1;

  if (currentState != prevState) {
    last_blink = millis();
    digitalWrite(LED_BUILTIN, LOW);
    prevState = currentState;
  }

  switch(currentState) {
    case STATE_CONNECTING:
    case STATE_NTP_SYNCHRO: {
      unsigned long interval = (currentState == STATE_CONNECTING) ? 100 : 300;
      if (millis() - last_blink > interval) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        last_blink = millis();
      }
    }
    break;
      
    case STATE_CONNECTED: {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    break;
      
    default: {
      digitalWrite(LED_BUILTIN, LOW);
    }
    break;
  }
}

static char ip_str[sizeof("255.255.255.255") + 1];
const char *getMyIP(void) {
  snprintf(ip_str, sizeof(ip_str), "%s", (WIFI_CONNECTED) ? WiFi.localIP().toString().c_str() : "0.0.0.0");
  return ip_str;
}

void stateMachine(void) {
  switch(currentState) {
    case STATE_NOT_CONNECTED: {
      deb("Not connected. Trying to reconnect...");

      memset(buffer, 0, sizeof(buffer));

      WiFi.disconnect(true);     
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      connectionStartTime = millis();

      currentState = STATE_CONNECTING;
    }
    break;

    case STATE_CONNECTING: {

      if(millis() - connectionStartTime > WIFI_TIMEOUT_MS) {
        deb("\n connection timeout!");
        currentState = STATE_NOT_CONNECTED;
        return;
      }      

      static unsigned long last_connecting_cycle;
      if(millis() - last_connecting_cycle > 200) {
        last_connecting_cycle = millis();
        if(WIFI_CONNECTED) {
          deb("Connected. IP address: %s", getMyIP());

          setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
          tzset();
          configTime(0, 0, ntpServer1, ntpServer2);

          server.begin();

          currentState = STATE_NTP_SYNCHRO;
        }
      }
    }
    break;

    case STATE_NTP_SYNCHRO: {
      static unsigned long ntpStartTime = 0;
      if (ntpStartTime == 0) {
        ntpStartTime = millis();
      }

      if (WIFI_CONNECTED) {

        if(time(nullptr) > 24 * 3600 * 2) {
          currentState = STATE_CONNECTED;
          ntpStartTime = 0;
          return;
        }

        if (millis() - ntpStartTime > NTP_TIMEOUT_MS) { 
          deb("NTP synchro error!");
          currentState = STATE_NOT_CONNECTED;
          ntpStartTime = 0;
          return;
        }

      } else {
        currentState = STATE_NOT_CONNECTED;
      }
    }
    break;

    case STATE_CONNECTED: {
      if (WIFI_CONNECTED) {

        static unsigned long last_print_cycle;
        if(millis() - last_print_cycle > 500) {
          last_print_cycle = millis();

          time_t now;
          struct tm timeinfo;

          static unsigned long lastSync = 0;
      
          if (millis() - lastSync > HOURS_SYNC_INTERVAL * 3600 * 1000) {
            configTime(0, 0, ntpServer1, ntpServer2);
            lastSync = millis();
          }

          time(&now);
          localtime_r(&now, &timeinfo);
          
          strftime(buffer, sizeof(buffer), "%A, %d %B %Y %H:%M:%S", &timeinfo);
        }
      } else {
        currentState = STATE_NOT_CONNECTED;
      }
    }
    break;
  }
}

static char dateHourStart[PARAM_LENGTH] = "";
static char dateHourEnd[PARAM_LENGTH] = "";
static char isOn[PARAM_LENGTH] = "";

bool findParameter(const char *toFind, const char *query, char *parameterValue) {
  if(toFind == NULL || query == NULL || parameterValue == NULL) {
    return false;
  }    
    
  const char *found = strstr(query, toFind);
  if (found) {
    if (found != query && *(found - 1) != '&') return false;

    found += strlen(toFind);
    const char *start = found;

    while (*found != '&' && *found != '\0') {
      found++;
    }

    size_t len = found - start;
    if (len >= PARAM_LENGTH) len = PARAM_LENGTH - 1;

    char raw[PARAM_LENGTH] = {0};
    strncpy(raw, start, len);
    raw[len] = '\0';

    urlDecode(raw, parameterValue);
    return true;
  }
  return false;    
}

bool parseParameters(const char* query) {

  int parametersFound = 0;

  if(findParameter("dateHourStart=", query, dateHourStart)) {
    parametersFound++;
  }
  if(findParameter("dateHourEnd=", query, dateHourEnd)) {
    parametersFound++;
  }
  if(findParameter("isOn=", query, isOn)) {
    parametersFound++;
  }
  
  return (parametersFound == 3);
}

void handleHTTPClient() {
  if (!currentClient) {
    currentClient = server.accept();
    if (currentClient) {
      bufferIndex = 0;
      lastActivityTime = millis();
      memset(requestBuffer, 0, sizeof(requestBuffer));
    }
  }

  if (currentClient && currentClient.connected()) {
    while (currentClient.available()) {
      char c = currentClient.read();
      if (bufferIndex < sizeof(requestBuffer) - 1) {
        requestBuffer[bufferIndex++] = c;
        requestBuffer[bufferIndex] = 0;
      }

      lastActivityTime = millis();

      if (currentClient.available() == 0) {
        deb("Full request:");
        deb(requestBuffer);

        char responseBuffer[HTTP_BUFFER]; 
        char* method = strtok(requestBuffer, " ");
        char* path = strtok(NULL, " ");

        if (method && path) {
          if(CHECK_METHOD("GET")) {
            char* query = strchr(path, '?');
            if (query) {
              *query = '\0';
              parseParameters(query + 1);
            }

            snprintf(responseBuffer, HTTP_BUFFER,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Connection: close\r\n\r\n"
              "dateHourStart:%s\r\n"
              "dateHourEnd:%s\r\n"
              "isOn:%s\r\n"
              "localTime:%s\r\n",
              dateHourStart, dateHourEnd, isOn, strlen(buffer) ? buffer : "not available yet.");

          } else if (CHECK_METHOD("POST")) {
            char* body = requestBuffer + strlen("POST") + 1 + strlen("/?");
            bool ok = false;
            if (body) {
              deb("body: %s", body);
              ok = parseParameters(body);
            }

            snprintf(responseBuffer, HTTP_BUFFER,
              "HTTP/1.1 %s\r\n"
              "Content-Type: text/plain\r\n"
              "Connection: close\r\n\r\n"
              "dateHourStart:%s\r\n"
              "dateHourEnd:%s\r\n"
              "isOn:%s\r\n"
              "localTime:%s\r\n", (ok) ? "200 OK" : "400 Bad request", 
              dateHourStart, dateHourEnd, isOn, strlen(buffer) ? buffer : "not available yet.");

          } else {
            snprintf(responseBuffer, HTTP_BUFFER,
              "HTTP/1.1 405 Method Not Allowed\r\n"
              "Content-Type: text/plain\r\n"
              "Connection: close\r\n\r\n"
              "Method not allowed");
          }
        } else {
          snprintf(responseBuffer, HTTP_BUFFER,
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n"
            "Bad Request");
        }

        currentClient.print(responseBuffer);
        currentClient.stop();
        return;
      }
    }

    if (millis() - lastActivityTime > HTTP_TIMEOUT) {
      currentClient.stop();
    }
  }
}

void setup() {
  debugInit();
 
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);

  WiFi.setHostname(customHostname);

  deb("System has started.");
  delay(1000);

  initialized = true;
}

void loop() {

  if(!initialized) {
    return;
  }

  stateMachine();
  update_build_in_led();

  if(WIFI_CONNECTED) {
    handleHTTPClient();
  }

  static unsigned long last_loop_cycle;
  if(millis() - last_loop_cycle > PRINT_INTERVAL_MS) {
    last_loop_cycle = millis();

    if (WIFI_CONNECTED && 
        currentState >= STATE_CONNECTED) {

      deb("%s, IP: %s, signal strength: %d/5", buffer, getMyIP(), getWifiStrength(WiFi.RSSI()));
    }
  }

  delay(1);
}
