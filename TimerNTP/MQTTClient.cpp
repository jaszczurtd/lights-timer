
#include "MyWebServer.h"
#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"
#include <Credentials.h>

NTPMachine& MQTTClient::ntp() { return logic.ntpObj(); }
MyHardware& MQTTClient::hardware() { return logic.hardwareObj(); }

MQTTClient::MQTTClient(Logic& l) : logic(l), mqttClient(currentClient), currentClient() { }

static MQTTClient* g_mqtt = nullptr;

void callback(char* topic, byte* payload, unsigned int length) {
  if (!g_mqtt) return;
  g_mqtt->handleMessage(topic, payload, length);
}

void MQTTClient::handleMessage(char* topic, uint8_t* payload, unsigned int length) {

  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  cJSON* root = cJSON_Parse(msg.c_str());
  if(!root) {
    deb("MQTT: problem with json parsing");
    return;
  }
  String myHostName = String(hardware().getMyHostname());

  deb("MQTT: topic:%s update: %s", topic, msg.c_str());
  if( (String(MQTT_TOPIC_TIME_SET) + myHostName).equalsIgnoreCase(String(topic)) ){
    deb("MQTT: time update requested from broker!");

    cJSON* start = cJSON_GetObjectItem(root, "dateHourStart");
    int dateHourStart = start->valueint;
    deb("start: %d", dateHourStart);

    cJSON* end = cJSON_GetObjectItem(root, "dateHourEnd");
    int dateHourEnd = end->valueint;
    deb("end: %d", dateHourEnd);

    hardware().setTimeRange(dateHourStart, dateHourEnd);
    publishPending = true;

  }

  cJSON_Delete(root);
}

void MQTTClient::start() {
  deb("MQTT: connect attempt! %s", MQTT_BROKER);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  g_mqtt = this;
  mqttClient.setCallback(callback);
  reconnect();
  clientInitialized = true;
}

void MQTTClient::stop() {
  publishPending = clientInitialized = false;
}

void MQTTClient::publish() {
  if(WIFI_CONNECTED && mqttClient.connected()) {

    long s = 0, e = 0;
    hardware().loadStartEnd(&s, &e);
    bool *switches = hardware().getSwitchesStates();

    bool send = false;
    if(lastDateHourStart != s) {
      lastDateHourStart = s; send = true;
    }
    if(lastDateHourEnd != e) {
      lastDateHourEnd = e; send = true;
    }
    for(int a = 0; a < MAX_AMOUNT_OF_RELAYS; a++) {
      if(switches[a] != lastStates[a]) {
        lastStates[a] = switches[a]; send = true;
      }
    }

    if(!send) {
      deb("MQTT: skipping send the same data again");
      return;
    }

    bool ok = true;
    String response;
    cJSON* root = cJSON_CreateObject();
    if(root) {

      ok &= (cJSON_AddStringToObject(root, "status", "ok") != nullptr);
      ok &= (cJSON_AddNumberToObject(root, "dateHourStart", s) != nullptr);
      ok &= (cJSON_AddNumberToObject(root, "dateHourEnd", e) != nullptr);
      ok &= (cJSON_AddBoolToObject(root, "isOn1", switches[0]) != nullptr);
      ok &= (cJSON_AddBoolToObject(root, "isOn2", switches[1]) != nullptr);
      ok &= (cJSON_AddBoolToObject(root, "isOn3", switches[2]) != nullptr);
      ok &= (cJSON_AddBoolToObject(root, "isOn4", switches[3]) != nullptr);
      ok &= (cJSON_AddStringToObject(root, "localTime", 
              strlen(ntp().getTimeFormatted()) ? ntp().getTimeFormatted() : "not available yet.") != nullptr);

      if (ok) {
        char* json = cJSON_PrintUnformatted(root);
        if (json) {
          response = String(json);
          free(json);
        }
      } else {
        ok = false;
      }
      cJSON_Delete(root);
    } else {
      ok = false;
    }
    if(!ok) {
      response = "{\"status\":\"JSON build failed\"}";
    }

    String topic = String(MQTT_TOPIC_STATUS) + String(hardware().getMyHostname());
    if(topic) {
      deb("MQTT: topic:%s publish: %s", topic.c_str(), response.c_str());

      mqttClient.publish(topic.c_str(), response.c_str(), true);
    } else {
      publishPending = true;
    }
  } else {
    publishPending = true;
  }
}

bool MQTTClient::reconnect() {
  deb("MQTT: WiFi status: %d", WiFi.status());
  if(WIFI_CONNECTED) {
    watchdog_update();
    if (mqttClient.connect(hardware().getMyHostname(), MQTT_USER, MQTT_PASSWORD)) {
      watchdog_update();

      String timeUpdateTopic = String(MQTT_TOPIC_TIME_SET) + String(hardware().getMyHostname());
      mqttClient.subscribe(timeUpdateTopic.c_str());

      deb("MQTT: connected!");
      return true;
    } 
    
    deb("MQTT: connect failed! state=%d", mqttClient.state());

  } else {
    deb("MQTT: wifi is not connected!");
  }

  return false;
}

void MQTTClient::handleMQTTClient() {
  if(clientInitialized) {
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastReconnectAttempt > MQTT_RECONNECT_TIME) {
        lastReconnectAttempt = now;
        if (reconnect()) lastReconnectAttempt = 0;
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
