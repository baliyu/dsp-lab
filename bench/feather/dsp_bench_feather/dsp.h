/* dsp.h: small fixed-point filters for 16-bit samples. Portable C99, no dynamic memory, no floating point.
   Built with AI assistance (Claude). The exact arithmetic is specified in docs/DESIGN.md and mirrored, bit for bit, by python/dsp_lab/model.py. */
#ifndef DSP_H
#define DSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Biquad (second-order IIR section), direct form I -------------------------------------------------------------------
   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]        (the denominator's a0 is 1, as in SciPy's convention)
   Samples are int16 (Q15). Coefficients are int16 in Q2.14, so they can be in the range -2 to +1.99994.
   The sum is kept in 64 bits, rounded to nearest (+2^13) and shifted right by 14, then saturated to int16. */
#define DSP_BIQUAD_SHIFT 14

typedef struct { int16_t b0, b1, b2, a1, a2; } dsp_biquad_coefs_t;
typedef struct { int16_t x1, x2, y1, y2; } dsp_biquad_state_t;

void dsp_biquad_init(dsp_biquad_state_t *state);                                   /* clears the history */
int16_t dsp_biquad_step(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, int16_t x);

/* A cascade of `sections` biquads: the output of section i (an int16, so rounded and saturated) is the input of section i+1.
   `state` must hold `sections` entries. in and out may be the same buffer. Call it again with the next block and the same state to continue. */
void dsp_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections,
                        const int16_t *in, int16_t *out, size_t n);

/* ---- FIR filter -------------------------------------------------------------------------------------------------------
   y[n] = sum over k of coefs[k] * x[n-k], k = 0 .. taps-1.  Coefficients int16 in Q15 (range -1 to +0.99997); the sum is kept in
   64 bits, rounded to nearest (+2^14), shifted right by 15 and saturated to int16. The caller provides `delay`, an array of `taps` int16. */
#define DSP_FIR_SHIFT 15

typedef struct { int16_t *delay; size_t taps; size_t pos; } dsp_fir_state_t;

void dsp_fir_init(dsp_fir_state_t *state, int16_t *delay, size_t taps);              /* clears the delay line */
int16_t dsp_fir_step(const int16_t *coefs, dsp_fir_state_t *s, int16_t x);
void dsp_fir(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif
