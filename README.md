# dsp-lab

Small fixed-point digital filters for 16-bit samples, in portable C, with a Python reference and tests that require **bit-exact** agreement. The
aim: filters I can explain end to end, with measured error, measured cost on real microcontrollers, and honest limits.

Built with AI assistance (Claude). I chose the project and its checks and ran everything; the code and tests were written with Claude.

## Status, stated plainly

- **Stage 1 of 5 is done:** the library and its tests on the PC. **Nothing has run on a board yet.** No cycle counts yet.
- A biquad cascade (IIR) and a FIR filter, 16-bit samples, 64-bit accumulators, rounding to nearest, saturation.
- 75 tests. The C code, built into a library and called from Python, must equal an integer model **bit for bit** on every test signal; and the model must stay within a stated error of the ideal floating-point (SciPy) filter.
- The tests were checked by breaking the code on purpose: 25 deliberate bugs, each caught. One check found a real hole in my tests (the FIR's start-up clearing was never exercised because my test helper pre-zeroed the memory); it is fixed.
- Findings so far (see [docs/DESIGN.md](docs/DESIGN.md)): SciPy's default section gains do not fit 16-bit coefficients and must be shared between sections; the 50 Hz DC blocker loses up to 361 LSB to coefficient quantisation, while the 1000 Hz low-pass stays within 4.2 LSB; a 1 LSB limit cycle remains after an impulse.

## Plan

1. Library, integer model, tests (done).
2. Cycle-count harness and the M4F (Arduino Nano 33 BLE): cycles per sample, output identical to the PC.
3. The same on the Cortex-M0+ (Adafruit Feather M0), where the 64-bit accumulator will be slow.
4. An optimisation with measured before-and-after numbers (32-bit accumulation, gain scaling by signal level, CMSIS-DSP for comparison).
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
| `python/dsp_lab/model.py` | The integer model: exactly what the C code must compute |
| `python/dsp_lab/design.py` | Filter design in floating point, quantisation to the integer formats, and the checks that show what quantisation cost |
| `python/dsp_lab/vectors.py` | Seeded test signals |
| `tests/` | C against the model (bit-exact) and against floating point (bounded error) |
| `docs/DESIGN.md` | The arithmetic, the measured numbers and the limits |

## Licence

MIT.
