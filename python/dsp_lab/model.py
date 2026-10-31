"""The integer model: exactly what c/dsp.c must compute, written with plain Python integers (no floating point, no overflow).
The tests require the C output to equal this model bit for bit. docs/DESIGN.md states the arithmetic."""
from __future__ import annotations

BIQUAD_SHIFT = 14
FIR_SHIFT = 15
INT16_MIN, INT16_MAX = -32768, 32767


def sat16(v: int) -> int:
    return INT16_MAX if v > INT16_MAX else INT16_MIN if v < INT16_MIN else v


class Biquad:
    """y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2 (Q2.14 coefficients as ints), rounded to nearest, saturated to int16."""

    def __init__(self, b0: int, b1: int, b2: int, a1: int, a2: int):
        self.c = (b0, b1, b2, a1, a2)
        self.x1 = self.x2 = self.y1 = self.y2 = 0

    def step(self, x: int) -> int:
        b0, b1, b2, a1, a2 = self.c
        acc = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2
        y = sat16((acc + (1 << (BIQUAD_SHIFT - 1))) >> BIQUAD_SHIFT)      # Python's >> on a negative int rounds toward minus infinity, as C does on the target compilers
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


def biquad_cascade(sections: list[tuple[int, int, int, int, int]], x) -> list[int]:
    stages = [Biquad(*s) for s in sections]
    out = []
    for v in x:
        v = int(v)
        for st in stages:
            v = st.step(v)
        out.append(v)
    return out


def fir(coefs: list[int], x) -> list[int]:
    """y[n] = sum_k coefs[k] x[n-k] (Q15 coefficients), zero history, rounded to nearest, saturated to int16."""
    taps = len(coefs)
    hist = [0] * taps
    out = []
    for v in x:
        hist = [int(v)] + hist[:-1]
        acc = sum(c * h for c, h in zip(coefs, hist))
        out.append(sat16((acc + (1 << (FIR_SHIFT - 1))) >> FIR_SHIFT))
    return out
