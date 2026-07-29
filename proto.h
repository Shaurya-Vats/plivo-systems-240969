#ifndef PROTO_H
#define PROTO_H
#include <stdint.h>
#define FEC_K 4
#define FEC_R 2
#define FRAME_PAYLOAD_SIZE 160
#pragma pack(push, 1)
typedef struct {
    uint32_t seq;        // Sequence number
    uint8_t  type;       // 0 = DATA, 1 = ROW_PARITY, 2 = INT_PARITY
    uint32_t group_base; // Base sequence number for the FEC group
} Header;
#pragma pack(pop)
#endif // PROTO_H
