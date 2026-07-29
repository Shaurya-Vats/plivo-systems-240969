#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#define FEC_GROUP 2
#define FRAME_PAYLOAD_SIZE 160

#pragma pack(push, 1)
typedef struct {
    uint32_t seq;        // Harness sequence number
    uint8_t  type;       // 0 = DATA frame, 1 = XOR Parity frame
    uint32_t group_base; // Base sequence number for the FEC group
} Header;
#pragma pack(pop)

#endif // PROTO_H
