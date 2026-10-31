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

## A limit cycle, found by measurement

After an impulse the 1000 Hz low-pass never settles to zero: its output stays at **plus or minus 1 LSB** indefinitely (a known effect of rounding in
recursive fixed-point filters). Silence in gives silence out. A test pins this at "at most 1 LSB".

## Limits

- Nothing here has run on a microcontroller yet. The 64-bit accumulator is simple and safe; on a Cortex-M0+ (no 64-bit multiply) it will be slow, and how slow is not yet measured.
- The error figures come from these test vectors, not from a proof; the bounds are not worst-case guarantees for every possible input.
- One filter type per kind has been tested. No FFT, no resampling yet.
- Python 3.12 and 3.13 are tested in CI; the C code is built with `-Wall -Wextra -Werror -Wconversion`.
