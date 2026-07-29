#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#define FRAME_PAYLOAD_SIZE 160
#define FEC_K 4  // Data frames per group
#define FEC_R 2  // Redundancy / Parity frames per group

#pragma pack(push, 1)
typedef struct {
    uint32_t seq;        // Original sequence number
    uint8_t  type;       // 0 = DATA, 1 = ROW PARITY, 2 = DIAGONAL PARITY
    uint32_t group_base; // Base sequence number for the FEC block
} Header;
#pragma pack(pop)

#endif // PROTO_H
