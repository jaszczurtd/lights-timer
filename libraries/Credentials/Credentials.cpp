#include "Credentials.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static const char *not_found = "not_found";

static void normalize_mac(const char* input, char* output, size_t output_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < output_len - 1; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == ':' || c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        output[j++] = (char)tolower(c);
    }
    output[j] = '\0';
}

static const MacEntry* findByMac(const char* mac) {
    char normalized[20] = {0};
    normalize_mac(mac, normalized, sizeof(normalized));

    for (size_t i = 0; i < mac_table_size; i++) {
        char entry_normalized[20] = {0};
        normalize_mac(mac_table[i].mac, entry_normalized, sizeof(entry_normalized));
        if (strcmp(entry_normalized, normalized) == 0) {
            return &mac_table[i];
        }
    }
    return NULL;
}

const char* getFriendlyHostname(const char* mac) {
    static char hostname[32];

    if (const MacEntry* e = findByMac(mac)) {
        return e->hostname;
    }

    char normalized[20] = {0};
    normalize_mac(mac, normalized, sizeof(normalized));

    size_t len = strlen(normalized);
    if (len >= 6) {
        snprintf(hostname, sizeof(hostname), "pico-%s", &normalized[len - 6]);
    } else {
        snprintf(hostname, sizeof(hostname), "pico-%s", normalized);
    }

    return hostname;
}

int getSwitchesNumber(const char* mac) {
    if (const MacEntry* e = findByMac(mac)) return e->switches;
    return 0;
}

const char* getWireguardPrivateKey(const char* mac) {
    if (const MacEntry* e = findByMac(mac)) return e->wireguard_private_key;
    return not_found;
}

const char* getWireguardLocalIP(const char* mac) {
    if (const MacEntry* e = findByMac(mac)) return e->wireguard_local_ip;
    return not_found;
}
