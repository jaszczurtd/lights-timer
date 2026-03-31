#include "Credentials.h"

#include <hal/hal.h>
#include "MyHardware.h"
#include "NTPMachine.h"
#include "MQTTClient.h"
#include "Logic.h"

#include <string.h>

namespace {
constexpr uint16_t KV_KEY_START_MIN = 1;
constexpr uint16_t KV_KEY_END_MIN = 2;
constexpr uint16_t KV_KEY_SWITCHES = 3;
constexpr uint32_t OTA_INIT_RETRY_MS = 5000;
}

NTPMachine& MyHardware::ntp() { return logic.ntpObj(); }
MQTTClient& MyHardware::mqtt() { return logic.mqttObj(); }

MyHardware::MyHardware(Logic& l) : logic(l) { }

void MyHardware::start() {

  hal_eeprom_init(HAL_EEPROM_RP2040, 512, 0);
  if (!hal_kv_init(0, 512)) {
    derr("hal_kv_init failed");
  }

  hal_i2c_init(PIN_SDA, PIN_SCL, 400000);

  hal_gpio_set_mode(LED_BUILTIN, HAL_GPIO_OUTPUT);
  hal_gpio_write(LED_BUILTIN, false);

  bool displayReady = hal_display_init_ssd1306_i2c(
      SCREEN_WIDTH,
      SCREEN_HEIGHT,
      I2C_ADDR,
      -1,
      HAL_DISPLAY_VCC_SWITCHCAP,
      false);
  if (!displayReady) {
    derr("SSD1306 init failed at addr 0x%02X", I2C_ADDR);
  }
  clearDisplay();
  drawCenteredText("NO CONNECTION");

  for(int a = 0; a < getSwitchesNumber(getMyMAC()); a++) {
    //relays
    hal_gpio_set_mode(relaysPins[a], HAL_GPIO_OUTPUT);
    //buttons
    hal_gpio_set_mode(buttonPins[a], HAL_GPIO_INPUT_PULLUP);
    lastStates[a] = true;
  }

  displayTimer.begin(nullptr, 500);
  blinkTimer.begin(nullptr, 100);
}

void MyHardware::restartWiFi(void) {
  hal_wifi_disconnect(true);
  hal_delay_ms(50);
  hal_wifi_set_hostname(getMyHostname());
  hal_wifi_set_mode(HAL_WIFI_MODE_STA);
  hal_wifi_begin_station(WIFI_SSID, WIFI_PASSWORD, true);
}

const char *MyHardware::getMyIP(void) {
  if (!hal_wifi_is_connected() || !hal_wifi_get_local_ip(ip_str, sizeof(ip_str))) {
    snprintf(ip_str, sizeof(ip_str), "%s", "0.0.0.0");
  }
  return (const char *)ip_str;
}

const char *MyHardware::getMyMAC(void) {
  if (!hal_wifi_get_mac(mac_str, sizeof(mac_str))) {
    snprintf(mac_str, sizeof(mac_str), "%s", "00:00:00:00:00:00");
  }
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
  static int prevState = -1;

  if (ntp().getCurrentState() != prevState) {
    blinkTimer.restart();
    hal_gpio_write(LED_BUILTIN, false);
    prevState = ntp().getCurrentState();
  }

  switch(ntp().getCurrentState()) {
    case STATE_CONNECTING:
    case STATE_NTP_SYNCHRO: {
      unsigned long interval = (ntp().getCurrentState() == STATE_CONNECTING) ? 100 : 300;
      blinkTimer.time(interval);
      if (blinkTimer.available()) {
        blinkTimer.restart();
        hal_gpio_write(LED_BUILTIN, !hal_gpio_read(LED_BUILTIN));
      }
    }
    break;

    case STATE_CONNECTED: {
      hal_gpio_write(LED_BUILTIN, true);
    }
    break;

    default: {
      hal_gpio_write(LED_BUILTIN, false);
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
  if (!hal_kv_set_u32(KV_KEY_START_MIN, (uint32_t)start)) {
    derr("KV save failed for start=%ld", start);
  }
  if (!hal_kv_set_u32(KV_KEY_END_MIN, (uint32_t)end)) {
    derr("KV save failed for end=%ld", end);
  }
}

void MyHardware::saveSwitches(void) {
  uint8_t packed[MAX_AMOUNT_OF_RELAYS] = {0};
  for(int a = 0; a < MAX_AMOUNT_OF_RELAYS; a++) {
    packed[a] = switches[a] ? 1U : 0U;
    deb("saved %d switch as %s", a, switches[a] ? "on" : "off");
  }

  if (!hal_kv_set_blob(KV_KEY_SWITCHES, packed, (uint16_t)MAX_AMOUNT_OF_RELAYS)) {
    derr("KV save failed for switches");
  }
}

void MyHardware::loadStartEnd(long *start, long *end) {
  uint32_t start_u32 = 0;
  uint32_t end_u32 = 0;

  if (!hal_kv_get_u32(KV_KEY_START_MIN, &start_u32)) {
    start_u32 = 0;
  }
  if (!hal_kv_get_u32(KV_KEY_END_MIN, &end_u32)) {
    end_u32 = 0;
  }

  *start = (long)start_u32;
  *end = (long)end_u32;

  if(*start > MAX_TIME || *start < 0) {
    *start = 0;
  }
  if(*end > MAX_TIME || *end < 0) {
    *end = 0;
  }

  deb("loaded: %ld %ld", *start, *end);
}

void MyHardware::loadSwitches(void) {
  memset(switches, 0, sizeof(switches));

  uint8_t packed[MAX_AMOUNT_OF_RELAYS] = {0};
  uint16_t loaded_len = 0;
  if (hal_kv_get_blob(KV_KEY_SWITCHES, packed, (uint16_t)MAX_AMOUNT_OF_RELAYS, &loaded_len)) {
    if (loaded_len > MAX_AMOUNT_OF_RELAYS) {
      loaded_len = MAX_AMOUNT_OF_RELAYS;
    }
    for (uint16_t a = 0; a < loaded_len; a++) {
      switches[a] = (packed[a] != 0);
    }
  }

  for(int a = 0; a < MAX_AMOUNT_OF_RELAYS; a++) {
    deb("loaded %d switch as %s", a, switches[a] ? "on" : "off");
  }
}

void MyHardware::checkConditionsForStartEnAction(long timeNow) {
  long start = startHour * 60 + startMinute;
  long end = endHour * 60 + endMinute;

  bool flagLights = is_time_in_range(timeNow, start, end);
  if(lastLights != flagLights) {
    lastLights = flagLights;
    //modules start action!
    setLightsTo(flagLights);
    mqtt().publish();
  }
}

void MyHardware::setLightsTo(bool state) {
  deb("got order to set the lights to %s!", (state) ? "on" : "off");
  setRelayTo(0, state);
}

void MyHardware::setRelayTo(int index, bool state) {
  switches[index] = state;
  deb("got order to set the relay %d to %s!", index, (state) ? "on" : "off");
  hal_gpio_write(relaysPins[index], switches[index]);
}

void MyHardware::applyRelays(void) {
  for(int a = 1; a < MAX_AMOUNT_OF_RELAYS; a++) {
    hal_gpio_write(relaysPins[a], switches[a]);
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
      hal_display_fill_rect(x, y, barWidth, heights[i], HAL_COLOR_WHITE);
    } else {
      hal_display_draw_rect(x, y, barWidth, heights[i], HAL_COLOR_WHITE);
    }
  }
}

void MyHardware::updateDisplayInNormalOperationMode(void) {
  if (displayTimer.available()) {
    displayTimer.restart();
    updateDisplay();
  }
}

void MyHardware::hardwareLoop(void) {
  for (int i = 0; i < getSwitchesNumber(getMyMAC()); i++) {
    bool currentState = hal_gpio_read(buttonPins[i]);

    if (lastStates[i] == false && currentState == true) {
      handleButtonRelease(i);
    }

    lastStates[i] = currentState;
  }
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

  const char* timeStr = ntp().getTimeFormatted();
  const char* switches = getSwitchStatus();

  if (strcmp(times, lastTimes) != 0) {
    hal_display_print_line(0, LINE_HEIGHT, times, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastTimes, times, sizeof(lastTimes));
  }

  if (strcmp(timeStr, lastTime) != 0) {
    hal_display_print_line(1, LINE_HEIGHT, timeStr, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastTime, timeStr, sizeof(lastTime));
  }

  if (strcmp(switches, lastSwitches) != 0) {
    hal_display_print_line(2, LINE_HEIGHT, switches, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastSwitches, switches, sizeof(lastSwitches));
  }

  drawWifiSignal(hal_wifi_get_strength());
  hal_display_flush();
}

void MyHardware::clearDisplay(void) {
  hal_display_fill_screen(HAL_COLOR_BLACK);
  hal_display_set_text_size(1);
  hal_display_set_text_color(HAL_COLOR_WHITE);
}

void MyHardware::drawCenteredText(const char* text) {
  hal_display_draw_text_centered(text, HAL_COLOR_WHITE, HAL_COLOR_BLACK, true, true);
}

void MyHardware::handleButtonRelease(int buttonIndex) {
  deb("button action for: %d", buttonIndex);
  setRelayTo(buttonIndex, !switches[buttonIndex]);
  mqtt().publish();
  saveSwitches();
}

void MyHardware::configureOTAUpdates(void) {
  if (otaActive) {
    return;
  }

  const uint32_t now = hal_millis();
  if ((int32_t)(now - otaRetryAtMs) < 0) {
    return;
  }

  if (!LittleFS.begin()) {
    deb("LittleFS mount failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    otaActive = false;
    otaRetryAtMs = now + OTA_INIT_RETRY_MS;
    return;
  }

  ArduinoOTA.onStart([]() {
    const char *type = (ArduinoOTA.getCommand() == U_FS) ? "filesystem" : "sketch";
    deb("OTA start: %s", type);
  });
  ArduinoOTA.onEnd([]() {
    deb("OTA end: success");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) {
      return;
    }
    deb("OTA progress: %u%%", (progress * 100U) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    switch (error) {
      case OTA_AUTH_ERROR:    derr("OTA error: auth"); break;
      case OTA_BEGIN_ERROR:   derr("OTA error: begin"); break;
      case OTA_CONNECT_ERROR: derr("OTA error: connect"); break;
      case OTA_RECEIVE_ERROR: derr("OTA error: receive"); break;
      case OTA_END_ERROR:     derr("OTA error: end"); break;
      default:                derr("OTA error: unknown=%u", (unsigned)error); break;
    }
  });

  ArduinoOTA.setPort(OTA_UPDATE_PORT);
  char hostname_ascii[64];
  remove_non_ascii(getMyHostname(), hostname_ascii, sizeof(hostname_ascii));
  ArduinoOTA.setHostname(hostname_ascii);
  ArduinoOTA.setPassword(OTA_UPDATE_PASSWORD);
  ArduinoOTA.begin();

  otaActive = true;
  otaRetryAtMs = 0;
  deb("OTA ready: host=%s port=%d", hostname_ascii, OTA_UPDATE_PORT);
}

void MyHardware::handleOTAUpdates(void) {
  if (!hal_wifi_is_connected()) {
    return;
  }

  if (!otaActive) {
    configureOTAUpdates();
    return;
  }

  ArduinoOTA.handle();
}

