"""C++ biquad against the integer model, bit for bit. Built with AI assistance (Claude)."""
import ctypes
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from dsp_lab.design import butter_sos, quantise_sos
from dsp_lab.model import biquad_cascade
from dsp_lab.vectors import make_vectors

VECTORS = make_vectors()
CASES = [("cpp_lp1000_i32", 1000, "lowpass"), ("cpp_lp1000_i64", 1000, "lowpass"), ("cpp_hp50_i64", 50, "highpass")]


@pytest.fixture(scope="module")
def cpplib(tmp_path_factory):
    if shutil.which("g++") is None:
        pytest.skip("g++ is needed to build the C++ library")
    out = tmp_path_factory.mktemp("cpp") / "libcpp.so"
    subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-shared", "-fPIC",
                    "-I", str(ROOT / "cpp"), str(ROOT / "cpp" / "biquad_wrap.cpp"), "-o", str(out)], check=True)
    return ctypes.CDLL(str(out))


def test_vector_count():
    assert len(VECTORS) == 11


@pytest.mark.parametrize("name,fc,kind", CASES)
@pytest.mark.parametrize("vec", sorted(VECTORS))
def test_cpp_equals_model(cpplib, name, fc, kind, vec):
    x = np.ascontiguousarray(VECTORS[vec], dtype=np.int16)
    sections = quantise_sos(butter_sos(8000, fc, 4, kind))
    expected = np.array(biquad_cascade(sections, x.tolist()), dtype=np.int16)
    out = np.zeros_like(x)
    p = ctypes.POINTER(ctypes.c_int16)
    f = getattr(cpplib, name)
    f.argtypes = [p, p, ctypes.c_size_t]
    f(x.ctypes.data_as(p), out.ctypes.data_as(p), len(x))
    assert np.array_equal(out, expected)
