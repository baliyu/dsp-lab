# dsp-lab

Small fixed-point digital filters for 16-bit samples, in portable C, with a Python reference and tests that require **bit-exact** agreement. The aim: filters I can explain end to end, with measured error, measured cost on two real microcontrollers, and honest limits.

Built with AI assistance (Claude). I chose the project and its checks, ran the builds, tests and board measurements, and checked the results; the code and tests were written with Claude.

## Results

Cycles per sample on a hardware cycle counter: minimum of 3 repeats, mean over 11 test signals, 2000-sample blocks. Every figure is for output that is **bit-identical to the integer model** (a CRC-32 of each output block is compared). Logs are in [bench/logs](bench/logs); method and limits in [docs/BENCH.md](docs/BENCH.md).

**Biquad cascade, 2 sections** (4th-order Butterworth low-pass, 1000 Hz, fs 8 kHz)

| Variant | Nano 33 BLE (Cortex-M4F, 64 MHz, `-O2`) | Feather M0 (Cortex-M0+, 48 MHz, `-Os`) | Feather M0, function-level `-O2` |
|---|---|---|---|
| 64-bit accumulator (the start) | 134 | 1019 | 1007 |
| 32-bit accumulator | 87 | 244 | 247 |
| 32-bit, section by section | **50** | 158 | **126** |

**FIR, 31 taps** (low-pass, 1000 Hz)

| Variant | Nano 33 BLE | Feather M0 (`-Os`) | Feather M0, function-level `-O2` |
|---|---|---|---|
| 64-bit accumulator (the start) | 376 | 3453 | 2858 |
| 32-bit accumulator | 361 | 1087 | 563 |
| 32-bit, linear history | 270 | 764 | 449 |
| 32-bit, linear history, symmetric fold | **201** | 527 | **390** |

What the numbers show:

- The 64-bit accumulator is expensive on the Cortex-M0+, which has no 64-bit multiply (each product is a library call): a 32-bit accumulator cuts the cycles 4.2 times (biquad) and 3.2 times (FIR). On the Cortex-M4F it matters much less: 1.5 times and 1.04 times.
- Best variant against the 64-bit start: Feather 8.1 times (biquad) and 8.9 times (FIR); Nano 2.7 times and 1.9 times.
- At 8 kHz, for the filter code alone, the share of the time between samples goes from 17% to 2.1% (biquad) and from 58% to 6.5% (FIR) on the Feather, and from 1.7% to 0.63% and from 4.7% to 2.5% on the Nano.
- I wrote predictions down before the Nano run. Some were wrong (the 32-bit accumulator helped the M4F biquad 1.5 times where I expected little change); the scorecard is in [docs/BENCH.md](docs/BENCH.md).

## Limits, stated plainly

- One board per core, one run of each program. Differences of 1 to 2% between programs are not meaningful.
- Interrupts were masked and the data was in RAM, so these are best-case figures. Flash wait states and cache effects are not separated.
- The two builds differ in compiler and flags (GCC 9.2.1 at `-Os` on the Feather, GCC 14.3.0 at `-O2` on the Nano). The Feather "function-level `-O2`" column uses a compiler pragma inside an `-Os` program, not a whole-program `-O2` build. How much of the Feather-to-Nano gap is the core and how much the compiler is not separated.
- The 32-bit accumulator is exact only if the sum of the absolute coefficient values is at most 65535 (per biquad section; over all taps for the FIR). That holds for the filters used here and **not** for the 50 Hz DC blocker. The code checks the bound and the benchmark refuses to run a 32-bit variant that fails it. The symmetric fold also needs a symmetric design.
- Two filters, 11 test signals. Nothing here says anything about other cores, filters or compilers. I did not compare with CMSIS-DSP.

## How it was checked

- 227 tests. The C code, built into a library and called from Python, must equal an integer model **bit for bit** on every test signal; the model must stay within a stated error of the ideal floating-point (SciPy) filter; the 32-bit variants are tested against the model including at the worst-case coefficient bound, with data fed in uneven chunks; the log checker is tested against damaged and wrong logs.
- The tests were themselves checked by breaking the code on purpose, in several rounds: 25 deliberate bugs in the first round on the filters, then further rounds on the benchmark and the 32-bit code (see [docs/BENCH.md](docs/BENCH.md)). Checks that went unnoticed exposed real holes in my tests; those are fixed.
- Every board measurement is checked by comparing each output block's CRC-32 from the board with the model. A log with a wrong or missing block fails the check.

Findings about the arithmetic itself (see [docs/DESIGN.md](docs/DESIGN.md)): SciPy's default section gains do not fit 16-bit coefficients and must be shared between sections; the 50 Hz DC blocker loses up to 361 LSB to coefficient quantisation, while the 1000 Hz low-pass stays within 4.2 LSB; a 1 LSB limit cycle remains after an impulse.

## Status

1. Library, integer model, tests: done.
2. Cycle counts on the Cortex-M4F (Arduino Nano 33 BLE): done.
3. The same on the Cortex-M0+ (Adafruit Feather M0): done.
4. Optimisations with measured before-and-after numbers: accumulator width, optimisation level and loop structure are done. Gain scaling by signal level and a CMSIS-DSP comparison were planned and are **not done**; CMSIS-DSP rounds differently, so it would be compared by error against the ideal filter, not by bit-exactness.
5. Results table and limits: this README.

## Run the tests

```
pip install -r requirements.txt
pytest -q
```

Needs `gcc` (the tests build the C library with strict warnings).

## What is where

| Path | What it is |
|---|---|
| `c/dsp.h`, `c/dsp.c` | The filters: biquad cascade and FIR, 64-bit accumulator, no floating point, no dynamic memory |
| `c/dsp32.h`, `c/dsp32.c` | The same filters with a 32-bit accumulator, the restructured versions, and the check that says when that is exact |
| `python/dsp_lab/model.py` | The integer model: exactly what the C code must compute |
| `python/dsp_lab/design.py` | Filter design in floating point, quantisation to the integer formats, and the checks that show what quantisation cost |
| `python/dsp_lab/vectors.py` | Seeded test signals |
| `bench/` | The benchmark: portable core, PC build, Zephyr app for the Nano 33 BLE, Arduino sketch for the Feather M0, and the board logs in `bench/logs/` |
| `python/dsp_lab/bench.py` | Writes the benchmark's C header and checks a log from a board against the model |
| `tests/` | C against the model (bit-exact) and against floating point (bounded error); the benchmark and its log checker |
| `docs/DESIGN.md` | The arithmetic, the measured numbers and the limits |
| `docs/BENCH.md` | How the cycles are measured, every result, the prediction scorecard, and what the measurements do not cover |

## Licence

MIT.
