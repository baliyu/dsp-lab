"""The biquad cascade: the C code against the integer model (bit for bit) and against the ideal floating-point filter (within measured bounds)."""
import numpy as np
import pytest
from scipy import signal

from conftest import CascadeRunner
from dsp_lab import design, model, vectors

FS = 8000.0
V = vectors.make_vectors(FS)
# name -> (cutoff, kind, bound in LSB on |integer filter - ideal float filter|). Bounds are about twice the measured worst case (see docs/DESIGN.md).
FILTERS = {"lp1000": (1000, "lowpass", 8), "lp300": (300, "lowpass", 45), "hp50": (50, "highpass", 720)}


def make(name):
    fc, kind, bound = FILTERS[name]
    sos = design.butter_sos(FS, fc, 4, kind)
    return sos, design.quantise_sos(sos), bound


@pytest.mark.parametrize("name", FILTERS)
@pytest.mark.parametrize("vec", list(V))
def test_the_c_code_equals_the_integer_model_bit_for_bit(lib, name, vec):
    _, q, _ = make(name)
    assert CascadeRunner(lib, q).process(V[vec]).tolist() == model.biquad_cascade(q, V[vec])


@pytest.mark.parametrize("block", [1, 7, 64, 333])
def test_processing_in_blocks_gives_the_same_output_as_one_pass(lib, block):
    _, q, _ = make("lp1000")
    x = V["noise"]
    whole = CascadeRunner(lib, q).process(x)
    r = CascadeRunner(lib, q)
    parts = [r.process(x[i:i + block]) for i in range(0, len(x), block)]
    assert np.concatenate(parts).tolist() == whole.tolist()


@pytest.mark.parametrize("name", FILTERS)
def test_the_error_against_the_ideal_float_filter_stays_within_the_documented_bound(lib, name):
    sos, q, bound = make(name)
    worst = 0.0
    for vec in ("sine_100", "sine_500", "sine_1000", "sine_2000", "sine_3500", "chirp", "noise", "step", "impulse"):
        y = CascadeRunner(lib, q).process(V[vec]).astype(float)
        ideal = np.clip(signal.sosfilt(sos, V[vec].astype(float)), -32768, 32767)
        worst = max(worst, float(np.abs(y - ideal).max()))
    assert worst <= bound, f"{name}: worst error {worst:.1f} LSB, bound {bound}"
    assert worst > 0                                                       # a test that can never fail would prove nothing


def test_a_gain_above_one_saturates_and_does_not_wrap(lib):
    q = [(32767, 0, 0, 0, 0)]                      # y = x * 1.99994
    x = np.array([20000, -20000, 5000, 16383, -16384, 0], dtype=np.int16)
    y = CascadeRunner(lib, q).process(x)
    assert y.tolist() == model.biquad_cascade(q, x)
    for xi, yi in zip(x.tolist(), y.tolist()):
        exact = xi * 32767 / 16384
        if exact > 32767:
            assert yi == 32767                      # not -25536, which is what a wrapped 40000 would be
        elif exact < -32768:
            assert yi == -32768
        else:
            assert abs(yi - exact) <= 0.5 + 1e-9    # in range: the exact product, rounded to nearest


def test_a_full_scale_input_never_goes_outside_int16_and_matches_the_model(lib):
    _, q, _ = make("hp50")
    y = CascadeRunner(lib, q).process(V["square_fullscale"])
    assert y.tolist() == model.biquad_cascade(q, V["square_fullscale"])
    assert y.max() == 32767 or y.min() == -32768                         # the high-pass overshoots the input, so it really saturates


def test_silence_stays_silence_but_an_impulse_leaves_a_one_lsb_limit_cycle(lib):
    _, q, _ = make("lp1000")
    assert not CascadeRunner(lib, q).process(V["silence"]).any()
    tail = CascadeRunner(lib, q).process(V["impulse"])[500:]
    assert np.abs(tail).max() == 1                                         # a known fixed-point effect, documented in docs/DESIGN.md, bounded at 1 LSB


@pytest.mark.parametrize("name", FILTERS)
def test_the_quantised_designs_are_stable_and_close_to_the_ideal_response(name):
    fc, kind, _ = FILTERS[name]
    sos = design.butter_sos(FS, fc, 4, kind)
    sq = design.sos_from_q(design.quantise_sos(sos))
    assert design.pole_radius(sq) < 1.0
    lo, hi = (10, fc) if kind == "lowpass" else (fc, 3500)
    assert design.response_error_db(sos, sq, FS, lo, hi) < 0.05


def test_the_design_refuses_coefficients_that_do_not_fit_and_sharing_the_gain_fixes_it():
    sos = design.butter_sos(FS, 1000, 4, "lowpass")
    with pytest.raises(ValueError):
        design.quantise_sos(sos, share=False)                              # SciPy's second section has b1 = 2.0, which is outside Q2.14
    shared = design.share_gain(sos)
    w = np.linspace(10, 3900, 200)
    _, h1 = signal.sosfreqz(sos, worN=w, fs=FS)
    _, h2 = signal.sosfreqz(shared, worN=w, fs=FS)
    assert np.allclose(h1, h2, rtol=1e-9, atol=1e-12)                      # sharing the gain does not change the response


def test_init_clears_the_history_and_in_place_processing_works(lib):
    import ctypes
    _, q, _ = make("lp1000")
    r = CascadeRunner(lib, q)
    first = r.process(V["noise"][:200])
    for i in range(r.n):
        r.lib.dsp_biquad_init(ctypes.byref(r.s[i]))
    assert r.process(V["noise"][:200]).tolist() == first.tolist()
    buf = np.ascontiguousarray(V["noise"][:200].copy())
    p = buf.ctypes.data_as(ctypes.POINTER(ctypes.c_int16))
    r2 = CascadeRunner(lib, q)
    lib.dsp_biquad_cascade(r2.c, r2.s, r2.n, p, p, len(buf))
    assert buf.tolist() == first.tolist()


def test_init_clears_a_state_that_held_garbage(lib):
    r = CascadeRunner(lib, make("lp1000")[1])
    for i in range(r.n):
        r.s[i].x1, r.s[i].x2, r.s[i].y1, r.s[i].y2 = 1111, -2222, 3333, -4444
        lib.dsp_biquad_init(__import__("ctypes").byref(r.s[i]))
    assert r.process(V["silence"]).tolist() == [0] * len(V["silence"])
