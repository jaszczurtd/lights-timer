
#include "MyWebServer.h"
#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"
#include <Credentials.h>

NTPMachine& MQTTClient::ntp() { return logic.ntpObj(); }
MyHardware& MQTTClient::hardware() { return logic.hardwareObj(); }

MQTTClient::MQTTClient(Logic& l) : logic(l), mqttClient(currentClient), currentClient() { }



void callback(char* topic, byte* payload, unsigned int length) {


}

void MQTTClient::start() {
  deb("MQTT: connect attempt! %s", MQTT_BROKER);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callback);
  reconnect();
  clientInitialized = true;
}

void MQTTClient::updateRelaysStatesForClient() {
  deb("MQTT: updateRelaysStatesForClient");

}

void MQTTClient::setTimeRangeForResponses(long s, long e) {
  deb("MQTT: setTimeRangeForResponses: %d %d", s, e);


}

bool MQTTClient::reconnect() {
  deb("MQTT: WiFi status: %d", WiFi.status());
  if(WIFI_CONNECTED) {
    watchdog_update();
    if (mqttClient.connect(hardware().getMyHostname(), MQTT_USER, MQTT_PASSWORD)) {
      watchdog_update();

      //pub relays & topic
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
    }
  } else {
      mqttClient.loop();
  }

}
