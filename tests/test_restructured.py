"""The restructured 32-bit variants (c/dsp32.c: dsp32_biquad_blk, dsp32_fir_lin, dsp32_fir_sym): bit-identical to the integer model, including when the data
arrives in uneven chunks (they carry state between calls in a different layout), and the extra check for the symmetric version."""
import ctypes

import numpy as np
import pytest

from conftest import CascadeRunner, FirRunner, as_i16
from dsp_lab import design, model, vectors

FS = 8000.0
V = vectors.make_vectors(FS)
LIMIT = 65535
CHUNKS = [1, 7, 30, 31, 32, 100, 3, 64, 2, 5]
FIRS = {"lin": "dsp32_fir_lin", "sym": "dsp32_fir_sym"}


def chunked(runner, x, sizes=CHUNKS):
    out, pos, i = [], 0, 0
    while pos < len(x):
        n = sizes[i % len(sizes)]
        out.append(runner.process(x[pos:pos + n]))
        pos, i = pos + n, i + 1
    return np.concatenate(out)


def fir_sym_ok(lib, coefs):
    arr = (ctypes.c_int16 * len(coefs))(*coefs)
    return bool(lib.dsp32_fir_sym_ok(arr, len(coefs)))


def scaled(rng, c):
    c = np.asarray(c, dtype=np.int64)
    l1 = int(np.abs(c).sum())
    if l1 > LIMIT:
        c = np.trunc(c * (LIMIT / l1)).astype(np.int64)
    assert int(np.abs(c).sum()) <= LIMIT
    return [int(v) for v in c]


def sym_coefs(rng, taps):
    half = rng.integers(-32768, 32768, taps // 2)
    mid = rng.integers(-32768, 32768, taps % 2)
    return scaled(rng, np.concatenate([half, mid, half[::-1]]))


def extreme_input(rng, n):
    return np.where(rng.random(n) < 0.7, rng.choice([-32768, 32767], n), rng.integers(-32768, 32768, n)).astype(np.int16)


# ---- the real filters, whole blocks and chunks --------------------------------------------------------------------------------------------

@pytest.mark.parametrize("fc", [1000, 300])
@pytest.mark.parametrize("vec", list(V))
def test_the_section_by_section_biquad_equals_the_model(lib, fc, vec):
    q = design.quantise_sos(design.butter_sos(FS, fc, 4, "lowpass"))
    assert CascadeRunner(lib, q, fn="dsp32_biquad_blk").process(V[vec]).tolist() == model.biquad_cascade(q, V[vec])


@pytest.mark.parametrize("vec", list(V))
def test_the_section_by_section_biquad_keeps_its_state_across_uneven_chunks(lib, vec):
    q = design.quantise_sos(design.butter_sos(FS, 1000, 4, "lowpass"))
    assert chunked(CascadeRunner(lib, q, fn="dsp32_biquad_blk"), V[vec]).tolist() == model.biquad_cascade(q, V[vec])


@pytest.mark.parametrize("kind", list(FIRS))
@pytest.mark.parametrize("vec", list(V))
def test_the_linear_and_symmetric_firs_equal_the_model_for_whole_blocks_and_for_uneven_chunks(lib, kind, vec):
    c = design.fir_lowpass_q15(FS, 1000, 31)
    want = model.fir(c, V[vec])
    assert FirRunner(lib, c, fn=FIRS[kind]).process(V[vec]).tolist() == want
    assert chunked(FirRunner(lib, c, fn=FIRS[kind]), V[vec]).tolist() == want


def test_the_section_by_section_biquad_works_in_place(lib):
    q = design.quantise_sos(design.butter_sos(FS, 1000, 4, "lowpass"))
    r = CascadeRunner(lib, q, fn="dsp32_biquad_blk")
    x = V["noise"].copy()
    xin, p = as_i16(x)
    r.fn(r.c, r.s, r.n, p, p, len(xin))                                       # the same array as input and output
    assert xin.tolist() == model.biquad_cascade(q, V["noise"])


def test_a_cascade_of_no_sections_copies_the_input(lib):
    r = CascadeRunner(lib, [], fn="dsp32_biquad_blk")
    assert r.process(V["chirp"]).tolist() == V["chirp"].tolist()


# ---- random coefficients within the limit, many tap counts, extreme inputs, chunked ---------------------------------------------------------------

def test_random_biquads_within_the_limit_match_the_model_with_chunked_extreme_input(lib):
    rng = np.random.default_rng(11)
    for _ in range(40):
        q = [tuple(scaled(rng, rng.integers(-32768, 32768, 5))) for _ in range(int(rng.integers(1, 4)))]
        x = extreme_input(rng, 250)
        assert chunked(CascadeRunner(lib, q, fn="dsp32_biquad_blk"), x, [1, 3, 17, 50]).tolist() == model.biquad_cascade(q, x)


@pytest.mark.parametrize("taps", [1, 2, 3, 4, 5, 8, 31, 32, 33])
def test_random_firs_within_the_limit_match_the_model_with_chunked_extreme_input(lib, taps):
    rng = np.random.default_rng(100 + taps)
    for _ in range(8):
        x = extreme_input(rng, 220)
        c = scaled(rng, rng.integers(-32768, 32768, taps))
        assert chunked(FirRunner(lib, c, fn="dsp32_fir_lin"), x, [1, 2, 3, 9, 40]).tolist() == model.fir(c, x)
        s = sym_coefs(rng, taps)
        assert fir_sym_ok(lib, s)
        assert chunked(FirRunner(lib, s, fn="dsp32_fir_sym"), x, [1, 2, 3, 9, 40]).tolist() == model.fir(s, x)


# ---- the symmetric version's check and its worst cases -------------------------------------------------------------------------------------------

def test_the_symmetric_check_accepts_a_symmetric_design_and_rejects_the_rest(lib):
    c = design.fir_lowpass_q15(FS, 1000, 31)
    assert fir_sym_ok(lib, c)
    bent = list(c)
    bent[3] += 1
    assert not fir_sym_ok(lib, bent)                                          # one tap differs from its mirror
    assert fir_sym_ok(lib, [5, 9, 9, 5]) and not fir_sym_ok(lib, [5, 9, 8, 5])          # even length
    assert fir_sym_ok(lib, [7]) and fir_sym_ok(lib, [])                       # nothing to mirror
    assert not fir_sym_ok(lib, [-32768, 0, -32768])                           # symmetric, but the sum of |coefficient| is 65536
    assert fir_sym_ok(lib, [32767, 1, 32767])                                 # 65535 exactly


@pytest.mark.parametrize("level", [32767, -32768])
def test_the_symmetric_fir_at_the_limit_with_the_worst_inputs_matches_the_model(lib, level):
    c = [32767, 1, 32767]                                                     # sum 65535; the folded product is 32767 * 65534 (or 65536), still below 2^31
    x = np.full(40, level, dtype=np.int16)
    assert FirRunner(lib, c, fn="dsp32_fir_sym").process(x).tolist() == model.fir(c, x)
    peak = 32767 * 2 * abs(level) + abs(level)
    assert peak + (1 << 14) < 2 ** 31
