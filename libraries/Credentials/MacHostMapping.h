#ifndef MACHOSTMAPPING_C
#define MACHOSTMAPPING_C

#pragma once

#include "Credentials.h"

typedef struct {
    const char* mac;
    const char* hostname;
    int switches;
} MacHostnamePair;

extern const MacHostnamePair mac_table[];
extern const size_t mac_table_size;

#endif
