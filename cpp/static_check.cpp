#include <cstdio>
#include "dsp.hpp"
using namespace dsp;

int main()
{
    Biquad<int32_t, 1655, 3311, 1655, -14015, 3436> lp1;
    Biquad<int32_t, 1655, 3311, 1655, -18236, 9405> lp2;
    Biquad<int64_t, 15969, -31938, 15969, -31597, 15237> hp64;
#ifdef EXPECT_REJECT
    Biquad<int32_t, 15969, -31938, 15969, -31597, 15237> hp32;
    (void)hp32;
#endif
    std::printf("%d %d %d\n", lp1.step(1000), lp2.step(1000), hp64.step(1000));
    return 0;
}
