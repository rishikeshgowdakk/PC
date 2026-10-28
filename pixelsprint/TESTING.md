# PixelSprint test log

This file records commands and results from the final audit of the committed source and a
separate final submission-verification run. It is a test log, not a separate VTU project
report. Run the commands from `pixelsprint/`.

## Final submission verification (new run)

Date: 28 October 2026. This run was performed separately from the original final audit
recorded below. It used the same committed source and did not change the source code.

| Check | Command / method | Result |
|---|---|---|
| Clean build | `make clean` | PASS; removed the executable and generated output files |
| Build | `make` | PASS; exit 0; GCC emitted no warnings for `-O2 -Wall -Wextra -fopenmp` |
| Help | `./pixelsprint --help` | PASS; exit 0 |
| Demo | `./pixelsprint --demo` | PASS; exit 0; V0 total 4,194,304; V2-V5 `differing_bins=0`; V1 raced in all three attempts |
| Benchmark | `./pixelsprint --benchmark --size 256` | PASS; exit 0; 21 data rows and 10 columns; V2-V5 `PASS`; V1 `RACE` at 2, 4, and 12 threads |
| Demo thread variants | `--demo --size 256 --threads 1`, then `2`, then `4` | PASS; all runs exit 0; V2-V5 `differing_bins=0`; V1 no race at 1 thread and races at 2 and 4 threads |
| Valid/malformed PPM | Temporary P6/P5 fixtures and a truncated P6 | PASS; valid and commented P6 accepted; P5, truncated, and trailing-byte PPM rejected with exit 1 |
| Uniform image | 16 x 16 P6, all RGB values 128 | PASS; `before.bmp` and `after.bmp` were byte-identical |
| Unsafe selection | `--demo --size 16 --threads 2 --variant unsafe` | PASS; warned that V1 is unsafe and equalized with the V0 fallback |
| Input overwrite protection | PPM named `before.bmp` in the output directory | PASS; exit 1; input checksum unchanged |
| BMP layout | Known 3 x 2 PPM processed to BMP | PASS; `BM` signature, 54-byte offset, 24 bpp, image size 24, bottom-up rows, grayscale BGR triples, and row padding verified |
| Invalid CLI | 20 cases covering missing/duplicate/unknown options, invalid sizes/threads, incompatible modes, and a nonexistent input | PASS; every case exited 1 |
| Output/error paths | Missing parent, output path that is a file, and read-only output directory | PASS; each exited 1; no output files were written in the read-only case |
| Allocation failure | `ulimit -v 32768` with `--demo --size 4096` | PASS; exit 1 with a clear allocation error |
| Actual thread count | `OMP_THREAD_LIMIT=8 ... --benchmark --size 256 --threads 32` | PASS; requested 12 and 32 both reported actual 8, and efficiency used 8 |
| ASan/UBSan | Demo 256 at 4 threads, valid PPM, truncated PPM | PASS; no AddressSanitizer, UBSan, or leak diagnostics; the truncated PPM exited 1 as intended |

Demo 2048 V1 observed losses were 1,586,796, 1,635,427, and 1,526,220 pixels in the
three attempts. A missing race in a later run would not make V1 safe.

The requested 256 x 256 benchmark wrote a fresh `output/results.csv` for validation. The
tracked `output/results.csv` was then restored byte-for-byte to the committed 2048 x 2048
final-audit run (SHA-256
`a99a19784b80df980d7f2d288ccf881768d45878a6cc3bbd998f9ca13ccabfd3`) so the README's
2048 x 2048 table and the committed CSV remain consistent. The 256 run is not substituted
into that table, and no benchmark values were edited. Its highlights were V0
`0.054889 ms`; V4 at 12 threads `0.040555 ms`, speedup `1.353446`, efficiency `0.112787`;
V5 at 12 threads `0.032676 ms`, speedup `1.679795`, efficiency `0.139983`; LUT at 12
threads `0.029450 ms`, speedup `0.917386`, efficiency `0.076449`. These small-image timings
are overhead-dominated and are not directly comparable with the 2048 x 2048 results.

The sections below are the original final-audit record. This submission-verification pass
did not rerun ThreadSanitizer; the prior limitation remains: a reliable clean TSan pass was
not obtained. The full 8192 x 8192 case remains **NOT RUN**.

## Environment

- Date of final audit: 28 October 2026
- OS: Ubuntu Linux
- Compiler: `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
- OpenMP macro: `_OPENMP=201511` (OpenMP 4.5)
- CPU: 13th Gen Intel(R) Core(TM) i7-1355U, 12 logical processors
- Runtime maximum OpenMP threads: 12
- Benchmark image: default generated 2048 x 2048 image (4,194,304 pixels)

## Build

```bash
make clean
make
```

Result: PASS. GCC emitted no warnings with `-O2 -Wall -Wextra -fopenmp`.
The build command was:

```text
gcc -O2 -Wall -Wextra -fopenmp src/pixelsprint.c -o pixelsprint
```

`make clean` removed the executable and the generated/sample output files, while leaving
the source, Makefile, and documentation untouched.

## Acceptance tests

| Test | Command / method | Result |
|---|---|---|
| Help | `./pixelsprint --help` | PASS, exit 0 |
| Default command | `./pixelsprint` | PASS, exit 0; wrote `output/before.bmp` and `output/after.bmp` |
| Demo | `./pixelsprint --demo` | PASS, exit 0; V0 and V2-V5 matched all 256 bins |
| Demo, 256, 1 thread | `./pixelsprint --demo --size 256 --threads 1 --output <tmp>` | PASS, exit 0; V2-V5 matched |
| Demo, 256, 2 threads | `./pixelsprint --demo --size 256 --threads 2 --output <tmp>` | PASS, exit 0; V2-V5 matched |
| Demo, 256, 4 threads | `./pixelsprint --demo --size 256 --threads 4 --output <tmp>` | PASS, exit 0; V2-V5 matched |
| Demo, 16, 1 thread | `./pixelsprint --demo --size 16 --threads 1 --output <tmp>` | PASS, exit 0; V2-V5 matched |
| Benchmark | `make benchmark` | PASS, exit 0; wrote 21-row `output/results.csv` |
| Benchmark thread selection | `./pixelsprint --benchmark --size 256 --threads 3 --output <tmp>` | PASS; thread set was `1, 2, 4, 12, 3` |
| Requested vs actual threads | `OMP_THREAD_LIMIT=8 ./pixelsprint --benchmark --size 256 --threads 32 --output <tmp>` | PASS; requested 12 and 32 both used actual 8, and efficiency used 8 |
| Unsafe fallback | `./pixelsprint --demo --size 256 --threads 4 --variant unsafe --output <tmp>` | PASS, exit 0; warned and equalized with V0 fallback |
| V2-V5 exact correctness | Demo output at 2048, 256, and 16; all bins compared | PASS for every tested thread count; differing_bins=0 |
| V1 race demonstration | Three attempts in default `--demo` | Attempt 1: race, lost 1,739,379; attempt 2: race, lost 1,818,993; attempt 3: race, lost 1,517,204 |
| V1 at 1 thread | 256 x 256 demo | NO RACE OBSERVED in all three attempts (expected possibility at 1 thread) |
| V1 at 2 threads | 256 x 256 demo | Attempt 1 no race; attempt 2 race, lost 2,471; attempt 3 no race |
| V1 at 4 threads | 256 x 256 demo | Race observed in all three attempts; lost 21,055, 22,154, and 26,625 |
| Uniform image | 16 x 16 PPM, all RGB values 128 | PASS; before and after BMP files were byte-identical; range stayed 0 |
| Valid PPM | `--input valid-4x4.ppm` | PASS; wrote valid 4 x 4 BMPs |
| PPM with comment | `--input comment-4x4.ppm` | PASS; wrote valid 4 x 4 BMPs |
| PPM raster begins with whitespace | 16 x 16 PPM whose first raster byte is `0x20` | PASS; the separator was consumed once and the raster byte was preserved |
| Malformed/truncated PPM | P5, P3, bad maxval, zero width, oversized width, extra whitespace, trailing byte, truncated raster, directory input | PASS (all rejected with exit 1) |
| Input overwrite protection | PPM named `before.bmp` with the same output directory | PASS; exit 1; input checksum unchanged |
| Benchmark input overwrite protection | PPM named `results.csv` with the same output directory | PASS; exit 1; input checksum unchanged |
| Unwritable output | Directory mode 0500 with `--demo --size 16` | PASS; exit 1; no files written |
| Allocation failure | `ulimit -v 16384` with `--demo --size 4096` | PASS; exit 1 with a clear allocation error |
| Missing output parent | `--output <nonexistent>/child` | PASS; exit 1 with a clear directory error |
| Output path is a file | Existing regular file passed as `--output` | PASS; exit 1 with a clear directory error |
| Invalid CLI | 27 cases: missing/duplicate/unknown options, invalid or out-of-range sizes and threads, incompatible modes, nonexistent input | PASS; every case exited 1 |
| BMP structure | Known 3 x 2 PPM processed to BMP | PASS; `BM` signature, 54-byte header, 24 bpp, image size 24, bits offset 54, BGR grayscale bytes, bottom-up rows, and 3-byte row padding verified |

The 27 invalid CLI cases included `--size`/`--threads`/`--variant`/`--input`/`--output`
without values, duplicate options, unknown options, sizes 0/15/8193/abc/-1, thread counts
0/257/abc/-1, `--demo --benchmark`, `--benchmark --variant atomic`, `--input` with
`--size`, nonexistent input, and an unwritable output path.

## Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer were built with:

```bash
gcc -O1 -g -Wall -Wextra -fopenmp -fsanitize=address,undefined \
    -fno-omit-frame-pointer src/pixelsprint.c -o /tmp/pixelsprint-asan
```

The ASan/UBSan binary ran `--demo --size 256 --threads 4`, a valid PPM input, and a
truncated PPM input. The demo and valid PPM exited 0; the truncated PPM exited 1. No
sanitizer diagnostics were reported.

ThreadSanitizer was attempted with:

```bash
gcc -O1 -g -Wall -Wextra -fopenmp -fsanitize=thread \
    -fno-omit-frame-pointer src/pixelsprint.c -o /tmp/pixelsprint-tsan
```

The TSan build succeeded, but runs against the correct-atomic and correct-reduction paths
reported races at OpenMP synchronization points (the parallel-for directive and
`detect_actual_threads`'s `omp single`). The GCC/libgomp OpenMP runtime used here does not
model all OpenMP synchronization for this TSan build, so the result is **not a clean TSan
pass** and was not used as correctness evidence. A full 8192 x 8192 run was **NOT RUN**;
a clean TSan pass was not obtained.

## Final benchmark snapshot

The committed `output/results.csv` was generated by the final `make benchmark` run. It has
one header row and 21 data rows. A CSV consistency check confirmed 10 columns per row,
V0's baseline/N/A fields, V1's unsafe N/A fields, protected speedups computed as
`V0 median / row median`, efficiencies divided by actual threads, and repeated LUT columns
for the same requested thread count.

Final run highlights (medians; the full table is in the CSV):

- V0 sequential: 2.409210 ms.
- V4 private, 12 threads: 1.274011 ms, speedup 1.891043, efficiency 0.157587.
- V5 reduction, 12 threads: 1.451923 ms, speedup 1.659324, efficiency 0.138277.
- LUT apply, 12 threads: 1.110866 ms, speedup 3.643201, efficiency 0.303600.

V1 was `NO_RACE_OBSERVED` at one thread and `RACE` at 2, 4, and 12 threads in this run.
Its timings are not valid speedups. The benchmark numbers are machine- and load-specific;
repeated runs can produce materially different medians.

## Not covered

- Full 8192 x 8192 execution (the maximum supported generated size).
- A reliable ThreadSanitizer pass, for the reason above.
- A formal VTU report, presentation, or viva document; this repository contains the
  README and this test log only.
