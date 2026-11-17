# Acukoto CPU: the kernels' last instructions — 2026-11-16

Acukoto 0.25.20 against 0.25.19 (`c6ed97b`), at the default settings:
48 kHz, 1× oversampling, played through the Performer. The brief was the
[CPU ceiling plan](../plans/cpu-ceiling-plan-2026-11-16.md)'s step 2: the
exact-output savings first, each null-tested against 0.25.19, then the
savings that change the sound, each only through an A–Z listening test.

The short of it: two changes shipped, both the same bits in every build
and at every rate tested. The native build takes 5.8% less time, mostly
because GCC 13 had been widening the FMA kernel's radiation weights through
the stack; the Reason stand-in takes 2.7% less. What would lower either
build much further is fewer body modes, which changes the sound: that went
to the owner as a listening test ([Fewer body modes](#fewer-body-modes)).

| Version | Change | Grade |
| --- | --- | --- |
| 0.25.20 | The FMA and AVX2 kernels widen four floats to doubles with one `vcvtps2pd` under GCC | the same bits |
| 0.25.20 | The drive of the modes unrolls its 26 coordinates, in the portable and FMA kernels | the same bits |

## The builds and the protocol

- **FMA3**: GCC 13.3 `-O3 -std=c++20 -DNDEBUG` (the CMake Release build);
  the engine picks its AVX2 and FMA3 kernels at run time.
- **Reason stand-in**: `clang++-18 -O2 -std=c++17 -DNDEBUG
  -DACUKOTO_DISABLE_AVX2 -fno-builtin -msse3`, the portable kernels, as in
  the [2026-11-09 round](cpu-consolidation-2026-11-09.md). It is not a U45.
- **Clang native**: `clang++-18 -O2 -std=c++17 -DNDEBUG -Wall -Wextra
  -Wpedantic -Werror`, the AVX2 and FMA3 kernels through Clang's own path.
- **aarch64**: `clang++-18 --target=aarch64-linux-gnu -fsyntax-only -Werror`
  only; no Apple silicon machine was available, so nothing on it was timed
  or rendered.

**The null test.** Each build of 0.25.19 and of the candidate renders, as
raw float32: both README demos and both stress phrases through the shared
demo renderer, and on each preset a scripted 14 s gesture (a chord, every
string at mixed velocities with oshide between, body strikes, the extension
keys, every latched technique, sways, glissandi and scrapes, live changes
of the press, bend, mute, room and width, the pedal, legato ato-oshi, a
fast repeated note and an instrument switch mid-ring) at 48 kHz in blocks
of 256, 44.1 kHz in 64, 96 kHz in 37 and 48 kHz at 2× in 128. Twelve files
a build.

**Time.** The perf tier's renderer (`RenderAcukotoPresetDemos --cpu
--repeats 3`) on both demos and both stress phrases, the builds alternated
round by round, the median real-time factor of each workload over the
rounds. The machine is a 2.1 GHz Xeon vCPU (four cores, shared with this
session's own builds at times): development-machine figures, never Reason
or DAW CPU. Instruction counts are callgrind's, which load does not
disturb: preset 0's demo and stress phrase, 850,500 samples, only
`Performer::process` collected.

## Where the time went

Callgrind's line profile of 0.25.19, in instructions per sample of
`AcukotoEngine.cpp` (the Performer and the output stage are not in it):

| Part | Stand-in | FMA3 |
| --- | ---: | ---: |
| Body: the modes' drive from the 26 bar coordinates | 7,289 (25%) | 1,822 (13%) |
| Body: the modes' pull on the coordinates | 6,661 (23%) | 1,946 (14%) |
| Body: the modes' step (FMA3: with the radiation) | 3,043 (10%) | 3,106 (22%) |
| Body: the four radiation sums | 2,602 (9%) | in the step |
| The ji junction's two 26 × 26 products | 2,294 (8%) | 811 (6%) |
| The 52 string loops and their loss sections | 5,124 (18%) | 4,366 (32%) |
| The per-sample loop and the rest | 2,221 (8%) | 1,806 (13%) |
| Total | 29,234 | 13,857 |

In the stand-in the drive and the pull run at the three instructions a
vector that SSE allows (a load, a multiply, an add); what is left in them is
the loops' own bookkeeping. In the FMA3 build the step's radiation sums
were 1,900 of the step's 3,106: GCC 13 compiles `__builtin_convertvector`
of four floats to four doubles as two halves through the stack (two
`vcvtps2pd xmm`, a spill and reload, `vinsertf128`), six instructions where
`vcvtps2pd ymm, m128` is one, and it did that for each of the four weight
tables in every group of four modes. The kernel already used the builtin for
the drive's conversion; the radiation weights and the pull's reductions did
not.

## What shipped

**One widening instruction (FMA3 and AVX2 kernels).** `widenAvx` is
`__builtin_ia32_cvtps2pd256` under GCC and `__builtin_convertvector` under
Clang, which already emitted the one instruction. Every float-to-double
widening in the AVX2 and FMA3 kernels goes through it: the radiation
weights, the drive, the pull's lane reductions. A widening is exact, so the
bits cannot move; the null test confirms it.

**The drive unrolled.** Each block of modes takes the 26 coordinates in
turn; the loop over them kept a count, a step and a jump per coordinate, an
eighth of the portable kernel's drive and a third of the FMA3 kernel's.
Unrolled, every load takes a fixed offset. The sums still add the
coordinates in order.

**The sound: the same bits.** All twelve files are identical to the bit,
0.25.19 against 0.25.20, in the FMA3 build, the stand-in and the Clang
native build (the last on the demos, the stress phrases and the 48 kHz
gesture).

**The cost.**

Instructions per sample (callgrind, preset 0's demo and stress phrase,
`Performer::process` and everything under it):

| Build | 0.25.19 | 0.25.20 | Change |
| --- | ---: | ---: | ---: |
| FMA3 | 14,192 | 12,183 | −14.2% |
| Reason stand-in | 30,159 | 28,953 | −4.0% |

Real-time factor, the median of five alternating rounds (load average
under 1.5, nothing else running), and the median of the rounds' worst
256-frame blocks in percent of one core:

| Workload | FMA3, 0.25.19 | FMA3, 0.25.20 | Stand-in, 0.25.19 | Stand-in, 0.25.20 |
| --- | --- | --- | --- | --- |
| 01 Koto Hirajoshi | 0.0787 / 8.6% | 0.0738 / 8.0% (−6.2%) | 0.0989 / 10.6% | 0.0977 / 10.2% (−1.2%) |
| 01, stress phrase | 0.0827 / 10.1% | 0.0774 / 9.8% (−6.4%) | 0.1003 / 11.8% | 0.0976 / 11.6% (−2.7%) |
| 02 Koto Hankumoijoshi | 0.0786 / 8.5% | 0.0742 / 8.2% (−5.6%) | 0.0973 / 10.5% | 0.0941 / 10.1% (−3.3%) |
| 02, stress phrase | 0.0836 / 9.8% | 0.0794 / 9.3% (−5.0%) | 0.1012 / 11.5% | 0.0977 / 11.0% (−3.5%) |
| All four | | −5.8% | | −2.7% |

The FMA3 build's 14% of instructions is 6% of its time: the instructions
removed were moves and conversions beside the multiply-adds, which the core
overlapped with them. The CI runner was not used; by these ratios its worst
blocks would stand near 13.5-15.3%, still over U12's 10%. `Acukoto.CpuBudget`
passes (it warns only).

## Tried and dropped

- **Smaller string rings** (1,024 and 2,048 slots for the strings' lines,
  where the delay capacity is 16,384): the same bits, but 8% slower in the
  stand-in, not faster. The working memory shrinks, yet the time is set by
  instructions here (about 4 to 5 a cycle), not by misses.
- **The pull's reductions four coordinates at a time** (`vhaddpd` and two
  permutes for four coordinates, the same additions in the same order): the
  same bits and 370 instructions a sample fewer in the FMA3 build, but 0.4%
  on the clock, inside the rounds' scatter. Not worth the code.
- **Two-float loads for the stand-in's radiation weights** (`cvtps2pd`
  from memory for every pair, where Clang shuffled every second one): the
  same bits, 0.1% on the clock.
- **The junction's products in float**: weighed again and left, for the
  reasons the [2026-11-10 pass](cpu-consolidation-2026-11-10.md#looked-at-and-not-shipped)
  gives; in the FMA3 build it is now 0.4% of the time.

The largest exact saving left is in the strings: a pair of loops keeps its
two planes' state in two `Loop` objects 128 KB apart, so every paired filter
loads two scalars and packs them, and unpacks them to store, about a third
of the strings' instructions in the FMA3 build. Keeping each pair's state
side by side would remove that, but it moves every field of a structure
whose size and placement are known to matter (eight bytes cost 17% in the
2026-11-09 round); it needs its own pass and a quiet machine.

## Fewer body modes

What lowers the body's cost is fewer modes, and fewer modes are a different
sound. Three bodies were rendered with a lower `maxWallModes`, every wall
mode still kept to 2.2 kHz and fewer, longer runs above it:

| Cap | Runs above 2.2 kHz | Body modes | FMA3 time | Stand-in time |
| --- | ---: | ---: | ---: | ---: |
| 210 (0.25.19) | 107 | 291 | | |
| 185 | 87 | 268 | 7.0% less | 3.8% less |
| 160 | 62 | 244 | 6.7% less | 7.9% less |
| 130 | 32 | 214 | 15.0% less | 14.5% less |

(Three rounds on a shared machine, against the bit-exact candidate; the
185 and 160 rows overlap within the rounds' scatter.) None is the same
sound: the difference from 0.25.19 stands 7 to 12 dB under the signal. The
185 and 160 bodies went to the owner as an A–Z set, whose key holds their
third octaves; 130 was left out as not barely audible, its third octaves
from 5 to 8 kHz 8 to 10 dB under 0.25.19's in every clip. The verdict, when it comes, goes in
[`Docs/decisions.md`](../decisions.md).
