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
constexpr uint16_t KV_KEY_WDT_BOOT_COUNT = 4;
constexpr uint32_t DS18B20_REQUEST_INTERVAL_MS = (SECOND * 10);
constexpr uint32_t DS18B20_INIT_RETRY_INTERVAL_MS = (SECOND * 5);
constexpr float DS18B20_PUBLISH_DELTA_C = 0.1f;
constexpr float DS18B20_MIN_VALID_C = -55.0f;
constexpr float DS18B20_MAX_VALID_C = 125.0f;
}

NTPMachine& MyHardware::ntp() { return logic.ntpObj(); }
MQTTClient& MyHardware::mqtt() { return logic.mqttObj(); }

MyHardware::MyHardware(Logic& l) : logic(l), oledFlow(OLED_ACTIVE_WINDOW_MS) { }

void MyHardware::start() {

  hal_eeprom_init(HAL_EEPROM_RP2040, 512, 0);
  if (!hal_kv_init(0, 512)) {
    derr("hal_kv_init failed");
  }
  uint32_t loadedWdtBootCount = 0;
  if (hal_kv_get_u32(KV_KEY_WDT_BOOT_COUNT, &loadedWdtBootCount)) {
    wdtBootCount = loadedWdtBootCount;
  } else {
    wdtBootCount = 0;
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
  ds18b20InitRetryTimer.begin(nullptr, DS18B20_INIT_RETRY_INTERVAL_MS);
  oledFlow.begin(hal_millis());

  initDs18b20();
}

void MyHardware::restartWiFi(void) {
  hal_watchdog_feed();
  if (!hal_wifi_disconnect(true)) {
    derr("hal_wifi_disconnect failed");
  }
  hal_delay_ms(50);
  if (!hal_wifi_set_hostname(getMyHostname())) {
    derr("hal_wifi_set_hostname failed");
  }
  if (!hal_wifi_set_mode(HAL_WIFI_MODE_STA)) {
    derr("hal_wifi_set_mode(STA) failed");
  }
  if (!hal_wifi_begin_station(WIFI_SSID, WIFI_PASSWORD, true)) {
    derr("hal_wifi_begin_station failed");
  }
  if (!hal_wifi_set_timeout_ms(MAX_TIMEOUT)) {
    derr("hal_wifi_set_timeout_ms failed");
  }
  hal_watchdog_feed();
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
  const long oldStart = startHour * 60 + startMinute;
  const long oldEnd = endHour * 60 + endMinute;

  extractTime(start, end);
  saveStartEnd(start, end);

  if (oldStart != start || oldEnd != end) {
    wakeDisplayForEvent();
  }
}

void MyHardware::saveStartEnd(long start, long end) {
  // Coalesce both KV writes into a single flash commit (one sector erase
  // instead of two) to reduce flash wear and shorten the IRQ-off window.
  hal_kv_set_auto_commit(false);
  if (!hal_kv_set_u32(KV_KEY_START_MIN, (uint32_t)start)) {
    derr("KV save failed for start=%ld", start);
  }
  if (!hal_kv_set_u32(KV_KEY_END_MIN, (uint32_t)end)) {
    derr("KV save failed for end=%ld", end);
  }
  hal_kv_commit();
  hal_kv_set_auto_commit(true);
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

  const bool desiredLightsState = is_time_in_range(timeNow, start, end);

  // Scheduler has lower priority than user actions:
  // apply only on schedule edge transitions (enter/leave window), not continuously.
  if (lastLights != desiredLightsState) {
    const bool changed = setLightsTo(desiredLightsState);
    if (changed) {
      saveSwitches();
      mqtt().requestPublish();
    }
  }

  lastLights = desiredLightsState;
}

bool MyHardware::setLightsTo(bool state) {
  if (getSwitchesNumber(getMyMAC()) <= 0) {
    return false;
  }

  deb("got order to set the lights to %s!", (state) ? "on" : "off");
  return setRelayTo(0, state);
}

bool MyHardware::setRelayTo(int index, bool state) {
  if (index < 0 || index >= MAX_AMOUNT_OF_RELAYS) {
    return false;
  }

  const bool changed = (switches[index] != state);
  switches[index] = state;
  deb("got order to set the relay %d to %s! changed=%s",
      index, state ? "on" : "off", changed ? "true" : "false");
  hal_gpio_write(relaysPins[index], switches[index]);

  if (changed) {
    wakeDisplayForEvent();
  }

  return changed;
}

void MyHardware::applyRelays(bool allowSchedulerRelayRestore) {
  int relaysCount = getSwitchesNumber(getMyMAC());
  if (relaysCount < 0) {
    relaysCount = 0;
  } else if (relaysCount > MAX_AMOUNT_OF_RELAYS) {
    relaysCount = MAX_AMOUNT_OF_RELAYS;
  }

  if (!allowSchedulerRelayRestore && relaysCount > 0 && switches[0]) {
    // Fail-safe: after reboot, do not restore relay 0 to ON until time
    // is synchronized and scheduler is evaluated.
    switches[0] = false;
    deb("boot fail-safe: suppressing restored relay 0 ON state");
  }

  for(int a = 0; a < relaysCount; a++) {
    hal_gpio_write(relaysPins[a], switches[a]);
    deb("apply relay %d as %s", a, switches[a] ? "on" : "off");
  }

  if (relaysCount > 0) {
    lastLights = switches[0];
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

    tm local = {};
    tm* localPtr = nullptr;
    if (hal_time_get_local(&local)) {
      localPtr = &local;
    }

    oledFlow.update(hal_millis(), localPtr);

    if (oledFlow.consumeJustWentToSleep()) {
      clearDisplay();
      hal_display_flush();
      return;
    }

    if (!oledFlow.isAwake()) {
      return;
    }

    updateDisplay(oledFlow.consumeForceRedraw());
  }
}

void MyHardware::hardwareLoop(void) {
  serviceDs18b20();

  for (int i = 0; i < getSwitchesNumber(getMyMAC()); i++) {
    bool currentState = hal_gpio_read(buttonPins[i]);

    if (lastStates[i] == false && currentState == true) {
      handleButtonRelease(i);
    }

    lastStates[i] = currentState;
  }
}

void MyHardware::initDs18b20(void) {
  if (ds18b20) {
    return;
  }

  hal_ds18b20_config_t cfg = {};
  cfg.data_pin = PIN_DS18B20;
  cfg.use_rom = false;
  cfg.resolution_hint = HAL_DS18B20_RES_12_BIT;

  ds18b20 = hal_ds18b20_init(&cfg);
  if (!ds18b20) {
    derr("DS18B20 init failed on GPIO %d", PIN_DS18B20);
    return;
  }

  deb("DS18B20 initialized on GPIO %d", PIN_DS18B20);

  ds18b20RequestTimer.begin(nullptr, DS18B20_REQUEST_INTERVAL_MS);
  if (!hal_ds18b20_request(ds18b20)) {
    derr("DS18B20 first request failed on GPIO %d", PIN_DS18B20);
  }
}

void MyHardware::serviceDs18b20(void) {
  if (!ds18b20) {
    if (ds18b20InitRetryTimer.available()) {
      ds18b20InitRetryTimer.restart();
      initDs18b20();
    }
    return;
  }

  hal_ds18b20_poll(ds18b20);

  float latestTemp = 0.0f;
  bool fresh = false;
  if (hal_ds18b20_take_latest(ds18b20, &latestTemp, &fresh)) {
    if (latestTemp >= DS18B20_MIN_VALID_C && latestTemp <= DS18B20_MAX_VALID_C) {
      float delta = latestTemp - ds18b20TemperatureC;
      if (delta < 0.0f) {
        delta = -delta;
      }

      const bool firstSample = !ds18b20TemperatureValid;
      ds18b20TemperatureC = latestTemp;
      ds18b20TemperatureValid = true;

      if (fresh && (firstSample || delta >= DS18B20_PUBLISH_DELTA_C)) {
        mqtt().requestPublish();
      }
    } else {
      deb("DS18B20 sample out of range: %.4f C", (double)latestTemp);
    }
  }

  if (ds18b20RequestTimer.available()) {
    ds18b20RequestTimer.restart();
    if (!hal_ds18b20_is_busy(ds18b20) && !hal_ds18b20_request(ds18b20)) {
      deb("DS18B20 request skipped/failed on GPIO %d", PIN_DS18B20);
    }
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

void MyHardware::updateDisplay(bool forceRefresh) {
  static char times[20];
  snprintf(times, sizeof(times), "%02d:%02d / %02d:%02d", startHour, startMinute, endHour, endMinute);

  const char* timeStr = ntp().getTimeFormatted();
  const char* switches = getSwitchStatus();

  if (forceRefresh) {
    resetDisplayCache();
    clearDisplay();
  }

  if (strcmp(times, lastTimes) != 0) {
    hal_display_print_line(0, LINE_HEIGHT, times, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastTimes, times, sizeof(lastTimes));
    lastTimes[sizeof(lastTimes) - 1] = '\0';
  }

  if (strcmp(timeStr, lastTime) != 0) {
    hal_display_print_line(1, LINE_HEIGHT, timeStr, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastTime, timeStr, sizeof(lastTime));
    lastTime[sizeof(lastTime) - 1] = '\0';
  }

  if (strcmp(switches, lastSwitches) != 0) {
    hal_display_print_line(2, LINE_HEIGHT, switches, true, HAL_COLOR_WHITE, HAL_COLOR_BLACK);
    strncpy(lastSwitches, switches, sizeof(lastSwitches));
    lastSwitches[sizeof(lastSwitches) - 1] = '\0';
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
  wakeDisplayForEvent();
  hal_display_draw_text_centered(text, HAL_COLOR_WHITE, HAL_COLOR_BLACK, true, true);
}

void MyHardware::handleButtonRelease(int buttonIndex) {
  if (!oledFlow.onButtonRelease(hal_millis())) {
    deb("button %d woke display only", buttonIndex);
    return;
  }

  deb("button action for: %d", buttonIndex);
  setRelayTo(buttonIndex, !switches[buttonIndex]);
  mqtt().requestPublish();
  saveSwitches();
}

void MyHardware::wakeDisplayForEvent(void) {
  oledFlow.wake(hal_millis());
}

void MyHardware::resetDisplayCache(void) {
  memset(lastTimes, 0, sizeof(lastTimes));
  memset(lastTime, 0, sizeof(lastTime));
  memset(lastSwitches, 0, sizeof(lastSwitches));
}

void MyHardware::handleOTAUpdates(void) {
  otaUpdates.handle(hal_wifi_is_connected(), getMyHostname());
}

bool MyHardware::getDs18b20TemperatureC(float *temperatureC) const {
  if (!temperatureC || !ds18b20TemperatureValid) {
    return false;
  }

  *temperatureC = ds18b20TemperatureC;
  return true;
}

uint32_t MyHardware::markWatchdogBootAndGetCount(bool watchdogBoot) {
  if (!watchdogBoot) {
    return wdtBootCount;
  }

  if (wdtBootCount != UINT32_MAX) {
    wdtBootCount++;
  }

  if (!hal_kv_set_u32(KV_KEY_WDT_BOOT_COUNT, wdtBootCount)) {
    derr("KV save failed for watchdog boot count=%lu", (unsigned long)wdtBootCount);
  }

  return wdtBootCount;
}

uint32_t MyHardware::getWdtBootCount() const {
  return wdtBootCount;
}
