# Stage 2: the benchmark on the Arduino Nano 33 BLE (Cortex-M4F)

Built with AI assistance (Claude). **Status: the harness has run on one Nano 33 BLE.** The output was bit-identical to the model in every complete run, and the
first cycle counts are below. The log of that run is committed as `bench/logs/nano_run1.log`.

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
- Nothing has run on the Cortex-M0+ yet (stage 3), and no optimisation has been measured (stage 4).
