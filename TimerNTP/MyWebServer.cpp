
#include "MyWebServer.h"

void parse_example();

MyWebServer::MyWebServer() : server(80), currentClient() { }

void MyWebServer::start(NTPMachine *n, MyHardware *h) {
  server.begin();
  ntp = n;
  hardware = h;
}

char* MyWebServer::extractPostBody(char* http_request) {
  char* body = (char*)strstr(http_request, "\r\n\r\n");
  if (!body) body = (char*)strstr(http_request, "\n\n");
  return body ? body + ((body[1] == '\n') ? 2 : 4) : http_request;
}

bool MyWebServer::findParameter(const char *toFind, const char *query, char *parameterValue) {
    if (!toFind || !query || !parameterValue) return false;

    const char *found = strstr(query, toFind);
    if (!found) return false;
    char prev = (found == query) ? '\0' : *(found - 1);
    if (prev != '\0' && prev != '&' && prev != '\r' && prev != '\n') return false;

    const char *start = found + strlen(toFind);
    const char *end = strchr(start, '&');
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= PARAM_LENGTH) len = PARAM_LENGTH - 1;

    char raw[PARAM_LENGTH] = {0};
    strncpy(raw, start, len);
    raw[len] = '\0';

    urlDecode(raw, parameterValue);
    return true;
}

bool MyWebServer::parsePOSTParameters(const char* query) {

  int parametersFound = 0;

  if(findParameter("dateHourStart=", query, dateHourStart)) {
    parametersFound++;
  }
  if(findParameter("dateHourEnd=", query, dateHourEnd)) {
    parametersFound++;
  }
  if(findParameter("isOn1=", query, isOn1)) {
    parametersFound++;
  } 
  if(findParameter("isOn2=", query, isOn2)) {
    parametersFound++;
  }
  if(findParameter("isOn3=", query, isOn3)) {
    parametersFound++;
  }
  if(findParameter("isOn4=", query, isOn4)) {
    parametersFound++;
  }
  
  return (parametersFound > 0);
}

long MyWebServer::processToken(const char* token) {
  long value = -1;
  if (strcmp(token, "dateHourStart") == 0) {
    deb("giving start hour"); 
    value = strtol(dateHourStart, NULL, 10);
  }
  else if (strcmp(token, "dateHourEnd") == 0) {
    deb("giving end hour"); 
    value = strtol(dateHourEnd, NULL, 10);
  }
  else if (strcmp(token, "isOn1") == 0) {
    deb("giving relay 1 state"); 
    value = (strcasecmp(isOn1, "true") == 0);
  }
  else if (strcmp(token, "isOn2") == 0) {
    deb("giving relay 2 state"); 
    value = (strcasecmp(isOn2, "true") == 0);
  }
  else if (strcmp(token, "isOn3") == 0) {
    deb("giving relay 3 state"); 
    value = (strcasecmp(isOn3, "true") == 0);
  }
  else if (strcmp(token, "isOn4") == 0) {
    deb("giving relay 4 state"); 
    value = (strcasecmp(isOn1, "true") == 0);
  }
  else {
    deb("unknown token: %s\n", token);
  }

  return value;
}

char *MyWebServer::parseGETParameters(const char *query) {
  deb("query: %s\n", query);

  const char* delim = "&";
  char* token = strtok((char*)query, delim);
  long value;

  memset(returnBuffer, 0, sizeof(returnBuffer));
  cJSON* root = cJSON_CreateObject();
  if(root) {
    while (token != NULL) {
      value = processToken(token);
      if(value != -1) {
        if(startsWith(token, "isOn")) {
          cJSON_AddBoolToObject(root, token, value != 0);
        } else {
          cJSON_AddNumberToObject(root, token, value);
        }
      }
      token = strtok(NULL, delim);
    }

    char *json = cJSON_PrintUnformatted(root); 
    if(json) {
      strncpy(returnBuffer, json, sizeof(returnBuffer) - 1);
      free(json);
    }
    cJSON_Delete(root);
  }

  return returnBuffer;
}

void MyWebServer::updateRelaysStatesForClient(void) {
  char *representation[4] = {isOn1, isOn2, isOn3, isOn4};
  for(int a = 0; a < 4; a++) {
    snprintf(representation[a], PARAM_LENGTH, "%s", (hardware->getSwitchesStates()[a]) ? "true" : "false");
  }
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

        char responseBuffer[HTTP_BUFFER] = {0}; 
        char* method = strtok(requestBuffer, " ");
        char* path = strtok(NULL, " ");

        if (method && path) {

          if(CHECK_METHOD("GET")) {
            char* query = strchr(path, '?');
            if (query) {
              *query = '\0';

              updateRelaysStatesForClient();
              char *json = parseGETParameters(query + 1);
              deb("response json: %s", json);

              snprintf(responseBuffer, HTTP_BUFFER,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "%s", json);
            }

          } else if (CHECK_METHOD("POST")) {
            char* body = requestBuffer + strlen("POST") + 1 + strlen("/?");
            bool ok = false;
            if (body) {
              body = extractPostBody(body);
              deb("body: %s", body);
              ok = parsePOSTParameters(body);

              long timeStart = 0, timeEnd = 0;

              if(ok) {
                timeStart = strtol(dateHourStart, NULL, 10);
                timeEnd = strtol(dateHourEnd, NULL, 10);
                hardware->setTimeRange(timeStart, timeEnd);
                hardware->checkConditionsForStartEnAction(ntp->getTimeNow());
                char *representation[4] = {isOn1, isOn2, isOn3, isOn4};
                for(int a = 0; a < 4; a++) {
                  hardware->setRelayTo(a, (strcasecmp(representation[a], "true") == 0));
                }
                updateRelaysStatesForClient();
              }

              memset(returnBuffer, 0, sizeof(returnBuffer));
              cJSON* root = cJSON_CreateObject();
              if(root) {
                cJSON_AddNumberToObject(root, "dateHourStart", timeStart);
                cJSON_AddNumberToObject(root, "dateHourEnd", timeEnd);
                cJSON_AddBoolToObject(root, "isOn1", (strcasecmp(isOn1, "true") == 0));
                cJSON_AddBoolToObject(root, "isOn2", (strcasecmp(isOn2, "true") == 0));
                cJSON_AddBoolToObject(root, "isOn3", (strcasecmp(isOn3, "true") == 0));
                cJSON_AddBoolToObject(root, "isOn4", (strcasecmp(isOn4, "true") == 0));
                cJSON_AddStringToObject(root, "localTime", strlen(ntp->getTimeFormatted()) ? ntp->getTimeFormatted() : "not available yet.");

                char* json = cJSON_PrintUnformatted(root);
                if(json) {
                  strncpy(returnBuffer, json, sizeof(returnBuffer) - 1);
                  free(json);
                }
                cJSON_Delete(root);
              }

              snprintf(responseBuffer, HTTP_BUFFER,
                "HTTP/1.1 %s\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "%s", ok ? "200 OK" : "400 Bad Request", returnBuffer);

            }

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

void MyWebServer::setTimeRangeForHTTPResponses(long start, long end) {
  memset(dateHourStart, 0, sizeof(dateHourStart));
  memset(dateHourEnd, 0, sizeof(dateHourEnd));

  snprintf(dateHourStart, sizeof(dateHourStart) -1, "%ld", start);
  snprintf(dateHourEnd, sizeof(dateHourEnd) - 1, "%ld", end);

  deb("set start/end values for web: %s %s", dateHourStart, dateHourEnd);
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