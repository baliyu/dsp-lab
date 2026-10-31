/* The list of implementations the benchmark times. Each variant is one implementation of both filters. "acc64" is dsp.c (64-bit accumulator) and "acc32" is dsp32.c, both at the
   build's default optimisation level; with BENCH_O2_UNITS set (the PC build and the Feather sketch) the same two are also built at function-level -O2 ("acc64_O2", "acc32_O2").
   Every variant must produce the model's checksums; the 32-bit ones are refused for coefficients that fail dsp32_*_ok. */
#ifndef BENCH_VARIANTS_H
#define BENCH_VARIANTS_H

#include "bench_core.h"
#include "dsp.h"
#include "dsp32.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BENCH_O2_UNITS) && BENCH_O2_UNITS
void dsp_o2_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections, const int16_t *in, int16_t *out, size_t n);
void dsp_o2_fir(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);
void dsp32_o2_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections, const int16_t *in, int16_t *out, size_t n);
void dsp32_o2_fir(const int16_t *coefs, dsp_fir_state_t *state, const int16_t *in, int16_t *out, size_t n);
#endif

static const bench_variant_t bench_variants[] = {
    { "acc64", dsp_biquad_cascade, dsp_fir, 0, 0 },
    { "acc32", dsp32_biquad_cascade, dsp32_fir, dsp32_biquad_ok, dsp32_fir_ok },
#if defined(BENCH_O2_UNITS) && BENCH_O2_UNITS
    { "acc64_O2", dsp_o2_biquad_cascade, dsp_o2_fir, 0, 0 },
    { "acc32_O2", dsp32_o2_biquad_cascade, dsp32_o2_fir, dsp32_biquad_ok, dsp32_fir_ok },
#endif
};
#define BENCH_NVARIANTS (sizeof bench_variants / sizeof bench_variants[0])

#ifdef __cplusplus
}
#endif

#endif
