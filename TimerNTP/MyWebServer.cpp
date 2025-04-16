
#include "MyWebServer.h"


MyWebServer::MyWebServer() : server(80), currentClient() { }

void MyWebServer::start(NTPMachine *n, MyHardware *h) {
  server.begin();
  ntp = n;
  hardware = h;
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
              dateHourStart, dateHourEnd, isOn, strlen(ntp->getTimeFormatted()) ? ntp->getTimeFormatted() : "not available yet.");

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
              dateHourStart, dateHourEnd, isOn, strlen(ntp->getTimeFormatted()) ? ntp->getTimeFormatted() : "not available yet.");

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
