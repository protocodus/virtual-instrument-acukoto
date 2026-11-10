# Acukoto consolidation audit, second pass — 2026-11-10

Acukoto 0.25.13 (`0504830`) to 0.25.15, and 0.25.16's body
([below](#the-body-of-02516)). A fix-only pass over the model after
[the 2026-11-09 audit](consolidation-audit-2026-11-09.md): no new control,
mechanism or preset, and no re-voicing. The CPU work of the same day is in
[CPU: consolidation, second pass](../benchmarks/cpu-consolidation-2026-11-10.md).

No recording was fetched or used. Every figure here is the engine measured
against itself.

## What was read and checked

- `Source/DSP/AcukotoEngine.cpp`, `AcukotoEngine.h`, `AcukotoPerformer.cpp`
  and `AcukotoPresets.h` end to end again: units and conversions, what each
  reset, sleep and preset switch clears, what a re-pluck keeps, the control
  period's block-size independence, every path a parameter takes to a
  sounding string, the scheduling of chords and oshide, and the states that
  can decay toward zero.
- The 2026-11-09 audit's open findings 8, 10, 11, 12 and 13.
- `AcukotoLevels` on both presets at velocities 0.3, 0.7 and 1.0, every key
  from D3 to C8 and the twelve strikes.
- Every open string's pitch over its first two seconds
  (the protocol is under [Strings on body modes](#strings-on-body-modes)).
- The README's How it works and Known gaps against the code.

## Findings

| # | Finding | Outcome |
| --- | --- | --- |
| 1 | The strings' own touches no longer fitted their rule (the 2026-11-09 audit's finding 8). 0.25.11 made a pluck's release follow its touch, which moved every string's level; the key touches were regenerated then, the strings' hand-written touches were not. At velocity 0.7 Koto Hirajoshi's D3 stood 0.43 dB over its intended level and Koto Hankumoijoshi's E♭5 0.50 under, with six more strings 0.2 to 0.3 dB off. | **Fixed, 0.25.14.** Both sets refitted together until every string and key stands within 0.01 dB of its intended level. `Acukoto.Levels` now holds the strings to 0.1 dB; it fails on 0.25.13's touches (0.43 and 0.50 dB). |
| 2 | Pressed C5 on Koto Hirajoshi sounds 9 cents flat and its fundamental decays fast (finding 10 of 2026-11-09, not investigated then). | **Explained, not changed.** C5 lies on the box's 523.8 Hz mode (Q 113). It is the model's body, as E♭5 and G3 on theirs are; moving it is re-voicing. Measurements below; the Known gap now names all three. |
| 3 | The README said each instrument "draws its own scatter" about Ando's mean tuning. No scatter has been drawn since 0.24.0; the Known gap beside it already said so. | Fixed in the README. |
| 4 | The README gave ato-oshi's press "about 80 ms". The press moves over 0.35 s, the middle four fifths of the way in 0.18 s (`pressGlide`, a least-jerk movement since 0.19.0; 80 ms was the time constant of the glide before it). | Fixed in the README. |
| 5 | The Known gap on E♭5 and G3 quoted figures from before the box took paulownia's Q along the grain (0.25.6): G3's mode at Q 59, E♭5's at Q 53-56, and split pitches that no longer measure. | Rewritten from this pass's measurements. |
| 6 | `AcukotoPresets.h`'s comment gave the box's bending modes as 95 and 234 Hz, the section's first breathing mode as 293 Hz and the lower air mode as 86 Hz. The engine's are 94.4 and 231.3, 299.2 and 85.3 Hz. | Comment corrected. No code change. |
| 7 | `Tools/RenderPresetDemos.cpp` quoted a 0.455× realtime median of 2026-10-29 beside its budget (finding 11 of 2026-11-09). | Comment corrected to say what a demo takes now and that the budget is not re-set from a CI measurement. The budget itself is untouched. |
| 8 | A pluck on a sounding string scaled or zeroed all 64 KB of its running-energy ring, of which at most a round trip can be read. | 0.25.15, the same bits; it is a CPU change and is in the CPU document. |

Read again and found sound: the block-end refresh of each string's energy
and the sleep test that reads it; `reset()`, `sleep()` and the preset
crossfade's clear against every state added since 0.25.7; the schedule of a
chord's strokes (kakite) and of an oshide's press before its pluck; the
bend table and its interpolation (the pressed C5 reads the same 8.9 cents
flat when the press is made to land on a table point, so the table is not
its cause); the hand press's fluctuation and its low-pass at every
oversampling rate; the differentiator after the radiation sums and the
Nyquist zero that cancels its pole.

## The refit

The strings' intended level is the owner's 80% rule (README, Played with
tsume). Since 0.25.11 a touch moves the release as well as the force, so a
touch read off `AcukotoLevels` moves the level a little less than its dB
says and the fit must be iterated; and the key touches are fitted on the
strings' mean, so they move with the strings. Five rounds of "write the
printed string touches, rebuild, regenerate the key touches" settle both:

| Round | Strings' worst | Keys' worst |
| --- | ---: | ---: |
| 0 (0.25.13) | 0.50 dB | 0.01 dB |
| 1 | 0.03 | 0.45 |
| 2 | 0.01 | 0.07 |
| 3 | 0.01 | 0.02 |
| 4 | 0.01 | 0.01 |

What moved, at velocity 0.7 (level before, in dB from the intended level):

| String | Koto Hirajoshi | Koto Hankumoijoshi |
| --- | ---: | ---: |
| 1 (D3) | +0.43 | +0.30 |
| 5 (D4) | +0.28 | +0.10 |
| 6 (E♭4) | −0.24 | +0.19 |
| 9 (B♭4 / C5) | +0.29 | −0.10 |
| 11 (E♭5) | −0.04 | −0.50 |
| 13 (A5) | −0.25 | +0.10 |
| the other seven | within 0.19 | within 0.22 |

No string's level moves by more than 0.5 dB, so no listening set was
rendered: it is under what a level change of one note in a phrase can be
heard as, and the rule it restores is the owner's own.

After the refit, between the loudest and quietest key from D3 to C8:

| Velocity | Koto Hirajoshi | Koto Hankumoijoshi |
| --- | ---: | ---: |
| 0.3 | 3.24 dB | 3.58 dB |
| 0.7 | 2.49 | 2.59 |
| 1.0 | 2.72 | 3.00 |

and the strings' own notes stand 0.96 and 1.19 dB apart at 0.7 (4.82 and
5.94 without touches), which is what the 80% rule leaves.

## Strings on body modes

Protocol: each open string plucked alone at velocity 0.7 on the engine
(no Performer scatter, room off), 48 kHz. The fundamental's pitch is the
peak of a Hann-windowed spectrum scanned in 0.5-cent steps within 40 cents
of the string's designed pitch, in three windows after the pluck; a second
peak in the scan is given with its level under the first. Cents are from
the string's own tuning. The modes' frequencies, Qs and the ji's mobility
come from the engine's `modeReport()` and `bridgeMobilityMagnitude()`.

| Note | Body mode beside it | 0.08-0.5 s | 0.5-1 s | 1-2 s |
| --- | --- | --- | --- | --- |
| G3, string 2, 195.8 Hz | box, 195.3 Hz, Q 135 | −5.0 | −2.5 | +3.0 (and −17.0, 9 dB lower) |
| E♭5, string 11, 618.8 Hz, both presets | 610.6 Hz (Q 54) and 614.7 Hz (Q 94); the ji's mobility there 0.0093 m/s/N | −10.0 (and +4.0, 5 dB lower) | −4.5 | −0.5 (and −11.0, 6 dB lower) |
| C5, Hankumoijoshi's string 9, 522.3 Hz | box, 523.8 Hz, Q 113 | −0.5 (and +14.5, 5 dB lower) | +3.5 | 0.0 (and +11.0, 4 dB lower) |
| C5 pressed on Hirajoshi (B♭4's string, a whole tone), 523.25 Hz | the same mode | −8.8 (and +14.0, 7 dB lower) | −1.2 (and +11.2, 1 dB lower; −21, 2 dB lower) | +9.0 (and −21.5, 6 dB lower) |

Every other string stays within 2.5 cents of its tuning in all three
windows (the largest: B♭4 −2.5, E♭4 −2.0, D4 −1.5 then +1.5).

A string whose fundamental lies within a body mode's bandwidth couples to
it: the pair has two normal modes, a few cents either side, and the pluck
excites both, so the pitch read off a short window moves as their balance
changes and the fundamental beats as it decays. That is the 41 dB/s decay
with a 5.1 dB fit error the survey reads on the pressed C5. A real koto's
string does the same on its own body's modes; these are the model's modes,
which are Coaldrake's measured ones only for the first bending, twist and
air modes. Nothing here is a defect in the maths, and detuning a mode or a
string to hide it would be voicing.

## The body of 0.25.16

0.25.16 keeps every wall mode to 2.2 kHz where 0.25.15 keeps them to 3 kHz,
291 body modes for 356: the owner's choice by ear of the 2026-11-09
`wall-modes` set's B ([decisions](../decisions.md)), for the CPU it saves
([CPU document](../benchmarks/cpu-consolidation-2026-11-10.md#fewer-wall-modes)).
It is not a fix and not a measured equivalence. Everything above this
section was measured on 0.25.15's body; this is what was measured again on
0.25.16's.

**The modes the strings sit on did not move.** The body's modes under
2.2 kHz are the same to 0.1 Hz (523.8 Hz, Q 113; 610.7 and 614.8 Hz, Q 54
and 95; 195.3 Hz, Q 135; E♭5's ji's mobility at 614.7 Hz 0.0093 m/s/N), and
the table under [Strings on body modes](#strings-on-body-modes) reads the
same within 0.5 cents and 1 dB on every open string and on the pressed C5.

**The levels moved, note by note.** `AcukotoLevels`, velocity 0.7, before
any touch (a string's level less its touch in dB, which since 0.25.11 is
approximate):

| | 0.25.15 | 0.25.16 |
| --- | ---: | ---: |
| Strings' mean, Koto Hirajoshi / Koto Hankumoijoshi | −24.75 / −24.39 dB | −25.86 / −25.46 dB |
| Loudest string over quietest, without touches | 4.82 / 5.94 dB | 6.14 / 7.00 dB |
| The same with touches (the 80% rule) | 0.96 / 1.19 dB | 1.23 / 1.41 dB |
| Keys D3 to C8 at velocity 0.7 | 2.49 / 2.59 dB | 3.01 / 2.39 dB |
| at velocity 0.3 | 3.24 / 3.58 dB | 4.97 / 4.16 dB |
| at velocity 1.0 | 2.72 / 3.00 dB | 3.07 / 2.44 dB |

On 0.25.15's touches the new body put the strings up to 1.2 dB from their
intended levels (G4 +1.21, B♭4 −1.14, A5 −1.20 on Koto Hirajoshi) and the
keys up to 4.7 dB (F6 and F7 3.8 to 4.7 dB under, E7 4.0 under on Koto
Hirajoshi, C8 2.5 over on Koto Hankumoijoshi; before the refit Koto
Hirajoshi's keys stood 7.65 dB apart). Five rounds of the refit put every
string and key back within 0.01 dB at velocity 0.7. The touches that do it:

- The strings' touches run from 0.72 to 1.37 (0.76 to 1.31 before). The
  largest moves are B♭3 on Koto Hankumoijoshi, 1.09 to 1.37, and A5 on Koto
  Hirajoshi, 1.03 to 1.18; without touches B♭3 stands 2.2 and 3.6 dB lower
  than it did on the two presets, B♭4 2.5 and G5 2.0 to 2.3 lower.
- The key touches run from 0.57 to 2.28 (0.64 to 2.12). On Koto
  Hankumoijoshi F6's goes from 0.66 to 1.18 and F7's from 0.74 to 1.31, a
  pluck 5 dB harder; on Koto Hirajoshi F♯6's falls from 1.64 to 1.12.
  A harder pluck also leaves the tsume faster, so those keys change in
  attack as well as in what the body gives them.

**The body strikes kept their sound and lost their place.** Each strike's
level is within 0.04 dB of what it was, but its rule sets it by the strings'
mean, which fell 1.1 dB: on 0.25.15's touches the twelve stood 0.75 to
1.05 dB over their intended levels on Koto Hirajoshi and 0.43 to 0.73 over
on Koto Hankumoijoshi, and `Acukoto.Levels` failed. Their touches
(`AcukotoStrikes.h`, one table for both presets) are 7 to 10% lower after
four rounds: within 0.02 dB on Koto Hirajoshi, which the table follows as it
did, and within 0.35 dB on Koto Hankumoijoshi.

From 2.2 to 3 kHz the modes that mostly move the back and the sides are
gone and one mode stands for each run of the top's, so a partial there
meets fewer modes than it did. Which notes lose by it follows where their
partials fall: the same A5 string stands 2.5 dB lower without its touch on
Koto Hirajoshi and 0.2 dB higher on Koto Hankumoijoshi, which differs only
in two strings' tuning and ji.

The owner chose this body on one phrase on the strings' own range. A second
set, 0.25.13 against 0.25.16 on both demos, on every key from C6 to C8 and
on the soft-note material of 2026-11-09, was rendered for the owner's ear
and is not committed; B is trimmed +0.55 to +1.16 dB to A's whole-file RMS
there, so 0.25.16 plays that much quieter.

**Not measured again on this body:** the README's figures for the attack's
fall-off and build-up, the upper strings' early fall, the sympathetic
levels after ato-oshi and the comparison with recordings. They stand as
measured on the 356-mode body.

## Left open

| Item | Why it is left |
| --- | --- |
| D4's fundamental decays slowly (3.9 dB/s) and a few attack centroids stand off their neighbours'. | Each sits beside a body mode (D4 by the section's 299 Hz breathing mode); not traced to a defect. |
| `ACUKOTO_ENABLE_SANITIZERS` does not include `float-cast-overflow` (finding 12 of 2026-11-09). | A CI change, and a sanitized run was not affordable on the shared machine in this pass. The fuzz and range tests pass. |
| Which kernels Reason runs on an AVX2 CPU (finding 13 of 2026-11-09). | Needs a U45 build in Reason; it is the Rack adapter's question. |
| `strokeString()` on a string the left hand still presses keeps the press. | An edge of the scripted techniques; no key reaches it. Untouched. |
| A string's energy is refreshed at block ends, so `touch()`'s gate at −170 dBFS can lag by a block. | 170 dB under full scale; inaudible. Untouched. |
| There is no `Acukoto.ParameterReach` (U11). | Acukoto has no plug-in and its testing standard is U1-U10; it comes with the plug-in. |
