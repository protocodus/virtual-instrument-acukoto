# Acukoto consolidation audit — 2026-11-09

Acukoto 0.25.7 (`6a76701`) to 0.25.12 (`c06a415`). A pass to finish what
exists before a release: read the model end to end, survey every key, fix
what a measurement or a derivation settles, and take the CPU that costs
nothing in sound. The CPU evidence is in
[CPU: consolidation](../benchmarks/cpu-consolidation-2026-11-09.md).

No recording was fetched or used in this pass. Every figure here is the
engine measured against itself.

## What was read and checked

- The README's How it works and Known gaps against the engine, the recent
  entries of `Docs/decisions.md`, the four benchmark documents and the
  validation reports of November.
- `Source/DSP/AcukotoEngine.cpp` and `AcukotoPerformer.cpp` end to end:
  the pluck's force and release laws, the loops' losses and dispersion, the
  ji junction and its passivity bound, the body's mode bank and its runs
  above 3 kHz, radiation, the room, the output stage's place, reset, sleep
  and the preset crossfade, every conversion to an integer, every library
  call on the playing path, and every state that can decay toward zero.
- The code shared with the siblings: the string loss sections and Thiran
  design copied from Acustra 1.9.0, and the loop and Performer shared with
  Aculyre, both ways (below).
- Every key from D3 to C8 on both presets at velocities 0.3, 0.7 and 1.0,
  the body-strike keys for level, and a soft line through the Performer.

## Findings

| # | Finding | Outcome |
| --- | --- | --- |
| 1 | At soft velocities the keys above F6 fell quiet: 7.6 and 8.4 dB between the loudest and quietest key at velocity 0.3. The pluck's release time followed the velocity but not the key's touch, which is force too. | Fixed at the cause, 0.25.11 (`93aae28`): 3.4 and 3.4 dB. Audible; listening set `soft-notes`. |
| 2 | Every note-on raised the CPU's underflow and denormal-operand flags (the pluck shape's far cells), and the first soft note after a sleep stored a subnormal in a string's running-energy ring. Reason is not known to set flush-to-zero. | Fixed, 0.25.9 (`b14a5cc`) and 0.25.10 (`f98397e`), the same bits. `Acukoto.Subnormals` is the contract. |
| 3 | In Reason's build (`-fno-builtin`) a note-on made about 2600 `erfc` and `exp` calls for a low string, and `lround` and `floor` ran on the playing path. | Fixed, 0.25.9 and 0.25.12 (`c06a415`), the same bits. |
| 4 | The blocks in which an engine clears its strings cost 18 to 35% of a core: falling asleep, `Performer::reset()` and the end of a preset switch's crossfade. `Loop::silence()` zeroes 6.8 MB. | **Not fixed.** Aculyre's bounded clears (`91c8b86`) are the fix to port; see the CPU document. |
| 5 | The FMA3 kernel stepped the modes and then read every velocity back for the radiation sums. | 0.25.8 (`2ce02f1`): one pass, 8.5% fewer instructions, rounding only (165 dB under the signal). |
| 6 | Comments still named the lyre's yoke for the koto's fixed ends, and the engine header called itself a proof of concept. | Fixed (`c15e276`), no behaviour change. |
| 7 | README figures that had drifted: the strings' own spread with and without touches, the CPU gap (34-38% of a core, from 0.15.x), the key touches' spread by velocity. | Rewritten from this pass's measurements. |
| 8 | After 0.25.11, D3's own string stands 0.43 dB over the level its touch is meant to give it (`Acukoto.Levels` allows 0.5): the strings' touches in `AcukotoPresets.h` were fitted before the release followed the touch. The key touches were regenerated; the strings' were not. | **Left.** Refit them (`AcukotoLevels` prints the touches) once the owner has accepted 0.25.11 by ear. |
| 9 | In a soft line, a note's level strays from its key's surveyed level by more than the keys differ from each other (below). It is the hand's scatter, by design since 0.7.0. | **Left for the owner's ear.** Nothing is wrong by measurement. |
| 10 | Pressed C5 on Koto Hirajoshi (B♭4's string, pressed two semitones) sounds 8.9 cents flat by its spectral peak, its fundamental decays at 41 dB/s where its neighbours' decay at 19 to 30, and its attack is 0.7 octave duller than theirs; C♯5 beside it decays as fast. Koto Hankumoijoshi's own C5 string does not do it. | **Not investigated.** The same in 0.25.7. It looks like E♭5 on its body modes (Known gaps), on a pressed string. |
| 11 | `Tools/RenderPresetDemos.cpp` quotes a 0.455× realtime median of 2026-10-29 beside a budget of one whole core. | **Left**: the CPU ratchet belongs to the work that brings U12 to Acukoto. |
| 12 | `ACUKOTO_ENABLE_SANITIZERS` does not include `float-cast-overflow`, and the engine converts doubles to `int` in many places (`floorInt` and `nearestInt` now among them) on the understanding that the value fits. | **Left**: a CI change. The fuzz and range tests pass. |
| 13 | Which kernels run in Reason on an AVX2 CPU is not known. The Rack adapter does not define `ACUKOTO_DISABLE_AVX2`, and `CpuFeatures.h` was written to ask the CPU from inside the Rack toolchain, so the AVX2 and FMA3 kernels may run there; the CPU document plans by a build without them. | **Left** for the next U45 build: it decides whether Reason on Intel costs 8-10% or 10-11% of a core. |

Looked at and found sound: the junction's passivity bound (the loop gain is
capped at 0.9999, 3 parts in 10,000 above the largest any key takes, C8 at
44.1 kHz); the loop's losses and Thiran design against Acustra 1.9.0's
(identical but for Acukoto's own viscous term) and the bending-loss
section (identical); the hardest chords at the output (all thirteen
strings at full velocity reconstruct to −1.37 and −1.41 dBFS at 8×
against the stage's −1.5 dBFS sample ceiling, an overshoot of 0.09 to
0.13 dB at 1× and 2×, so Aculyre's 0.6 dB reserve is not needed here); a
re-pluck's scaling of the running-energy ring, where Aculyre had found
subnormals (a sounding string's squares are far above that range; a
string plucked in the first samples of its sympathetic motion could leave
a square or two there, each read once, which is no sustained cost, and no
test raises a flag).

## The key-range survey

`AcukotoKeyboardSurvey` renders every key alone for 2 s and
`Tools/measure_keyboard_survey.py` measures it: K-weighted level, the
attack's spectral centroid, the fundamental's decay, the spectral peak
against equal temperament and the second partial against the first. Keys
38 to 108 on both presets at velocities 0.3, 0.7 and 1.0, 0.25.7 and
0.25.12. The engine is reset before each key, so the hand's scatter is the
same draw for every key and does not show.

Level, loudest key over quietest, D3 to C8 (59 keys):

| | velocity 0.3 | 0.7 | 1.0 |
| --- | ---: | ---: | ---: |
| Koto Hirajoshi, 0.25.7 | 7.6 dB (sd 1.76) | 2.7 (0.60) | 2.4 (0.51) |
| Koto Hirajoshi, 0.25.12 | 3.4 dB (sd 0.91) | 2.6 (0.56) | 2.8 (0.63) |
| Koto Hankumoijoshi, 0.25.7 | 8.4 dB (sd 1.71) | 2.8 (0.57) | 2.4 (0.48) |
| Koto Hankumoijoshi, 0.25.12 | 3.4 dB (sd 0.77) | 2.6 (0.53) | 3.0 (0.61) |

`AcukotoLevels --velocity` gives the same: every key within 3.38 and
3.44 dB at 0.3, 2.60 and 2.58 at 0.7, 2.83 and 2.99 at 1.0. The thirteen
strings' own notes stand 0.85 and 0.86 dB apart at 0.7 (4.8 and 5.9
without their touches), 1.8 and 2.1 at 0.3 and 1.0 and 0.9 at 1.0. The
body strikes stand 3.8 dB apart at 0.7 (3.6 intended), 8.3 at 0.3 and 2.9
to 3.0 at 1.0.

In 0.25.7, two keys stood more than 2 dB from the median of their six
neighbours (F7 +2.1 dB and B7 −2.6 dB, Hankumoijoshi at 0.3). In 0.25.12
none does at any velocity on either preset.

The other measures, which the pass did not move at 0.7 and 1.0:

- **Tuning.** The spectral peak sits within 6 cents of equal temperament
  on every key but these: E♭4 −13.5 and E♭5 −19.6 (tuned 11.3 cents flat
  as Ando's players tune them; E♭5 a further 8 on its body modes, a Known
  gap), B♭4 −6.0, A♭4 −10.0 on Hankumoijoshi (its retuned string, tuned
  flat by the same method), F♯5 −6.1 and Hirajoshi's pressed C5 −8.9
  (finding 10). The instrument does the first four; the last two were not
  traced.
- **Decay.** The fundamental's decay rises smoothly from 8 dB/s at D3 to
  about 100 at C8. Off the trend by more than an octave: F♯3 and G3, 26
  and 24 dB/s, on the 195 Hz twist (Known gaps, "The low fundamentals fade
  too soon"), which the body does, and D4, 3.9 dB/s, 5 Hz under the
  shell's 299 Hz mode, which was not traced further.
- **Attack brightness.** At 0.7, four keys on Hirajoshi (C♯4, C5, C♯5, C6)
  and ten on Hankumoijoshi (B♭3, B♭4, B4, C5, C♯5, C6, E6, C7, F♯7, B7)
  sit half an octave or more from their neighbours' centroid, as nearly
  all of them did in 0.25.7. C♯5 and C6 are keys whose second partial is
  15 and 8 dB under the trend, a partial fallen between body modes (Known
  gaps, "Single notes vary from the recordings"). The others were not
  traced one by one.

What 0.25.11 moved, key by key: at 0.7 no level by more than 0.15 dB and
no attack centroid by more than 0.13 octave (the strings' own notes 0.03);
at 0.3 levels by −1.5 to +5.1 dB and centroids by −0.33 to +0.27 octave;
at 1.0 levels by −0.6 to +0.2 dB.

## Why the soft notes fell quiet

A pluck's force is `pluckForce × touch × velocity^1.4`, and the tsume
slips off faster the harder it plucks, as harpists' strings do
(Chadefaux): the release time went as `velocity^−1.68`, an exponent chosen
by ear on 2026-10-28. The touch was left out of it. A key's touch is how much
harder that key is plucked so that it sounds as loud as the strings; the
moved-ji keys above F6 carry touches of 1.2 to 2.2. Plucked at velocity
0.3 they got a soft pluck's slow release, a one-pole near 2.2 kHz, at a
fundamental of 1.4 to 4.2 kHz, and lost it.

The proof that this is the whole cause: with the release held at its
velocity-0.7 value, the keys at 0.3 stand exactly as far apart as at 0.7
(2.74 dB), 10.3 dB lower each.

The fix has one defensible form. After the tsume slips, the string leaves
at a velocity of force over twice its wave impedance, whatever its length,
so the release follows the force, and the touch is force. 0.25.11
multiplies the release by `touch^−1.2`, the same exponent on the same
quantity the velocity already had (1.68 = 1.4 × 1.2), inside the same
clamp. No new constant. `Acukoto.Engine` holds `release × force^1.2` equal
across a key with a small touch, a string's own key and a key with a large
one; without the fix their logarithms differ by 1.38.

At full velocity the release is already at the fast end of the clamp for
every key with a touch of 1 or more, so there only the small-touch keys
change, which is why the 1.0 column widened by 0.4 to 0.6 dB. That is the
price of the 4 to 5 dB at 0.3, and it is why 0.25.11 is its own commit
with a listening set.

Since a key's level is now linear in its touch only to about 0.6 dB,
`AcukotoLevels --write-keys` is run until `--check` holds: four runs here,
the last moving one entry by 0.001.

## Notes in a phrase

A soft line through the Performer (velocity 0.3, pressed and moved-ji keys
among the strings' own) was measured note by note against the survey.
Each note's attack level differs from its key's surveyed level by −3.4 to
+5.4 dB, a standard deviation of 2.1 dB. The keys themselves differ by a
standard deviation of 0.8 to 0.9 dB at that velocity.

The cause is the hand, not the keys: each pluck draws its own force (14%,
about 1.2 dB), place (8 mm of 45, about 1.5 dB) and angle (4°, about
1 dB), spreads taken from Chadefaux's harpists and an estimate for the
place. It has been so since 0.7.0 and nothing measured here says it is
wrong. Whether a line played softly should be as uneven as that is a
question for the ear, and the levelling of the keys cannot be heard under
it either way.

## Known gaps rechecked

Each gap in the README was read against the engine and, where this pass
measured the same thing, against the survey.

- **Changed.** "The keys above the strings are no koto's": the key
  touches' spread by velocity is now 3.4 dB at 0.3, 2.6 at 0.7 and 2.8 to
  3.0 at 1.0 (was 8.6-9.2, 2.8-2.9 and 2.3-2.4). "It is heavy on the CPU":
  rewritten with this pass's figures and the state-clearing blocks. "The
  hand on the body": the strikes stand 8.3 dB apart at 0.3 (the README
  said 8.1).
- **Still true as written**, by the survey: E♭5 on its body modes (−19.6
  cents by spectral peak, 39 dB/s); the low fundamentals fading too soon
  (G3 24 dB/s); single notes varying (second partial up to 21 dB off the
  trend); the upper keys' second partial far under the fundamental.
- **Not re-measured**: the gaps that compare with recordings (the attacks'
  fall-off, the silk's loss and stiffness, D3's thinness, the tuning
  against the recordings), the energy budget, the floor, the body's
  weight, ato-oshi's sympathetic strings and the techniques. Nothing in
  this pass touches what they describe, and none is cheap to close: each
  needs recordings, a new mechanism or a measurement of a koto.

## Shared code

Diffed both ways against Aculyre (`Source/DSP` at its branch tip) and
against the block copied from Acustra 1.9.0.

- The 311 lines of loss sections and Thiran design are Aculyre's to the
  character except Acukoto's viscous term; `bendingLossSection` is
  identical. Nothing to take or give there.
- Taken from the siblings: the idea of exact inline floors and roundings
  (Aculele `d6c184b`, Aculyre `61889b1`), made exact at the halves here.
- Examined and not applicable: Aculyre `f1adc6a` (a clamp of two laws
  Acukoto does not have), `4b067cb` (contact taps: `Loop::setHand` and
  `Acukoto.HandContact` already do it), `e793ef5` (output reserve, above),
  `097f01c` (a table Acukoto does not use).
- Not taken, and wanted: Aculyre `91c8b86`, the bounded ring clears
  (finding 4).
- Offered to Aculyre: the release following the touch (`93aae28`), the
  pluck shape's far-field shortcut (`b14a5cc`), the one-pass FMA3 step
  (`2ce02f1`), the subnormal guard on the energy ring with its sticky-flag
  test (`f98397e`), and the exact `nearestInt` (`c06a415`).

## Controls and stale state

Acukoto has no `ParameterReach` test yet, and this pass did not write one.
Read from `AcukotoEngine::setControls` and the places each control is
used, this is how each would have to be declared:

| Control | Acts on | Declaration |
| --- | --- | --- |
| `technique`, `pluckDistance`, `normalShare` | the next pluck (read in `pluck`) | `NewNotes`: they describe the pluck itself |
| `oshide`, `legato` | which string a key plucks or presses (read in `noteOn`) | `NewNotes`, or `Exempt` as routing |
| `release` | strings whose key is up (marks every string's damping dirty) | reaches sounding strings, but a held note does not show it: the test has to let the key go |
| `sympathy` | strings nobody has plucked | reaches at once; heard through the sympathetic strings |
| `mute`, `pitchBend`, `handPress` | sounding strings | reach at once (`handPress` on the last plucked string) |
| `room`, `width`, `volume` | the output, over a glide | reach at once |

A preset switch crossfades two engines over 20 ms and resets the one that
fades out. A keyswitch latched on one preset is not carried to the other
(a Known gap).

## Tests

Added in this pass, each failing without its fix:

- `Acukoto.Subnormals` (unit): 49 s of playing with flush-to-zero and
  denormals-are-zero off leaves MXCSR's denormal-operand and underflow
  flags clear. Fails on 0.25.8 at every note-on and on 0.25.9 at the soft
  note after a sleep.
- `Acukoto.PluckShape`, a new block: the far-field shortcut equals the
  full expression exactly, about 407 thousand shape values.
- `Acukoto.Engine`, a new block: `release × force^1.2` is equal across a
  light, an own and a heavy key. Fails on 0.25.10 with 1.38 and 0.51.
- `Acukoto.LoopPair`, a new block: `floorInt` and `nearestInt` against
  `std::floor` and `std::lround`. An `int(x + 0.5)` rounding fails it at
  0.5 − 2^−54.
- 0.25.8's one-pass FMA3 kernel added no test: `Acukoto.ModalRadiation`
  already holds every fused kernel's output to the portable bank's, and
  the null test is its evidence.

At `c15e276` (0.25.12's code with the comments corrected), GCC 13.3
Release, on this FMA3 CPU: the unit tier passes, 14 of 14; the audio tier
passes, 2 of 2 (`Acukoto.AudioDemos`, `Acukoto.Levels`); the perf tier
passes, 1 of 1 (`Acukoto.CpuBudget`, 0.095× and 0.087× realtime against
its budget of 1×); every target builds without a warning, `AcukotoDSPCxx17`
among them. The fuzz tier's `Acukoto.DspFuzz` needs a Clang fuzzer build
and was not run; `Acukoto.DspFuzzReplay` is in the unit tier.

## Not done, in order

1. Bounded ring clears (finding 4): the largest blocks the instrument has.
2. The strings' own touches refitted to 0.25.11 (finding 8), after the
   owner's verdict on `soft-notes`.
3. Pressed C5, C♯5 and F♯5 on Hirajoshi traced to their body modes or to
   the press (finding 10), and the attack-centroid outliers one by one.
4. The figures on arm64, and whether the AVX2 kernels reach Reason at all
   (finding 13).
5. A `ParameterReach` test and the 10% ratchet, with the declarations
   above.
6. Fewer wall modes, if the owner's ear allows it (`wall-modes`): the only
   reduction that pays in Reason's build.
