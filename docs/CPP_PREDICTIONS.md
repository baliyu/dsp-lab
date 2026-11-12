# C++ biquad: predictions, written before any C++ code

Written 12 November 2026. Built with AI assistance (Claude). Nothing here is measured yet.

Scope: a header-only C++ biquad, templated on accumulator type (int64 or int32). No heap, no exceptions, no RTTI. It must match python/dsp_lab/model.py bit for bit on the same test vectors.

## C baselines (cycles per sample, 2-section cascade, from docs/BENCH.md)

| Variant | Nano 33 BLE (M4F) | Feather M0 (M0+, -Os) |
|---|---|---|
| int64 (acc64) | 133.9 | 1019.2 |
| int32 plain (acc32) | 87.2 | 244.0 |
| int32 block (opt32) | 50.3 | 158.3 (126.3 at -O2) |

## Predictions

1. Bit-exact: the C++ int64 and int32 versions match the Python model on all 11 vectors. High confidence.
2. C++ block int32 is within 3% of 50.3 (Nano) and 158.3 (Feather, -Os). Moderate confidence. BENCH.md shows the same source varying by up to 1.9% between runs, so only a gap over 3% counts as a miss.
3. C++ per-sample int32 is within 3% of 87.2 (Nano) and 244.0 (Feather).
4. C++ int64 is within 3% of 133.9 (Nano) and 1019 (Feather).
5. Zero __aeabi_lmul calls in the C++ int32 build for the M0+, checked with objdump. High confidence.
6. static_assert rejects the 50 Hz DC blocker on the int32 path (110,710 is over 65535) and accepts the 1000 Hz low-pass. Needs no board.
7. Flash size is within +5% of the C version.

## How I could be wrong

The Nano and Feather builds use different compilers and flags. An abstraction may cost cycles I have not foreseen. An earlier prediction about the M4F int64 cost was already wrong.

## Scorecard

To be filled in after measuring. Every miss goes in.

### Result 6 (12 November 2026): HIT
g++ -std=c++17 with -Wall -Wextra -Werror -Wconversion. The LP1000 sections compile on int32. HP50 section 1 on int32 is rejected by the static_assert; on int64 it compiles. Limit: one compiler, compile-time check only. Predictions 1 to 5 and 7 are not yet tested.

### Result 1 (12 November 2026): HIT
tests/test_cpp.py: 34 passed. The C++ biquad (LP1000 on int32 and int64, HP50 on int64) equals the integer model bit for bit on all 11 vectors. Limits: x86 g++ only, per-sample version only, HP50 on int32 cannot be built (the static_assert refuses it). The test has not yet been checked by breaking the code on purpose.

### Misses so far
- biquad_wrap.cpp line count: predicted 38, actual 32. A miscount by the AI, no effect on the result.
