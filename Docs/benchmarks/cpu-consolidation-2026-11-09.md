# Acukoto CPU: consolidation — 2026-11-09

Acukoto 0.25.12 (`c06a415`) against 0.25.7 (`6a76701`), at the default
settings: 48 kHz, 1× oversampling, both presets, played through the
Performer. Four changes touch the cost. None changes a mode, a coefficient,
a control period or a level.

| Version | Commit | Change | Sound |
| --- | --- | --- | --- |
| 0.25.8 | `2ce02f1` | FMA3 kernel: each group of eight modes takes its step and adds its radiation in one pass; the pull on the bar runs thirteen coordinates to a pass | rounding only, 165 dB or more under the signal |
| 0.25.9 | `b14a5cc` | A pluck's shape calls `erfc` and `exp` only around its contact | the same bits |
| 0.25.10 | `f98397e` | No subnormal reaches the running-energy ring | the same bits |
| 0.25.12 | `c06a415` | No library call for a floor or a rounding while playing | the same bits |

0.25.11 (`93aae28`) is an audible fix with no measurable cost; it is in the
[audit](../validation/consolidation-audit-2026-11-09.md).

The short of it: on a CPU with AVX2 and FMA3 the engine takes 8.5 to 8.7%
fewer instructions and its note-on call is a fifth to a quarter shorter.
In the build that stands in for Reason's, the steady cost is unchanged
(+0.3%):
that build runs the portable kernels, and nothing numerically neutral was
found that pays there. The 10% ceiling is not met in either build. The
largest blocks left are not note-ons but the ones that clear the engine's
state, and they are not fixed (see "The blocks that clear the state").

## The build Reason runs

Reason's toolchain compiles a Rack Extension with clang to bitcode
(`-O2 -std=c++17 -fno-builtin -ffreestanding`) and lowers it for x86-64 with
SSE3 or for arm64 with NEON. This document takes it that no AVX2 or FMA3
kernel runs there, so on x86-64 the engine runs its portable kernels (four
floats or two doubles to a vector, written with the compiler's vector
extensions), and the AVX2 and FMA3 kernels serve the tests, this machine and
a future plug-in.

The stand-in measured here is a native build of the same sources:

```sh
clang++-18 -O2 -std=c++17 -DNDEBUG -DACUKOTO_DISABLE_AVX2 -fno-builtin -msse3
```

It is not a U45. No Jukebox SDK build was made, Reason's code generator may
differ from clang 18's, and its math library is not glibc's.

Which kernels Reason really runs on an AVX2 CPU is an open question, and it
decides which column of the tables below applies there. The Rack adapter
does not define `ACUKOTO_DISABLE_AVX2`; `CpuFeatures.h` asks the CPU itself
because "the freestanding Rack toolchain has no libgcc CPU cache"; and
0.25.4's validation records a compile-only Jukebox SDK pass with the AVX2
kernels in the source
([CPU and hand contact](../validation/cpu-and-hand-contact-2026-11-08.md)).
So the kernels' per-function `target("avx2,…")` attributes may survive that
toolchain and run. Nobody has measured it in Reason. Until someone does,
the stand-in is the figure to plan by, and the FMA3 column the one to hope
for on Intel.

Under `-fno-builtin` every `std::floor`, `std::lround`, `std::pow`,
`std::exp` and `erfc` is a real call into the host's library. Two of this
round's changes are about that:

- A note-on's pluck shape made about 2600 `erfc` and `exp` calls for a low
  string. 0.25.9 calls the library only for the few cells around the
  contact; past 8.5 contact widths the shape is its asymptote to the last
  bit.
- While playing, `lround` ran once per string per control period and
  `floor` whenever a loop's delay moved. 0.25.12's `floorInt` and
  `nearestInt` give the library's integers without the call
  (`Acukoto.LoopPair` compares them with `std::floor` and `std::lround` at
  the halves, either side of them and of the whole numbers, both signs, and
  on two million random values). In the stand-in's dense phrase the
  library's share fell from 41 to 16 instructions per sample (glibc's
  counts: `lround` 25, now 0.01; `pow` 6, `log` and `log10` 5, `erf` and
  `exp` 2 remain, all at control rate or at note-ons).

Reason does not document setting flush-to-zero or denormals-are-zero, and
the Rack adapter does not set them. `Acukoto.Subnormals` (0.25.10) plays
49 s with both off and requires MXCSR's denormal-operand and underflow flags
to stay clear: twelve strings stopped by the hand beside one that rings, a
release into sleep, a soft high note left to die away, a tuning switch. It
failed on 0.25.8 at every note-on (the pluck shape's far cells) and on
0.25.9 at the first soft note after a sleep (the running-energy ring).

## Instruction counts

Callgrind, 1.5 s (72,000 samples) of each phrase on preset 0 through the
Performer, only the `process()` calls collected, instructions per sample:

| Build | Phrase | 0.25.7 | 0.25.10 | 0.25.12 | Change |
| --- | --- | ---: | ---: | ---: | ---: |
| GCC 13.3 `-O3`, FMA3 kernels | dense | 15,235 | 13,963 | 13,937 | −8.5% |
| | chords | 15,271 | 13,976 | 13,947 | −8.7% |
| Reason stand-in | dense | 34,940 | 35,065 | 35,042 | +0.3% |
| | chords | 34,836 | 34,927 | 34,900 | +0.2% |

"Dense" is thirteen held strings, one re-plucked every 128 ms; "chords" is
every string struck at once at full velocity every 400 ms with the pedal
down, the heaviest note-on block there is.

The stand-in's 0.3 to 0.4% rise from 0.25.7 to 0.25.10 comes with
0.25.10's guard on each string's squared velocity, the only change to the
portable per-sample path in that span; 0.25.12 takes 25 instructions back.

Where the stand-in's 35,065 instructions per sample go (0.25.10, dense):

| Part | Instructions per sample | Share |
| --- | ---: | ---: |
| Body: the 26 bar coordinates' drive of the 356 modes | 8,840 | 25% |
| Body: the modes' pull back on the coordinates | 8,600 | 25% |
| Body: the modes' recursion | 3,200 | 9% |
| Body: radiation to four microphones | 3,000 | 9% |
| The ji junction's two 26 × 26 products | 2,314 | 7% |
| The 52 string loops (delay, Thiran, dispersion, loss) | 6,800 | 19% |
| The per-sample loop, the room, the output stage | 2,300 | 7% |

Two thirds is the body, and half is two float products of 26 × 356 terms
each. In SSE3 a term is a load, a multiply and an add on four lanes; the
FMA3 kernels run eight lanes and fuse the multiply and add, which is most
of why that build takes 0.40 times the instructions.

## Timings

Thread CPU time of each 256-frame `Performer::process()` call (5.33 ms at
48 kHz), the least of seven takes per block so that another tenant's
interruption does not count, then the mean, the 99th percentile and the
worst over the blocks; flush-to-zero and denormals-are-zero set, as a
plug-in host sets them. Two rounds, both shown as a range. GCC 13.3 `-O3`
(FMA3 kernels) and the Reason stand-in (clang 18.1.3), on a 4-vCPU Intel
Xeon @ 2.10 GHz (Sapphire Rapids) container with the other sessions idle
(load average 0.9 to 2.4). Development-machine figures, never Reason or DAW
CPU.

Percent of one core, mean / 99th percentile / worst block:

| Phrase | 0.25.7, FMA3 | 0.25.12, FMA3 | 0.25.12, Reason stand-in |
| --- | --- | --- | --- |
| Demo 01, Koto Hirajoshi | 8.40-8.45 / 9.62-9.64 / 10.27-10.45 | 8.08-8.12 / 9.05-9.23 / 9.48-9.65 | 10.11-10.27 / 10.85-11.09 / 11.26-11.33 |
| Demo 02, Koto Hankumoijoshi | 9.68-10.00 / 10.55-11.86 / 15.08-15.31 | 9.46-9.50 / 10.36-10.61 / 15.15-15.41 | 10.53 / 12.27-13.02 / 20.18-20.54 |
| Dense | 8.58-8.71 / 9.61-10.01 / 10.82-11.59 | 8.10-8.11 / 8.72-8.78 / 9.72-9.75 | 10.23-10.34 / 10.87-11.00 / 11.50-12.72 |
| Chords | 9.67-10.00 / 10.83-11.39 / 11.79-11.91 | 9.73-9.92 / 10.92-10.94 / 11.46-11.73 | 10.34-10.83 / 11.38-12.33 / 12.14-13.05 |

The FMA3 build's means fell 2 to 7% except on the chords, which did not
move in time though they took 8.7% fewer instructions; why was not found.
An earlier pair on the loaded machine (least of five takes) gave dense
9.02 to 8.71% mean and 12.28 to 11.23% worst, chords 10.48 to 9.81% and
14.02 to 12.92%: the two pairs disagree on the chords by more than the
change itself.

These figures are lower than the 10.9 to 12.5% medians in
[fused multiply-adds](cpu-fma-2026-11-09.md) for the same 0.25.7 because
that document timed single takes of 64-frame callbacks on a shared machine;
the least-of-seven here removes the other tenants.

The note-on call itself (`Performer::noteOn`, the pluck's shape written
into the loops), FMA3 build:

| Phrase | 0.25.7 average / slowest | 0.25.12 average / slowest |
| --- | --- | --- |
| Demo 01 | 36.6 µs / 85-87 µs | 29.5-30.9 µs / 62-65 µs |
| Demo 02 | 33.5-35.3 µs / 61-72 µs | 24.5-28.8 µs / 52-58 µs |
| Dense | 24.2-24.6 µs / 78 µs | 18.8-19.2 µs / 64 µs |

The slowest note-on is 1.2% of a block. A note-on is not what makes a block
large.

**The 10% ceiling is not met.** With the FMA3 kernels the two demos average
8.1 and 9.5% and every phrase but the Hirajoshi demo and the dense one has
blocks over 10%. The Reason stand-in averages 10.1 to 10.8% and its
ordinary worst blocks are 11 to 13%.

`Acukoto.CpuBudget` still holds the demos to real time on one core
(`cpuBudget = 1.0` in `Tools/RenderPresetDemos.cpp`, whose comment quotes a
0.455× median from 2026-10-29); the 10% ratchet is not on Acukoto yet and
this round did not touch it. At 0.25.12 it reports 0.095× and 0.087×
realtime for the two demos (median of three; 0.099-0.100× and
0.104-0.105× over two more runs of five), where 0.25.7 gave 0.127× and
0.141× while four other sessions loaded the machine.

The same tool's "worst block" is one take's wall clock, the largest of a
demo's 2200 blocks: 51 and 52% of a block in the test's run, and 22 to 56%
over the two further runs. On this shared container that is the
scheduler's figure, not the engine's: the same demos' worst blocks by
thread CPU and the least of seven takes are 9.5-9.7% and 15.2-15.4%. A 10%
ratchet on worst blocks needs a runner that is not shared, or the least of
several takes.

## The blocks that clear the state

The largest blocks are the ones in which an engine clears its strings, and
they are still there:

| Block | FMA3 build | Reason stand-in |
| --- | --- | --- |
| An ordinary block before sleep | 0.427 ms (8.0%) | 0.537 ms (10.1%) |
| The block in which the engine falls asleep | 0.988 ms (18.5%) | 1.302 ms (24.4%) |
| `Performer::reset()`, both engines | 1.445 ms (27.1%) | 1.846 ms (34.6%) |
| Demo 02, block 3: the end of the preset switch's 20 ms crossfade | 14.3-15.4% | 20.2-20.9% |

(One D4, released, with Release at its default: the engine sleeps 13.70 s
after the key-up, 0.5 s after its output has fallen under −170 dBFS.)

The cause is `Loop::silence()`: it zeroes the whole `line` and `pending`
rings of all 52 loops, 16,384 floats each, 6.8 MB, and `sleep()` and
`reset()` also zero the thirteen 64 KB running-energy rings. The Performer
resets the engine it has faded out when a preset switch's crossfade ends,
which is demo 02's block 3 and its worst.

Aculyre has the fix for the same engine family (`91c8b86`, `Loop::reach`):
clear only the stretch of each ring that playing can have written, a bound
computed at `setInstrument` from every design's longest delay. It was not
ported in this round. Acukoto's bound has to cover four loops a string, the
113-point bend table, the hand's presses and the moved-ji keys, the
running-energy rings need the same treatment, and it wants a test that the
cleared engine is the freshly prepared one to the bit. It is the first CPU
item for the next round, and it needs no listening: the result is the same
bits.

## The sound

- **0.25.8.** Null test against 0.25.7 on both demos, the dense phrase and
  the chords, FMA3 build: the output moves by rounding only, 165 to 168 dB
  under the signal, peak −144.5 dBFS (one 24-bit step is −138.5). The
  portable and AVX2-only kernels are untouched, so the Reason stand-in and
  an AVX2-only CPU render 0.25.7's bits.
- **0.25.9.** Both demos, the dense phrase and the chords render the same
  bits as 0.25.8, in the portable and the FMA3 builds. `Acukoto.PluckShape`
  holds the shortcut to the full expression exactly over about 407 thousand
  shape values.
- **0.25.10.** The same four renders, the same bits as 0.25.9.
- **0.25.12.** Both demos, the dense phrase, the chords, a soft line over
  pressed and moved-ji keys and a phrase render the same bits as 0.25.11
  in the FMA3 build and in the Reason stand-in, where the library calls
  were.

Two commit messages misstate a figure and cannot be amended on a pushed
branch. `b14a5cc` says `Acukoto.PluckShape` holds 97,686 values; it holds
about 407 thousand. `2ce02f1` quotes 9343 instructions per sample "before"
for the modal step and radiation on demo 01 and gives no "after"; the
whole-engine counts in the table above are the measure to use.

## On arm64

Not measured in this round: this machine is x86-64 and no U45 was built.
The only Apple Silicon figure is 0.25.4's, 8.97% of one core at 1× on an
M1 Max (native Apple Clang `-O3`, the median 64-frame callback averaged
over four cases, [CPU and hand contact](../validation/cpu-and-hand-contact-2026-11-08.md)).
0.25.7 and 0.25.8 are for x86-64 with FMA3 only, and this round left the
portable kernels' cost where it was (+0.3% in the stand-in), so that
figure has not been improved on. What the source implies:

- The portable kernels are the ones that run. Their vectors are the
  compiler's 128-bit vector types, which clang lowers to NEON as it lowers
  them to SSE.
- arm64 always has a fused multiply-add, and clang contracts a product and
  a sum written in one expression by default. The drive, the pull and the
  strings' filters are written that way, so they should run as fused
  instructions: fewer instructions a term than SSE3's, and an output that
  differs from the x86-64 portable build's by rounding, as the FMA3
  kernels' does (135 dB or more under the signal there). A session rendered
  on an Intel and on an Apple Silicon machine would then not be the same to
  the bit; neither is it today between an FMA3 CPU and an older one.
- `pullBlock` is already 9 coordinates on arm64 against 6 on x86-64, for
  its 32 vector registers.

The owner's Apple Silicon machine, with the Rack device built there, is the
place to take the figures that matter; the stand-in table above is the
x86-64 half only.

## Tried or weighed, and dropped

- **Shorter delay rings** (`delayCapacity` 1024 for the keys that fit): no
  gain while playing. The cost of their size is the clearing above.
- **Skipping small participations** in the 26 × 356 drive and pull: every
  one of the 26 coordinates is significant for most modes, so there is no
  sparsity to use without changing the sound.
- **Float mode states or float radiation sums**: not neutral. Below 4 kHz
  the residual rises above the 120 dB line.
- **A mixed bank**, double below 4 kHz and float above: under 1%.
- **One pass through the portable kernel**, as 0.25.8 did for the FMA3 one:
  about 2% at best, and the portable kernel has to stay the AVX2 kernel's
  reference to the bit, so it was left.
- **Folding the junction's products into a second pull table, four-lane
  strings, an AVX-512 tier**: weighed and not built. The last two cannot
  run in Reason's build, and the first was not measured.
- What [the body's mode bank](cpu-bank-2026-11-03.md),
  [register blocking](cpu-blocking-2026-11-05.md) and
  [fused multiply-adds](cpu-fma-2026-11-09.md) already tried was not
  repeated.

## What is left, and what it would cost in sound

Nothing neutral remains that pays in Reason's build except the bounded
clears. The one audible lever that does is the number of wall modes: the
body's share scales with the mode count. Three reductions were rendered as
a listening set for the owner (`wall-modes`, in the project files, with its
key); the branch carries none of them.

| Letter | Wall modes | Body modes | Stand-in instructions (estimate) | Largest third-octave change after level matching |
| --- | --- | ---: | ---: | --- |
| A, shipping | all below 3 kHz, 92 runs above | 356 | 34,940 | |
| B | all below 2.2 kHz, 107 runs above | 291 | −12.4% | −3.8 dB at 2.5 kHz |
| C | all below 1.2 kHz, 126 runs above | 247 | −20.7% | −5.0 dB at 1.26 kHz |
| D | all below 3 kHz, 46 runs above | 309 | −8.9% | −4.5 dB at 6.3 kHz |

None is transparent by measurement. At a quality factor near 60 a wall mode
is 1/41 octave wide and the shipping runs just tile the band; wider runs
leave gaps the strings' partials fall into.
