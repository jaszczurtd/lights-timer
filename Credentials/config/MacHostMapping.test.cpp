#include "../MacHostMapping.h"

#include <hal/core/hal_array.h>

// Locally administered MAC plus RFC 5737 documentation address. These values
// are non-secret and reserved for tests.
const MacEntry mac_table[] = {
    {
        "02:00:00:00:00:01",
        "ci-lights-timer",
        4,
        "192.0.2.11",
        "ci-example-wireguard-private-key"
    }
};

const size_t mac_table_size = COUNTOF(mac_table);
