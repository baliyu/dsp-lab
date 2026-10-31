# dsp-lab

Small fixed-point digital filters for 16-bit samples, in portable C, with a Python reference and tests that require **bit-exact** agreement. The
aim: filters I can explain end to end, with measured error, measured cost on real microcontrollers, and honest limits.

Built with AI assistance (Claude). I chose the project and its checks and ran everything; the code and tests were written with Claude.

## Status, stated plainly

- **Stage 1 of 5 is done:** the library and its tests on the PC.
- **Stage 2 is done:** on an Arduino Nano 33 BLE (Cortex-M4F, 64 MHz) the output was bit-identical to the PC model for every test block in 3 complete runs. Measured with the DWT cycle counter, best case (interrupts masked, data in RAM): **132 cycles per sample** for the 2-section biquad cascade and **373** for the 31-tap FIR. One board, one compiler setting; the capture has small gaps. See [docs/BENCH.md](docs/BENCH.md) for the method, the log and the limits.
- **Stage 4, first result (Feather M0):** a 32-bit accumulator gives bit-identical output with **4.2 times fewer cycles for the biquad cascade (1018 to 244 per sample) and 3.2 times fewer for the FIR (3453 to 1087)** at the Arduino IDE's `-Os`; with function-level `-O2` the FIR reaches 563 (6.1 times). This is exact only when the sum of the absolute coefficients is at most 65535, which holds for the filters used here but **not** for the 50 Hz DC blocker; the code checks it and refuses otherwise. One board, one run; the Nano has not run the 32-bit variant yet. Restructured loops (biquad section by section, a linear-history FIR, a folded symmetric FIR) are built and tested but not yet timed. See [docs/BENCH.md](docs/BENCH.md).
- **Stage 3 is done:** on an Adafruit Feather M0 (Cortex-M0+, 48 MHz, built by the Arduino IDE at `-Os`) the output was bit-identical to the model in 4 complete runs. Best case, as above: **1038 cycles per sample** for the biquad cascade and **3485** for the FIR, about 8 and 9 times the Nano's cycles. That ratio mixes the missing 64-bit multiplier with a different compiler version and flags, and I have not separated them. See [docs/BENCH.md](docs/BENCH.md).
- A biquad cascade (IIR) and a FIR filter, 16-bit samples, 64-bit accumulators, rounding to nearest, saturation.
- 227 tests (75 for the filters, 152 for the 32-bit variants, the benchmark harness, its log checker and the Feather sketch folder). The C code, built into a library and called from Python, must equal an integer model **bit for bit** on every test signal; and the model must stay within a stated error of the ideal floating-point (SciPy) filter.
- The tests were checked by breaking the code on purpose: 25 deliberate bugs, each caught. One check found a real hole in my tests (the FIR's start-up clearing was never exercised because my test helper pre-zeroed the memory); it is fixed.
- Findings so far (see [docs/DESIGN.md](docs/DESIGN.md)): SciPy's default section gains do not fit 16-bit coefficients and must be shared between sections; the 50 Hz DC blocker loses up to 361 LSB to coefficient quantisation, while the 1000 Hz low-pass stays within 4.2 LSB; a 1 LSB limit cycle remains after an impulse.

## Plan

1. Library, integer model, tests (done).
2. Cycle-count harness and the M4F (Arduino Nano 33 BLE): cycles per sample, output identical to the PC (done).
3. The same on the Cortex-M0+ (Adafruit Feather M0), where the 64-bit accumulator will be slow (done).
4. Optimisations with measured before-and-after numbers (32-bit accumulation and optimisation level: measured on the Feather; still to do: the Nano, loop restructuring, gain scaling by signal level, CMSIS-DSP for comparison).
5. A results table and a README with the limits.

## Run the tests

```
pip install -r requirements.txt
pytest -q
```

Needs `gcc` (the tests build the C library with strict warnings).

## What is where

| Path | What it is |
|---|---|
| `c/dsp.h`, `c/dsp.c` | The filters: biquad cascade and FIR, no floating point, no dynamic memory |
| `c/dsp32.h`, `c/dsp32.c` | Stage 4: the same filters with a 32-bit accumulator, and the check that says when that is exact |
| `python/dsp_lab/model.py` | The integer model: exactly what the C code must compute |
| `python/dsp_lab/design.py` | Filter design in floating point, quantisation to the integer formats, and the checks that show what quantisation cost |
| `python/dsp_lab/vectors.py` | Seeded test signals |
| `bench/` | Stages 2 and 3: the benchmark (portable core, PC build, Zephyr app for the Nano 33 BLE, Arduino sketch for the Feather M0) |
| `python/dsp_lab/bench.py` | Writes the benchmark's C header and checks a log from the board against the model |
| `tests/` | C against the model (bit-exact) and against floating point (bounded error); the benchmark and its log checker |
| `docs/DESIGN.md` | The arithmetic, the measured numbers and the limits |
| `docs/BENCH.md` | Stage 2: how the cycles are measured, how to run it, what it does not cover |

## Licence

MIT.
