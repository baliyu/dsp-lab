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

/* ---- restructured versions: see dsp32.h ---- */

static int16_t fir_out(int32_t acc)
{
    return sat16_32((acc + ((int32_t)1 << (DSP_FIR_SHIFT - 1))) >> DSP_FIR_SHIFT);
}

int dsp32_fir_sym_ok(const int16_t *coefs, size_t taps)
{
    for (size_t k = 0; k < taps / 2; k++)
        if (coefs[k] != coefs[taps - 1 - k]) return 0;
    return dsp32_fir_ok(coefs, taps);
}

void dsp32_biquad_blk(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *state, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    const int16_t *src = in;

    if (sections == 0) {
        for (size_t i = 0; i < n; i++) out[i] = in[i];
        return;
    }
    for (size_t k = 0; k < sections; k++) {
        const int32_t b0 = c[k].b0, b1 = c[k].b1, b2 = c[k].b2, a1 = c[k].a1, a2 = c[k].a2;
        int32_t x1 = state[k].x1, x2 = state[k].x2, y1 = state[k].y1, y2 = state[k].y2;

        for (size_t i = 0; i < n; i++) {
            const int32_t x = src[i];
            const int32_t acc = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            const int32_t y = sat16_32((acc + ((int32_t)1 << (DSP_BIQUAD_SHIFT - 1))) >> DSP_BIQUAD_SHIFT);
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            out[i] = (int16_t)y;
        }
        state[k].x1 = (int16_t)x1;
        state[k].x2 = (int16_t)x2;
        state[k].y1 = (int16_t)y1;
        state[k].y2 = (int16_t)y2;
        src = out;                                      /* the next section reads what this one wrote */
    }
}

/* the sample k steps back from in[i]: from in[] if i >= k, otherwise from the history (the last taps-1 inputs of the previous call, oldest first) */
static int32_t fir_back(const int16_t *hist, size_t hlen, const int16_t *in, size_t i, size_t k)
{
    return (k <= i) ? in[i - k] : hist[hlen + i - k];
}

static void fir_history_update(int16_t *hist, size_t hlen, const int16_t *in, size_t n)
{
    if (hlen == 0) return;
    if (n >= hlen) {
        for (size_t j = 0; j < hlen; j++) hist[j] = in[n - hlen + j];
    } else {
        for (size_t j = 0; j + n < hlen; j++) hist[j] = hist[j + n];
        for (size_t j = 0; j < n; j++) hist[hlen - n + j] = in[j];
    }
}

/* the first taps-1 outputs of a block, which need the history; the plain sum, no folding */
static size_t fir_edge(const int16_t *coefs, const dsp_fir_state_t *s, const int16_t *in, int16_t *out, size_t n)
{
    size_t taps = s->taps, hlen = taps - 1;
    size_t edge = (n < hlen) ? n : hlen;

    for (size_t i = 0; i < edge; i++) {
        int32_t acc = 0;
        for (size_t k = 0; k < taps; k++) acc += (int32_t)coefs[k] * fir_back(s->delay, hlen, in, i, k);
        out[i] = fir_out(acc);
    }
    return edge;
}

void dsp32_fir_lin(const int16_t *coefs, dsp_fir_state_t *s, const int16_t *in, int16_t *out, size_t n)
{
    size_t taps = s->taps, hlen = taps - 1;
    size_t edge = fir_edge(coefs, s, in, out, n);

    for (size_t i = edge; i < n; i++) {
        int32_t acc = 0;
        for (size_t k = 0; k < taps; k++) acc += (int32_t)coefs[k] * in[i - k];
        out[i] = fir_out(acc);
    }
    fir_history_update(s->delay, hlen, in, n);
}

void dsp32_fir_sym(const int16_t *coefs, dsp_fir_state_t *s, const int16_t *in, int16_t *out, size_t n)
{
    size_t taps = s->taps, hlen = taps - 1, half = taps / 2;
    size_t edge = fir_edge(coefs, s, in, out, n);

    for (size_t i = edge; i < n; i++) {
        int32_t acc = 0;
        for (size_t k = 0; k < half; k++) acc += (int32_t)coefs[k] * ((int32_t)in[i - k] + in[i - hlen + k]);
        if (taps & 1u) acc += (int32_t)coefs[half] * in[i - half];
        out[i] = fir_out(acc);
    }
    fir_history_update(s->delay, hlen, in, n);
}
