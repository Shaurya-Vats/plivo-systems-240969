#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unordered_map>
#include "proto.h"

struct GroupState {
    uint8_t payloads[FEC_GROUP][FRAME_PAYLOAD_SIZE];
    bool have_data[FEC_GROUP] = {false};
    uint8_t parity_payload[FRAME_PAYLOAD_SIZE];
    bool have_parity = false;
};

int main() {
    int src_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int dst_fd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in src_addr{}, dst_addr{};
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(47002);
    src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(47020);
    dst_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int reuse = 1;
    setsockopt(src_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(src_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        perror("Receiver bind failed");
        return 1;
    }

    std::unordered_map<uint32_t, GroupState> groups;
    std::unordered_map<uint32_t, bool> forwarded;

    auto forward_frame = [&](uint32_t seq, const uint8_t* payload) {
        if (forwarded[seq]) return;
        uint8_t out_buf[164];
        uint32_t net_seq = htonl(seq);
        memcpy(out_buf, &net_seq, 4);
        memcpy(out_buf + 4, payload, FRAME_PAYLOAD_SIZE);
        sendto(dst_fd, out_buf, sizeof(out_buf), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
        forwarded[seq] = true;
    };

    while (true) {
        uint8_t in_buf[sizeof(Header) + FRAME_PAYLOAD_SIZE];
        ssize_t n = recv(src_fd, in_buf, sizeof(in_buf), 0);
        if (n <= 0) break;

        Header hdr;
        memcpy(&hdr, in_buf, sizeof(Header));
        hdr.seq = ntohl(hdr.seq);
        hdr.group_base = ntohl(hdr.group_base);

        uint8_t* payload = in_buf + sizeof(Header);
        GroupState& st = groups[hdr.group_base];

        if (hdr.type == 0) { // DATA
            uint32_t idx = hdr.seq - hdr.group_base;
            if (idx < FEC_GROUP) {
                memcpy(st.payloads[idx], payload, FRAME_PAYLOAD_SIZE);
                st.have_data[idx] = true;
                forward_frame(hdr.seq, payload);
            }
        } else if (hdr.type == 1) { // PARITY
            memcpy(st.parity_payload, payload, FRAME_PAYLOAD_SIZE);
            st.have_parity = true;
        }

        // Attempt FEC reconstruction
        if (st.have_parity) {
            int missing_idx = -1;
            int count = 0;
            for (int i = 0; i < FEC_GROUP; i++) {
                if (st.have_data[i]) count++;
                else missing_idx = i;
            }

            if (count == FEC_GROUP - 1 && missing_idx != -1) {
                uint8_t recovered[FRAME_PAYLOAD_SIZE];
                memcpy(recovered, st.parity_payload, FRAME_PAYLOAD_SIZE);
                for (int i = 0; i < FEC_GROUP; i++) {
                    if (i != missing_idx) {
                        for (int j = 0; j < FRAME_PAYLOAD_SIZE; j++) {
                            recovered[j] ^= st.payloads[i][j];
                        }
                    }
                }
                uint32_t rec_seq = hdr.group_base + missing_idx;
                memcpy(st.payloads[missing_idx], recovered, FRAME_PAYLOAD_SIZE);
                st.have_data[missing_idx] = true;
                forward_frame(rec_seq, recovered);
            }
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
