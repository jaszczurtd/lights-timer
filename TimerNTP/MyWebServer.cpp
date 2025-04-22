
#include "MyWebServer.h"

void parse_example();

MyWebServer::MyWebServer() : server(80), currentClient() { }

void MyWebServer::start(NTPMachine *n, MyHardware *h) {
  server.begin();
  ntp = n;
  hardware = h;

  parse_example();
}

bool MyWebServer::findParameter(const char *toFind, const char *query, char *parameterValue) {
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

bool MyWebServer::parseParameters(const char* query) {

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

void MyWebServer::handleHTTPClient() {
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
      } else {
        currentClient.print("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n");
        currentClient.stop();
        return;
      }

      lastActivityTime = millis();

      if (currentClient.available() == 0) {
        deb("Full request:");
        deb(requestBuffer);

        char responseBuffer[HTTP_BUFFER]; 
        char* method = strtok(requestBuffer, " ");
        char* path = strtok(NULL, " ");

        if (method && path) {

          cJSON* root = cJSON_CreateObject();
          cJSON_AddNumberToObject(root, "dateHourStart", strtoul(dateHourStart, NULL, 10));
          cJSON_AddNumberToObject(root, "dateHourEnd", strtoul(dateHourEnd, NULL, 10));
          cJSON_AddBoolToObject(root, "isOn", (strcasecmp(isOn, "true") == 0));
          cJSON_AddStringToObject(root, "localTime", strlen(ntp->getTimeFormatted()) ? ntp->getTimeFormatted() : "not available yet.");

          char* json = cJSON_PrintUnformatted(root); 

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
              "%s", json);

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
              "%s", ok ? "200 OK" : "400 Bad Request", json);

          } else {
            snprintf(responseBuffer, HTTP_BUFFER,
              "HTTP/1.1 405 Method Not Allowed\r\n"
              "Content-Type: text/plain\r\n"
              "Connection: close\r\n\r\n"
              "Method not allowed");
          }

          cJSON_Delete(root);
          free(json);

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

const char* json = "{\"temp\":22.5,\"status\":\"ok\"}";

void parse_example() {
    cJSON* root = cJSON_Parse(json);
    if (!root) return;

    cJSON* temp = cJSON_GetObjectItem(root, "temp");
    cJSON* status = cJSON_GetObjectItem(root, "status");

    if (cJSON_IsNumber(temp)) {
        deb("Temp: %f\n", temp->valuedouble);
    }
    if (cJSON_IsString(status)) {
        deb("Status: %s\n", status->valuestring);
    }

    cJSON_Delete(root);
}