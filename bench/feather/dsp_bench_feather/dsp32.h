/* dsp32: the same filters as dsp.h with a 32-bit accumulator instead of a 64-bit one. On a Cortex-M0+ (no 64-bit multiply, so every int64 product is a library call)
   this removes the library call; the result is bit-identical to dsp.c, but ONLY when the coefficients pass the check below. Built with AI assistance (Claude).

   Why it is exact: samples are int16, so |x| and |y| are at most 32768. The sum is at most 32768 * (sum of |coefficient|) plus the rounding constant (at most 2^14).
   If the sum of |coefficient| is at most 65535, that is at most 2,147,467,264, which is below 2^31 = 2,147,483,648: no overflow, and every int32 sum equals the int64 sum.
   For a biquad the sum is over the five coefficients of one section (b0, b1, b2, a1, a2); for a FIR it is over all taps. If the check fails, do not call the 32-bit functions:
   a signed overflow in C is undefined behaviour. The rounding and the saturation are the same as in dsp.c; the state and init functions are dsp.c's. */
#ifndef DSP32_H
#define DSP32_H

#include "dsp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP32_L1_LIMIT 65535u

/* 1 if every section's (or the FIR's) sum of |coefficient| is at most DSP32_L1_LIMIT, so the 32-bit functions are exact; 0 otherwise */
int dsp32_biquad_ok(const dsp_biquad_coefs_t *c, size_t sections);
int dsp32_fir_ok(const int16_t *coefs, size_t taps);

int16_t dsp32_biquad_step(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, int16_t x);
void dsp32_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections,
                          const int16_t *in, int16_t *out, size_t n);
int16_t dsp32_fir_step(const int16_t *coefs, dsp_fir_state_t *s, int16_t x);
void dsp32_fir(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);

/* ---- restructured versions (stage 4): the same arithmetic in a different loop order, so the output is bit-identical to dsp32_* (and so to dsp.c) under the same condition ----

   dsp32_biquad_blk: processes the block one section at a time, with that section's coefficients and state held in local variables. Needs dsp32_biquad_ok. in and out may be the same array.

   dsp32_fir_lin: keeps the delay line as a linear history (the previous taps-1 inputs, oldest first, in delay[0 .. taps-2]) instead of a circular buffer, so the inner loop has no wrap test.
   Its state is NOT interchangeable with dsp_fir's or dsp32_fir's (dsp_fir_init gives a valid all-zero start for it). in and out must not overlap. Needs dsp32_fir_ok.

   dsp32_fir_sym: as dsp32_fir_lin, and for a symmetric FIR (coefs[k] == coefs[taps-1-k], as for a linear-phase design) it adds the two inputs that share a coefficient before multiplying,
   so it does about half the multiplies. Needs dsp32_fir_sym_ok: symmetric AND within the dsp32_fir_ok limit. The limit also keeps each folded product inside int32: a coefficient that is
   counted twice in the sum is at most 32767 in size, and 32767 x 65536 (the largest sum of two inputs) = 2,147,418,112 < 2^31. in and out must not overlap. */
int dsp32_fir_sym_ok(const int16_t *coefs, size_t taps);
void dsp32_biquad_blk(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections, const int16_t *in, int16_t *out, size_t n);
void dsp32_fir_lin(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);
void dsp32_fir_sym(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif
