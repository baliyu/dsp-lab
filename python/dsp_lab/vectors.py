"""Test signals (int16), all seeded so every run and every machine sees the same numbers."""
from __future__ import annotations

import numpy as np


def clip16(x) -> np.ndarray:
    return np.clip(np.round(x), -32768, 32767).astype(np.int16)


def make_vectors(fs: float = 8000.0, n: int = 2000, seed: int = 12345) -> dict[str, np.ndarray]:
    t = np.arange(n) / fs
    rng = np.random.default_rng(seed)
    v = {}
    imp = np.zeros(n); imp[0] = 16384
    v["impulse"] = clip16(imp)
    v["step"] = clip16(np.full(n, 12000.0))
    for f in (100, 500, 1000, 2000, 3500):
        v[f"sine_{f}"] = clip16(12000 * np.sin(2 * np.pi * f * t))
    v["chirp"] = clip16(14000 * np.sin(2 * np.pi * (50 * t + (fs / 2 - 50) / (2 * (n / fs)) * t ** 2)))
    v["noise"] = clip16(rng.normal(0, 4000, n))
    v["square_fullscale"] = clip16(np.where((np.arange(n) // 25) % 2 == 0, 32767, -32768))
    v["silence"] = np.zeros(n, dtype=np.int16)
    return v
