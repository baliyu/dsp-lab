# dsp-lab

Small fixed-point digital filters for 16-bit samples, in portable C, with a Python reference and tests that require **bit-exact** agreement. The
aim: filters I can explain end to end, with measured error, measured cost on real microcontrollers, and honest limits.

Built with AI assistance (Claude). I chose the project and its checks and ran everything; the code and tests were written with Claude.

## Status, stated plainly

- **Stage 1 of 5 is done:** the library and its tests on the PC.
- **Stage 2 is done:** on an Arduino Nano 33 BLE (Cortex-M4F, 64 MHz) the output was bit-identical to the PC model for every test block in 3 complete runs. Measured with the DWT cycle counter, best case (interrupts masked, data in RAM): **132 cycles per sample** for the 2-section biquad cascade and **373** for the 31-tap FIR. One board, one compiler setting; the capture has small gaps. See [docs/BENCH.md](docs/BENCH.md) for the method, the log and the limits.
- **Stage 4 is started, nothing measured yet:** a 32-bit-accumulator variant (exact for the filters used here, with a stated and tested safety bound; it is *not* exact for the 50 Hz DC blocker) and a way to time several variants in one upload are built and tested on the PC and in an emulator. No board has run them.
- **Stage 3 is done:** on an Adafruit Feather M0 (Cortex-M0+, 48 MHz, built by the Arduino IDE at `-Os`) the output was bit-identical to the model in 4 complete runs. Best case, as above: **1038 cycles per sample** for the biquad cascade and **3485** for the FIR, about 8 and 9 times the Nano's cycles. That ratio mixes the missing 64-bit multiplier with a different compiler version and flags, and I have not separated them. See [docs/BENCH.md](docs/BENCH.md).
- A biquad cascade (IIR) and a FIR filter, 16-bit samples, 64-bit accumulators, rounding to nearest, saturation.
- 155 tests (75 for the filters, 80 for the 32-bit variant, the benchmark harness, its log checker and the Feather sketch folder). The C code, built into a library and called from Python, must equal an integer model **bit for bit** on every test signal; and the model must stay within a stated error of the ideal floating-point (SciPy) filter.
- The tests were checked by breaking the code on purpose: 25 deliberate bugs, each caught. One check found a real hole in my tests (the FIR's start-up clearing was never exercised because my test helper pre-zeroed the memory); it is fixed.
- Findings so far (see [docs/DESIGN.md](docs/DESIGN.md)): SciPy's default section gains do not fit 16-bit coefficients and must be shared between sections; the 50 Hz DC blocker loses up to 361 LSB to coefficient quantisation, while the 1000 Hz low-pass stays within 4.2 LSB; a 1 LSB limit cycle remains after an impulse.

## Plan

1. Library, integer model, tests (done).
2. Cycle-count harness and the M4F (Arduino Nano 33 BLE): cycles per sample, output identical to the PC (done).
3. The same on the Cortex-M0+ (Adafruit Feather M0), where the 64-bit accumulator will be slow (done).
4. Optimisations with measured before-and-after numbers (32-bit accumulation: built, not yet measured; optimisation level; gain scaling by signal level; CMSIS-DSP for comparison).
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
