#include "Logic.h"
#include <hal/codecs/cjson/cJSON.h>
#include <hal/impl/.mock/hal_mock.h>
#include <hal/impl/rp2040/drivers/flash/rp_flash_storage.h>
#include <hal/impl/rp2040/drivers/flash/rp_flash_transaction.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

uint8_t test_rp_flash[4u * 1024u * 1024u];
static jh_rp_flash_partition_t partition;
static unsigned writes;

static void check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

static void checkFlashRange(uint32_t offset, size_t size, size_t alignment) {
  check(offset % alignment == 0 && size % alignment == 0, "flash alignment");
  check(offset >= partition.flash_offset &&
            offset - partition.flash_offset <= partition.size &&
            size <= partition.size - (offset - partition.flash_offset),
        "write stays inside the EEPROM partition");
  ++writes;
}

extern "C" void flash_range_erase(uint32_t offset, size_t size) {
  checkFlashRange(offset, size, FLASH_SECTOR_SIZE);
  std::memset(test_rp_flash + offset, 0xff, size);
}

extern "C" void flash_range_program(uint32_t offset, const uint8_t *data,
                                    size_t size) {
  checkFlashRange(offset, size, FLASH_PAGE_SIZE);
  for (size_t i = 0; i < size; ++i) {
    test_rp_flash[offset + i] &= data[i];
  }
}

extern "C" hal_status_t jh_rp_flash_transaction_execute(
    jh_rp_flash_operation_t operation, void *context, uint32_t) {
  return operation(context);
}

// Public Credentials API with independent test data; no private archive needed.
extern "C" const char *getFriendlyHostname(const char *) { return "test-timer"; }
extern "C" int getSwitchesNumber(const char *) { return 4; }
extern "C" const char *getWireguardPrivateKey(const char *) { return ""; }
extern "C" const char *getWireguardLocalIP(const char *) { return "0.0.0.0"; }
extern "C" char *getCredential(Cred) {
  static char empty[] = "";
  return empty;
}
extern "C" int getCredentialInt(Cred) { return 0; }

static void expectSchedule(Logic &logic, long start, long end) {
  long loadedStart = -1, loadedEnd = -1;
  logic.hardwareObj().loadStartEnd(&loadedStart, &loadedEnd);
  check(loadedStart == start && loadedEnd == end, "stored schedule");
  logic.mqttObj().publish();
  check(std::strcmp(hal_mock_mqtt_get_last_publish_topic(),
                    "status-test-timer") == 0, "status topic");
  const auto *payload = hal_mock_mqtt_get_last_publish_payload();
  cJSON *json = cJSON_ParseWithLength(
      reinterpret_cast<const char *>(payload), hal_mock_mqtt_get_last_publish_len());
  check(json != nullptr, "status JSON");
  const cJSON *first = cJSON_GetObjectItem(json, "dateHourStart");
  const cJSON *last = cJSON_GetObjectItem(json, "dateHourEnd");
  check(cJSON_IsNumber(first) && first->valueint == start &&
            cJSON_IsNumber(last) && last->valueint == end, "published schedule");
  cJSON_Delete(json);
}

static void updateSchedule(Logic &logic, long start, long end) {
  char json[96];
  const int length = std::snprintf(json, sizeof(json),
      "{\"dateHourStart\":%ld,\"dateHourEnd\":%ld}", start, end);
  check(length > 0 && static_cast<size_t>(length) < sizeof(json), "input JSON");
  logic.mqttObj().handleMessage("time-test-timer",
      reinterpret_cast<const uint8_t *>(json), static_cast<uint16_t>(length));
  expectSchedule(logic, start, end);
}

int main() {
  std::memset(test_rp_flash, 0xff, sizeof(test_rp_flash));
  check(jh_rp_flash_storage_partition(JH_RP_FLASH_PARTITION_EEPROM, &partition) ==
            HAL_OK, "EEPROM partition");
  check(hal_eeprom_init(HAL_EEPROM_FLASH, 512, 0) == HAL_OK, "old EEPROM init");
  check(hal_kv_init_ex(0, 512) == HAL_EINVAL, "old KV layout is rejected");
  check(hal_kv_set_u32_ex(1, 1316) == HAL_EUNINIT, "old layout cannot save");
  check(writes == 0, "invalid layout leaves flash untouched");

  hal_mock_wifi_set_connected(true);
  hal_mock_mqtt_set_connected(true);
  // TimerNTP.cpp also keeps the application in static storage.
  static Logic logic;
  logic.hardwareObj().start();
  logic.hardwareObj().loadSwitches();
  expectSchedule(logic, 0, 0);
  updateSchedule(logic, 1316, 1436);
  uint16_t size = 0;
  check(hal_eeprom_size_ex(&size) == HAL_OK && size == partition.size,
        "firmware uses the reserved EEPROM size");

  logic.hardwareObj().setRelayTo(2, true);
  logic.hardwareObj().saveSwitches();
  // Reload EEPROM from flash, discarding the EEPROM mirror and KV cache.
  logic.hardwareObj().start();
  logic.hardwareObj().loadSwitches();
  expectSchedule(logic, 1316, 1436);
  check(logic.hardwareObj().getSwitchesStates()[2], "relay state survives reload");

  updateSchedule(logic, 0, 1440);
  logic.hardwareObj().start();
  expectSchedule(logic, 0, 1440);
  updateSchedule(logic, 0, 0);
  logic.hardwareObj().start();
  logic.hardwareObj().loadSwitches();
  expectSchedule(logic, 0, 0);
  check(logic.hardwareObj().getSwitchesStates()[2], "schedule preserves relay state");
  std::puts("PASS: MQTT schedule, status, flash reload and relay state");
}
