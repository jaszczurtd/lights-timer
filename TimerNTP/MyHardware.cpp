
#include "MyHardware.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

MyHardware::MyHardware() { }

void MyHardware::start(NTPMachine *m, MyWebServer *w) {
  ntp = m;
  web = w;

  EEPROM.begin(512);

  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);

  display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR); 
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  for(int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    //relays
    pinMode(relaysPins[a], OUTPUT);
    //buttons
    pinMode(buttonPins[a], INPUT_PULLUP);
    lastStates[a] = digitalRead(buttonPins[a]);
  }
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

void MyHardware::saveSwitches(void) {
  for(int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    EEPROM.write(9 + a, switches[a]);
  }
  EEPROM.commit();
}

void MyHardware::loadStartEnd(long *start, long *end) {

  *start = *end = 0;

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

void MyHardware::loadSwitches(void) {
  for(int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    switches[a] = EEPROM.read(9 + a);
  }
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
  deb("got order to set the lights to %s!", (state) ? "on" : "off");
  setRelayTo(0, state);
}

void MyHardware::setRelayTo(int index, bool state) {
  switches[index] = state;
  deb("got order to set the relay %d to %s!", index, (state) ? "on" : "off");

  for(int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    digitalWrite(relaysPins[a], switches[a]);
  }
}

bool *MyHardware::getSwitchesStates(void) {
  return switches;
}

void MyHardware::drawWifiSignal(uint8_t strength) {

  const uint8_t barWidth = 3;
  const uint8_t spacing = 2;
  const uint8_t maxBars = 5;
  const uint8_t heights[maxBars] = {1, 3, 5, 7, 9};

  int baseX = SCREEN_WIDTH - (maxBars * (barWidth + spacing));
  int baseY = SCREEN_HEIGHT - 1;

  for (uint8_t i = 0; i < maxBars; i++) {
    int x = baseX + i * (barWidth + spacing);
    int y = baseY - heights[i] + 1;

    if (i < strength) {
      display.fillRect(x, y, barWidth, heights[i], SSD1306_WHITE);
    } else {
      display.drawRect(x, y, barWidth, heights[i], SSD1306_WHITE);
    }
  }
}

void MyHardware::hardwareLoop(void) {
  unsigned long now = millis();

  if (now - lastUpdateMillis >= updateInterval) {
    lastUpdateMillis = now;
    updateDisplay();
  }

  for (int i = 0; i < getSwitchesNumber(getMyMAC()); i++) {
    bool currentState = digitalRead(buttonPins[i]);

    if (lastStates[i] == LOW && currentState == HIGH) {
      handleButtonRelease(i);
    }

    lastStates[i] = currentState;
  }
}

void MyHardware::clearLine(int line) {
  display.fillRect(0, line * LINE_HEIGHT, SCREEN_WIDTH, LINE_HEIGHT, SSD1306_BLACK);
}

const char* MyHardware::getSwitchStatus(void) {
  static char buffer[32];
  size_t pos = 0;

  for (int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    int written = snprintf(buffer + pos, sizeof(buffer) - pos, "s%d:%d ", a + 1, switches[a] ? 1 : 0);
    if (written < 0 || written >= (int)(sizeof(buffer) - pos)) break;
    pos += written;
  }

  return buffer;
}

void MyHardware::updateDisplay(void) {
  static char lastTimes[20] = "";
  static char lastTime[20] = "";
  static char lastSwitches[32] = "";

  static char times[20];
  snprintf(times, sizeof(times), "%02d:%02d / %02d:%02d", startHour, startMinute, endHour, endMinute);

  const char* timeStr = ntp->getTimeFormatted();
  const char* switches = getSwitchStatus();

  if (strcmp(times, lastTimes) != 0) {
    clearLine(0);
    display.setCursor(0, 0);
    display.print(times);
    strncpy(lastTimes, times, sizeof(lastTimes));
  }

  if (strcmp(timeStr, lastTime) != 0) {
    clearLine(1);
    display.setCursor(0, LINE_HEIGHT);
    display.print(timeStr);
    strncpy(lastTime, timeStr, sizeof(lastTime));
  }

  if (strcmp(switches, lastSwitches) != 0) {
    clearLine(2);
    display.setCursor(0, 2 * LINE_HEIGHT);
    display.print(switches);
    strncpy(lastSwitches, switches, sizeof(lastSwitches));
  }

  drawWifiSignal(getWifiStrength());
  display.display();
}

void MyHardware::drawCenteredText(const char* text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;

  display.setCursor(x, y);
  display.print(text);
  display.display();
}

void MyHardware::handleButtonRelease(int buttonIndex) {
  deb("button action for: %d", buttonIndex);
  setRelayTo(buttonIndex, !switches[buttonIndex]);
  web->updateRelaysStatesForClient();
}
