#include "NTPMachine.h"
#include "Logic.h"
#include "MyHardware.h"
#include "MQTTClient.h"

MyHardware& NTPMachine::hardware() { return logic.hardwareObj(); }
MQTTClient& NTPMachine::mqtt() { return logic.mqttObj(); }

void NTPMachine::start() {
  currentState = STATE_NOT_CONNECTED;
  hardware().start();

  long s = 0, e = 0;
  hardware().loadStartEnd(&s, &e);
  hardware().loadSwitches();
  hardware().extractTime(s, e);
  hardware().applyRelays();
}

int NTPMachine::getCurrentState(void) {
  return currentState;
}

const char *NTPMachine::getTimeFormatted(void) {
  return (const char *)buffer;
}

void NTPMachine::stateMachine(void) {
  switch(currentState) {
    case STATE_NOT_CONNECTED: {
      deb("Not connected to WiFi. Trying to reconnect...");

      memset(buffer, 0, sizeof(buffer));

      mqtt().stop();

      hardware().restartWiFi();
      hardware().drawCenteredText("CONNECTING...");

      connectionStartTime = millis();

      currentState = STATE_CONNECTING;
    }
    break;

    case STATE_CONNECTING: {

      if(millis() - connectionStartTime > WIFI_TIMEOUT_MS) {
        deb("\nWiFi connection timeout!");
        currentState = STATE_NOT_CONNECTED;
        hardware().drawCenteredText("NO CONNECTION");
        return;
      }      

      static unsigned long last_connecting_cycle;
      if(millis() - last_connecting_cycle > 200) {
        last_connecting_cycle = millis();
        if(WIFI_CONNECTED) {
          deb("Connected to WiFi. Local IP address: %s", hardware().getMyIP());
          deb("DNS IP:%s", WiFi.dnsIP().toString().c_str());

          hardware().drawCenteredText("CONNECTED");
          hardware().configureOTAUpdates();

          setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
          tzset();
          configTime(0, 0, ntpServer1, ntpServer2);

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

        hardware().drawCenteredText("NTP SYNCHRO");

        if(time(nullptr) > 24 * 3600 * 2) {
          currentState = STATE_CONNECTED;
          ntpStartTime = 0;

          localTimeHasBeenSet = true;
          mqtt().start();
          
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
        hardware().drawCenteredText("NO CONNECTION");
      }
    }
    break;

    case STATE_CONNECTED: {
      if (WIFI_CONNECTED) {

        static unsigned long last_print_cycle;
        if(millis() - last_print_cycle > 500) {
          last_print_cycle = millis();

          static unsigned long lastSync = 0;
      
          if (millis() - lastSync > HOURS_SYNC_INTERVAL * 3600 * 1000) {
            configTime(0, 0, ntpServer1, ntpServer2);
            lastSync = millis();
          }
        }

        mqtt().handleMQTTClient();
        hardware().hardwareLoop();

      } else {
        currentState = STATE_NOT_CONNECTED;
        hardware().drawCenteredText("NO CONNECTION");
      }
    }
    break;
  }

  hardware().updateBuildInLed();
  hardware().handleOTAUpdates();

  static unsigned long lastCall = 0;
  static unsigned long last_loop_cycle;

  unsigned long now = millis();
  if (now - lastCall >= EVALUATE_TIME_FOR_RELAY_MS) {
    lastCall = now;
    if(localTimeHasBeenSet) {
      evaluateTimeCondition();
    }
  }

  if(now - last_loop_cycle > PRINT_INTERVAL_MS) {
    last_loop_cycle = now;

    if (WIFI_CONNECTED && 
        currentState >= STATE_CONNECTED) {

      deb("%s, IP:%s, host:%s, mac:%s, wifi strength: %d/5", 
          getTimeFormatted(), 
          hardware().getMyIP(), 
          getFriendlyHostname(hardware().getMyMAC()), 
          hardware().getMyMAC(), 
          hardware().getWifiStrength());
    }
  }
}

void NTPMachine::evaluateTimeCondition() {
  time_t now;
  struct tm timeinfo;

  time(&now);
  localtime_r(&now, &timeinfo);
  
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);

  now_time = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  hardware().checkConditionsForStartEnAction(now_time);
}

long NTPMachine::getTimeNow(void) {
  return now_time;
}



