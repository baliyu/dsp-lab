// dsp.hpp: header-only C++ biquad, templated on accumulator type.
// Built with AI assistance (Claude). Arithmetic is specified in docs/DESIGN.md.
#pragma once
#include <cstddef>
#include <cstdint>

namespace dsp {

constexpr int kBiquadShift = 14;
constexpr int32_t kL1Limit = 65535;

constexpr int32_t abs16(int16_t v) { return v < 0 ? -static_cast<int32_t>(v) : static_cast<int32_t>(v); }

constexpr int32_t l1(int16_t b0, int16_t b1, int16_t b2, int16_t a1, int16_t a2)
{
    return abs16(b0) + abs16(b1) + abs16(b2) + abs16(a1) + abs16(a2);
}

// One direct form I section. Coefficients are template parameters, so the
// 32-bit overflow check happens at compile time.
template <typename Acc, int16_t B0, int16_t B1, int16_t B2, int16_t A1, int16_t A2>
struct Biquad {
    static_assert(sizeof(Acc) >= 8 || l1(B0, B1, B2, A1, A2) <= kL1Limit,
                  "32-bit accumulator can overflow: sum of absolute coefficients exceeds 65535");

    int16_t x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    int16_t step(int16_t x)
    {
        const Acc acc = static_cast<Acc>(B0) * x + static_cast<Acc>(B1) * x1 + static_cast<Acc>(B2) * x2
                      - static_cast<Acc>(A1) * y1 - static_cast<Acc>(A2) * y2;
        const Acc r = (acc + (static_cast<Acc>(1) << (kBiquadShift - 1))) >> kBiquadShift;
        const int16_t y = r > INT16_MAX ? INT16_MAX : (r < INT16_MIN ? INT16_MIN : static_cast<int16_t>(r));
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

}  // namespace dsp
