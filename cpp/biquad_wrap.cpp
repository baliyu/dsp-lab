// C entry points so the pytest suite can call the C++ templates.
// Built with AI assistance (Claude). Coefficients must match
// quantise_sos(butter_sos(8000, fc, 4, kind)); the test checks that.
#include <cstddef>
#include <cstdint>
#include "dsp.hpp"
using namespace dsp;

extern "C" {

void cpp_lp1000_i32(const int16_t *in, int16_t *out, size_t n)
{
    Biquad<int32_t, 1655, 3311, 1655, -14015, 3436> s1;
    Biquad<int32_t, 1655, 3311, 1655, -18236, 9405> s2;
    for (size_t i = 0; i < n; i++) out[i] = s2.step(s1.step(in[i]));
}

void cpp_lp1000_i64(const int16_t *in, int16_t *out, size_t n)
{
    Biquad<int64_t, 1655, 3311, 1655, -14015, 3436> s1;
    Biquad<int64_t, 1655, 3311, 1655, -18236, 9405> s2;
    for (size_t i = 0; i < n; i++) out[i] = s2.step(s1.step(in[i]));
}

void cpp_hp50_i64(const int16_t *in, int16_t *out, size_t n)
{
    Biquad<int64_t, 15969, -31938, 15969, -31597, 15237> s1;
    Biquad<int64_t, 15969, -31938, 15969, -32258, 15899> s2;
    for (size_t i = 0; i < n; i++) out[i] = s2.step(s1.step(in[i]));
}

}  // extern "C"
