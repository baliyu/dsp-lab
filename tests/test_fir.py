"""The FIR filter: the C code against the integer model (bit for bit) and against the floating-point convolution."""
import numpy as np
import pytest

from conftest import FirRunner
from dsp_lab import design, model, vectors

FS = 8000.0
V = vectors.make_vectors(FS)
C31 = design.fir_lowpass_q15(FS, 1000, 31)


@pytest.mark.parametrize("vec", list(V))
def test_the_c_code_equals_the_integer_model_bit_for_bit(lib, vec):
    assert FirRunner(lib, C31).process(V[vec]).tolist() == model.fir(C31, V[vec])


@pytest.mark.parametrize("taps", [1, 2, 3, 8, 31])
def test_every_tap_count_including_the_edge_cases_matches_the_model(lib, taps):
    coefs = design.fir_lowpass_q15(FS, 1000, taps) if taps > 2 else [20000, -5000][:taps]
    assert FirRunner(lib, coefs).process(V["noise"]).tolist() == model.fir(coefs, V["noise"])


@pytest.mark.parametrize("block", [1, 5, 31, 32, 500])
def test_blocks_give_the_same_output_as_one_pass(lib, block):
    x = V["noise"]
    whole = FirRunner(lib, C31).process(x)
    r = FirRunner(lib, C31)
    assert np.concatenate([r.process(x[i:i + block]) for i in range(0, len(x), block)]).tolist() == whole.tolist()


def test_the_only_error_against_the_float_convolution_of_the_same_coefficients_is_the_final_rounding(lib):
    h = np.array(C31) / 32768.0
    for vec in ("sine_100", "sine_1000", "chirp", "noise", "step", "impulse"):
        y = FirRunner(lib, C31).process(V[vec]).astype(float)
        ideal = np.clip(np.convolve(V[vec].astype(float), h)[:len(y)], -32768, 32767)
        assert np.abs(y - ideal).max() <= 0.5 + 1e-9                         # round-to-nearest of an exact sum


def test_quantising_the_coefficients_costs_a_few_lsb_against_the_unquantised_design(lib):
    from scipy import signal
    ideal_h = signal.firwin(31, 1000, fs=FS, window="hamming")
    ideal_h = ideal_h / ideal_h.sum() * 0.999
    y = FirRunner(lib, C31).process(V["noise"]).astype(float)
    ideal = np.convolve(V["noise"].astype(float), ideal_h)[:len(y)]
    worst = np.abs(y - ideal).max()
    assert 0.3 < worst < 3.0, worst                                            # measured: about 1 LSB; the lower limit proves the check can fail


def test_a_constant_input_settles_to_the_dc_gain(lib):
    y = FirRunner(lib, C31).process(np.full(100, 16000, dtype=np.int16))
    expected = 16000 * sum(C31) / 32768
    assert abs(float(y[-1]) - expected) <= 1.0 and (y[-1] == y[-2])


def test_a_large_gain_saturates_instead_of_wrapping(lib):
    coefs, x = [32767, 32767], np.array([30000, 30000, -30000, -30000], dtype=np.int16)
    y = FirRunner(lib, coefs).process(x)
    assert y.tolist() == model.fir(coefs, x)
    xs = [0] + x.tolist()
    for n in range(len(x)):
        exact = (32767 * xs[n + 1] + 32767 * xs[n]) / 32768       # the exact sum for these two taps, no rounding yet
        if exact > 32767:
            assert y[n] == 32767                                    # a wrapped 59998 would show as a negative number
        elif exact < -32768:
            assert y[n] == -32768
        else:
            assert abs(float(y[n]) - exact) <= 0.5 + 1e-9
    assert y.max() == 32767 and y.min() == -32768                   # both clamps were really exercised


def test_init_clears_a_delay_line_that_held_garbage(lib):
    assert FirRunner(lib, C31, dirty=True).process(V["noise"]).tolist() == model.fir(C31, V["noise"])
    assert FirRunner(lib, C31, dirty=True).process(V["silence"]).tolist() == [0] * len(V["silence"])
