# Acukoto CPU: consolidation, second pass — 2026-11-10

Acukoto 0.25.15 and 0.25.16 against 0.25.13 (`0504830`), at the default settings:
48 kHz, 1× oversampling, played through the Performer. The brief was CPU
with no audible loss: a change ships only if the output is the same to the
bit, differs in rounding alone, or leaves a residual at least 90 dB under
the signal with no peak above −80 dBFS on both demos and the stress
phrases. This follows [the 2026-11-09 round](cpu-consolidation-2026-11-09.md),
whose protocol and build definitions it keeps.

The short of it: one change shipped on those grades, the same bits, and it
removes memory traffic from note-ons, not instructions from ordinary
playing. The body's kernels, two thirds of the cost in the build that
stands in for Reason's, are at the floor of what their arithmetic allows;
what lowers them is fewer modes, which is a change to the sound. The owner
chose that by ear, and 0.25.16 takes 12.1% of the stand-in's instructions
with it ([Fewer wall modes](#fewer-wall-modes)).

| Version | Change | Grade |
| --- | --- | --- |
| 0.25.15 | A pluck on a sounding string scales or clears only the stretch of its energy ring that can be read | the same bits |
| 0.25.16 | The body keeps every wall mode to 2.2 kHz, not 3 kHz: 291 modes for 356 | audible; the owner's choice by ear, none of this document's grades |

Six sessions shared the machine's four cores during this pass (load
average 9 to 28), so nothing here was accepted or rejected on time.
Instruction counts are callgrind's, which load does not disturb.

## The builds

- **FMA3**: GCC 13.3 `-O3 -std=c++20 -DNDEBUG`; the engine picks its AVX2
  and FMA3 kernels at run time.
- **Reason stand-in**: `clang++-18 -O2 -std=c++17 -DNDEBUG
  -DACUKOTO_DISABLE_AVX2 -fno-builtin -msse3`, the portable kernels. It is
  not a U45; see the 2026-11-09 document for what it does and does not
  stand for. Which kernels Reason runs on an AVX2 CPU is still not known.

Counts are of 1.5 s (72,000 samples) on preset 0, only the processing loop
collected. "Dense" is thirteen held strings, one re-plucked every 128 ms;
"chords" is every string at full velocity every 400 ms with the pedal down.

## A re-pluck keeps to the round trip (0.25.15)

Each string keeps a ring of its squared velocity, whose sum over one round
trip is the string's energy. A pluck on a string that is still sounding
rescales that ring by what the contact leaves of the energy (the tsume) or
clears it (a bare finger). Until 0.25.15 it did so over the whole ring,
16,384 floats, 64 KB a string and 832 KB for a thirteen-string chord, more
than this CPU's first-level cache for one string and a good part of its
second for a chord. Nothing reads the ring further than the string's
longest round trip behind its cursor (the bound 0.25.13 derived for the
clears, about 120 to 690 samples at 48 kHz), and the pluck sums that stretch
again before it returns. `scaleSquares()` now keeps to it, as
`clearSquares()` has since 0.25.13.

**The sound: the same bits.** Both demos (12 s each), the dense phrase and
the chords (6 s each, both presets), FMA3 build and stand-in, 0.25.13
against the candidate: all twelve pairs are identical to the bit.

`Acukoto.BoundedClears` holds it: thirteen sounding strings are plucked
again, with the tsume and with a bare finger, on preset 0 at 48 kHz and
preset 1 at 96 kHz; an engine whose energy rings hold a mark everywhere
beyond the round trip must leave every mark as it was, play the same bits
as one whose rings are zero there and hold the same energy sums. It fails
on 0.25.14's engine ("changed its energy ring beyond the round trip": at
48 kHz 208,894 slots).

Instructions, 72,000 samples:

| Build | Phrase | 0.25.13 | 0.25.15 | Change |
| --- | --- | ---: | ---: | ---: |
| Reason stand-in | dense | 2,522,996,097 | 2,522,683,612 | −312,485 (−0.012%) |
| | chords | 2,512,782,521 | 2,512,052,627 | −729,894 (−0.029%) |
| FMA3 | dense | 1,004,027,604 | 1,003,570,542 | −457,062 (−0.046%) |
| | chords | 1,004,742,648 | 1,003,670,685 | −1,071,963 (−0.107%) |

In the stand-in that is 19,000 to 31,000 instructions for each pluck on a
sounding string, about half to one sample's work, in the note-on call
(GCC's build counts more for the same fill, as the 2026-11-09 document
explains). It
does not move the mean. What it takes away is the 64 KB a string that each
such pluck read and wrote in the middle of a block. Its effect on a block's
time was not measured: the machine was too loaded to time a block.

## Where the stand-in's instructions go

Callgrind's line profile of the stand-in's dense phrase at 0.25.13,
35,042 instructions per sample:

| Part | Instructions per sample | Share |
| --- | ---: | ---: |
| Body: the 26 bar coordinates' drive of the 356 modes (float) | 8,930 | 25% |
| Body: the modes' pull back on the coordinates (float) | 8,600 | 25% |
| Body: the modes' recursion (double) | 3,200 | 9% |
| Body: radiation to four microphone sums (double) | 2,900 | 8% |
| The ji junction's two 26 × 26 products (double) | 2,300 | 7% |
| The 52 string loops | 3,710 | 11% |
| The rest: the per-sample loop around them, the room, the output stage | 5,400 | 15% |

The two float products are 26 × 356 terms each, four to an SSE vector, and
their inner loops run three instructions to a vector's multiply and add:
a load, a multiply, an add. Nothing is left in them to take without a
fused multiply-add or a wider vector, which is what the FMA3 kernels are.
The strings' loops are short (one dispersion stage and at most two pairs a
loop); 71 instructions a loop a sample.

## Looked at and not shipped

Each of these would lower the stand-in's count. None is a change of the
grades this pass may ship without a measurement it could not make, and
none was built.

| Candidate | Estimated saving, stand-in | Why not |
| --- | ---: | --- |
| The radiation sums in float | about 1,400 a sample (4%) | The far-field pressure is the sums' bilinear derivative, whose pole at z = −1 is cancelled by each mode's own zero at Nyquist. A float sum's rounding noise has no such zero, and the pole integrates it as a random walk: it fails over long playing. Not a rounding-grade change. |
| The modes' recursion in float | about 1,600 (5%) | The transposed direct form's roundoff gain is about 10⁵ at the 85 Hz air mode (Q 20 at 48 kHz); in float that is noise 40 dB under the mode, not rounding. |
| One of the junction's two 26 × 26 products folded into a second pull table, P′ = S P | about 1,150 (3.3%) | It adds a second 37 KB table to a body whose tables and state already exceed this CPU's 48 KB first-level cache, so the instructions saved are not the time saved, and that needs a quiet machine. It also makes the table the modes pull with differ by rounding from the one they are driven with; today they are one table and the junction's reciprocity is exact. |
| The junction's two products in float | about 800 (2.3%), 0.4% in the FMA3 build | Rounding-grade on paper (`Acukoto.StrikeEnergy`'s endpoint error is 7 × 10⁻⁷ against its 2 × 10⁻⁵ allowance, and a float solve would add about as much again), but it changes the precision of the one equation every string and mode must share, for 2%. Left for a pass that can time it. |
| Rings sized to each loop's reach | none in instructions | The 52 loops' lines and pending rings are 64 KB each, 6.8 MB that the cursors cycle through every 0.34 s. Rings of each loop's own reach would be about 0.4 MB and the same bits. But a loop's size and place are known to matter beyond what instruction counts show (eight bytes more in `Loop` cost 17% in the 2026-11-09 round), so this is a change to time on a quiet machine, not to ship on a count. |

The last one is the candidate most worth a quiet machine: it is exact, and
it would take the engine's working memory from beyond a second-level cache
to well inside one.

## Timings

None were taken that are fit to plan by: with six sessions on four cores
the same binary's block times moved by more than any change here. The
2026-11-09 figures stand for 0.25.15's ordinary playing, which runs the
same instructions to 0.03%: a demo averages 8.1 to 9.5% of one 2.1 GHz Xeon
vCPU with the FMA3 kernels and 10.1 to 10.5% in the Reason stand-in.
Development-machine figures, never Reason or DAW CPU.

What the loaded machine gave, for the record and no more: thread CPU time
of each 256-frame block, the least of five takes, mean / 99th percentile /
worst in percent of one core, each binary run once, one after another, at
a load average of 15 to 28:

| Demo | 0.25.13, FMA3 | 0.25.16, FMA3 | 0.25.13, stand-in | 0.25.16, stand-in |
| --- | --- | --- | --- | --- |
| 01, Koto Hirajoshi | 8.58 / 10.30 / 10.70 | 8.51 / 10.41 / 10.88 | 10.42 / 12.94 / 13.68 | 9.91 / 11.88 / 12.58 |
| 02, Koto Hankumoijoshi | 10.10 / 12.27 / 12.78 | 7.69 / 9.36 / 9.89 | 10.10 / 11.09 / 11.86 | 9.17 / 11.13 / 12.68 |

One demo's mean falls by 1% and the other's by 24% in the same build:
that spread is the machine, not the change.

## Fewer wall modes

The saving the kernels leave is the body's mode count, and it is not one a
measurement can license. The 2026-11-09 listening set `wall-modes` thinned
the walls' modes three ways against the shipping body. Its B keeps every
wall mode up to 2.2 kHz where the shipping body keeps them to 3 kHz, with a
cap of 210 wall modes for 280: 98 modes below the limit and 107 runs above
it, 291 body modes for 356. Its difference from the shipping body is
6.1 dB under the signal in the set's phrase, with third octaves moving up
to 3.8 dB, so it is no change of this document's grades. The owner chose B
by ear on 2026-11-10 ([decisions](../decisions.md)) and 0.25.16 is that
body, with the strings', keys' and strikes' touches fitted to it. What it
costs in evenness is in the
[audit](../validation/consolidation-audit-2026-11-10.md#the-body-of-02516).

Instructions, 72,000 samples, and per sample:

| Build | Phrase | 0.25.15 | 0.25.16 | Change |
| --- | --- | ---: | ---: | ---: |
| Reason stand-in | dense | 2,522,683,612 (35,037) | 2,217,905,144 (30,804) | −12.08% |
| | chords | 2,512,052,627 (34,890) | 2,208,673,756 (30,676) | −12.08% |
| FMA3 | dense | 1,003,570,542 (13,938) | 934,351,438 (12,977) | −6.90% |
| | chords | 1,003,670,685 (13,940) | 934,161,984 (12,974) | −6.93% |

The listening set's key estimated 12.4% for the stand-in by scaling the
modal bank's share with the mode count; the count is 12.1%. The FMA3
build gains less because its body kernels were already a smaller share.

By the counts alone, the 2026-11-09 means (10.1 to 10.5% of a core for a
demo in the stand-in, 8.1 to 9.5% with the FMA3 kernels) would become
about 8.9 to 9.2% and 7.5 to 8.8%. That is arithmetic, not a timing: no
block of 0.25.16 was timed on a machine quiet enough to print, and whether
it stands under the 10% ceiling in Reason is still to be measured there.
