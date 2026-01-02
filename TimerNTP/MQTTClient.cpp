#include "PubSubClient.h"
#include "ca_cert.h"
#include "MQTTClient.h"

#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"
#include <Credentials.h>
#include <cstring>
#include <cstdio>

NTPMachine& MQTTClient::ntp() { return logic.ntpObj(); }
MyHardware& MQTTClient::hardware() { return logic.hardwareObj(); }

MQTTClient::MQTTClient(Logic& l) : logic(l), mqttClient(currentClient), currentClient() { }

static MQTTClient* g_mqtt = nullptr;

void callback(char* topic, byte* payload, unsigned int length) {
  if(!g_mqtt) return;
  g_mqtt->handleMessage(topic, payload, length);
}

void MQTTClient::handleMessage(char* topicArrived, uint8_t* payload, unsigned int length) {
  if(length >= sizeof(msg)) length = sizeof(msg) - 1;
  memcpy(msg, payload, length);
  msg[length] = '\0';

  cJSON *root = cJSON_Parse(msg);
  if(!root) {
    deb("MQTT: problem with json parsing");
    return;
  }

  const char *myHostName = hardware().getMyHostname();

  deb("MQTT: topic:%s update: %s", topicArrived, msg);

  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_TIME_SET, myHostName);
  if(strcasecmp(topicArrived, topic) == 0) {
    deb("MQTT: time update requested from broker!");

    cJSON *start = cJSON_GetObjectItem(root, "dateHourStart");
    cJSON *end = cJSON_GetObjectItem(root, "dateHourEnd");

    if(start && cJSON_IsNumber(start) && end && cJSON_IsNumber(end)) {
      int dHourStart = start->valueint;
      int dHourEnd = end->valueint;
      deb("Scheduler start: %d end: %d", dHourStart, dHourEnd);

      hardware().setTimeRange(dHourStart, dHourEnd);
      ntp().evaluateTimeCondition();
      publishPending = true;
    }
  }

  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, myHostName);
  if(strcasecmp(topicArrived, topic) == 0) {
    deb("MQTT: switch update requested from broker!");

    bool shouldSave = false;
    for(int a = 0; a < getSwitchesNumber(hardware().getMyMAC()); a++) {
      char key[8];
      snprintf(key, sizeof(key), "isOn%d", a + 1);
      cJSON *value = cJSON_GetObjectItem(root, key);
      if(cJSON_IsBool(value)) {
        hardware().setRelayTo(a, cJSON_IsTrue(value));
        shouldSave = true;
      }
    }
    if(shouldSave) {
      hardware().saveSwitches();
    }
    publishPending = true;
  }

  cJSON_Delete(root);
}

void MQTTClient::start(const char *brokerIP, const int port) {
  if(brokerIP == NULL) {
    derr("invalid broker IP address!");
    return;
  }
  deb("MQTT: connect attempt! %s / %d", brokerIP, port);
  
  IPAddress server;
  server.fromString(brokerIP);

  mqttClient.setServer(server, port);
  mqttClient.setSocketTimeout(MQTT_SOCKET_MAX_TIMEOUT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE);

  g_mqtt = this;
  mqttClient.setCallback(callback);
  reconnect();
  clientInitialized = true;
}

void MQTTClient::stop() {
  publishPending = clientInitialized = false;
  g_mqtt = nullptr;
}

void MQTTClient::publish() {
  if(WIFI_CONNECTED && mqttClient.connected()) {
    long s = 0, e = 0;
    hardware().loadStartEnd(&s, &e);
    bool *switches = hardware().getSwitchesStates();

    cJSON *root = cJSON_CreateObject();
    if(root) {
      memset(response, 0, sizeof(response));
      bool ok = true;
      ok &= cJSON_AddStringToObject(root, "status", "ok") != nullptr;
      ok &= cJSON_AddNumberToObject(root, "dateHourStart", s) != nullptr;
      ok &= cJSON_AddNumberToObject(root, "dateHourEnd", e) != nullptr;
      ok &= cJSON_AddBoolToObject(root, "isOn1", switches[0]) != nullptr;
      ok &= cJSON_AddBoolToObject(root, "isOn2", switches[1]) != nullptr;
      ok &= cJSON_AddBoolToObject(root, "isOn3", switches[2]) != nullptr;
      ok &= cJSON_AddBoolToObject(root, "isOn4", switches[3]) != nullptr;
      const char *time = ntp().getTimeFormatted();
      ok &= cJSON_AddStringToObject(root, "localTime", strlen(time) ? time : "not available yet.") != nullptr;
      ok &= cJSON_AddNumberToObject(root, "localMillis", millis()) != nullptr;
      char strength[10]; snprintf(strength, sizeof(strength), "5/%d", hardware().getWifiStrength());
      ok &= cJSON_AddStringToObject(root, "wifi", strength) != nullptr;

      if(ok) {
        char *json = cJSON_PrintUnformatted(root);
        if(json) {
          strncpy(response, json, sizeof(response) - 1);
          free(json);
        }
      } else {
        strncpy(response, "{\"status\":\"JSON build failed\"}", sizeof(response) - 1);
      }
      cJSON_Delete(root);
    }

    snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_STATUS, hardware().getMyHostname());
    deb("MQTT: topic:%s publish: %s", topic, response);

    mqttClient.publish(topic, response, true);
  } else {
    publishPending = true;
  }
}

bool MQTTClient::reconnect() {
  deb("MQTT: WiFi status: %d", WiFi.status());

  if(WIFI_CONNECTED && WiFi.localIP() != INADDR_NONE) {
    const char *hostName = hardware().getMyHostname();
    watchdog_update();

    if(mqttClient.connect(hostName, MQTT_USER, MQTT_PASSWORD)) {
      watchdog_update();

      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_STATUS, hostName);
      mqttClient.publish(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_UPDATE, hostName);
      mqttClient.publish(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_TIME_SET, hostName);
      mqttClient.publish(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, hostName);
      mqttClient.publish(topic, "", true);

      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_TIME_SET, hostName);
      mqttClient.subscribe(topic);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, hostName);
      mqttClient.subscribe(topic);

      publishPending = true;

      deb("MQTT: (re)connected successfully!");

      return true;
    }
    deb("MQTT: connect failed! state=%d", mqttClient.state());

  } else {
    deb("MQTT: wifi is not connected!");
  }

  return false;
}

void MQTTClient::handleMQTTClient() {

  if(ntp().isBrokerAvailable()) {
    if(clientInitialized) {
      if(!mqttClient.connected()) {
        unsigned long now = millis();
        if(now - lastReconnectAttempt > MQTT_RECONNECT_TIME) {
          lastReconnectAttempt = now;
          if(reconnect()) lastReconnectAttempt = 0;
        }
      } else {
        mqttClient.loop();

        if(publishPending) {
          publish();
          publishPending = false;
        }
      }
    } else {
      mqttClient.loop();
    }
  }
}
