#include "dsp.h"

static int16_t sat16(int64_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

/* Right shift of a negative number: arithmetic (rounds toward minus infinity) on every compiler used here. docs/DESIGN.md states this. */

void dsp_biquad_init(dsp_biquad_state_t *s)
{
    s->x1 = s->x2 = s->y1 = s->y2 = 0;
}

int16_t dsp_biquad_step(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, int16_t x)
{
    int64_t acc = (int64_t)c->b0 * x + (int64_t)c->b1 * s->x1 + (int64_t)c->b2 * s->x2
                - (int64_t)c->a1 * s->y1 - (int64_t)c->a2 * s->y2;
    int16_t y = sat16((acc + ((int64_t)1 << (DSP_BIQUAD_SHIFT - 1))) >> DSP_BIQUAD_SHIFT);
    s->x2 = s->x1;
    s->x1 = x;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

void dsp_biquad_cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections,
                        const int16_t *in, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int16_t v = in[i];
        for (size_t k = 0; k < sections; k++)
            v = dsp_biquad_step(&c[k], &state[k], v);
        out[i] = v;
    }
}

void dsp_fir_init(dsp_fir_state_t *s, int16_t *delay, size_t taps)
{
    s->delay = delay;
    s->taps = taps;
    s->pos = 0;
    for (size_t i = 0; i < taps; i++) delay[i] = 0;
}

int16_t dsp_fir_step(const int16_t *coefs, dsp_fir_state_t *s, int16_t x)
{
    size_t taps = s->taps, idx = s->pos;
    s->delay[idx] = x;
    int64_t acc = 0;
    for (size_t k = 0; k < taps; k++) {
        acc += (int64_t)coefs[k] * s->delay[idx];
        idx = (idx == 0) ? taps - 1 : idx - 1;
    }
    s->pos = (s->pos + 1 == taps) ? 0 : s->pos + 1;
    return sat16((acc + ((int64_t)1 << (DSP_FIR_SHIFT - 1))) >> DSP_FIR_SHIFT);
}

void dsp_fir(const int16_t *coefs, dsp_fir_state_t *s, const int16_t *in, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) out[i] = dsp_fir_step(coefs, s, in[i]);
}
