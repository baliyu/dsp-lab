"""Copies the C sources of the benchmark into the Arduino sketch folder (bench/feather/dsp_bench_feather), because the Arduino IDE only compiles
files inside the sketch folder. A test requires the copies to equal the originals, so they cannot drift.

    PYTHONPATH=python python -m dsp_lab.feather_sketch          (run from the repository root)
Built with AI assistance (Claude)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / "bench" / "feather" / "dsp_bench_feather"
COPIES = {"dsp.c": ROOT / "c" / "dsp.c", "dsp.h": ROOT / "c" / "dsp.h", "dsp32.c": ROOT / "c" / "dsp32.c", "dsp32.h": ROOT / "c" / "dsp32.h",
          "dsp_o2.c": ROOT / "bench" / "core" / "dsp_o2.c", "dsp32_o2.c": ROOT / "bench" / "core" / "dsp32_o2.c",
          "bench_core.c": ROOT / "bench" / "core" / "bench_core.c", "bench_core.h": ROOT / "bench" / "core" / "bench_core.h",
          "bench_variants.h": ROOT / "bench" / "core" / "bench_variants.h", "bench_data.h": ROOT / "bench" / "core" / "bench_data.h"}


def main() -> int:
    SKETCH.mkdir(parents=True, exist_ok=True)
    for name, src in COPIES.items():
        (SKETCH / name).write_text(src.read_text(), newline="\n")
        print(f"copied {src.relative_to(ROOT)} -> {(SKETCH / name).relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
