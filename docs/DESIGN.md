# Design: the arithmetic, and what was measured

Built with AI assistance (Claude). This page is the specification that `c/dsp.c` and `python/dsp_lab/model.py` both follow. The tests require
the C output to equal the model's **bit for bit**, so if this page and the code ever disagree, a test fails.

## Number formats

- Samples are `int16` (a signed 16-bit integer; 32767 is the largest). Every result is **saturated** to the `int16` range, never wrapped.
- **Biquad** (second-order IIR section, direct form I): `y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]`.
  Coefficients are `int16` in **Q2.14** (the real value is the integer divided by 16384, so the range is -2 to +1.99994).
  The five products are summed in a 64-bit integer, 2^13 is added (round to nearest), the sum is shifted right by 14, and the result is saturated.
- **FIR**: `y[n] = sum of coefs[k] * x[n-k]`, coefficients `int16` in **Q15**; 64-bit sum, +2^14, shift right by 15, saturate. The delay line is a circular buffer of `taps` samples.
- A cascade passes each section's `int16` output (already rounded and saturated) to the next section.
- A right shift of a negative number is arithmetic (it rounds toward minus infinity). Python's `>>` does the same; the arm-none-eabi and x86 compilers used here do too.

## Why the gain is shared between sections

SciPy puts the whole gain of a filter into its first section; the other sections then have numerators like `[1, 2, 1]`, and `2` does not fit Q2.14
(the first design attempt failed on exactly this). `share_gain` scales each section's numerator to a largest coefficient of 1 and gives every
section an equal share of the total gain, so the cascade's response is unchanged (a test checks that). The cost: with gain spread thinly, the
quantisation steps are larger relative to the signal. Choosing the split by signal level instead ("L2 scaling") is a planned improvement, to be measured.

## What was measured (test vectors: impulse, step, five sines, a chirp, noise; fs = 8 kHz)

| Filter | Largest error against the ideal floating-point filter | Where | Largest magnitude-response error | Note |
|---|---|---|---|---|
| 4th-order Butterworth low-pass, 1000 Hz | 4.2 LSB | chirp | 0.002 dB | |
| 4th-order Butterworth low-pass, 300 Hz | 22.1 LSB | step | 0.007 dB | |
| 4th-order Butterworth high-pass, 50 Hz (a DC blocker) | **360.7 LSB** | step | 0.008 dB | poles close to the unit circle: 16-bit coefficients are too coarse; this is the filter that needs more precision |
| 31-tap Hamming FIR low-pass, 1000 Hz | 1.46 LSB | all | n/a | 0.5 LSB of that is the final rounding; the rest is the coefficient quantisation |

The test bounds are about twice these measured worst cases (8, 45 and 720 LSB), so a regression shows up and ordinary variation does not.

## The 32-bit accumulator variant (stage 4): `c/dsp32.c`

The same filters with an `int32` accumulator instead of `int64`. Rounding, shifts, saturation, state and delay line are exactly those above; only the width of the sum changes. On a
Cortex-M0+ (no 64-bit multiply) this removes the library call behind every `int64` product; the compiled code of `dsp32.c` makes no `__aeabi_lmul` call at all, where `dsp.c` makes 12 at `-Os`.

It is **not** exact for every coefficient set. Samples are `int16`, so `|x|` and `|y|` are at most 32768, and the sum is at most 32768 x (sum of the absolute values of the coefficients) plus the rounding
constant (2^13 for a biquad, 2^14 for the FIR). If the sum of absolute coefficient values (per biquad section; over all taps for the FIR) is at most **65535**, the largest possible sum is
2,147,467,264, below 2^31 = 2,147,483,648, so no overflow occurs and the result equals the 64-bit result bit for bit. At 65536 it can overflow, which is undefined behaviour in C.
`dsp32_biquad_ok` and `dsp32_fir_ok` test that condition; the benchmark refuses to run a 32-bit variant whose check fails, and the tests check the limit at both sides of the boundary.

For the filters used here (sums of absolute coefficient values in Q2.14 or Q15):

| Filter | Sum of absolute coefficients (per section for biquads) | 32-bit exact? |
|---|---|---|
| Butterworth low-pass 1000 Hz | 24,072 and 34,262 | yes |
| Butterworth low-pass 300 Hz | 37,568 and 43,734 | yes |
| 31-tap FIR | 45,221 | yes |
| Butterworth high-pass 50 Hz (the DC blocker) | 110,710 and 112,033 | **no**: the check rejects it |

The 50 Hz result is the same fact as the 361 LSB quantisation error below, seen from the other side: poles close to the unit circle need large coefficients, and large coefficients are what a narrow accumulator
cannot take. The bound is for any `int16` inputs, not only the test vectors.

### Restructured versions (same arithmetic, different loop order)

Three further functions in `dsp32.c` compute exactly what `dsp32_biquad_cascade` and `dsp32_fir` compute, so they are bit-identical to the model under the same check:

- `dsp32_biquad_blk` runs the block through one section at a time, holding that section's coefficients and state in local variables (each section is causal and independent, so the order does not change the result).
- `dsp32_fir_lin` keeps the delay line as a linear history of the previous `taps - 1` inputs instead of a circular buffer, so the inner loop has no wrap test. Its state is not interchangeable with the circular versions' (it uses the same structure, all zero at the start), and its input and output must not overlap.
- `dsp32_fir_sym` does the same for a symmetric FIR (`coefs[k] == coefs[taps-1-k]`, as a linear-phase design gives) and adds the two inputs that share a coefficient before multiplying, so it does about half the multiplies. It needs `dsp32_fir_sym_ok`
  (symmetric and within the 65535 limit). The limit also covers the folded product: a coefficient that appears twice in the sum is at most 32767 in size, and 32767 x 65536 (the largest sum of two inputs) = 2,147,418,112 is below 2^31.

Whether they are faster has to be measured; see [BENCH.md](BENCH.md).

## A limit cycle, found by measurement

After an impulse the 1000 Hz low-pass never settles to zero: its output stays at **plus or minus 1 LSB** indefinitely (a known effect of rounding in
recursive fixed-point filters). Silence in gives silence out. A test pins this at "at most 1 LSB".

## Limits

- The code has run on two cores, with cycle counts in [BENCH.md](BENCH.md): a Cortex-M4F (Arduino Nano 33 BLE) and a Cortex-M0+ (Adafruit Feather M0), one board of each, built with different compilers and flags. The Cortex-M0+ has no 64-bit multiply, so the 64-bit accumulator is simple and safe but costs about 8 to 9 times the Nano's cycles there; how much of that is the multiply and how much the compiler and flags is not yet separated.
- The error figures come from these test vectors, not from a proof; the bounds are not worst-case guarantees for every possible input.
- One filter type per kind has been tested. No FFT, no resampling yet.
- Python 3.12 and 3.13 are tested in CI; the C code is built with `-Wall -Wextra -Werror -Wconversion`.
