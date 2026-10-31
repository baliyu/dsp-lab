#include "dsp32.h"

static int16_t sat16_32(int32_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static uint32_t abs16(int16_t v)
{
    return (uint32_t)(v < 0 ? -(int32_t)v : (int32_t)v);
}

int dsp32_biquad_ok(const dsp_biquad_coefs_t *c, size_t sections)
{
    for (size_t k = 0; k < sections; k++) {
        uint32_t sum = abs16(c[k].b0) + abs16(c[k].b1) + abs16(c[k].b2) + abs16(c[k].a1) + abs16(c[k].a2);
        if (sum > DSP32_L1_LIMIT) return 0;
    }
    return 1;
}

int dsp32_fir_ok(const int16_t *coefs, size_t taps)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < taps; i++) {
        sum += abs16(coefs[i]);
        if (sum > DSP32_L1_LIMIT) return 0;             /* stops early, so the sum itself cannot overflow */
    }
    return 1;
}

int16_t dsp32_biquad_step(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, int16_t x)
{
    int32_t acc = (int32_t)c->b0 * x + (int32_t)c->b1 * s->x1 + (int32_t)c->b2 * s->x2
                - (int32_t)c->a1 * s->y1 - (int32_t)c->a2 * s->y2;
    int16_t y = sat16_32((acc + ((int32_t)1 << (DSP_BIQUAD_SHIFT - 1))) >> DSP_BIQUAD_SHIFT);
    s->x2 = s->x1;
    s->x1 = x;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

void dsp32_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections,
                          const int16_t *in, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int16_t v = in[i];
        for (size_t k = 0; k < sections; k++)
            v = dsp32_biquad_step(&c[k], &state[k], v);
        out[i] = v;
    }
}

int16_t dsp32_fir_step(const int16_t *coefs, dsp_fir_state_t *s, int16_t x)
{
    size_t taps = s->taps, idx = s->pos;
    s->delay[idx] = x;
    int32_t acc = 0;
    for (size_t k = 0; k < taps; k++) {
        acc += (int32_t)coefs[k] * s->delay[idx];
        idx = (idx == 0) ? taps - 1 : idx - 1;
    }
    s->pos = (s->pos + 1 == taps) ? 0 : s->pos + 1;
    return sat16_32((acc + ((int32_t)1 << (DSP_FIR_SHIFT - 1))) >> DSP_FIR_SHIFT);
}

void dsp32_fir(const int16_t *coefs, dsp_fir_state_t *s, const int16_t *in, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) out[i] = dsp32_fir_step(coefs, s, in[i]);
}
