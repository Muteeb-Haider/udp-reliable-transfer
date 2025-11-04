#include "crc32.h"

static uint32_t table[256];
static int table_init = 0;

static void init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = 0xEDB88320u ^ (c >> 1);
            else       c >>= 1;
        }
        table[i] = c;
    }
    table_init = 1;
}

uint32_t ru_crc32(const uint8_t* data, size_t len) {
    if (!table_init) init_table();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}
