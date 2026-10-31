/* dsp-lab stage 3, board side: Adafruit Feather M0 (ATSAMD21G18, Cortex-M0+, 48 MHz). Built with AI assistance (Claude).

   Runs the same benchmark as the Nano 33 BLE (bench_core.c: the biquad cascade and the FIR over the 11 test vectors, each with a 64-bit and a 32-bit accumulator, at the
   default -Os and at function-level -O2) and prints the same log lines
   over USB serial, so python/dsp_lab/bench.py checks them in the same way. The Cortex-M0+ has no DWT cycle counter, so the cycles are counted by the
   SAMD21's 32-bit timer TC4+TC5 (32-bit mode), clocked from GCLK0 (the CPU clock, 48 MHz) with no prescaler: one count per CPU cycle. The count rate is checked
   at start against micros(), which comes from the core's own SysTick tick. The Cortex-M0+ has no 64-bit multiplier: the compiler calls a library routine (__aeabi_lmul).

   The files next to this sketch (dsp.c, dsp.h, dsp32.c, dsp32.h, dsp_o2.c, dsp32_o2.c, bench_core.c, bench_core.h, bench_variants.h, bench_data.h) are copies made by python/dsp_lab/feather_sketch.py from the files in
   c/ and bench/core/; a test fails if they differ. Open this folder in the Arduino IDE, board "Adafruit Feather M0 (SAMD21)". */
#include <Arduino.h>

#define BENCH_O2_UNITS 1                       /* also time dsp.c and dsp32.c built at function-level -O2 (see dsp_o2.c) */
extern "C" {
#include "bench_core.h"
#include "bench_variants.h"
}

#define REPEATS 3u
#define PAUSE_BETWEEN_RUNS_MS 10000UL

static bool timer_ok = false;
static uint32_t timer_hz = 0;

/* ---- the cycle counter: TC4+TC5 as one 32-bit counter at the CPU clock ------------------------------------------------------------------------ */

static void timer_start(void)
{
    PM->APBCMASK.reg |= PM_APBCMASK_TC4 | PM_APBCMASK_TC5;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5);
    while (GCLK->STATUS.bit.SYNCBUSY) { }

    TC4->COUNT32.CTRLA.reg = TC_CTRLA_SWRST;
    while (TC4->COUNT32.CTRLA.bit.SWRST || TC4->COUNT32.STATUS.bit.SYNCBUSY) { }
    TC4->COUNT32.CTRLA.reg = TC_CTRLA_MODE_COUNT32 | TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_WAVEGEN_NFRQ;
    TC4->COUNT32.CTRLA.reg |= TC_CTRLA_ENABLE;
    while (TC4->COUNT32.STATUS.bit.SYNCBUSY) { }
}

static inline uint32_t timer_now(void)
{
    TC4->COUNT32.READREQ.reg = TC_READREQ_RREQ | TC_READREQ_ADDR(0x10);      /* 0x10 = COUNT */
    while (TC4->COUNT32.STATUS.bit.SYNCBUSY) { }
    return TC4->COUNT32.COUNT.reg;
}

/* ---- the platform for bench_core ------------------------------------------------------------------------------------------------------------ */

static uint32_t plat_cycles(void) { return timer_now(); }

static unsigned plat_irq_lock(void)
{
    unsigned primask = (unsigned)__get_PRIMASK();
    __disable_irq();
    return primask;
}

static void plat_irq_unlock(unsigned primask)
{
    if (primask == 0u) __enable_irq();
}

/* bit-by-bit CRC-32 (IEEE, reflected, polynomial 0xEDB88320), the same result as zlib */
static uint32_t plat_crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

static void plat_emit(const char *line)
{
    Serial.println(line);
    delay(5);                                   /* the USB serial buffer is small; do not outrun the PC */
}

static const bench_platform_t plat = { plat_cycles, plat_irq_lock, plat_irq_unlock, plat_crc32, plat_emit };

/* ---- run --------------------------------------------------------------------------------------------------------------------------------------- */

void setup(void)
{
    Serial.begin(115200);
    while (!Serial) { }                         /* wait until a terminal opens the port, so the first run is not lost */
    delay(1000);

    timer_start();
    uint32_t t0 = timer_now();
    uint32_t u0 = micros();
    while ((uint32_t)(micros() - u0) < 100000UL) { }                           /* 100 ms by the core's clock */
    uint32_t dt_us = (uint32_t)(micros() - u0);
    uint32_t ticks = timer_now() - t0;
    uint32_t hz = (uint32_t)(((uint64_t)ticks * 1000000ULL) / dt_us);
    timer_ok = (hz > (uint32_t)(F_CPU / 100u * 98u)) && (hz < (uint32_t)(F_CPU / 100u * 102u));

    timer_hz = hz;
}

void loop(void)
{
    if (!timer_ok) {
        Serial.println("ERROR: the timer count rate is not within 2% of F_CPU; no benchmark was run");
        delay(2000);
        return;
    }
    Serial.println("ENV board adafruit_feather_m0 (ATSAMD21G18, Cortex-M0+)");
    Serial.print("ENV f_cpu_configured "); Serial.println((unsigned long)F_CPU);
    Serial.print("ENV timer_hz_vs_micros "); Serial.println((unsigned long)timer_hz);
    Serial.print("ENV arduino_define "); Serial.println((unsigned long)ARDUINO);
    (void)bench_run(&plat, REPEATS, bench_variants, BENCH_NVARIANTS);
    delay(PAUSE_BETWEEN_RUNS_MS);
}
