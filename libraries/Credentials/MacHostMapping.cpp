
#include "MacHostMapping.h"

// To add a new device mapping, read the board MAC first using:
// TimerNTP/tools/ReadPicoMac/ReadPicoMac.ino
// Then copy the printed MAC (format: XX:XX:XX:XX:XX:XX) into a new entry below.

const MacEntry mac_table[] = {
    { "AA:BB:CC:DD:EE:01", "pico_device_01", 1, "10.8.0.11", "REPLACE_WITH_DEVICE_01_WG_PRIVATE_KEY_BASE64" },
    { "AA:BB:CC:DD:EE:02", "pico_device_02", 2, "10.8.0.12", "REPLACE_WITH_DEVICE_02_WG_PRIVATE_KEY_BASE64" },
};

const size_t mac_table_size = sizeof(mac_table) / sizeof(mac_table[0]);
