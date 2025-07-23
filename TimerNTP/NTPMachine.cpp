#include "NTPMachine.h"
#include "Logic.h"
#include "MyHardware.h"
#include "MyWebServer.h"

MyHardware& NTPMachine::hardware() { return logic.hardwareObj(); }
MQTTClient& NTPMachine::mqtt() { return logic.mqttObj(); }

void NTPMachine::start() {
  currentState = STATE_NOT_CONNECTED;
  hardware().start();
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
      deb("Not connected. Trying to reconnect...");

      memset(buffer, 0, sizeof(buffer));

      hardware().restartWiFi();
      hardware().drawCenteredText("CONNECTING...");
      mqtt().stop();
      
      connectionStartTime = millis();

      currentState = STATE_CONNECTING;
    }
    break;

    case STATE_CONNECTING: {

      if(millis() - connectionStartTime > WIFI_TIMEOUT_MS) {
        deb("\n connection timeout!");
        currentState = STATE_NOT_CONNECTED;
        hardware().drawCenteredText("NO CONNECTION");
        return;
      }      

      static unsigned long last_connecting_cycle;
      if(millis() - last_connecting_cycle > 200) {
        last_connecting_cycle = millis();
        if(WIFI_CONNECTED) {
          deb("Connected. IP address: %s", hardware().getMyIP());

          hardware().drawCenteredText("CONNECTED");

          setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
          tzset();
          configTime(0, 0, ntpServer1, ntpServer2);

          long s = 0, e = 0;
          hardware().loadStartEnd(&s, &e);
          hardware().loadSwitches();
          hardware().extractTime(s, e);
          hardware().applyRelays();

          mqtt().start();

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

          time_t now;
          struct tm timeinfo;

          static unsigned long lastSync = 0;
      
          if (millis() - lastSync > HOURS_SYNC_INTERVAL * 3600 * 1000) {
            configTime(0, 0, ntpServer1, ntpServer2);
            lastSync = millis();
          }

          time(&now);
          localtime_r(&now, &timeinfo);
          
          strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);

          now_time = timeinfo.tm_hour * 60 + timeinfo.tm_min;
          hardware().checkConditionsForStartEnAction(now_time);
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

  static unsigned long last_loop_cycle;
  if(millis() - last_loop_cycle > PRINT_INTERVAL_MS) {
    last_loop_cycle = millis();

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

long NTPMachine::getTimeNow(void) {
  return now_time;
}



