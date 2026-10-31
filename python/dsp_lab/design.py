"""Filter design in floating point, then quantisation to the integer formats of c/dsp.c, with the checks that show what quantisation cost."""
from __future__ import annotations

import numpy as np
from scipy import signal

Q_BIQUAD = 14          # biquad coefficients: Q2.14
Q_FIR = 15             # FIR coefficients: Q15


def butter_sos(fs: float, fc: float, order: int = 4, kind: str = "lowpass") -> np.ndarray:
    """Second-order sections [b0 b1 b2 1 a1 a2] from SciPy."""
    return signal.butter(order, fc, btype=kind, fs=fs, output="sos")


def share_gain(sos: np.ndarray) -> np.ndarray:
    """SciPy puts the whole filter gain into one section, and the others then have numerators such as [1, 2, 1], which do not fit Q2.14.
    Here each section's numerator is scaled to a largest coefficient of 1, and the total gain is shared equally (geometric mean) between the
    sections. The cascade's overall response is unchanged."""
    sos = sos.copy()
    scales = np.array([np.max(np.abs(sec[:3])) for sec in sos])
    total = float(np.prod(scales))
    each = total ** (1.0 / len(sos))
    for sec, sc in zip(sos, scales):
        sec[:3] = sec[:3] / sc * each
    return sos


def quantise_sos(sos: np.ndarray, share: bool = True) -> list[tuple[int, int, int, int, int]]:
    """Round each section's b0 b1 b2 a1 a2 to Q2.14. Raises if a coefficient does not fit in int16 (|value| must be below 2)."""
    out = []
    for s in (share_gain(sos) if share else sos):
        b0, b1, b2, a0, a1, a2 = s
        assert abs(a0 - 1.0) < 1e-12, "SciPy's sos has a0 = 1"
        q = [int(round(v * (1 << Q_BIQUAD))) for v in (b0, b1, b2, a1, a2)]
        if any(v > 32767 or v < -32768 for v in q):
            raise ValueError(f"a coefficient of {s} does not fit Q2.14 (needs |value| < 2)")
        out.append(tuple(q))
    return out


def sos_from_q(q: list[tuple[int, int, int, int, int]]) -> np.ndarray:
    return np.array([[b0 / (1 << Q_BIQUAD), b1 / (1 << Q_BIQUAD), b2 / (1 << Q_BIQUAD), 1.0, a1 / (1 << Q_BIQUAD), a2 / (1 << Q_BIQUAD)]
                     for b0, b1, b2, a1, a2 in q])


def pole_radius(sos: np.ndarray) -> float:
    """Largest distance of any pole from the origin; the filter is stable if it is below 1."""
    return float(max(abs(np.roots([1.0, s[4], s[5]])).max() for s in sos))


def response_error_db(sos_ideal: np.ndarray, sos_quant: np.ndarray, fs: float, f_lo: float, f_hi: float, points: int = 400) -> float:
    """Largest difference, in dB, between the ideal and the quantised magnitude response over f_lo..f_hi."""
    w = np.linspace(f_lo, f_hi, points)
    _, h1 = signal.sosfreqz(sos_ideal, worN=w, fs=fs)
    _, h2 = signal.sosfreqz(sos_quant, worN=w, fs=fs)
    return float(np.max(np.abs(20 * np.log10(np.abs(h2) + 1e-12) - 20 * np.log10(np.abs(h1) + 1e-12))))


def fir_lowpass_q15(fs: float, fc: float, taps: int = 31) -> list[int]:
    """A Hamming-window FIR low-pass, Q15, with the sum of the coefficients just below 1 so that DC does not saturate."""
    h = signal.firwin(taps, fc, fs=fs, window="hamming")
    h = h / h.sum() * 0.999
    return [int(round(v * (1 << Q_FIR))) for v in h]
