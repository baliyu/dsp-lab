// Run-time-coefficient C++ biquad variants for the benchmark table.
// Built with AI assistance (Claude). Same arithmetic as c/dsp.c and c/dsp32.c (docs/DESIGN.md).
#include <cstddef>
#include <cstdint>
#include "dsp.h"

#define DSP_RT_NOINLINE_STEP 1  // EXPERIMENT RUN: remove this line to restore the default
// Experiment switch: define DSP_RT_NOINLINE_STEP to stop the compiler inlining step() (off by default).
#ifdef DSP_RT_NOINLINE_STEP
#define DSP_RT_STEP_ATTR __attribute__((noinline))
#else
#define DSP_RT_STEP_ATTR
#endif

namespace {

template <typename Acc>
inline int16_t sat16(Acc v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return static_cast<int16_t>(v);
}

template <typename Acc>
DSP_RT_STEP_ATTR inline int16_t step(const dsp_biquad_coefs_t &c, dsp_biquad_state_t &s, int16_t x)
{
    const Acc acc = static_cast<Acc>(c.b0) * x + static_cast<Acc>(c.b1) * s.x1 + static_cast<Acc>(c.b2) * s.x2
                  - static_cast<Acc>(c.a1) * s.y1 - static_cast<Acc>(c.a2) * s.y2;
    const int16_t y = sat16<Acc>((acc + (static_cast<Acc>(1) << (DSP_BIQUAD_SHIFT - 1))) >> DSP_BIQUAD_SHIFT);
    s.x2 = s.x1;
    s.x1 = x;
    s.y2 = s.y1;
    s.y1 = y;
    return y;
}

template <typename Acc>
void cascade(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *st, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int16_t v = in[i];
        for (size_t k = 0; k < sections; k++) v = step<Acc>(c[k], st[k], v);
        out[i] = v;
    }
}

template <typename Acc>
void blk(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *st, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    const int16_t *src = in;
    if (sections == 0) {
        for (size_t i = 0; i < n; i++) out[i] = in[i];
        return;
    }
    for (size_t k = 0; k < sections; k++) {
        const Acc b0 = c[k].b0, b1 = c[k].b1, b2 = c[k].b2, a1 = c[k].a1, a2 = c[k].a2;
        Acc x1 = st[k].x1, x2 = st[k].x2, y1 = st[k].y1, y2 = st[k].y2;
        for (size_t i = 0; i < n; i++) {
            const Acc x = src[i];
            const Acc acc = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            const Acc y = sat16<Acc>((acc + (static_cast<Acc>(1) << (DSP_BIQUAD_SHIFT - 1))) >> DSP_BIQUAD_SHIFT);
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            out[i] = static_cast<int16_t>(y);
        }
        st[k].x1 = static_cast<int16_t>(x1);
        st[k].x2 = static_cast<int16_t>(x2);
        st[k].y1 = static_cast<int16_t>(y1);
        st[k].y2 = static_cast<int16_t>(y2);
        src = out;
    }
}

}  // namespace

extern "C" {

void cpp_biquad_acc64(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    cascade<int64_t>(c, s, sections, in, out, n);
}

void cpp_biquad_acc32(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    cascade<int32_t>(c, s, sections, in, out, n);
}

void cpp_biquad_blk32(const dsp_biquad_coefs_t *c, dsp_biquad_state_t *s, size_t sections, const int16_t *in, int16_t *out, size_t n)
{
    blk<int32_t>(c, s, sections, in, out, n);
}

}  // extern "C"
