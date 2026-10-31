/* The benchmark on the PC: used by the tests to check the benchmark logic (checksums, state clearing, timing calls) without a board.
     bench_host            real clock (nanoseconds stand in for cycles)
     bench_host flaky      the second checksum computed is wrong (a repeat that goes wrong once, then recovers)
     bench_host fake       a counter that starts near the 32-bit wrap and moves 10 per read, and prints how often each platform call was made */
#define _POSIX_C_SOURCE 199309L
#include "bench_core.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static unsigned n_cycles, n_lock, n_unlock;
static uint32_t fake_now = 0xFFFFFF00u;
static int fake_mode, flaky_mode;
static unsigned n_crc;

static uint32_t cycles(void)
{
    n_cycles++;
    if (fake_mode) {
        uint32_t v = fake_now;
        fake_now += 10u;
        return v;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec) + 1u;
}

static unsigned lock(void) { n_lock++; return 0x1234u; }

static void unlock(unsigned key)
{
    if (key == 0x1234u) n_unlock++;
}

/* bit-by-bit CRC-32, reflected, polynomial 0xEDB88320: a third implementation, independent of zlib (Python) and of Zephyr's crc32_ieee */
static uint32_t crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    n_crc++;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return (flaky_mode && n_crc == 2u) ? ~c ^ 1u : ~c;    /* flaky: the second checksum ever computed is wrong, every other one is right */
}

static void emit(const char *line) { puts(line); }

int main(int argc, char **argv)
{
    static const bench_platform_t plat = { cycles, lock, unlock, crc32, emit };
    fake_mode = (argc > 1 && strcmp(argv[1], "fake") == 0);
    flaky_mode = (argc > 1 && strcmp(argv[1], "flaky") == 0);
    puts(fake_mode ? "ENV platform host PC, fake counter (10 per read), not cycles" : "ENV platform host PC, counter is nanoseconds, not cycles");
    int failed = bench_run(&plat, 3);
    if (fake_mode) printf("HOSTINFO cycles_calls=%u locks=%u unlocks=%u\n", n_cycles, n_lock, n_unlock);
    return failed ? 1 : 0;
}
