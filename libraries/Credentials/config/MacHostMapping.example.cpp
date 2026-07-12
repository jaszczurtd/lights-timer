#include "../MacHostMapping.h"

// Copy this file to MacHostMapping.local.cpp and enter the MAC address of your
// board. The private key is returned as a newly allocated string and is erased
// and freed by the Tracker after WireGuard initialization.
const MacEntry mac_table[] = {
    {
        "AA:BB:CC:DD:EE:01",
        "lights-timer",
        4,
        "10.8.0.11",
        "YOUR_DEVICE_WIREGUARD_PRIVATE_KEY"
    }
};

const size_t mac_table_size = sizeof(mac_table) / sizeof(mac_table[0]);
