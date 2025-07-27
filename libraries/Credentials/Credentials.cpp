#include "Credentials.h"

static void normalize_mac(const char* input, char* output, size_t output_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < output_len - 1; i++) {
        if (input[i] != ':') {
            output[j++] = tolower((unsigned char)input[i]);
        }
    }
    output[j] = '\0';
}

static char hostname[32];
static char normalized[20];
static char entry_normalized[20];

const char* getFriendlyHostname(const char* mac) {
    normalize_mac(mac, normalized, sizeof(normalized));

    for (size_t i = 0; i < mac_table_size; i++) {
        normalize_mac(mac_table[i].mac, entry_normalized, sizeof(entry_normalized));
        if (strcmp(entry_normalized, normalized) == 0) {
            return mac_table[i].hostname;
        }
    }

    size_t len = strlen(normalized);
    if (len >= 6) {
        snprintf(hostname, sizeof(hostname), "pico-%s", &normalized[len - 6]);
    } else {
        snprintf(hostname, sizeof(hostname), "pico-%s", normalized);
    }

    return hostname;
}

int getSwitchesNumber(const char* mac) {
    normalize_mac(mac, normalized, sizeof(normalized));

    for (size_t i = 0; i < mac_table_size; i++) {
        normalize_mac(mac_table[i].mac, entry_normalized, sizeof(entry_normalized));
        if (strcmp(entry_normalized, normalized) == 0) {
            return mac_table[i].switches;
        }
    }

    return 0;
}
