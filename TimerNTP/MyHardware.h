#ifndef HARDWARE_C
#define HARDWARE_C

#include "Config.h"

#include <Credentials.h>
#include <tools.h>

#include "OledDisplayFlow.h"
#include "OTAUpdates.h"

#pragma once

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
#define PIN_DS18B20 15

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define LINE_HEIGHT 10

class NTPMachine;
class MQTTClient;
class Logic;

class MyHardware {
public:
  explicit MyHardware(Logic& l);
  void start();
  void restartWiFi(void);
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
  bool setRelayTo(int index, bool state);
  bool *getSwitchesStates(void);
  void updateDisplayInNormalOperationMode(void);
  void hardwareLoop(void);
  void clearDisplay(void);
  void drawCenteredText(const char* text);
  void loadSwitches(void);
  void saveSwitches(void);
  void applyRelays(void);
  void handleOTAUpdates(void);
  void wakeDisplayForEvent(void);
  bool getDs18b20TemperatureC(float *temperatureC) const;

private:
  Logic& logic;

  NTPMachine& ntp();
  MQTTClient& mqtt();

  void updateDisplay(bool forceRefresh);
  void drawWifiSignal(uint8_t strength);
  const char* getSwitchStatus(void);
  void handleButtonRelease(int buttonIndex);
  void resetDisplayCache(void);
  void initDs18b20(void);
  void serviceDs18b20(void);

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

  SmartTimers displayTimer;
  SmartTimers blinkTimer;
  SmartTimers ds18b20RequestTimer;
  SmartTimers ds18b20InitRetryTimer;
  OledDisplayFlow oledFlow;

  int relaysPins[MAX_AMOUNT_OF_RELAYS] = {PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_3, PIN_RELAY_4};
  int buttonPins[MAX_AMOUNT_OF_RELAYS] = {PIN_BUTTON_1, PIN_BUTTON_2, PIN_BUTTON_3, PIN_BUTTON_4};
  bool lastStates[MAX_AMOUNT_OF_RELAYS];

  bool lastLights = false;
  OTAUpdates otaUpdates;

  hal_ds18b20_t ds18b20 = nullptr;
  float ds18b20TemperatureC = 0.0f;
  bool ds18b20TemperatureValid = false;
};


#endif
