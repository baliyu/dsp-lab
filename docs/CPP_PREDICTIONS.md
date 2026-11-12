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

### Result 1b (12 November 2026): HIT, test checked by breaking the code
Changed kBiquadShift from 14 to 13 in cpp/dsp.hpp. tests/test_cpp.py: 30 failed, 4 passed (test_vector_count and the three silence cases, as predicted: all-zero input gives all-zero output whatever the shift). Restored with git restore; 34 passed again. Limit: one deliberate bug only. Run in a new venv (numpy 2.5.4, scipy 1.18.1, pytest 9.2.0, Python 3.13); versions may differ from the old venv.

### More misses
- Predicted a clean git tree before the edit; it was already modified (the shift had been applied in a terminal that was then closed). The AI first guessed line-ending noise, which was wrong.
- The AI said requirements.txt was pinned; it is not, so the new venv took the newest versions.
- The AI's grep pattern for showing the diff matched nothing.

### Results on the host and the Feather M0 (12 November 2026)
Host PC build with BENCH_CPP: 11 variants, 17 cases, runs=187 failed=0, all 33 cpp result lines ok=1. Predicted, HIT.
Feather M0 (log bench/logs/feather_run4.log, 4 complete runs, checker PASS, every output CRC matches the model). Predicted bit-exact, HIT.
Cycles per sample, 2-section biquad, last run, compared with the C variants in the same run:
- cpp64 947.19 against acc64 1040.42 (-9.0%). Predicted within 3%. MISS (faster).
- cpp32 215.44 against acc32 244.03 (-11.7%). Predicted within 3%. MISS (faster).
- cpp32blk 152.45 against opt32 158.33 (-3.7%). Predicted within 3%. MISS (narrowly, faster).
The cause of the speed difference is not known. Not yet looked at: the C++ compile flags in the Arduino core, the generated code, and whether the C++ build still calls __aeabi_lmul (prediction 5). Flash size (prediction 7) has no baseline yet. Limits: one board, one run, the table shows the last of 4 runs.

### More misses
- dsp_rt.cpp line count: predicted 76, actual 88.
- Predicted no pytest failures after editing bench_variants.h. One failed, because the sketch folder must hold an exact copy.
- Predicted the three C++ cycle counts within 3% of the C ones. All three were faster.

### Why the C++ is faster on the Feather: first look (12 November 2026)
Compiled with the board's own compiler (GCC 9.2.1, arm-none-eabi 9-2019q4) and the Arduino core flags (-Os; C as gnu11, C++ as gnu++11 with -fno-rtti -fno-exceptions). Flags are not the cause: both use -Os. The number of 64-bit multiply call sites is not the cause: 5 in dsp_biquad_step, 5 in cpp_biquad_acc64. Predicted equal, HIT.
Function sizes (instruction lines, approximate, they include literal pool data): the C cascade is 37 lines and calls a separate 107-line step function (64-bit) or 52-line step function (32-bit). The C++ variants hold the step inside one function (127 lines for acc64, 88 for acc32). So the C version probably pays a function call per section per sample and the C++ version does not. This fits the order of the gaps (cpp64 -9.0%, cpp32 -11.7%, cpp32blk -3.7%, and cpp32blk is 112 lines like the C block version). It is NOT proved.
Test to settle it, not yet run: mark the C++ step noinline, rebuild, rerun on the board. Prediction: cpp32 returns to within 3% of the C acc32.
Limit: line counts are not cycle counts.

### Experiment: C++ step() not inlined (12 November 2026)
Code: DSP_RT_NOINLINE_STEP defined at the top of cpp/dsp_rt.cpp (committed in this state with the log, then removed in the next commit). Log: bench/logs/feather_run5.log, 4 complete runs, checker PASS, every output CRC matches the model.
Cycles per sample, 2-section biquad, last run, against the C variants in the same run:
- cpp64 1035.42 against acc64 1038.42 (-0.3%). Predicted within 3%. HIT. With the inlining removed, the 9% gain of run4 is gone.
- cpp32 235.07 against acc32 244.03 (-3.7%). Predicted within 3%. MISS, narrowly. About 69% of the run4 gain is gone; a 3.7% gap remains, cause unknown.
- cpp32blk 152.45 against opt32 158.33 (-3.7%), the same as run4. Predicted unchanged. HIT. This variant does not call step, so its gap is not explained by inlining either.
Conclusion, limited: inlining of step explains the whole int64 gain and most of the int32 per-sample gain. The remaining 3.7% in both int32 C++ variants is unexplained. Limits: one board, one run, flash size of this build not recorded.
