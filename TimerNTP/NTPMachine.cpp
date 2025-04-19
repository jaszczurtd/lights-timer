#include "NTPMachine.h"

NTPMachine::NTPMachine() { }

void NTPMachine::start(MyHardware *h, MyWebServer *w) {
  currentState = STATE_NOT_CONNECTED;
  (hardware = h)->start(this);
  web = w;

  WiFi.setHostname(getMyHostname());
}

int NTPMachine::getCurrentState(void) {
  return currentState;
}

int NTPMachine::getWifiStrength(void) {

  int32_t rssi = WiFi.RSSI();

  if (rssi >= 0) return 0; 
  if (rssi >= -50) return 5;
  if (rssi >= -60) return 4;
  if (rssi >= -70) return 3;
  if (rssi >= -80) return 2;
  if (rssi >= -90) return 1;
  return 0; 
}

const char *NTPMachine::getTimeFormatted(void) {
  return (const char *)buffer;
}

const char *NTPMachine::getMyIP(void) {
  snprintf(ip_str, sizeof(ip_str), "%s", (WIFI_CONNECTED) ? WiFi.localIP().toString().c_str() : "0.0.0.0");
  return (const char *)ip_str;
}

const char *NTPMachine::getMyMAC(void) {
  snprintf(mac_str, sizeof(mac_str), "%s", WiFi.macAddress().c_str());
  return (const char *)mac_str;
}

const char *NTPMachine::getMyHostname(void) {
  snprintf(hostname_str, sizeof(hostname_str), "%s", getFriendlyHostname(getMyMAC()));
  return (const char *)hostname_str;
}

const char*NTPMachine::getAmountOfSwitches(void) {
  snprintf(switches_str, sizeof(switches_str), "SW:%d", getSwitchesNumber(getMyMAC()));
  return (const char *)switches_str;
}

void NTPMachine::stateMachine(void) {
  switch(currentState) {
    case STATE_NOT_CONNECTED: {
      deb("Not connected. Trying to reconnect...");

      memset(buffer, 0, sizeof(buffer));

      WiFi.disconnect(true);     
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      connectionStartTime = millis();

      currentState = STATE_CONNECTING;
    }
    break;

    case STATE_CONNECTING: {

      if(millis() - connectionStartTime > WIFI_TIMEOUT_MS) {
        deb("\n connection timeout!");
        currentState = STATE_NOT_CONNECTED;
        return;
      }      

      static unsigned long last_connecting_cycle;
      if(millis() - last_connecting_cycle > 200) {
        last_connecting_cycle = millis();
        if(WIFI_CONNECTED) {
          deb("Connected. IP address: %s", getMyIP());

          setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
          tzset();
          configTime(0, 0, ntpServer1, ntpServer2);

          web->start(this, hardware);

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
          
          strftime(buffer, sizeof(buffer), "%A, %d %B %Y %H:%M:%S", &timeinfo);
        }

        web->handleHTTPClient();

      } else {
        currentState = STATE_NOT_CONNECTED;
      }
    }
    break;
  }

  hardware->updateBuildInLed();

  static unsigned long last_loop_cycle;
  if(millis() - last_loop_cycle > PRINT_INTERVAL_MS) {
    last_loop_cycle = millis();

    if (WIFI_CONNECTED && 
        currentState >= STATE_CONNECTED) {

      deb("%s, IP:%s, host:%s, mac:%s, wifi strength: %d/5", 
          getTimeFormatted(), getMyIP(), getFriendlyHostname(getMyMAC()), getMyMAC(), getWifiStrength());
    }
  }

}



