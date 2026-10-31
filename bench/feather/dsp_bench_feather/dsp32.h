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

#ifdef __cplusplus
}
#endif

#endif
