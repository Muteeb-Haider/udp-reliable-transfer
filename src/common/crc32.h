#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t ru_crc32(const uint8_t* data, size_t len);

#endif /* CRC32_H */
