#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "proto.h"

int main() {
    int src_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int dst_fd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in src_addr{}, dst_addr{};
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(47010);
    src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(47001);
    dst_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int reuse = 1;
    setsockopt(src_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(src_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        perror("Sender bind failed");
        return 1;
    }

    uint8_t group_payloads[FEC_K][FRAME_PAYLOAD_SIZE];
    uint32_t group_seqs[FEC_K];
    int group_count = 0;

    while (true) {
        uint8_t in_buf[164];
        ssize_t n = recv(src_fd, in_buf, sizeof(in_buf), 0);
        if (n <= 0) break;

        uint32_t seq = ntohl(*(uint32_t*)in_buf);
        uint8_t* payload = in_buf + 4;

        // Forward Data Frame immediately
        Header data_hdr;
        data_hdr.seq = htonl(seq);
        data_hdr.type = 0;
        data_hdr.group_base = htonl(seq - group_count);

        uint8_t out_buf[sizeof(Header) + FRAME_PAYLOAD_SIZE];
        memcpy(out_buf, &data_hdr, sizeof(Header));
        memcpy(out_buf + sizeof(Header), payload, FRAME_PAYLOAD_SIZE);
        sendto(dst_fd, out_buf, sizeof(out_buf), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));

        memcpy(group_payloads[group_count], payload, FRAME_PAYLOAD_SIZE);
        group_seqs[group_count] = seq;
        group_count++;

        if (group_count == FEC_K) {
            // 1. Row Parity (XOR across 0, 1, 2, 3)
            uint8_t row_parity[FRAME_PAYLOAD_SIZE] = {0};
            for (int i = 0; i < FEC_K; i++) {
                for (int j = 0; j < FRAME_PAYLOAD_SIZE; j++) {
                    row_parity[j] ^= group_payloads[i][j];
                }
            }

            Header row_hdr;
            row_hdr.seq = htonl(group_seqs[0]);
            row_hdr.type = 1;
            row_hdr.group_base = htonl(group_seqs[0]);

            memcpy(out_buf, &row_hdr, sizeof(Header));
            memcpy(out_buf + sizeof(Header), row_parity, FRAME_PAYLOAD_SIZE);
            sendto(dst_fd, out_buf, sizeof(out_buf), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));

            // 2. Interleaved Parity (XOR across even indices 0 ^ 2)
            uint8_t int_parity[FRAME_PAYLOAD_SIZE] = {0};
            for (int i = 0; i < FEC_K; i += 2) {
                for (int j = 0; j < FRAME_PAYLOAD_SIZE; j++) {
                    int_parity[j] ^= group_payloads[i][j];
                }
            }

            Header int_hdr;
            int_hdr.seq = htonl(group_seqs[0]);
            int_hdr.type = 2;
            int_hdr.group_base = htonl(group_seqs[0]);

            memcpy(out_buf, &int_hdr, sizeof(Header));
            memcpy(out_buf + sizeof(Header), int_parity, FRAME_PAYLOAD_SIZE);
            sendto(dst_fd, out_buf, sizeof(out_buf), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));

            group_count = 0;
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
