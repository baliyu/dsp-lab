# Stage 2: the benchmark on the Arduino Nano 33 BLE (Cortex-M4F)

Built with AI assistance (Claude). **Status: the harness has run on one Nano 33 BLE (stage 2) and one Feather M0 (stages 3 and 4).** The output was bit-identical to the model in every complete
run on both, and the first cycle counts are below. The logs are committed as `bench/logs/nano_run1.log`, `bench/logs/feather_run1.log` and `bench/logs/feather_run2.log`.

## What it does

The same 2-section biquad cascade (4th-order Butterworth low-pass, 1000 Hz, fs 8 kHz) and 31-tap FIR as the PC tests run over the 11 test
vectors (2000 samples each). For every (filter, vector) block the program prints:

- a **CRC-32** of the output (as little-endian 16-bit integers), compared with the checksum the **integer model** gives (`python/dsp_lab/model.py`);
- the **cycle count** of the block (DWT cycle counter), as the minimum and maximum of 3 repeats.

**Pass condition:** every checksum printed by the board equals the model's. Cycle counts are only reported if that holds.

## Files

| Path | What it is |
|---|---|
| `python/dsp_lab/bench.py` | Writes `bench/core/bench_data.h` (vectors, coefficients, expected checksums) and checks a log: `gen` and `check` |
| `bench/core/bench_core.c`, `.h` | The benchmark logic, portable C. The machine-specific parts (counter, interrupt mask, CRC, output) are passed in |
| `bench/host/bench_host.c` | The PC platform, used by the tests |
| `bench/zephyr/` | The Zephyr app for the Nano: `src/main.c` is the board platform (DWT counter, USB console) |
| `tests/test_bench.py` | The tests of all of the above |

## Run it on the board

WSL (the window whose prompt shows `(.venv) (base)`), in the repository folder:

```
cd /mnt/c/dsp-lab
export ZEPHYR_BASE=$HOME/zephyrproject/zephyr
west build -p always -b arduino_nano_33_ble -d ~/dsp-bench-build bench/zephyr
mkdir -p /mnt/c/zephyr_flash && cp ~/dsp-bench-build/zephyr/zephyr.bin /mnt/c/zephyr_flash/dsp_bench.bin
```

PowerShell, with the Nano in bootloader mode (double-tap RESET until the orange LED fades). `$B` is the path to Arduino's `bossac.exe`:

```
& $B --port=COM7 -U -i -e -w -v 'C:\zephyr_flash\dsp_bench.bin' -R
```

The running app has its own serial port, usually a different COM number from the bootloader's (find it with
`Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description`). The program waits until a terminal opens the port, then prints one run about every 10 seconds.
Close any terminal program, then capture 35 seconds to a file with PowerShell (here the port is COM14):

```
$p = New-Object System.IO.Ports.SerialPort COM14,115200
$p.DtrEnable = $true
$p.Open()
$sb = New-Object System.Text.StringBuilder
$end = (Get-Date).AddSeconds(35)
while ((Get-Date) -lt $end) {
  if ($p.BytesToRead -gt 0) { [void]$sb.Append($p.ReadExisting()) } else { Start-Sleep -Milliseconds 100 }
}
$p.Close()
[IO.File]::WriteAllText('C:\dsp-lab\bench\logs\nano_run1.log', $sb.ToString())
```

The capture can start in the middle of a run (the board prints whether or not a terminal is open), so the file may begin with a partial run; the checker uses only complete runs
and reports how many lines it ignored. Back in WSL:

```
PYTHONPATH=python python -m dsp_lab.bench check bench/logs/nano_run1.log
```

The check prints the cycles per sample and ends with PASS only if every checksum matches the model.

## How the cycles are measured

- `DWT->CYCCNT`, the Cortex-M4 cycle counter: `CoreDebug->DEMCR` bit TRCENA and `DWT->CTRL` bit CYCCNTENA are set at start; the program checks the counter exists and
  counts. (Zephyr's `k_cycle_get_32()` is the 32.768 kHz RTC on this chip and is not used for timing.)
- Each block (2000 samples) is timed with two counter reads around the filter call, with interrupts masked; the difference is taken in unsigned 32-bit arithmetic, so
  a counter wrap is handled. Cycles per sample = block cycles / 2000.
- The program also prints the cost of two back-to-back counter reads (`OVERHEAD`) and measures the CPU clock against the RTC (`cpu_hz_measured`).
- The input block, output block, coefficients and filter state are in RAM during timing. The code runs from flash behind the nRF52840 instruction cache.
- The log records the compiler version and Zephyr version. The build uses `CONFIG_SPEED_OPTIMIZATIONS=y` (`-O2`, `-mcpu=cortex-m4 -mthumb`).

## First result (one board, one run of the program, 31 October 2026)

Log: `bench/logs/nano_run1.log` (3 complete runs). Checked with `python -m dsp_lab.bench check`: **every one of the 66 output blocks (3 runs x 22) has the model's CRC-32**, and the
minimum cycle count of every block is identical from the first complete run to the last.

| Filter | Cycles per sample (minimum of 3 repeats) | Spread over the 11 test vectors |
|---|---|---|
| Biquad cascade, 2 sections | **132.06** | 132.06 on ten vectors; 132.90 on `square_fullscale` |
| FIR, 31 taps | **373.10** | 373.10 on ten vectors; 373.35 on `square_fullscale` |

- The maximum over the 3 repeats is within 0.02 cycles per sample of the minimum, so the cache and flash did not make the repeats differ visibly in this run.
- Why `square_fullscale` costs a little more is not investigated (saturation is the obvious suspect, not tested).
- Derived, not measured separately: at the measured CPU clock (about 63.98 MHz) that is about 2.06 us per sample for the biquad cascade and 5.83 us for the FIR, which is about
  1.7% and 4.7% of the 125 us between samples at 8 kHz, for the filter code alone. The FIR costs 2.8 times as much as the 2-section cascade here.
- Counter overhead: 18 cycles per block, so it does not matter at 2000 samples. The CPU clock measured against the RTC was 63.98 MHz (resolution about 0.03%).
- Build: Zephyr 4.4.2, GCC 14.3.0 from Zephyr SDK 1.0.1, `CONFIG_SPEED_OPTIMIZATIONS=y`. Zephyr compiles that with `-O2`; I confirmed the `-O2` flag in my own sandbox build of the
  same Zephyr version, not by reading the compile command of this build.
- **The capture has gaps.** The file starts in the middle of a run (line 1 is the tail of a line), and the first two runs in it are cut short: line 10 ends mid-line, and in
  line 15 the end of one line is glued to the start of the next run. Those three lines are the only damaged ones, and all lie before the first complete run; the last three runs are
  whole and end exactly at the end of the file. Whether the bytes were lost by the board's USB output or by my PowerShell capture is **not established**. A damaged line inside a
  complete run would make the check fail (a test covers it), and none did.
- Not shown by this run: other boards, other compiler flags, a different `reps` value, interrupts running, inputs read from flash instead of RAM, or any comparison with another implementation.

## What the measurement does not cover

- **Interrupts:** masked on purpose, so their cost is excluded. In a real system they would add to the time a block takes.
- **Flash wait states and the instruction cache:** the first repeat can include cache misses; the minimum of 3 repeats is reported, with the maximum next to it. Whether the cache
  is warm in a real application is not measured. Data is read from RAM, not flash.
- Only these two filters, one compiler and one set of flags, one board, 2000-sample blocks. Cycle counts can depend on the data (saturation, branches); the table shows
  each vector, not just an average.
- A cycle count is for this code on this core; it says nothing about other cores. Stage 3 is the Cortex-M0+.

## What was checked before any board run

- 97 tests pass on the PC (75 from stage 1, 22 for the benchmark and its checker). The benchmark core is built on the PC with strict warnings and its checksums equal the model's, a third CRC implementation
  (bit by bit, in C) agreeing with Python's zlib. A fake counter that starts near the 32-bit wrap checks that exactly two counter reads surround each block, interrupts are masked and
  unmasked in pairs, and the wrap is handled.
- The checker is tested against logs with a wrong checksum, a device that reports failure, a stale header, missing or duplicated lines, wrong totals, wrong configuration, and
  zero cycle counts.
- The Zephyr app builds for `arduino_nano_33_ble` (Zephyr 4.4.2, GNU Arm Embedded 13.2.1 from Ubuntu).
- The same `bench_core.c` and `dsp.c`, compiled with the Zephyr flags for a Cortex-M4, were run in QEMU (`mps2/an386`) in my sandbox with Zephyr's own `crc32_ieee`:
  all 22 checksums matched the model. QEMU has no DWT counter, so this proves the arithmetic and the log format on ARM code, **not** the timing or the USB console. That QEMU build is not in this
  repository.
- Deliberate bugs, run by hand in my sandbox (not a script in the repository): 16 changes to the benchmark and the checker. 15 were caught by the tests. One change, removing the
  pre-fill of the output buffer, changes nothing observable because the filter overwrites every sample (an equivalent mutant); removing it **and** skipping the filter is caught. One real hole
  was found: nothing checked that a repeat other than the last one was right; a test with a checksum that is wrong once now covers it.

## Not yet verified

- One run on one board. The DWT counter and the USB console worked in that run; the cause of the capture gaps above is open.
- No optimisation has been measured yet (stage 4).

## Stage 3: Adafruit Feather M0 (Cortex-M0+, ATSAMD21G18, 48 MHz)

The same benchmark and log format, as an Arduino sketch in `bench/feather/dsp_bench_feather/` (the C files in that folder are copies of `c/` and `bench/core/`, made by
`python/dsp_lab/feather_sketch.py`; a test fails if they differ). The first result is below.

What differs from the Nano run, and why it matters:

- **Cycle counter.** The Cortex-M0+ has no DWT. The sketch counts with the SAMD21's 32-bit timer TC4+TC5, clocked from GCLK0 (the CPU clock, 48 MHz) with no prescaler. At start it checks the
  count rate against `micros()` (which comes from the core's SysTick) and refuses to run if it is not within 2% of `F_CPU`; it is not checked against anything independent of the CPU clock.
  Reading this timer needs a synchronisation wait, so the per-read overhead is larger than on the Nano; the log prints it (`OVERHEAD`).
- **No 64-bit multiplier.** The compiler turns each `int64` product into a call to a library routine (`__aeabi_lmul`). That is the cost stage 4 sets out to reduce.
- **Compiler flags are not the same as the Nano's.** The Arduino IDE builds the Adafruit SAMD core with `-Os -std=gnu11` (read from the core's `platform.txt`); the Nano build used `-O2`. A
  comparison between the two boards therefore mixes the core and the flags; the log's `ENV optimisation` line shows which one the board build used.
- Interrupts are masked (PRIMASK) during each timed block, as on the Nano. SysTick is left alone.

### First result (one board, one run of the sketch, 31 October 2026)

Log: `bench/logs/feather_run1.log`: 4 complete runs (88 output blocks), **every one with the model's CRC-32**. Unlike the Nano capture this one is clean: it starts at the start of a run,
ends on a `SUMMARY` line, and has no damaged lines. The minimum cycle count of every block is identical from the first run to the last, and the maximum over the 3 repeats equals the minimum.

| Filter | Cycles per sample (minimum of 3 repeats, mean of the 11 vectors) | Range over the vectors |
|---|---|---|
| Biquad cascade, 2 sections | **1038.4** | 1032.06 (`step`, `silence`) to 1042.56 (`square_fullscale`) |
| FIR, 31 taps | **3485.1** | 3472.06 (`silence`) to 3494.86 (`square_fullscale`) |

- Derived, not measured separately: at 48 MHz that is about 21.6 us per sample for the biquad cascade and 72.6 us for the FIR, which is about **17% and 58%** of the 125 us between samples at 8 kHz,
  for the filter code alone.
- The time depends slightly on the data (about 1% for the biquad, 0.6% for the FIR). The likely cause is the library routine behind each `int64` product, which is not constant-time; I have not tested that.
- Timer: the TC4+TC5 count rate was 48,001,069 per second against `micros()`. Both come from the same CPU clock, so this shows the counter runs at the clock the core configured; it is not an independent
  measurement of that clock. Counter overhead: 50 cycles per block (a read needs a synchronisation wait), which does not matter at 2000 samples.
- Build, as printed by the sketch: GCC 9.2.1 (the arm-none-eabi-gcc of the Arduino IDE's Adafruit SAMD package), `-Os` (the core's own flags, which I read from its `platform.txt`; the log's
  optimisation line confirms size optimisation), `F_CPU` 48 MHz.
- **Compared with the Nano (not a like-for-like comparison):** about 7.9 times the cycles for the biquad cascade and 9.3 times for the FIR. The two builds differ in more than the core: GCC 9.2.1 at `-Os`
  here against GCC 14.3.0 at `-O2` on the Nano, and the Cortex-M0+ has no 64-bit multiplier, so each product is a library call. How much of the ratio comes from each of these has not been separated; that is
  what stage 4 is for.
- Not shown by this run: other boards, other compiler flags, interrupts running, inputs read from flash instead of RAM, any other clock speed, or a comparison with another implementation.

What was checked before the board run (in my sandbox, not in CI):

- The sketch and its C files compile against the real Adafruit SAMD core headers with the core's own compiler flags (no errors; the CMSIS core headers used are from the Zephyr module, not the exact
  version the IDE ships). Nothing was linked, because the core was not built.
- The same `bench_core.c` and `dsp.c` ran on an emulated Cortex-M0 (QEMU `microbit`), built with Zephyr at both `-Os` and `-O2`: all 22 checksums matched the model in each build. This checks the
  arithmetic on ARMv6-M code with the library multiply; it says nothing about timing or the SAMD21's timer.
- Not tested before the board run: the timer register code (`TC4`/`TC5`), the USB serial output, and the upload. All three worked in the first run.

To run it: install the "Adafruit SAMD Boards" package in the Arduino IDE; open `bench/feather/dsp_bench_feather/dsp_bench_feather.ino`; choose the board "Adafruit Feather M0 (SAMD21)" and its port; Upload
(this overwrites whatever sketch is on the board). Close the Serial Monitor, then capture 35 seconds with the PowerShell block above, using the board's COM port and the file name
`bench/logs/feather_run1.log`, and check it with `python -m dsp_lab.bench check`.

## Stage 4: variants (32-bit accumulator, optimisation level)

The benchmark now times a list of *variants* (`bench/core/bench_variants.h`) of both filters, and the log names each case `<filter>:<variant>` (for example `fir31_lp1000:acc32`). The log banner is
`BENCH 2 ... variants=N`; the older version-1 logs (`nano_run1.log`, `feather_run1.log`) are still read and still pass, and a test requires that.

| Variant | What it is |
|---|---|
| `acc64` | `c/dsp.c`: the 64-bit accumulator, built at the build's default optimisation level (`-Os` in the Arduino IDE, `-O2` in the Zephyr app) |
| `acc32` | `c/dsp32.c`: the 32-bit accumulator (specification and safety bound in [DESIGN.md](DESIGN.md)), at the default level |
| `acc64_O2`, `acc32_O2` | the same two compiled at function-level `-O2` (`#pragma GCC optimize`, in `bench/core/dsp_o2.c` and `dsp32_o2.c`), Feather sketch and PC build only |

The pass condition is unchanged: **every variant must give the model's checksum on every vector**. A 32-bit variant is refused (a `SKIPPED` line, counted as a failure) if the coefficients fail the safety check.
`python -m dsp_lab.bench check` prints, for each filter, the mean cycles per sample of every variant and its ratio to `acc64`.

The `-O2` variants are built with a pragma inside a program compiled at `-Os`; this is not the same as building the whole program at `-O2` (other compiler settings stay at `-Os`), and the GCC manual
advises against the pragma for production code. It is used here because it lets one upload time both levels. The first thing to confirm in a result is therefore how the `_O2` variants compare with the Nano's real `-O2` build.

### First result (Feather M0, one run of the sketch, 31 October 2026)

Log: `bench/logs/feather_run2.log`: 4 complete runs, 352 output blocks (8 cases x 11 vectors x 4 runs), **every one with the model's CRC-32, including every 32-bit block**. The fifth run in the
file is cut off by the end of the 120-second capture; no lines are damaged. The minimum cycle count of every block is identical from the first complete run to the last.

Cycles per sample (minimum of 3 repeats; mean of the 11 vectors, and the lowest to highest vector); the ratio is to `acc64` in the same run:

| Case | `acc64` | `acc32` | `acc64_O2` | `acc32_O2` |
|---|---|---|---|---|
| Biquad cascade, 2 sections | 1018.4 (1012.1 to 1022.6) | **244.0** (243.7 to 244.1), 4.17x | 1007.0 (999.1 to 1011.6), 1.01x | 247.3 (247.1 to 249.2), 4.12x |
| FIR, 31 taps | 3453.5 (3440.1 to 3463.4) | 1087.0 (1086.5 to 1087.1), 3.18x | 2857.6 (2844.4 to 2866.4), 1.21x | **563.2** (563.2 to 563.2), 6.13x |

- **The 32-bit accumulator cuts the cycles by about 4.2 times (biquad) and 3.2 times (FIR) at the build's `-Os`, with identical output**, for coefficients that pass the check in [DESIGN.md](DESIGN.md).
  That guarantee comes from the check, not from this run. It is smaller than I had guessed before measuring.
- Derived, at 48 MHz and for the filter code alone: the biquad cascade goes from 21.2 us per sample (17% of the 125 us between samples at 8 kHz) to 5.1 us (4.1%); the FIR from 72.0 us (58%) to 11.7 us (9.4%) with
  `acc32_O2`.
- **Against the Nano** (its stage 2 `-O2` build of the 64-bit code, 132.1 and 373.1 cycles): the Feather needed 7.7 and 9.3 times the cycles with `acc64`, and 1.85 and 1.5 times with the best 32-bit variant. Not like-for-like: the Nano has
  not been run with the 32-bit variant, and the builds differ in compiler (GCC 9.2.1 here, 14.3.0 there) and in how `-O2` is applied.
- **The `_O2` pragma:** it changes nothing for the biquad (1.01x) but speeds the 64-bit FIR by 1.2 times and the 32-bit FIR by 1.9 times (1087 to 563). The 32-bit FIR costs about 35 cycles per tap at `-Os` and 18 at `-O2`. I have not looked at
  why. Whether a real whole-program `-O2` build would give the same is not tested.
- The 32-bit biquad still costs about 122 cycles per section. I have not looked at the instructions or at flash wait states, so I cannot say what the remaining cost is.
- Spread over the vectors: about 1% for the 64-bit biquad and 0.7% for the 64-bit FIR; the 32-bit FIR variants are flat to 0.05% (`acc32_O2` is exactly constant). That fits the earlier guess that the library multiply's time depends on the data,
  but does not prove it.
- **Do not read differences of 1 to 2% as real.** The same source at the same flags (`acc64` at `-Os`) measured 1.9% lower for the biquad (1038.4 to 1018.4) and 0.9% lower for the FIR (3485.1 to 3453.5) than in stage 3, because it is a different
  program (different code layout). Compare variants inside one run.
- Timer: 48,001,269 counts per second against `micros()` (same caveat as stage 3); overhead 49 cycles per block. Interrupts masked, data in RAM, as before.

What was checked before any board run (sandbox and CI; no board):

- 155 tests pass (75 from stage 1; the rest cover the 32-bit variant and its limit, the benchmark and its checker, and the sketch folder). The 32-bit code equals the model on all test vectors for both low-pass filters and the FIR, on random
  coefficient sets inside the limit with extreme inputs, and at the exact limit with the worst-case FIR input; the check rejects the 50 Hz DC blocker and the limit is exact on both sides.
- All four variants run on the PC and equal the model. The 32-bit variants are refused when the coefficients are made unsafe.
- 23 deliberate bugs in the new code, the benchmark core and the checker, run by hand in my sandbox (not a script in the repository): all caught. Two tests were added because the first round found that a variant-count check and
  the `-O2` pragma were each protected only by something else.
- The 32-bit and 64-bit code ran on an emulated Cortex-M0 (QEMU) at `-Os` and `-O2` with every checksum matching the model; the Feather sketch compiles against the real Adafruit SAMD core; the Nano app builds. QEMU says nothing about timing.

To run it on the Feather: upload `bench/feather/dsp_bench_feather/dsp_bench_feather.ino` as in stage 3. One pass now takes roughly 20 to 30 seconds (estimated, not measured), so capture 120 seconds to
`bench/logs/feather_run2.log` and check it. The Nano app also runs `acc64` and `acc32` (at its `-O2`) but has not been re-flashed with this version.

