#ifndef MACHOSTMAPPING_C
#define MACHOSTMAPPING_C

#pragma once

#include <stddef.h>

typedef struct {
    const char *mac;
    const char *hostname;
    int switches;
    const char *wireguard_local_ip;
    const char *wireguard_private_key;
} MacEntry;

extern const MacEntry mac_table[];
extern const size_t mac_table_size;

#endif
