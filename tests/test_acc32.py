"""The 32-bit accumulator variant (c/dsp32.c): bit-identical to the integer model wherever its safety check passes, and the check itself."""
import ctypes

import numpy as np
import pytest

from conftest import CascadeRunner, FirRunner
from dsp_lab import design, model, vectors

FS = 8000.0
V = vectors.make_vectors(FS)
LIMIT = 65535
FILTERS = {"lp1000": (1000, "lowpass"), "lp300": (300, "lowpass")}


def q_of(name):
    fc, kind = FILTERS[name]
    return design.quantise_sos(design.butter_sos(FS, fc, 4, kind))


def biquad_ok(lib, q):
    arr = (lib.Coefs * len(q))(*[lib.Coefs(*s) for s in q])
    return bool(lib.dsp32_biquad_ok(arr, len(q)))


def fir_ok(lib, coefs):
    arr = (ctypes.c_int16 * len(coefs))(*coefs)
    return bool(lib.dsp32_fir_ok(arr, len(coefs)))


# ---- exactness on the real filters -------------------------------------------------------------------------------------------------------------

@pytest.mark.parametrize("name", FILTERS)
@pytest.mark.parametrize("vec", list(V))
def test_the_32bit_biquad_equals_the_model_bit_for_bit(lib, name, vec):
    q = q_of(name)
    assert biquad_ok(lib, q)
    assert CascadeRunner(lib, q, impl="dsp32").process(V[vec]).tolist() == model.biquad_cascade(q, V[vec])


@pytest.mark.parametrize("vec", list(V))
def test_the_32bit_fir_equals_the_model_bit_for_bit(lib, vec):
    c31 = design.fir_lowpass_q15(FS, 1000, 31)
    assert fir_ok(lib, c31)
    assert FirRunner(lib, c31, impl="dsp32").process(V[vec]).tolist() == model.fir(c31, V[vec])


# ---- the safety check ----------------------------------------------------------------------------------------------------------------------------

def test_the_check_accepts_the_filters_used_and_rejects_the_dc_blocker(lib):
    assert biquad_ok(lib, q_of("lp1000")) and biquad_ok(lib, q_of("lp300"))
    assert fir_ok(lib, design.fir_lowpass_q15(FS, 1000, 31))
    hp50 = design.quantise_sos(design.butter_sos(FS, 50, 4, "highpass"))
    assert not biquad_ok(lib, hp50)                                           # poles near the unit circle: the coefficients are large


def test_the_check_limit_is_exact_for_both_filters(lib):
    assert biquad_ok(lib, [(-32768, 32767, 0, 0, 0)]) and not biquad_ok(lib, [(-32768, -32768, 0, 0, 0)])      # 65535 passes, 65536 fails
    assert biquad_ok(lib, [(1, 2, 3, 4, 5), (-32768, 32767, 0, 0, 0)]) and not biquad_ok(lib, [(1, 2, 3, 4, 5), (0, 0, -32768, -32768, 0)])   # every section is checked
    assert fir_ok(lib, [-32768, 32767]) and not fir_ok(lib, [-32768, -32768])
    assert fir_ok(lib, [1] * 65535) and not fir_ok(lib, [1] * 65536)           # many small taps add up too


def test_the_arithmetic_bound_behind_the_limit_holds_exactly():
    worst_fir = LIMIT * 32768 + (1 << 14)
    worst_biquad = LIMIT * 32768 + (1 << 13)
    assert worst_fir < 2 ** 31 and worst_biquad < 2 ** 31                     # no overflow at the limit
    assert (LIMIT + 1) * 32768 >= 2 ** 31                                     # and the limit cannot be raised by one


# ---- randomised and worst-case inputs, against the model -------------------------------------------------------------------------------------

def scaled(rng, n):
    c = rng.integers(-32768, 32768, n)
    l1 = int(np.abs(c).sum())
    if l1 > LIMIT:
        c = np.trunc(c * (LIMIT / l1)).astype(np.int64)
    assert int(np.abs(c).sum()) <= LIMIT
    return [int(v) for v in c]


def extreme_input(rng, n):
    return np.where(rng.random(n) < 0.7, rng.choice([-32768, 32767], n), rng.integers(-32768, 32768, n)).astype(np.int16)


def test_random_coefficients_within_the_limit_and_extreme_inputs_match_the_model_for_the_biquad(lib):
    rng = np.random.default_rng(2026)
    for _ in range(60):
        q = [tuple(scaled(rng, 5)) for _ in range(int(rng.integers(1, 4)))]
        x = extreme_input(rng, 300)
        assert biquad_ok(lib, q)
        assert CascadeRunner(lib, q, impl="dsp32").process(x).tolist() == model.biquad_cascade(q, x)


def test_random_coefficients_within_the_limit_and_extreme_inputs_match_the_model_for_the_fir(lib):
    rng = np.random.default_rng(7)
    for taps in (1, 2, 3, 8, 31, 64):
        for _ in range(10):
            c = scaled(rng, taps)
            x = extreme_input(rng, 300)
            assert fir_ok(lib, c)
            assert FirRunner(lib, c, impl="dsp32").process(x).tolist() == model.fir(c, x)


def test_the_fir_worst_case_at_the_limit_matches_the_model_and_does_not_overflow(lib):
    c = [32767, -32768]                                                       # sum of |coefficient| = 65535
    x = np.array([-32768, 32767, 32767, -32768, -32768, 32767] * 20, dtype=np.int16)     # x[n]=32767 after x[n-1]=-32768: both products are positive and as large as they can be
    got = FirRunner(lib, c, impl="dsp32").process(x).tolist()
    assert got == model.fir(c, x)
    peak = max(abs(32767 * int(x[i]) - 32768 * int(x[i - 1])) for i in range(1, len(x)))
    assert peak + (1 << 14) < 2 ** 31                                         # the largest sum that occurs fits in int32
