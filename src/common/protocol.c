#include "protocol.h"
#include "crc32.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>

uint8_t* pack(const Packet* p, size_t* packed_size) {
    *packed_size = HEADER_SIZE + p->payload_size;
    uint8_t* out = malloc(*packed_size);
    if (!out) return NULL;

    out[0] = p->magic0;
    out[1] = p->magic1;
    out[2] = p->version;
    out[3] = p->ptype;

    uint32_t seq_n = htonl(p->seq);
    uint32_t total_n = htonl(p->total);
    uint16_t len_n = htons((uint16_t)p->payload_size);
    uint16_t win_n = htons(p->window);
    uint32_t chk = p->checksum;

    /* Calculate checksum for DATA packets if not already set */
    if (p->ptype == PT_DATA && chk == 0 && p->payload && p->payload_size > 0) {
        chk = ru_crc32(p->payload, p->payload_size);
    }
    uint32_t chk_n = htonl(chk);

    memcpy(&out[4], &seq_n, 4);
    memcpy(&out[8], &total_n, 4);
    memcpy(&out[12], &len_n, 2);
    memcpy(&out[14], &win_n, 2);
    memcpy(&out[16], &chk_n, 4);

    if (p->payload_size > 0 && p->payload) {
        memcpy(&out[HEADER_SIZE], p->payload, p->payload_size);
    }
    return out;
}

int unpack(const uint8_t* buf, size_t n, Packet* p) {
    if (n < HEADER_SIZE) return -1; /* short packet */
    
    p->magic0 = buf[0];
    p->magic1 = buf[1];
    p->version = buf[2];
    p->ptype = buf[3];
    
    if (p->magic0 != 'R' || p->magic1 != 'U' || p->version != VERSION) {
        return -2; /* bad magic/version */
    }
    
    uint32_t seq_n, total_n, chk_n;
    uint16_t len_n, win_n;
    memcpy(&seq_n, &buf[4], 4);
    memcpy(&total_n, &buf[8], 4);
    memcpy(&len_n, &buf[12], 2);
    memcpy(&win_n, &buf[14], 2);
    memcpy(&chk_n, &buf[16], 4);

    p->seq = ntohl(seq_n);
    p->total = ntohl(total_n);
    p->length = ntohs(len_n);
    p->window = ntohs(win_n);
    p->checksum = ntohl(chk_n);

    if (HEADER_SIZE + p->length > n) return -3; /* length mismatch */
    
    p->payload_size = p->length;
    if (p->payload_size > 0) {
        p->payload = malloc(p->payload_size);
        if (!p->payload) return -4; /* memory allocation failed */
        memcpy(p->payload, &buf[HEADER_SIZE], p->payload_size);
    } else {
        p->payload = NULL;
    }
    
    return 0; /* success */
}

void free_packet(Packet* p) {
    if (p->payload) {
        free(p->payload);
        p->payload = NULL;
    }
    p->payload_size = 0;
}
