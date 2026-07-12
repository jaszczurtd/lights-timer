#include "Credentials.h"
#include "MacHostMapping.h"
#include "config/CredentialsData.local.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(CREDENTIALS_LOCAL_CONFIGURED) || CREDENTIALS_LOCAL_CONFIGURED != 1
#error "Configure config/CredentialsData.local.h and set CREDENTIALS_LOCAL_CONFIGURED to 1"
#endif

static char *duplicateString(const char *value) {
    if (value == NULL) {
        return NULL;
    }
    const size_t size = strlen(value) + 1u;
    char *copy = (char *)malloc(size);
    if (copy != NULL) {
        memcpy(copy, value, size);
    }
    return copy;
}

static void normalizeMac(const char *input, char *output, size_t output_size) {
    if (input == NULL || output == NULL || output_size == 0u) {
        return;
    }
    size_t j = 0u;
    for (size_t i = 0u; input[i] != '\0' && j + 1u < output_size; ++i) {
        const unsigned char c = (unsigned char)input[i];
        if (c != ':' && c != '-' && !isspace(c)) {
            output[j++] = (char)tolower(c);
        }
    }
    output[j] = '\0';
}

static const MacEntry *findByMac(const char *mac) {
    char normalized[20] = {};
    normalizeMac(mac, normalized, sizeof(normalized));
    for (size_t i = 0u; i < mac_table_size; ++i) {
        char candidate[20] = {};
        normalizeMac(mac_table[i].mac, candidate, sizeof(candidate));
        if (strcmp(candidate, normalized) == 0) {
            return &mac_table[i];
        }
    }
    return NULL;
}

void initCredentials(void) {}

char *getCredential(Cred cred) {
    if (cred < 0 || cred >= CR_LAST) {
        return NULL;
    }
    return duplicateString(CREDENTIAL_VALUES[cred]);
}

int getCredentialInt(Cred cred) {
    char *value = getCredential(cred);
    if (value == NULL) {
        return -1;
    }
    const int result = atoi(value);
    free(value);
    return result;
}

const char *getFriendlyHostname(const char *mac) {
    static char fallback[32];
    const MacEntry *entry = findByMac(mac);
    if (entry != NULL) {
        return entry->hostname;
    }

    char normalized[20] = {};
    normalizeMac(mac, normalized, sizeof(normalized));
    const size_t length = strlen(normalized);
    const char *suffix = length >= 6u ? &normalized[length - 6u] : normalized;
    snprintf(fallback, sizeof(fallback), "pico-%s", suffix);
    return fallback;
}

const char *getWireguardPrivateKey(const char *mac) {
    const MacEntry *entry = findByMac(mac);
    return entry != NULL ? duplicateString(entry->wireguard_private_key) : NULL;
}

const char *getWireguardLocalIP(const char *mac) {
    const MacEntry *entry = findByMac(mac);
    return entry != NULL ? entry->wireguard_local_ip : NULL;
}

int getSwitchesNumber(const char *mac) {
    const MacEntry *entry = findByMac(mac);
    return entry != NULL ? entry->switches : 0;
}
