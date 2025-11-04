#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define VERSION 1
#define HEADER_SIZE 20

typedef enum {
    PT_HANDSHAKE = 0,
    PT_HANDSHAKE_ACK = 1,
    PT_DATA = 2,
    PT_ACK = 3,
    PT_FIN = 4,
    PT_FIN_ACK = 5,
    PT_ERROR = 6
} PacketType;

typedef struct {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t ptype;
    uint32_t seq;
    uint32_t total;
    uint16_t length;
    uint16_t window;
    uint32_t checksum; /* CRC32 for DATA, 0 for control */
    uint8_t* payload;
    size_t payload_size;
} Packet;

/* Function declarations */
uint8_t* pack(const Packet* p, size_t* packed_size);
int unpack(const uint8_t* buf, size_t n, Packet* p);
void free_packet(Packet* p);

#endif /* PROTOCOL_H */
