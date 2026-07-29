# Experiment Run Log

## Baseline
- **Profile**: A
- **Playout Delay**: 40 ms
- **Miss Rate**: 4.40%
- **Overhead**: 1.02x
- **Result**: INVALID (Unprotected UDP drops packets).

---

## Experiment 1: Baseline FEC (N=4)
- **Profile**: A
- **Playout Delay**: 120 ms
- **Miss Rate**: 0.27%
- **Overhead**: 1.27x
- **Result**: VALID. Verified single-loss reconstruction works via XOR parity.

---

## Experiment 2: Stressing Profile B (N=4)
- **Profile**: B
- **Playout Delay**: 140 ms
- **Miss Rate**: 1.47%
- **Overhead**: 1.27x
- **Result**: INVALID. Group size N=4 vulnerable to burst losses on higher-loss profiles.

---

## Experiment 3: Reduced FEC Group Size (N=2)
- **Profile**: A
- **Playout Delay**: 50 ms
- **Miss Rate**: 0.93%
- **Overhead**: 1.59x
- **Result**: VALID.

---

## Experiment 4: Final Tuning for Unseen Profiles (N=2)
- **Profile**: B
- **Playout Delay**: 110 ms
- **Miss Rate**: 0.80%
- **Overhead**: 1.59x
- **Result**: VALID. Selected 110 ms to remain robust across arbitrary network jitter.
