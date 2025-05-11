
#include "MyHardware.h"

MyHardware::MyHardware() { }

void MyHardware::start(NTPMachine *m, MyWebServer *w) {
  ntp = m;
  web = w;

  EEPROM.begin(512);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);
}

void MyHardware::restartWiFi(void) {
  WiFi.disconnect(true);     
  WiFi.setHostname(getMyHostname());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

int MyHardware::getWifiStrength(void) {

  int32_t rssi = WiFi.RSSI();

  if (rssi >= 0) return 0; 
  if (rssi >= -50) return 5;
  if (rssi >= -60) return 4;
  if (rssi >= -70) return 3;
  if (rssi >= -80) return 2;
  if (rssi >= -90) return 1;
  return 0; 
}

const char *MyHardware::getMyIP(void) {
  snprintf(ip_str, sizeof(ip_str), "%s", (WIFI_CONNECTED) ? WiFi.localIP().toString().c_str() : "0.0.0.0");
  return (const char *)ip_str;
}

const char *MyHardware::getMyMAC(void) {
  snprintf(mac_str, sizeof(mac_str), "%s", WiFi.macAddress().c_str());
  return (const char *)mac_str;
}

const char *MyHardware::getMyHostname(void) {
  snprintf(hostname_str, sizeof(hostname_str), "%s", getFriendlyHostname(getMyMAC()));
  return (const char *)hostname_str;
}

const char*MyHardware::getAmountOfSwitches(void) {
  snprintf(switches_str, sizeof(switches_str), "SW:%d", getSwitchesNumber(getMyMAC()));
  return (const char *)switches_str;
}

void MyHardware::updateBuildInLed(void) {
  static unsigned long last_blink;
  static int prevState = -1;

  if (ntp->getCurrentState() != prevState) {
    last_blink = millis();
    digitalWrite(LED_BUILTIN, LOW);
    prevState = ntp->getCurrentState();
  }

  switch(ntp->getCurrentState()) {
    case STATE_CONNECTING:
    case STATE_NTP_SYNCHRO: {
      unsigned long interval = (ntp->getCurrentState() == STATE_CONNECTING) ? 100 : 300;
      if (millis() - last_blink > interval) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        last_blink = millis();
      }
    }
    break;
      
    case STATE_CONNECTED: {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    break;
      
    default: {
      digitalWrite(LED_BUILTIN, LOW);
    }
    break;
  }
}

void MyHardware::extractTime(long start, long end) {
  extract_time(start, &startHour, &startMinute);
  extract_time(end, &endHour, &endMinute);

  deb("set start: %02d:%02d", startHour, startMinute);
  deb("set end: %02d:%02d", endHour, endMinute);
}

void MyHardware::setTimeRange(long start, long end) {
  extractTime(start, end);
  saveStartEnd(start, end);
}

void MyHardware::saveStartEnd(long start, long end) {
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i,       (start >> (8 * i)) & 0xFF);
    EEPROM.write(i + 4,   (end >> (8 * i)) & 0xFF);
  }
  EEPROM.commit();
}

void MyHardware::loadStartEnd(long *start, long *end) {
  for (int i = 0; i < 4; i++) {
    *start |= ((long)EEPROM.read(i))     << (8 * i);
    *end |= ((long)EEPROM.read(i + 4)) << (8 * i);
  }

  if(*start > MAX_TIME || *start < 0) {
    *start = 0;
  }
  if(*end > MAX_TIME || *end < 0) {
    *end = 0;
  }

  deb("loaded: %ld %ld", *start, *end);
}

void MyHardware::checkConditionsForStartEnAction(long timeNow) {
  long start = startHour * 60 + startMinute;
  long end = endHour * 60 + endMinute;

  flagLights = is_time_in_range(timeNow, start, end);
  if(lastStateFlagLights != flagLights) {
    lastStateFlagLights = flagLights;
    //modules start action!
    setLightsTo(flagLights);
    web->updateRelaysStatesForClient();
  }
}

void MyHardware::setLightsTo(bool state) {
  switches[0] = state;

  deb("got order to set the lights to %s!", (state) ? "on" : "off");

}

bool *MyHardware::getSwitchesStates(void) {
  return switches;
}