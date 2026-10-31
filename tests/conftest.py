"""Builds c/dsp.c into a shared library with strict warnings, and gives the tests thin wrappers around it."""
import ctypes
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))


@pytest.fixture(scope="session")
def lib(tmp_path_factory):
    if shutil.which("gcc") is None:
        pytest.skip("gcc is needed to build the C library")
    out = tmp_path_factory.mktemp("build") / "libdsp.so"
    subprocess.run(["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-shared", "-fPIC",
                    str(ROOT / "c" / "dsp.c"), "-o", str(out)], check=True)
    L = ctypes.CDLL(str(out))

    class Coefs(ctypes.Structure):
        _fields_ = [(n, ctypes.c_int16) for n in ("b0", "b1", "b2", "a1", "a2")]

    class BState(ctypes.Structure):
        _fields_ = [(n, ctypes.c_int16) for n in ("x1", "x2", "y1", "y2")]

    class FState(ctypes.Structure):
        _fields_ = [("delay", ctypes.POINTER(ctypes.c_int16)), ("taps", ctypes.c_size_t), ("pos", ctypes.c_size_t)]

    i16p = ctypes.POINTER(ctypes.c_int16)
    L.dsp_biquad_init.argtypes = [ctypes.POINTER(BState)]
    L.dsp_biquad_step.restype = ctypes.c_int16
    L.dsp_biquad_step.argtypes = [ctypes.POINTER(Coefs), ctypes.POINTER(BState), ctypes.c_int16]
    L.dsp_biquad_cascade.argtypes = [ctypes.POINTER(Coefs), ctypes.POINTER(BState), ctypes.c_size_t, i16p, i16p, ctypes.c_size_t]
    L.dsp_fir_init.argtypes = [ctypes.POINTER(FState), i16p, ctypes.c_size_t]
    L.dsp_fir_step.restype = ctypes.c_int16
    L.dsp_fir_step.argtypes = [i16p, ctypes.POINTER(FState), ctypes.c_int16]
    L.dsp_fir.argtypes = [i16p, ctypes.POINTER(FState), i16p, i16p, ctypes.c_size_t]
    L.Coefs, L.BState, L.FState = Coefs, BState, FState
    return L


def as_i16(a):
    arr = np.ascontiguousarray(a, dtype=np.int16)
    return arr, arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int16))


class CascadeRunner:
    """Holds a cascade's coefficients and state in C memory, so blocks can be fed one after another."""
    def __init__(self, lib, q):
        self.lib, self.n = lib, len(q)
        self.c = (lib.Coefs * self.n)(*[lib.Coefs(*s) for s in q])
        self.s = (lib.BState * self.n)()
        for i in range(self.n):
            lib.dsp_biquad_init(ctypes.byref(self.s[i]))

    def process(self, x):
        xin, pin = as_i16(x)
        out = np.zeros_like(xin)
        _, pout = as_i16(out)
        self.lib.dsp_biquad_cascade(self.c, self.s, self.n, pin, pout, len(xin))
        return out


class FirRunner:
    def __init__(self, lib, coefs, dirty=False):
        self.lib = lib
        self.coefs, self.pc = as_i16(coefs)
        self.delay = (ctypes.c_int16 * len(coefs))()
        if dirty:                                          # garbage in the delay line: init must clear it
            for i in range(len(coefs)):
                self.delay[i] = 12345 - 77 * i
        self.state = lib.FState()
        lib.dsp_fir_init(ctypes.byref(self.state), self.delay, len(coefs))

    def process(self, x):
        xin, pin = as_i16(x)
        out = np.zeros_like(xin)
        _, pout = as_i16(out)
        self.lib.dsp_fir(self.pc, ctypes.byref(self.state), pin, pout, len(xin))
        return out
