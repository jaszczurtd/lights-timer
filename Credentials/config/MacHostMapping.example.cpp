#include "../MacHostMapping.h"

#include <hal/core/hal_array.h>

// Copy this file to MacHostMapping.local.cpp and enter the MAC address of each
// controller. The private key is returned as a newly allocated string that the
// caller owns.
const MacEntry mac_table[] = {
    {
        "AA:BB:CC:DD:EE:01",
        "lights-timer",
        4,
        "10.8.0.11",
        "YOUR_DEVICE_WIREGUARD_PRIVATE_KEY"
    }
};

const size_t mac_table_size = COUNTOF(mac_table);
