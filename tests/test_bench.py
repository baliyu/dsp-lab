"""Stage 2 on the PC: the generated header, the benchmark logic (bench/core) and the log checker. The board adds only the cycle counter, the
interrupt masking and the USB console; everything else that decides pass or fail is exercised here."""
import re
import subprocess
import zlib
from pathlib import Path

import numpy as np
import pytest

from conftest import CascadeRunner, FirRunner, ROOT
from dsp_lab import bench, feather_sketch, model

CORE = ROOT / "bench" / "core"
HOST_SRC = [ROOT / "c" / "dsp.c", ROOT / "c" / "dsp32.c", CORE / "dsp_o2.c", CORE / "dsp32_o2.c", CORE / "bench_core.c", ROOT / "bench" / "host" / "bench_host.c"]
HEADER = ROOT / "bench" / "core" / "bench_data.h"


def build_host(tmp_path, extra=(), srcs=None, name="bench_host", cdir=ROOT / "c"):
    exe = tmp_path / name
    subprocess.run(["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-I", str(cdir), "-I", str(ROOT / "bench" / "core"),
                    *extra, *map(str, srcs or HOST_SRC), "-o", str(exe)], check=True)
    return exe


@pytest.fixture(scope="module")
def host_exe(tmp_path_factory):
    return build_host(tmp_path_factory.mktemp("host"))


def run(exe, *args):
    return subprocess.run([str(exe), *args], capture_output=True, text=True)


# ---- the header ---------------------------------------------------------------------------------------------------------------------------

def test_the_committed_header_is_what_the_generator_writes_now():
    assert HEADER.read_text() == bench.render_header(), "regenerate: PYTHONPATH=python python -m dsp_lab.bench gen bench/core/bench_data.h"


def test_the_header_carries_the_model_checksums_and_the_design_coefficients():
    text = HEADER.read_text()
    q, fir, vec = bench.config()
    exp = bench.expected()
    for tag, f in (("bq", bench.FILTER_BQ), ("fir", bench.FILTER_FIR)):
        m = re.search(rf"bench_expect_{tag}\[BENCH_NVEC\] = \{{\s*([^}}]*)\}}", text)
        assert [int(v.strip().rstrip("u"), 16) for v in m.group(1).split(",")] == [exp[(f, n)] for n in vec]
    assert str(q[0][3]) in text and str(fir[15]) in text


def test_crc_helper_is_the_standard_crc32_and_uses_little_endian_bytes():
    assert zlib.crc32(b"123456789") == 0xCBF43926                       # the standard check value
    assert bench.crc32_i16([0x0102, -2]) == zlib.crc32(bytes([0x02, 0x01, 0xFE, 0xFF]))


def test_expected_checksums_match_the_c_library_not_only_the_model(lib):
    q, fir, vec = bench.config()
    exp = bench.expected()
    for name, x in vec.items():
        assert bench.crc32_i16(CascadeRunner(lib, q).process(x)) == exp[(bench.FILTER_BQ, name)]
        assert bench.crc32_i16(FirRunner(lib, fir).process(x)) == exp[(bench.FILTER_FIR, name)]


# ---- the benchmark logic, run on the PC ---------------------------------------------------------------------------------------------------

def test_the_host_run_passes_the_checker(host_exe):
    r = run(host_exe)
    assert r.returncode == 0
    problems, _ = bench.check(r.stdout)
    assert problems == []


def test_every_variant_filter_and_vector_is_reported_once_with_the_models_checksum(host_exe):
    runs, _, _ = bench.parse_log(run(host_exe).stdout)
    assert len(runs) == 1 and runs[0].version == 2
    exp = bench.expected()
    nvec = len(bench.config()[2])
    assert len(runs[0].rows) == 2 * 4 * nvec and len({(r[0], r[1]) for r in runs[0].rows}) == len(runs[0].rows)
    assert all(r[3] == exp[(bench.base_of(r[0]), r[1])] for r in runs[0].rows)           # every variant has the model's checksum


def test_the_four_variants_are_declared_in_the_banner_and_present_for_both_filters(host_exe):
    runs, _, _ = bench.parse_log(run(host_exe).stdout)
    assert runs[0].banner[5] == 4
    assert {bench.variant_of(r[0]) for r in runs[0].rows} == {"acc64", "acc32", "acc64_O2", "acc32_O2"}
    assert {bench.base_of(r[0]) for r in runs[0].rows} == set(bench.FILTERS)


def test_fake_counter_shows_exactly_two_reads_per_block_with_interrupts_masked_and_wrap_handled(host_exe):
    out = run(host_exe, "fake").stdout
    runs, _, _ = bench.parse_log(out)
    rows = runs[0].rows
    assert all(r[6] == 10 and r[7] == 10 for r in rows)                  # one step of 10 between the two reads, across the 32-bit wrap
    assert runs[0].overhead == 10
    info = dict(kv.split("=") for kv in re.search(r"HOSTINFO (.*)", out).group(1).split())
    blocks, reps, empty = len(rows), 3, 8
    assert int(info["cycles_calls"]) == 2 * (blocks * reps + empty)
    assert int(info["locks"]) == int(info["unlocks"]) == blocks * reps + empty


def test_a_repeat_that_goes_wrong_once_is_reported_even_if_the_last_repeat_is_right(host_exe):
    r = run(host_exe, "flaky")                       # checksum #2 is wrong (first block, second repeat); the last repeat of that block is right
    assert r.returncode == 1
    runs, _, _ = bench.parse_log(r.stdout)
    first = runs[0].rows[0]
    assert first[5] == 0 and first[3] == first[4]    # ok=0 although the printed (last) checksum equals the expected one
    assert any("ok=0" in p for p in bench.check(r.stdout)[0])


def test_the_log_checker_tolerates_terminal_noise_and_repeated_runs(host_exe):
    good = run(host_exe).stdout
    noisy = "garbage before\r\n" + good.replace("\n", "\r\n") + "\r\nsome text\r\n" + good + "BENCH 2 n=2000 vectors=11 sections=2 taps=31 reps=3 variants=4\r\nRESULT cut off"
    problems, rep = bench.check(noisy)
    assert problems == [] and "2 complete run(s)" in rep[0]


# ---- the checker must notice every kind of wrong log -------------------------------------------------------------------------------------

@pytest.fixture(scope="module")
def good_log(host_exe):
    return run(host_exe).stdout


def mutate(log, old, new, count=1):
    assert old in log
    return log.replace(old, new, count)


def first_result(log):
    return next(l for l in log.splitlines() if l.startswith("RESULT"))


def test_checker_catches_a_wrong_checksum(good_log):
    line = first_result(good_log)
    crc = re.search(r"crc=([0-9a-f]{8})", line).group(1)
    bad = mutate(good_log, line, line.replace(f"crc={crc}", "crc=00000000", 1))
    assert any("model gives" in p for p in bench.check(bad)[0])


def test_checker_catches_a_device_that_reports_failure(good_log):
    line = first_result(good_log)
    assert any("ok=0" in p for p in bench.check(mutate(good_log, line, line.replace("ok=1", "ok=0")))[0])


def test_checker_catches_a_stale_header_on_the_device(good_log):
    line = first_result(good_log)
    bad = re.sub(r"expect=[0-9a-f]{8}", "expect=deadbeef", line)
    assert any("stale header" in p for p in bench.check(mutate(good_log, line, bad))[0])


def test_checker_catches_a_missing_or_duplicated_result(good_log):
    line = first_result(good_log)
    assert bench.check(mutate(good_log, line + "\n", ""))[0]
    assert bench.check(mutate(good_log, line + "\n", line + "\n" + line + "\n"))[0]


def test_checker_catches_a_summary_that_disagrees(good_log):
    assert bench.check(re.sub(r"failed=0", "failed=1", good_log))[0]
    assert bench.check(re.sub(r"runs=88", "runs=87", good_log))[0]


def test_checker_rejects_a_log_without_a_complete_run(good_log):
    assert bench.check("")[0]
    assert bench.check("\n".join(good_log.splitlines()[:6]))[0]


def test_checker_catches_the_wrong_configuration_and_zero_cycles(good_log):
    assert bench.check(mutate(good_log, "n=2000 vectors=11", "n=1000 vectors=11"))[0]
    line = first_result(good_log)
    assert bench.check(mutate(good_log, line, re.sub(r"cyc_min=\d+", "cyc_min=0", line)))[0]


# ---- the benchmark itself must fail when the code under test is wrong ------------------------------------------------------------------------

@pytest.mark.parametrize("pattern,replacement", [
    ("DSP_BIQUAD_SHIFT 14", "DSP_BIQUAD_SHIFT 13"),
    ("DSP_FIR_SHIFT 15", "DSP_FIR_SHIFT 14"),
])
def test_a_wrong_filter_makes_the_benchmark_fail(tmp_path, pattern, replacement):
    mut = tmp_path / "c"                                                  # a patched copy of the library, compiled in place of the real one
    mut.mkdir()
    (mut / "dsp.h").write_text(mutate((ROOT / "c" / "dsp.h").read_text(), pattern, replacement))
    for f in ("dsp.c", "dsp32.c", "dsp32.h"):
        (mut / f).write_text((ROOT / "c" / f).read_text())
    exe = build_host(tmp_path, cdir=mut, srcs=[mut / "dsp.c", mut / "dsp32.c", *HOST_SRC[2:]])
    r = run(exe)
    assert r.returncode != 0
    assert bench.check(r.stdout)[0]


# ---- capture gaps: lines cut short or glued together ----------------------------------------------------------------------------------------

def test_a_glued_line_is_ignored_and_reported_not_turned_into_a_fake_setting(good_log):
    glued = "ENV optimisationENV zephyr 4.4.2\n" + good_log                  # the end of one line glued to the start of the next, as in the first Nano capture
    runs, env, ignored = bench.parse_log(glued)
    assert "optimisationENV" not in env and ignored >= 1
    problems, rep = bench.check(glued)
    assert problems == [] and any("ignored" in r for r in rep)


def test_a_log_that_starts_mid_line_and_ends_on_a_summary_checks_only_the_complete_runs(good_log):
    cut = good_log.splitlines()
    partial = ["  speed (-O1, -O2 or -O3; read the exact flag from the build)", cut[3], cut[4][:60] + cut[0]] + cut      # a partial run, a cut line, then the real log
    problems, rep = bench.check("\n".join(partial) + "\n")
    assert problems == [] and "1 complete run(s)" in rep[0]


def test_two_lines_glued_inside_a_complete_run_make_the_check_fail(good_log):
    lines = good_log.splitlines()
    i = next(k for k, l in enumerate(lines) if l.startswith("RESULT fir31_lp1000:acc64 sine_500 "))
    lines[i - 1] = lines[i - 1] + lines[i]                                      # one RESULT line swallowed by its neighbour
    del lines[i]
    problems, _ = bench.check("\n".join(lines) + "\n")
    assert any("missing" in p for p in problems)


def test_a_missing_log_file_gives_a_plain_message_and_exit_code_2(capsys, tmp_path):
    assert bench.main(["bench", "check", str(tmp_path / "nope.log")]) == 2
    assert "cannot read the log file" in capsys.readouterr().out


# ---- stage 3: the Arduino sketch folder for the Feather M0 -------------------------------------------------------------------------------------

SKETCH = feather_sketch.SKETCH
SKETCH_COPIES = feather_sketch.COPIES


@pytest.mark.parametrize("name", list(SKETCH_COPIES))
def test_the_sketch_folder_holds_exact_copies_of_the_sources(name):
    assert (SKETCH / name).read_text() == SKETCH_COPIES[name].read_text(), "run: PYTHONPATH=python python -m dsp_lab.feather_sketch"


def test_the_sketch_folder_and_its_ino_file_have_the_same_name_as_the_arduino_ide_requires():
    assert (SKETCH / (SKETCH.name + ".ino")).is_file()


# ---- variants: missing, wrong, refused ----------------------------------------------------------------------------------------------------------

def test_checker_catches_a_missing_variant(good_log):
    lines = [l for l in good_log.splitlines() if ":acc32_O2" not in l]
    problems, _ = bench.check("\n".join(lines) + "\n")
    assert any("declares 4 variant" in p for p in problems) or any("runs=" in p for p in problems)


def test_checker_catches_a_wrong_checksum_in_the_32bit_variant(good_log):
    line = next(l for l in good_log.splitlines() if l.startswith("RESULT fir31_lp1000:acc32 "))
    bad = re.sub(r"crc=[0-9a-f]{8}", "crc=12345678", line)
    assert any("acc32" in p and "model gives" in p for p in bench.check(mutate(good_log, line, bad))[0])


def test_checker_catches_a_variant_that_has_only_one_of_the_two_filters(good_log):
    lines = [l for l in good_log.splitlines() if not l.startswith("RESULT fir31_lp1000:acc64_O2 ")]
    problems, _ = bench.check("\n".join(lines) + "\n")
    assert any("has no fir31_lp1000 results" in p for p in problems)


def test_the_committed_board_logs_still_pass_the_checker():
    logs = sorted((ROOT / "bench" / "logs").glob("*.log"))
    assert logs, "expected the committed board logs"
    for log in logs:
        problems, rep = bench.check(log.read_text(errors="replace"))
        assert problems == [], f"{log.name}: {problems}"


def test_the_32bit_variants_are_refused_for_coefficients_that_fail_the_check(tmp_path):
    core = tmp_path / "core"
    core.mkdir()
    for f in CORE.iterdir():
        (core / f.name).write_text(f.read_text())
    header = (core / "bench_data.h").read_text()
    assert "{1655, 3311, 1655, -14015, 3436}" in header
    (core / "bench_data.h").write_text(header.replace("{1655, 3311, 1655, -14015, 3436}", "{32767, 32767, 32767, -32768, 32767}"))     # sum of |coefficient| far above 65535
    exe = build_host(tmp_path, srcs=[HOST_SRC[0], HOST_SRC[1], core / "dsp_o2.c", core / "dsp32_o2.c", core / "bench_core.c", HOST_SRC[5]])
    r = run(exe)
    assert r.returncode != 0
    assert "SKIPPED biquad2_lp1000:acc32 " in r.stdout and "SKIPPED biquad2_lp1000:acc32_O2 " in r.stdout
    assert "SKIPPED fir31_lp1000" not in r.stdout                           # the FIR coefficients were not touched
    assert bench.check(r.stdout)[0]


def test_the_banner_variant_count_is_checked_even_when_the_summary_is_adjusted_to_match(good_log):
    lines = [l for l in good_log.splitlines() if ":acc32_O2" not in l]
    text = "\n".join(lines).replace("runs=88", "runs=66") + "\n"                 # the SUMMARY now agrees with the results; only the banner (variants=4) disagrees
    problems, _ = bench.check(text)
    assert any("declares 4 variant" in p for p in problems)


def test_the_o2_units_really_compile_differently_from_the_default_units(tmp_path):
    def text_size(src):
        obj = tmp_path / (src.stem + ".o")
        subprocess.run(["gcc", "-std=c99", "-Os", "-I", str(ROOT / "c"), "-c", str(src), "-o", str(obj)], check=True)
        out = subprocess.run(["size", str(obj)], capture_output=True, text=True, check=True).stdout.split("\n")[1].split()
        return int(out[0])
    assert text_size(ROOT / "c" / "dsp.c") != text_size(CORE / "dsp_o2.c")             # same source, built under -Os: only the pragma can make the code differ
    assert text_size(ROOT / "c" / "dsp32.c") != text_size(CORE / "dsp32_o2.c")
