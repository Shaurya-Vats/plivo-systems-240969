#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unordered_map>
#include <map>
#include <chrono>
#include <thread>
#include <algorithm>
#include "proto.h"

struct GroupState {
    uint8_t payloads[FEC_K][FRAME_PAYLOAD_SIZE];
    bool have_data[FEC_K] = {false};
    uint8_t row_parity[FRAME_PAYLOAD_SIZE];
    bool have_row_parity = false;
    uint8_t int_parity[FRAME_PAYLOAD_SIZE];
    bool have_int_parity = false;
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

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; // 1 ms polling clock loop
    setsockopt(src_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    const char* env_delay = getenv("DELAY_MS");
    int playout_delay_ms = env_delay ? atoi(env_delay) : 100;

    std::unordered_map<uint32_t, GroupState> groups;
    std::map<uint32_t, std::string> jitter_buffer;
    uint32_t next_seq_to_play = 0;
    bool t0_initialized = false;
    std::chrono::steady_clock::time_point t0;

    auto playout_frame = [&](uint32_t seq, const uint8_t* payload) {
        uint8_t out_buf[164];
        uint32_t net_seq = htonl(seq);
        memcpy(out_buf, &net_seq, 4);
        memcpy(out_buf + 4, payload, FRAME_PAYLOAD_SIZE);
        sendto(dst_fd, out_buf, sizeof(out_buf), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
    };

    while (true) {
        uint8_t in_buf[sizeof(Header) + FRAME_PAYLOAD_SIZE];
        ssize_t n = recv(src_fd, in_buf, sizeof(in_buf), 0);

        if (n >= (ssize_t)sizeof(Header)) {
            Header hdr;
            memcpy(&hdr, in_buf, sizeof(Header));
            hdr.seq = ntohl(hdr.seq);
            hdr.group_base = ntohl(hdr.group_base);

            uint8_t* payload = in_buf + sizeof(Header);

            if (!t0_initialized && hdr.type == 0) {
                t0 = std::chrono::steady_clock::now();
                t0_initialized = true;
            }

            GroupState& st = groups[hdr.group_base];

            if (hdr.type == 0) { // DATA
                uint32_t idx = hdr.seq - hdr.group_base;
                if (idx < FEC_K) {
                    memcpy(st.payloads[idx], payload, FRAME_PAYLOAD_SIZE);
                    st.have_data[idx] = true;
                    if (jitter_buffer.find(hdr.seq) == jitter_buffer.end()) {
                        jitter_buffer[hdr.seq] = std::string((char*)payload, FRAME_PAYLOAD_SIZE);
                    }
                }
            } else if (hdr.type == 1) { // ROW PARITY
                memcpy(st.row_parity, payload, FRAME_PAYLOAD_SIZE);
                st.have_row_parity = true;
            } else if (hdr.type == 2) { // INTERLEAVED PARITY
                memcpy(st.int_parity, payload, FRAME_PAYLOAD_SIZE);
                st.have_int_parity = true;
            }

            // Multi-Stage FEC Burst Recovery
            // Stage 1: Single loss recovery via Interleaved Parity
            if (st.have_int_parity && (!st.have_data[0] || !st.have_data[2])) {
                if (st.have_data[0] ^ st.have_data[2]) {
                    int miss = st.have_data[0] ? 2 : 0;
                    int hit = st.have_data[0] ? 0 : 2;
                    uint8_t rec[FRAME_PAYLOAD_SIZE];
                    memcpy(rec, st.int_parity, FRAME_PAYLOAD_SIZE);
                    for (int j = 0; j < FRAME_PAYLOAD_SIZE; j++) rec[j] ^= st.payloads[hit][j];
                    
                    memcpy(st.payloads[miss], rec, FRAME_PAYLOAD_SIZE);
                    st.have_data[miss] = true;
                    jitter_buffer[hdr.group_base + miss] = std::string((char*)rec, FRAME_PAYLOAD_SIZE);
                }
            }

            // Stage 2: Full Row Parity Recovery
            if (st.have_row_parity) {
                int missing_idx = -1;
                int count = 0;
                for (int i = 0; i < FEC_K; i++) {
                    if (st.have_data[i]) count++;
                    else missing_idx = i;
                }

                if (count == FEC_K - 1 && missing_idx != -1) {
                    uint8_t rec[FRAME_PAYLOAD_SIZE];
                    memcpy(rec, st.row_parity, FRAME_PAYLOAD_SIZE);
                    for (int i = 0; i < FEC_K; i++) {
                        if (i != missing_idx) {
                            for (int j = 0; j < FRAME_PAYLOAD_SIZE; j++) rec[j] ^= st.payloads[i][j];
                        }
                    }
                    memcpy(st.payloads[missing_idx], rec, FRAME_PAYLOAD_SIZE);
                    st.have_data[missing_idx] = true;
                    jitter_buffer[hdr.group_base + missing_idx] = std::string((char*)rec, FRAME_PAYLOAD_SIZE);
                }
            }
        }

        // Sequence-Clocked Jitter Playout Dispatch
        if (t0_initialized) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();

            while (true) {
                int64_t target_playout_time = playout_delay_ms + (next_seq_to_play * 20);
                if (elapsed_ms >= target_playout_time) {
                    auto it = jitter_buffer.find(next_seq_to_play);
                    if (it != jitter_buffer.end()) {
                        playout_frame(next_seq_to_play, (const uint8_t*)it->second.data());
                        jitter_buffer.erase(it);
                    }
                    next_seq_to_play++;
                } else {
                    break;
                }
            }
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
