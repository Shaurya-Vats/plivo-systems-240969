# Design Notes

1. **Architecture**: Forward Error Correction (FEC) using XOR parity blocks across groups of 2 data frames.
2. **Tradeoff**: Avoided NACK-based retransmission due to 2x RTT latency penalties across hostile networks.
3. **Bandwidth Overhead**: Fixed at 1.59x, safely under the 2.0x allowance.
4. **Grading Delay Recommendation**: 110 ms playout delay (`--delay_ms 110`).
5. **Robustness Limits**: Unrecoverable if 2 consecutive data frames in the same FEC block are dropped.
