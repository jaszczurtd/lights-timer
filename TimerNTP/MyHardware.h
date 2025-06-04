#ifndef HARDWARE_C
#define HARDWARE_C

#include <EEPROM.h>
#include <Credentials.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#pragma once

#define MAX_AMOUNT_OF_RELAYS 4

#define PIN_SDA 20
#define PIN_SCL 21
#define I2C_ADDR 0x3C

#define PIN_RELAY_1 0
#define PIN_RELAY_2 1
#define PIN_RELAY_3 2
#define PIN_RELAY_4 3

#define PIN_BUTTON_1 4
#define PIN_BUTTON_2 5
#define PIN_BUTTON_3 6
#define PIN_BUTTON_4 7

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define LINE_HEIGHT 10

class NTPMachine;
class MyWebServer;
class Logic;

class MyHardware {
public:
  explicit MyHardware(Logic& l) : logic(l) {}
  void start();
  void restartWiFi(void);
  int getWifiStrength(void);
  const char *getMyIP(void);
  const char *getMyMAC(void);
  const char *getMyHostname(void);
  const char *getAmountOfSwitches(void);
  void updateBuildInLed(void);
  void extractTime(long start, long end);
  void setTimeRange(long start, long end);
  void saveStartEnd(long start, long end);
  void loadStartEnd(long *start, long *end);
  void checkConditionsForStartEnAction(long timeNow);
  void setLightsTo(bool state);
  void setRelayTo(int index, bool state);
  bool *getSwitchesStates(void);
  void hardwareLoop(void);
  void drawCenteredText(const char* text);
  void loadSwitches(void);
  void saveSwitches(void);
  void applyRelays(void);

private:
  Logic& logic;

  NTPMachine& ntp();
  MyWebServer& web();

  void updateDisplay(void);
  void clearLine(int line);
  void drawWifiSignal(uint8_t strength);
  const char* getSwitchStatus(void);
  void handleButtonRelease(int buttonIndex);

  char ip_str[sizeof("255.255.255.255") + 1];
  char mac_str[sizeof("FF:FF:FF:FF:FF:FF") + 1];
  char hostname_str[32];

  int startHour = 0;
  int startMinute = 0;
  int endHour = 0;
  int endMinute = 0;

  char switches_str[8];

  bool switches[MAX_AMOUNT_OF_RELAYS];

  char lastTimes[21] = "";
  char lastTime[21] = "";
  char lastSwitches[33] = "";

  unsigned long lastUpdateMillis = 0;
  const unsigned long updateInterval = 500; 

  int relaysPins[MAX_AMOUNT_OF_RELAYS] = {PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_3, PIN_RELAY_4};
  int buttonPins[MAX_AMOUNT_OF_RELAYS] = {PIN_BUTTON_1, PIN_BUTTON_2, PIN_BUTTON_3, PIN_BUTTON_4};
  bool lastStates[MAX_AMOUNT_OF_RELAYS];

  bool lastLights = false;
};


#endif
