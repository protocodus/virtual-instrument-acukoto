# CPU qualification after the realism fixes — 2026-11-09

The initial acoustic work was frozen at `bfd6007` before this optimization. Its
source, library, native tools and reference PCM are retained outside the
checkout in `/workspace/scratch/acukoto-2026-11-09/`. Upstream `9d7bda9`
is a separate baseline for total before/after cost. Only the post-acoustic
baseline is expected to produce identical sound to the optimized version.

A later supported-range safety defect required a second acoustic correction,
`f6dbfc2`, after these initial measurements. The tables below explicitly
describe `bfd6007` → `ab29599`; they are not silently attributed to the later
source. A matched post-safety/no-fusion source is retained separately, and
final requalification is appended below.

## One exact change

The AVX2 modal path previously traversed the same modal velocity array four
times for left/right in-phase and quadrature radiation. The new traversal
shares those reads between four independent weight sets. Each output keeps
its original eight accumulation lanes, then reduces lanes 0 through 7 in
the original order. Four local scalar totals are assigned only after the
read-only traversal, avoiding repeated stores through potentially aliasing
output references. There is no arithmetic approximation or fused multiply-add.

The existing `target("avx2,no-fma")` dispatch and scalar-aligned, alias-safe
vector typedef remain. No model count, string loop, physical coefficient,
sample rate, oversampling ratio, noise draw, output gain, latency or test
limit changes. The portable path remains unchanged. This optimization's
timing claim therefore applies to native AVX2 execution, not to other
processors or the Reason target.

## Profile and assembly evidence

GCC 14.2, Release `-O3 -DNDEBUG`, x86-64 AMD EPYC 9V74; Valgrind 3.24
Callgrind uses the same flags with debug line information. Client requests
collect the complete callback loop: event/control dispatch, processing,
clock calls, timing stores and the preallocated audio copy after each native
timing bracket. Preparation, allocation, the 250.7 ms warmup, analysis and
file output are excluded. The separate native timing brackets include
dispatch and processing, with their common timer overhead.
The fixed native workload is Hirajoshi, 48 kHz, 64-frame blocks, 13 held
strings, 1x processing, 0.5 seconds of measured callbacks.

| Workload | Post-acoustic baseline instructions | Optimized instructions | Change |
| --- | ---: | ---: | ---: |
| Dense ordinary playing | 547,313,999 | 530,945,999 | −2.99% |
| Targeted contact/technique gestures | 548,440,090 | 532,072,090 | −2.98% |

The old four radiation traversals account for 7.54% of dense callback
instructions. The targeted variant uses harmonic, finger, tremolo, left,
staccato, glissando and scrape keys; it is explicitly a diagnostic workload,
not a replacement for factory-preset measurements. Pluck setup accounts for
about 0.44% of that short profile, so no speculative transcendental
approximation or contact-quality reduction was pursued.

The GCC assembly has eight vector accumulators, two modal-state registers
and one product register in the fused loop, with no stack spills in that
loop. The final scalar reductions use three short-lived stack temporaries;
they do not spill per mode. An intermediate version reduced directly into
the output references and used 532,265,999 dense instructions; local totals
remove its alias-driven repeated loads/stores. Intermediate evidence is
retained. Callgrind elapsed time is not used as a speed measurement.

## Exactness and regression coverage

`Acukoto.ModalRadiation` checks 192 banks against the unchanged portable
two-output reduction: zero count, ordinary and parity-padded counts through
the full 424-entry capacity, signed cancellation, widely varying finite
weights, silent states and NaNs beyond the active count. All four doubles
match bit for bit. The AVX2-specific check runs only where that dispatch is
supported.

Byte comparisons against frozen `bfd6007` also pass:

- 2,304,000 interleaved float32 samples: both native instrument designs, true 1x/2x/4x,
  dense and gesture workloads, two seconds per case.
- 4,608,000 float32 samples: all twelve body strikes, both presets, velocity
  1, two seconds per strike through the shipping Performer with latency
  trimmed. These directly exercise both quadrature radiation outputs.
- The entire accepted 11.06-second shipping review score, including harmonic
  attacks, repeated tremolo velocities, left-of-ji release and staccato.
  Its WAV SHA-256 remains
  `57848e39066c3b41ba7fc1117de8aacad2dc90fd7cd0690188993067ec58504a`.
- 31,104,000 float32 samples: all nine authored Rack patch control sets,
  true 1x/2x/4x, twelve seconds of the appropriate committed MIDI score and
  tail per case. Combined PCM SHA-256 is
  `30a07b157ba19b92e5abebf4e49f269fe4d9f492052c4da1629abb3b3123ff79`.

The rebuilt post-optimization suite passes all 14 unit/audio tests in
138.92 seconds, with unchanged limits. The strict C++17 target also builds.
The acoustic freeze already passed the Engine, HandContact, PlayingOwnership,
PluckShape and LoopPair sanitizer checks and 120-second libFuzzer run.
The new modal reduction also passes its ASan/UBSan boundary check.

## Initial freeze: quiet paired timings

Completed in the coordinated quiet window, 08:43–08:56 UTC, with the other
instrument workers' heavy jobs idle. The protocol uses the identical
`Tools/MeasureCpu.cpp` source and compiler flags for upstream, post-acoustic
and optimized binaries. Three alternating triplets measure both native designs,
true 1x/2x/4x processing and dense/gesture workloads for eight seconds per
case. Each callback records calling-thread CPU and steady wall time together;
all blocks, including deadline outliers, are retained. Raw PCM export is
separate from these timing runs. The existing `Acukoto.CpuBudget` is also
run unchanged. The source is acoustic `bfd6007`, then exact optimization
`ab29599`. The compiled binary hashes, compiler and flags are recorded in
`cpu/build-input-ledger.json`. These are native development-machine measurements, not a
DAW or target-chip deadline guarantee.

Each of the twelve native cases lasts eight seconds (6,000 callbacks), so
summing their thread CPU costs compares equal rendered durations. The
aggregate gives the costlier 4x cases their actual weight in total work;
it is not an equal-weight average of percentage improvements.

| Round | Upstream mean CPU | Acoustic mean CPU | Optimized mean CPU | Optimized vs acoustic | Optimized vs upstream |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1, upstream / acoustic / optimized | 26.67% | 28.92% | 27.40% | −5.28% | +2.73% |
| 2, reversed | 27.69% | 28.60% | 27.97% | −2.21% | +1.00% |
| 3, original order | 27.64% | 29.63% | 28.02% | −5.44% | +1.38% |

The median paired optimization reduction is **5.28%**. At that `ab29599` freeze, the complete
change costs **1.38% more** than upstream on the median paired aggregate.
These are different comparisons. Individual cases vary substantially:
their median final/upstream changes span −8.42% to +10.06%. The report
retains all three ratios per case, rather than treating the aggregate as
a universal gain. The final pinned follow-up and same-harness upstream
instruction profile below examine those differences after the safety fix;
they do not erase the variation at this earlier checkpoint.

Median final CPU across the three runs is 10.56–12.26% at 1x,
22.75–25.51% at 2x and 45.26–49.06% at 4x. The corresponding per-case median
p99 thread callback times span 0.294–0.345, 0.531–0.670 and 0.978–1.125 ms.
The 64-frame period is 1.333 ms. Across all 216,000 final callbacks there
are 568 thread-CPU and 572 wall deadline exceedances; maximum wall time
is 32.334 ms. These outliers are not discarded. The identical final
p0/2x/gestures callback 737 takes 292.0, 279.4 and 32,332.5 microseconds
of thread CPU in the three runs; p0/4x callback 944 takes 708.1, 23,913.7
and 712.2 microseconds. This demonstrates nonrepeatable runtime variation,
without identifying its scheduler, cache, frequency or virtualization cause.

Those two native designs with default controls are distinct from the nine
authored Rack factory patches. A separate bounded workload loads all nine
`.repatch` files using the independent wrapper contract's exact panel mapping,
retains their controls, and plays each design's committed MIDI demo. It
measures three alternating post-acoustic/optimized pairs at the shipping 1x
factor, 48 kHz and 64-frame callbacks, with complete twelve-second score/tail
windows. The two-design probe above supplies true 2x/4x stress coverage;
factory patch PCM parity additionally covers those factors. Hirajoshi's last
event is at 9.21875 seconds and Hankumoijoshi's at 8.75 seconds, so twelve
seconds includes both entire scores and their tails. Both the named
Hankumoijoshi and Wide Hall patches select the second design. Native
Performer execution of these control sets is not a Reason host benchmark.

Factory workload-total changes are **−1.77%, −6.51%, −5.12%**, median
**−5.12%**. Final aggregate CPU itself spans 10.04–10.12% across rounds.
Per-patch medians remain visible:

| Authored patch | Final CPU | Median paired change |
| --- | ---: | ---: |
| Koto Hirajoshi | 9.98% | −6.64% |
| Koto Hankumoijoshi | 10.08% | −6.43% |
| Finger Koto | 9.94% | −3.25% |
| Near the Ryukaku | 10.24% | +1.75% |
| Far Pluck | 10.03% | −3.88% |
| Plain Strings | 10.04% | −2.58% |
| Damped Tsume | 9.98% | −5.24% |
| Close Dry Koto | 10.00% | −0.48% |
| Wide Hall Koto | 9.99% | −5.76% |

Factory p99 thread times span 0.251–0.307 ms. All 243,000 final callbacks
are retained, including 74 thread and 76 wall exceedances of 1.333 ms;
the maximum wall time is 9.366 ms. There is no universal per-patch speedup
or hard-deadline claim.

The unchanged `Acukoto.CpuBudget` passes, with median realtime factors
0.11049 and 0.10848 against its 1.0 limit. Its 256-frame wall-only worst
block shares are 1.083 and 0.976, so this mean-cost gate also does not prove
every callback meets its deadline. Its authored demos contain at most two
simultaneous notes. The dense native probe holds thirteen strings before
timing. The final prepared-silent and ringing thirteen-note attack
qualification below measures dispatch and deferred contacts explicitly.

Scratch `cpu/` retains source/harness snapshots, instruction profiles,
assembly, PCM hashes, the timing runner and full block logs. The final
summary distinguishes optimization cost from the cost of the preceding
acoustic corrections rather than attributing both to the same change.

## Final supported-range correction and exact fast path

The final DSP source is `e899ec3`. The intervening `f6dbfc2` correction fixes
an inherited nonfinite C8/+12 output at 22.05 kHz with a stable first-order
short interpolation section and matched setup tuning. Its extended bend
lookup covers simultaneous master and hand bends; see the acoustic review.
That safety fix is a model correction, not a CPU quality tradeoff.

Profiling that corrected source exposed avoidable register/branch merging
in the ordinary paired path. A second exact optimization returns through
the retained scalar `read()` for a pair containing either first-order
section. The overwhelmingly common second-order SIMD path then remains
straight-line. Both polarisations remain independent; scalar/paired history
and public PCM tests include mixed orders. No filtering or delay arithmetic
was approximated. The final reduction is not attributed solely to AVX2:
this additional paired-path layout also contributes on the measured x86 host.

A fresh matched no-optimization reference was made from archived `f6dbfc2`
by reversing only the `bfd6007`→`ab29599` radiation-sum patch. This retains
all final physical corrections while omitting both exact CPU transformations.
`cpu/fusion-only.patch`, source hashes, compiler/flags and binary hashes
make that reference reproducible. It is configured with `BUILD_TESTING=OFF`
and builds only `AcukotoDSP`/`AcukotoCpuProbe`: the later optimization-specific
modal test intentionally names the fused helper absent from this reference.
Libraries and the ordinary native probe
use GCC 14.2 `-O3 -DNDEBUG -std=c++20 -fPIC -Wall -Wextra -Wpedantic`;
the identical factory/attack/profile harnesses use `-std=c++17`. Strict
C++17 portability is a separate, passing gate, not a different timing build.

Fresh Callgrind runs use the same collected 0.5-second dense callback-loop
scope described above, Hirajoshi/48 kHz/1x/64 frames:

| Source | Instructions |
| --- | ---: |
| Upstream `9d7bda9` | 546,398,526 |
| Final physical model, original sums and mixed-vector layout | 566,293,116 |
| Safety freeze `f6dbfc2`, fused sums | 549,925,116 |
| Final `e899ec3`, fused sums and rare scalar fallback | 534,065,747 |

The two exact optimizations save **5.69%** against the matched corrected
model; the complete final result uses **2.26% fewer** instructions than
upstream in this workload. These are instruction counts, not elapsed-time
claims. The intermediate safety freeze costs only 0.65% more instructions
than upstream; fixing the NaNs was not made contingent on a speed win.

The safety freeze `f6dbfc2` matches acoustic `bfd6007` on all nine factory
patches at 1x/2x/4x (31,104,000 floats) and all 128 keys of both presets
through the shipping Performer at 48 kHz/velocity 0.7 (49,152,000 floats). The final paired-path optimization then
matches that safety freeze on all twelve native design/factor/gesture cases
(2,304,000 floats), all 32 low-rate/high-key witnesses (635,040 floats), and
the independent mixed-order scalar history oracle. The accepted review WAV
is also unchanged through the safety freeze. Further final factory/attack
PCM comparisons accompany the quiet timing runs below.

The `f6dbfc2` full suite passes 15 non-CPU tests without changing limits;
its new range/paired tests also pass Clang ASan/UBSan and the configured
120-second libFuzzer gate completes without a finding (194.07 wall seconds
under shared load). The final exact layout is separately rebuilt: all 15 non-CPU tests pass
(179.92 seconds) and ASan/UBSan RangeSafety/LoopPair pass (51.46 seconds).
Root independently passes strict x86-64/AArch64 C++17 compilation and the
full native wrapper contract, with source-specific receipts rather than
relabelling old runs.

## Final quiet measurements — mixed native results

The final coordinated quiet window ran 10:36–10:46 UTC. All other instrument
workers and root heavy jobs were idle. These final probes pin CPU 2 and
retain every callback. Source is `e899ec3`, with the matched corrected
no-optimization reference described above; upstream remains `9d7bda9`.
Three alternating paired rounds are used in each workload. Input hashes
and exact commands are in `cpu/safety-build-input-ledger.json` and each
`safety-*/manifest.json`. A factory-summary invocation initially omitted its
directory argument; only that post-measurement analysis call was corrected,
without rerunning or changing any measured input.

| Dense pinned case | Final median CPU | Final vs corrected reference, three rounds | Median vs upstream |
| --- | ---: | --- | ---: |
| Hirajoshi, 1x | 10.476% | +6.94%, −6.39%, −0.085% | −4.51% |
| Hankumoijoshi, 2x | 21.012% | −6.20%, −8.25%, +0.79% | −6.97% |

The first case does not establish a speed improvement against the corrected
reference. The second has a median 6.20% reduction, with one slower round.
These are two bounded follow-up cases, not a new twelve-cell timing matrix.
Across their 36,000 final callbacks, 27 exceed the 1.333 ms period in both
clocks; the maximum wall callback is 5.121 ms.

All nine actual factory patches use their complete committed scores plus
tails, twelve seconds each, 48 kHz/64 frames/1x. Their final workload-total
changes versus the corrected reference are **−3.96%, +2.46%, +6.55%**,
median **+2.46%**. Final aggregate CPU spans 10.99–11.99%. Therefore the
final factory measurements do **not** establish a speed win; they must not
be replaced by the earlier favorable checkpoint or the instruction count.
Per-patch median paired changes range from −2.36% to +7.39%. The exact
optimizations are retained for their reduced instruction work and the
observed native benefit in some workloads, without claiming universal or
repeatable factory speed improvement. All source-specific raw rounds remain.
Among 243,000 final factory callbacks, 269 thread and 270 wall callbacks
exceed 1.333 ms; maximum wall time is 10.734 ms.

The attack supplement measures thirteen maximum-velocity native strings,
both designs, true 1x/2x/4x, tsume and bare finger, prepared-silent attacks
and ringing re-attacks. Each case has six trials and a 250.7 ms callback
window, including deferred kakite contacts. Preparation, reset, ringing
warmup, audio export and analysis are outside timing. Prepared silent means
reset DSP state in prepared memory, not cold OS caches or construction.

| Factor | Final first-block median range | Largest first block | Window p99 range |
| --- | ---: | ---: | ---: |
| 1x | 0.208–0.793 ms | 1.157 ms | 0.322–0.584 ms |
| 2x | 0.409–1.213 ms | 3.786 ms | 0.533–0.691 ms |
| 4x | 0.811–2.109 ms | 7.621 ms | 0.881–1.768 ms |

Each first-block median is based on six observations per run, then the
median across three rounds; its maximum is not called a reliable p99.
The thirteen-finger 4x medians are 1.876–2.109 ms, exceeding a 64-frame
period even though this extreme also exceeded it upstream. First-block
median paired changes versus upstream span −1.62% to +25.77% across the
24 cells: pluck preparation is not uniformly cheaper, despite the smaller
steady instruction count. In the full 81,216 final attack-window callbacks,
357 thread and 361 wall callbacks exceed the period; the largest thread
callback is 25.075 ms. These limits and outliers are retained, not hidden
by aggregate CPU or a guard. Maximum velocity is the tested scope, not a
proof of worst cost over every possible release-width branch.

All 24 attack cells render deterministically over the three repetitions;
corrected reference and final PCM match byte for byte. Factory timing runs
also retain their PCM outside the callback timing brackets for audit.

The unchanged final `Acukoto.CpuBudget` **passes**, with median realtime
factors **0.10670 / 0.10825**, against 1.0. Its worst 256-frame wall shares
are 0.555 / 0.522. This authored-demo mean-cost gate and the separate
stress measurements have different scopes; neither is a universal DAW or
target-chip deadline guarantee. No test limit, physical model, output gain,
quality setting or oversampling factor was weakened for these results.
