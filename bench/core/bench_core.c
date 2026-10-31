#include "bench_core.h"
#include "bench_data.h"
#include "dsp.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define EMPTY_REPS 8u

/* Everything the timed code touches lives in RAM: the input block, the output block, the coefficients and the filter state. The code itself
   runs from wherever the linker put it (flash, behind the instruction cache on the nRF52840). */
static int16_t in_ram[BENCH_N];
static int16_t out_ram[BENCH_N];
static dsp_biquad_coefs_t bq_coefs[BENCH_SECTIONS];
static dsp_biquad_state_t bq_state[BENCH_SECTIONS];
static int16_t fir_coefs[BENCH_TAPS];
static int16_t fir_delay[BENCH_TAPS];
static dsp_fir_state_t fir_state;

enum { KIND_BIQUAD = 0, KIND_FIR = 1 };

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void say(const bench_platform_t *p, const char *fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    p->emit(buf);
}

static void prepare(int kind)
{
    if (kind == KIND_BIQUAD) {
        for (size_t k = 0; k < BENCH_SECTIONS; k++) dsp_biquad_init(&bq_state[k]);
    } else {
        dsp_fir_init(&fir_state, fir_delay, BENCH_TAPS);
    }
    memset(out_ram, 0x5A, sizeof out_ram);              /* a filter that wrote nothing must fail the checksum, not inherit the last block's output */
}

static void apply(int kind)
{
    if (kind == KIND_BIQUAD)
        dsp_biquad_cascade(bq_coefs, bq_state, BENCH_SECTIONS, in_ram, out_ram, BENCH_N);
    else
        dsp_fir(fir_coefs, &fir_state, in_ram, out_ram, BENCH_N);
}

static unsigned run_kind(const bench_platform_t *p, int kind, const char *fname, const uint32_t *expect, unsigned reps, unsigned *runs)
{
    unsigned failed = 0;
    for (size_t v = 0; v < BENCH_NVEC; v++) {
        uint32_t cmin = 0xFFFFFFFFu, cmax = 0, crc = 0;
        int ok = 1;
        memcpy(in_ram, bench_vec[v], sizeof in_ram);
        for (unsigned r = 0; r < reps; r++) {
            prepare(kind);
            unsigned key = p->irq_lock();
            uint32_t c0 = p->cycles();
            apply(kind);
            uint32_t c1 = p->cycles();
            p->irq_unlock(key);
            uint32_t d = c1 - c0;
            if (d < cmin) cmin = d;
            if (d > cmax) cmax = d;
            crc = p->crc32((const uint8_t *)out_ram, sizeof out_ram);
            if (crc != expect[v]) ok = 0;               /* every repeat must be right, not only the last */
        }
        say(p, "RESULT %s %s n=%u crc=%08lx expect=%08lx ok=%d cyc_min=%lu cyc_max=%lu", fname, bench_vec_name[v], (unsigned)BENCH_N,
            (unsigned long)crc, (unsigned long)expect[v], ok, (unsigned long)cmin, (unsigned long)cmax);
        if (!ok) failed++;
        (*runs)++;
    }
    return failed;
}

int bench_run(const bench_platform_t *p, unsigned reps)
{
    unsigned runs = 0, failed = 0;
    uint32_t omin = 0xFFFFFFFFu;

    if (reps == 0) reps = 1;
    for (size_t k = 0; k < BENCH_SECTIONS; k++) {
        bq_coefs[k].b0 = bench_bq_coefs[k][0];
        bq_coefs[k].b1 = bench_bq_coefs[k][1];
        bq_coefs[k].b2 = bench_bq_coefs[k][2];
        bq_coefs[k].a1 = bench_bq_coefs[k][3];
        bq_coefs[k].a2 = bench_bq_coefs[k][4];
    }
    memcpy(fir_coefs, bench_fir_coefs, sizeof fir_coefs);

    say(p, "BENCH 1 n=%u vectors=%u sections=%u taps=%u reps=%u", (unsigned)BENCH_N, (unsigned)BENCH_NVEC, (unsigned)BENCH_SECTIONS,
        (unsigned)BENCH_TAPS, reps);
#if defined(__VERSION__)
    say(p, "ENV compiler %s", __VERSION__);
#endif
#if defined(__OPTIMIZE_SIZE__)
    say(p, "ENV optimisation size (-Os or -Oz)");
#elif defined(__OPTIMIZE__)
    say(p, "ENV optimisation speed (-O1, -O2 or -O3; read the exact flag from the build)");
#else
    say(p, "ENV optimisation none (-O0)");
#endif

    for (unsigned i = 0; i < EMPTY_REPS; i++) {         /* the cost of the measurement itself: two counter reads and nothing between them */
        unsigned key = p->irq_lock();
        uint32_t c0 = p->cycles();
        uint32_t c1 = p->cycles();
        p->irq_unlock(key);
        if (c1 - c0 < omin) omin = c1 - c0;
    }
    say(p, "OVERHEAD cyc_min=%lu", (unsigned long)omin);

    failed += run_kind(p, KIND_BIQUAD, BENCH_FILTER_BQ, bench_expect_bq, reps, &runs);
    failed += run_kind(p, KIND_FIR, BENCH_FILTER_FIR, bench_expect_fir, reps, &runs);
    say(p, "SUMMARY runs=%u failed=%u", runs, failed);
    return (int)failed;
}
