#include "MQTTClient.h"

#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"
#include <Credentials.h>
#include <cstring>
#include <cstdio>

namespace {
void halMqttCallback(const char *topic, const uint8_t *payload, uint16_t length, void *user) {
  MQTTClient *client = static_cast<MQTTClient *>(user);
  if (!client) {
    return;
  }

  client->handleMessage(topic, payload, length);
}
}

NTPMachine& MQTTClient::ntp() { return logic.ntpObj(); }
MyHardware& MQTTClient::hardware() { return logic.hardwareObj(); }

MQTTClient::MQTTClient(Logic& l) : logic(l) { }

void MQTTClient::handleMessage(const char* topicArrived, const uint8_t* payload, uint16_t length) {
  if(!topicArrived) {
    return;
  }

  if(!payload && length > 0) {
    deb("MQTT: payload was NULL");
    return;
  }

  if(length >= sizeof(msg)) {
    length = (uint16_t)(sizeof(msg) - 1);
  }
  if(length > 0) {
    memcpy(msg, payload, length);
  }
  msg[length] = '\0';

  cJSON *root = cJSON_Parse(msg);
  if(!root) {
    deb("MQTT: problem with json parsing");
    return;
  }
  // Long-running message handler: feed watchdog at each major stage to avoid
  // a hardware reset while flash commits / cJSON parsing are in flight (see
  // architectural notes about saveSwitches() doing a full sector flash erase).
  hal_watchdog_feed();

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
      hardware().wakeDisplayForEvent();
    }
  }

  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, myHostName);
  if(strcasecmp(topicArrived, topic) == 0) {
    deb("MQTT: switch update requested from broker!");

    bool shouldSave = false;
    bool anyChanged = false;
    for(int a = 0; a < getSwitchesNumber(hardware().getMyMAC()); a++) {
      char key[16];
      snprintf(key, sizeof(key), "isOn%d", a + 1);
      cJSON *value = cJSON_GetObjectItem(root, key);
      if(cJSON_IsBool(value)) {
        anyChanged |= hardware().setRelayTo(a, cJSON_IsTrue(value));
        shouldSave = true;
      }
      hal_watchdog_feed();
    }
    if(shouldSave && anyChanged) {
      hal_watchdog_feed();
      hardware().saveSwitches();
      hal_watchdog_feed();
      hardware().wakeDisplayForEvent();
    }
    publishPending = true;
  }

  cJSON_Delete(root);
  hal_watchdog_feed();
}

void MQTTClient::start(const char *brokerIP, const int port) {
  if(brokerIP == NULL || brokerIP[0] == '\0') {
    derr("invalid broker IP address!");
    return;
  }

  if (port <= 0 || port > 65535) {
    derr("MQTT: invalid broker port: %d", port);
    return;
  }

  deb("MQTT: connect attempt! %s / %d", brokerIP, port);

  if(!hal_mqtt_set_server(brokerIP, (uint16_t)port)) {
    derr("MQTT: hal_mqtt_set_server failed");
    return;
  }

  if(!hal_mqtt_set_socket_timeout(MQTT_SOCKET_MAX_TIMEOUT)) {
    derr("MQTT: hal_mqtt_set_socket_timeout failed");
  }

  if(!hal_mqtt_set_keepalive(MQTT_KEEP_ALIVE)) {
    derr("MQTT: hal_mqtt_set_keepalive failed");
  }

  const uint16_t mqttBufferSize = (uint16_t)(sizeof(response) + sizeof(topic) + 32U);
  if(!hal_mqtt_set_buffer_size(mqttBufferSize)) {
    deb("MQTT: keeping packet buffer size=%u", (unsigned)hal_mqtt_get_buffer_size());
  }

  if(!hal_mqtt_set_callback(halMqttCallback, this)) {
    derr("MQTT: hal_mqtt_set_callback failed");
    return;
  }

  reconnectTimer.begin(nullptr, MQTT_RECONNECT_TIME);
  clientInitialized = true;
  reconnect();
}

void MQTTClient::stop() {
  if (!clientInitialized) {
    return;
  }
  publishPending = false;
  clientInitialized = false;
  hal_mqtt_disconnect();
  hal_mqtt_set_callback(nullptr, nullptr);
}

void MQTTClient::publish() {
  if(hal_wifi_is_connected() && hal_mqtt_connected()) {
    long s = 0, e = 0;
    hardware().loadStartEnd(&s, &e);
    bool *switches = hardware().getSwitchesStates();
    cJSON *root = nullptr;
    char *json = nullptr;
    const char *time = nullptr;
    char strength[10];
    float ds18b20TempC = 0.0f;
    bool hasTemperature = false;

    memset(response, 0, sizeof(response));

    root = cJSON_CreateObject();
    NONULL(root);

    NONULL(cJSON_AddStringToObject(root, "status", "ok"));
    NONULL(cJSON_AddStringToObject(root, "build", BuildDateTime));
    NONULL(cJSON_AddNumberToObject(root, "dateHourStart", s));
    NONULL(cJSON_AddNumberToObject(root, "dateHourEnd", e));
    NONULL(cJSON_AddBoolToObject(root, "isOn1", switches[0]));
    NONULL(cJSON_AddBoolToObject(root, "isOn2", switches[1]));
    NONULL(cJSON_AddBoolToObject(root, "isOn3", switches[2]));
    NONULL(cJSON_AddBoolToObject(root, "isOn4", switches[3]));

    time = ntp().getTimeFormatted();
    NONULL(cJSON_AddStringToObject(root, "localTime", strlen(time) ? time : "not available yet."));
    NONULL(cJSON_AddNumberToObject(root, "localMillis", hal_millis()));

    snprintf(strength, sizeof(strength), "5/%d", hal_wifi_get_strength());
    NONULL(cJSON_AddStringToObject(root, "wifi", strength));

    hasTemperature = hardware().getDs18b20TemperatureC(&ds18b20TempC);
    NONULL(cJSON_AddBoolToObject(root, "temperatureAvailable", hasTemperature));
    if (hasTemperature) {
      NONULL(cJSON_AddNumberToObject(root, "temperatureC", (double)ds18b20TempC));
    } else {
      NONULL(cJSON_AddNullToObject(root, "temperatureC"));
    }

    json = cJSON_PrintUnformatted(root);
    NONULL(json);

    strncpy(response, json, sizeof(response) - 1);
    cJSON_free(json);
    cJSON_Delete(root);

    snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_STATUS, hardware().getMyHostname());
    deb("MQTT: topic:%s publish: %s", topic, response);

    if(!hal_mqtt_publish_str(topic, response, true)) {
      deb("MQTT: publish failed, state=%d", hal_mqtt_state());
      publishPending = true;
    }
    return;

error:
    if(json) {
      cJSON_free(json);
    }
    if(root) {
      cJSON_Delete(root);
    }
    strncpy(response, "{\"status\":\"JSON build failed\"}", sizeof(response) - 1);

    snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_STATUS, hardware().getMyHostname());
    deb("MQTT: topic:%s publish: %s", topic, response);
    if(!hal_mqtt_publish_str(topic, response, true)) {
      deb("MQTT: publish failed after JSON error, state=%d", hal_mqtt_state());
      publishPending = true;
    }
  } else {
    publishPending = true;
  }
}

void MQTTClient::prepareWatchdogEventIfNeeded() {
  if (watchdogEventPrepared) {
    return;
  }

  watchdogEventPrepared = true;

  if (!ntp().wasWatchdogResetOnBoot()) {
    return;
  }

  watchdogEventPending = true;
  watchdogEventBootCount = ntp().getWdtBootCount();
  watchdogEventLastStateBeforeReset = ntp().getLastStateBeforeReset();
  watchdogEventLastUptimeBeforeResetMs = ntp().getLastUptimeBeforeResetMs();

  deb("MQTT: watchdog reboot event prepared (wdtBootCount=%lu, lastState=%d, lastUptime=%lu ms)",
      (unsigned long)watchdogEventBootCount,
      watchdogEventLastStateBeforeReset,
      (unsigned long)watchdogEventLastUptimeBeforeResetMs);
}

bool MQTTClient::publishWatchdogEvent() {
  cJSON *root = nullptr;
  char *json = nullptr;

  memset(response, 0, sizeof(response));
  root = cJSON_CreateObject();
  NONULL(root);

  NONULL(cJSON_AddStringToObject(root, "reason", "watchdog"));
  NONULL(cJSON_AddStringToObject(root, "build", BuildDateTime));
  NONULL(cJSON_AddStringToObject(root, "hostname", hardware().getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, "mac", hardware().getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, "bootMillis", (double)hal_millis()));
  NONULL(cJSON_AddNumberToObject(root, "watchdogTimeoutMs", WATCHDOG_TIME));
  NONULL(cJSON_AddNumberToObject(root, "freeHeap", (double)hal_get_free_heap()));
  NONULL(cJSON_AddNumberToObject(root, "wdtBootCount", (double)watchdogEventBootCount));
  NONULL(cJSON_AddNumberToObject(root, "lastStateBeforeReset", watchdogEventLastStateBeforeReset));
  NONULL(cJSON_AddStringToObject(root, "lastStateBeforeResetName",
                                 ntp().getStateName(watchdogEventLastStateBeforeReset)));
  NONULL(cJSON_AddNumberToObject(root, "lastUptimeBeforeResetMs",
                                 (double)watchdogEventLastUptimeBeforeResetMs));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_EVENT, hardware().getMyHostname());
  deb("MQTT: topic:%s publish watchdog reboot event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, false)) {
    deb("MQTT: watchdog event publish failed, state=%d", hal_mqtt_state());
    return false;
  }

  return true;

error:
  if (json) {
    cJSON_free(json);
  }
  if (root) {
    cJSON_Delete(root);
  }
  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_EVENT, hardware().getMyHostname());
  if (!hal_mqtt_publish_str(topic, "{\"reason\":\"watchdog\",\"error\":\"json_build_failed\"}", false)) {
    deb("MQTT: watchdog event fallback publish failed, state=%d", hal_mqtt_state());
  }
  return false;
}

bool MQTTClient::reconnect() {
  deb("MQTT: WiFi status: %d", hal_wifi_status());

  if(hal_wifi_is_connected() && hal_wifi_has_local_ip()) {
    const char *hostName = hardware().getMyHostname();
    hal_watchdog_feed();

    if(hal_mqtt_connect_auth(hostName, MQTT_USER, MQTT_PASSWORD)) {
      hal_watchdog_feed();

      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_STATUS, hostName);
      hal_mqtt_publish_str(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_UPDATE, hostName);
      hal_mqtt_publish_str(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_TIME_SET, hostName);
      hal_mqtt_publish_str(topic, "", true);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, hostName);
      hal_mqtt_publish_str(topic, "", true);

      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_TIME_SET, hostName);
      hal_mqtt_subscribe(topic, 0);
      snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SWITCH_SET, hostName);
      hal_mqtt_subscribe(topic, 0);

      publishPending = true;

      deb("MQTT: (re)connected successfully!");

      return true;
    }
    deb("MQTT: connect failed! state=%d", hal_mqtt_state());

  } else {
    deb("MQTT: wifi is not connected!");
  }

  return false;
}

void MQTTClient::handleMQTTClient() {
  prepareWatchdogEventIfNeeded();

  if(ntp().isBrokerAvailable()) {
    if(clientInitialized) {
      if(!hal_mqtt_connected()) {
        if (reconnectTimer.available()) {
          reconnectTimer.restart();
          reconnect();
        }
      } else {
        hal_watchdog_feed();
        hal_mqtt_loop();
        hal_watchdog_feed();

        if(publishPending) {
          publishPending = false;
          publish();
        }

        if (watchdogEventPending && !watchdogEventPublished) {
          if (publishWatchdogEvent()) {
            watchdogEventPending = false;
            watchdogEventPublished = true;
          }
        }
      }
    }
  }
}
