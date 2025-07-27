
#include "MacHostMapping.h"

const MacHostnamePair mac_table[] = {
    { "28:cd:c1:05:b8:76", "akwarium_duże_w_salonie", 1 },
    { "28:cd:c1:05:b8:64", "akwarium_bojowniki_w_salonie", 2 },
    { "28:cd:c1:05:b8:7a", "akwaria_krewetki_w_sypialni", 3},
    { "28:cd:c1:05:b8:74", "akwarium_krewetki_w_kuchni", 1},
    { "28:cd:c1:0e:4b:9d", "akwarium_babki_w_dziupli", 1},
    { "28:cd:c1:08:dd:e4", "urządzenie_bardzo_testowe", 2},
};

const size_t mac_table_size = sizeof(mac_table) / sizeof(mac_table[0]);
