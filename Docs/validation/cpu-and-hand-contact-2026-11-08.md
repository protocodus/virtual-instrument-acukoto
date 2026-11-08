# Acukoto CPU and hand contact — 2026-11-08

Local DSP candidate **0.25.4**, based on `ff4a283ff4963b92f5806e36e838761ccfb11f14`.
The starting checkout also contained an uncommitted vectorization candidate;
it was copied into an isolated worktree and qualified here. The original
checkout was left intact. No modal resonances, string losses, calibration
tables, quality settings, sample rates, latency or factory gain were reduced.

## What changes

The portable body bank, bridge matrix products, string polarizations and
stereo phase networks use explicit 128-bit vectors on Clang/GCC. String
filters retain their scalar operation order and double precision. Paired
polarizations share designed coefficients but keep separate phase and history;
`Acukoto.LoopPair` compares all output bits and relevant histories with the
retained scalar implementation. The existing x86 AVX2 dispatch remains.

Bend interpolation is located once for the four loops of a string, unchanged
fractional-delay coefficients are reused, the waver normalization is prepared
once, and power-of-two room indices wrap with masks. Hot scalar metadata is
grouped before the large delay buffers. No audio-thread allocation was added.

The released hand's contact uses the current travelling period
`P = loopDelay / pitchRatio`: its outward and returning taps are `x P / 2`
and `(1 - x / 2) P`. The damping share uses current frequency too. Previously
those taps stayed at the open string's period during hand bends, master tune
and tension glide, moving the effective contact away from its physical place.
Only active contacts refresh when the ratio changes. Fractional read and
scatter remain transposes, so the contact removes rather than creates energy.
This is a lumped waveguide approximation; near short-line boundaries the
contact is clamped to accessible storage, not a full dispersive displacement
model of a fingertip.

The preceding hand change also replaced the established thumb-rest and
staccato stops with a soft point contact at the last pluck location. Near a
string end that contact damps the fundamental weakly. Both existing contracts
failed on the unchanged baseline: thumb-rest suppression was only 9.36 dB and
staccato 12.40 dB, against their original 20 dB requirements. Those explicit
stopping gestures regain their established distributed damping; ordinary
variable Release keeps its spatial contact. No rate or test limit was fitted
or relaxed to make the failures pass. The corrected thumb-rest witness now
suppresses the released lower string by **27.2 dB**, while a held lower note
changes by only 0.1 dB.

## Evidence

- CPU-only qualification: both presets, 1×/2×/4× at a 48 kHz host,
  64-frame callbacks, dense held playing and a gesture sequence. All
  **1,152,000 float samples** matched the baseline byte for byte
  across 12 cases. This comparison precedes the intentional hand correction.
  The final candidate also matches all **2,304,000 float samples** in the
  two-second version of that matrix: these held-string workloads do not
  engage the changed released-hand stops. The separate preview and engine
  tests exercise those intended sound changes.
- `Acukoto.HandContact`: four rates (44.1/48/96/192 kHz), four pitch ratios,
  a midpoint contact's nodal and antinodal response, and 480 passivity cases
  including overlapping fractional stencils. The baseline failed all 12 bent
  nodal cases: worst nodal/antinodal loss ratio 1.370613. Corrected worst ratio
  is 0.00000246; maximum floating-point energy growth in the passivity sweep
  is 0.0000000285 (within the unchanged 0.000001 roundoff bound).
- Jukebox **JukeboxSDK_500_028 / TargetVersion=5.0** compile-only passed:
  canonical candidate DSP and the existing Acukoto Rack wrapper compiled,
  linked and passed immutable-global checks.

## Native CPU measurement

Apple M1 Max, macOS arm64, Apple Clang 21.0.0. Both DSP libraries use
C++20 Release `-O3 -DNDEBUG`; both measurement harnesses use the identical
current source and `-std=c++17 -O3 -DNDEBUG` flags. Host rate is 48 kHz,
64-frame callbacks (1.333 ms deadline), both presets at true 1×/2×/4×.
Each case starts with thirteen held strings, warms up for 250.667 ms, then
renders two timed seconds of dense playing or notes plus gesture controls.
Three paired rounds alternate execution order (A/B, B/A, A/B). Builds and
this task's other renders/tests were stopped during these runs; the machine
remained a normal desktop environment. Each row averages the per-case median
calling-thread CPU percentage from the three rounds (four cases per setting).
Setup and report/export work are excluded; note/control handling is included.

| Oversampling | Baseline, % of one core | Candidate | Average CPU reduction |
| --- | ---: | ---: | ---: |
| 1× | 13.93% | 8.97% | 35.6% |
| 2× | 28.14% | 18.40% | 34.6% |
| 4× | 56.92% | 37.33% | 34.4% |

This is an average CPU improvement, not a deadline guarantee. Every round's
paired wall/thread timing is retained. Across 18,000 callbacks per setting and
version in the matched-harness run, deadline-miss counts were:

| Setting | Baseline wall / thread | Candidate wall / thread | Candidate max wall / thread |
| --- | ---: | ---: | ---: |
| 1× | 0 / 0 | 1 / 0 | 1.669 / 0.471 ms |
| 2× | 5 / 1 | 4 / 0 | 5.581 / 1.011 ms |
| 4× | 87 / 35 | 0 / 0 | 1.090 / 0.932 ms |

The matched-harness candidate had no calling-thread CPU overrun in this
workload, but did have wall overruns. An earlier repeated run used C++17 for
the baseline harness and CMake's C++20 for the candidate harness. That run
showed 34.2–34.8% average savings and **10 candidate thread-CPU overruns at
4×** (6 in its baseline). It is retained as `initial-performance-summary.json`
and the unprefixed round logs, not discarded or retroactively explained away.
The subsequent `qualified-*` run corrects the harness compiler mismatch;
re-running was not conditional on a timing result. Both runs produce identical
audio. This evidence supports average CPU savings, not guaranteed glitch-free
host playback. The initial one-second CPU-only comparison is retained too.

## Completed checks

- Release build including `AcukotoDSPCxx17`; all 11 unit/audio tests pass:
  tuning, hand contact, engine, strike energy, acoustic power budget/report,
  deterministic fuzz replay, MIDI source check, deterministic factory demos,
  calibrated levels and scalar/SIMD loop equivalence.
- `Acukoto.CpuBudget` passes on the existing factory-demo workload at
  48 kHz/256 frames; this is separate from the 64-frame probe above.
- ASan/UBSan: Engine, HandContact, StrikeEnergy and LoopPair all pass.
- Existing Rack host contract, patch levels, Lua/assets and immutable-global
  contracts pass against this canonical DSP through `ACUKOTO_INSTRUMENT_DIR`.
  The first asset invocation used CMake's Python 3.14 without Pillow and
  failed at import. Selecting the installed Python 3.11 with Pillow fixes
  that environment failure; its failed log is retained.
- Apple Clang 21 C++17 `-O3 -fno-builtin -Werror -fno-exceptions` syntax
  checks pass for arm64 and x86_64. The x86 check is compilation evidence,
  not a runtime or performance measurement.

## Listening preview

The diagnostic hand phrase uses the complete Performer signal path, preset 0,
48 kHz and MIDI velocities 78–100. It plays adjacent-string thumb rests,
explicit staccato, a hand bend, then a descending phrase. The source MIDI is
separate from the frozen factory demos. Before/after renders are 10.06 seconds:
whole-file RMS −28.069/−28.051 dBFS and peaks −4.970/−4.956 dBFS, without
normalization or gain changes. These whole-file figures include silences and
decay; they are not Reason mixer VU measurements. The correction restores
specified articulations; no claim of a recording-matched overall timbre follows
from these mathematical and regression tests.

## Reproduce and limits

`Tools/MeasureCpu.cpp` times the complete native Performer, including control
and note dispatch but excluding setup, file output and analysis. Every callback
records calling-thread CPU and wall time together; `--blocks-csv` retains the
raw paired data and `--samples` exports interleaved float32 for comparisons.
Use the same compiler/options and alternate baseline/candidate executables.

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DACUKOTO_REQUIRE_PYTHON_TESTS=ON
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp -L '^(unit|audio|perf)$' --output-on-failure
build-dsp/AcukotoCpuProbe --seconds 2 --scenario all --blocks-csv blocks.csv --samples samples.f32 > cpu.json
```

Native timing is not Reason host or translated target-chip CPU. SDK compile-only
is not installation, host playback, song persistence or release validation.
The candidate is source work on an isolated local branch; no factory patches,
Rack metadata or installed device were changed. Existing acoustic-power and
body-response limitations documented in the README remain.

Raw logs, baseline failures, binaries and previews are retained locally in
`/Users/vojta/Dev/reason-rack-extension/diagnostics/acukoto-cpu-realism-20261108/`.
