/* bench_core: runs the biquad cascade and the FIR filter over the generated test vectors, checksums every output block and measures the cycles
   of every block. It is portable C: everything that depends on the machine (cycle counter, interrupt masking, CRC, text output) comes in
   through bench_platform_t, so the same code runs on the PC (tests) and on the board (Zephyr). Built with AI assistance (Claude). */
#ifndef BENCH_CORE_H
#define BENCH_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t (*cycles)(void);                                   /* free-running 32-bit counter; differences are taken with unsigned arithmetic */
    unsigned (*irq_lock)(void);                                 /* mask interrupts; the timed block runs with them masked */
    void (*irq_unlock)(unsigned key);
    uint32_t (*crc32)(const uint8_t *data, size_t len);         /* CRC-32 (IEEE, as zlib), input bytes in memory order */
    void (*emit)(const char *line);                             /* one finished line of text, no newline */
} bench_platform_t;

/* Runs everything `reps` times per block and emits the log lines that python/dsp_lab/bench.py checks. Returns the number of blocks whose
   checksum differed from the expected one (0 = every output bit-identical to the model). Assumes a little-endian machine. */
int bench_run(const bench_platform_t *p, unsigned reps);

#endif
