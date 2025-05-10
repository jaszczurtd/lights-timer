
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
    value = strtoul(dateHourStart, NULL, 10);
  }
  else if (strcmp(token, "dateHourEnd") == 0) {
    deb("giving end hour"); 
    value = strtoul(dateHourEnd, NULL, 10);
  }
  else if (strcmp(token, "isOn1") == 0) {
    deb("giving relay 1 state"); 
    value = true;//(strcasecmp(isOn1, "true") == 0);
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

  cJSON* root = cJSON_CreateObject();
    
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

  return cJSON_PrintUnformatted(root); 
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
          cJSON_AddBoolToObject(root, "isOn1", (strcasecmp(isOn1, "true") == 0));
          cJSON_AddBoolToObject(root, "isOn2", (strcasecmp(isOn2, "true") == 0));
          cJSON_AddBoolToObject(root, "isOn3", (strcasecmp(isOn3, "true") == 0));
          cJSON_AddBoolToObject(root, "isOn4", (strcasecmp(isOn4, "true") == 0));
          cJSON_AddStringToObject(root, "localTime", strlen(ntp->getTimeFormatted()) ? ntp->getTimeFormatted() : "not available yet.");

          char* json = cJSON_PrintUnformatted(root); 

          if(CHECK_METHOD("GET")) {
            char* query = strchr(path, '?');
            if (query) {
              *query = '\0';
              free(json);
              json = parseGETParameters(query + 1);
              deb("response json: %s", json);
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
              ok = parsePOSTParameters(body);
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