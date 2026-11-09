# Acukoto realism review — 2026-11-09

Work in progress, candidate 0.25.5. The five-hour review began at 06:02 UTC. Canonical source
was refreshed from the environment snapshot `3d86094` to current upstream
`9d7bda9` (0.25.4) before the candidate was evaluated. All comparisons below
use that current source unless explicitly labelled otherwise. The original
snapshot is retained only as historical scratch evidence.

## Scope and constraints

Keep the koto's thirteen silk strings, movable ji, calibrated paulownia body,
owner-approved tuning and timbre. Correct demonstrated mistakes in contact,
attack and playing ownership; preserve the keyboard extension as the declared
artificial range. Do not infer an ear verdict from numerical measurements.
No new samples, EQ layer, audio-thread allocations or per-sample random sources.
CPU optimization follows the acoustic work and will be measured separately.

Frozen baseline source, executables, raw surveys and review WAVs live outside
the checkout in `/workspace/scratch/acukoto-2026-11-09/`.

## Findings and candidates

1. **One hand per tremolo string.** Same-key retriggers left earlier tremolo
   timers alive. In the last half-second of a one-second probe, zero through
   three retriggers produced 9, 18, 25 and 34 strokes, against about nine for
   the measured 58 ms hand. A soft retrigger also kept the old hard strokes
   (largest later force 98% of the initial hard force). A new note now retires
   this string's pending tremolo before starting its own; other strings keep
   their own timers. The latest velocity owns every subsequent stroke.
2. **Deferred strokes count key-ups.** A key-up marked every overlapping
   deferred kakite note released, and could consume a sounding owner too.
   Each string now consumes one held instance: its sounding owner first,
   otherwise its earliest still-held deferred stroke. This preserves the
   two-string oshi-awase owner and the established overlapping-note contract.
   When a deferred stroke finally sounds, its saved key-up reaches only that
   string: broadcasting it again had released its oshi-awase companion twice.
   A new pitch on the same physical string also retires the displaced
   pitch's deferred contact, so an old hard stroke cannot reclaim a newly
   released soft note. Same-pitch deferred overlaps remain counted.
3. **The release hand touches the played stretch.** A left-of-ji pluck sent
   Release and Staccato to the speaking stretch, leaving the directly played
   rest stretch free. With default Sympathy, the late tail differed by less
   than 0.002 dB between Release 0 and 1. Its existing passive fractional
   contact now operates at the left stroke's physical place, using that
   stretch's frequency and current travelling period; the explicit Staccato
   stop uses its existing distributed damping. On keys 50, 62 and 81 the
   1.0–1.5 s tail loses 13.6, 26.2 and 23.9 dB with Release 0. The pedal
   retains it sample-for-sample. This is a contact-routing correction, not a
   new fitted rate. The hand retires only after the actual left stretch has
   stayed below the existing idle-energy threshold for half a second, or
   when the whole engine sleeps. Its bounded silence counter cannot grow
   indefinitely under a held key. A public regression checks that waking
   after this pluck leaves no phantom contact changing sympathy.
4. **Fixed held-string boundary and octave-harmonic projection.**
   The rounded pluck used an infinite-line Gaussian kink without enforcing
   its fixed endpoints. A broad contact near an end displaced the supposed
   fixed end and lost part of its force outside the string. The candidate
   integrates the static string equation with fixed endpoints and a Gaussian
   force normalized on the string. A midpoint harmonic is projected in
   physical space before sample integration, rather than shifting a sampled
   waveform by half its fractional period in a `ceil(period)`-sample ring.
   This removes the continuum projection's odd modes without changing its
   even modes or adding deformation energy. Across all 59 pitched keys of Hirajoshi at 48 kHz and velocity 0.7,
   the octave now exceeds the residual fundamental on every key (56 by more
   than 20 dB). Example H2–H1 changes are G4 36→47 dB, A5 29→53, F7 2→34
   and C8 −15→9. Ordinary plucks at the same settings change at most
   0.00035 dB in K-weighted level and 0.00017 dB/s in early decay slope.
   Wider technique qualification is ongoing.

The release's circular one-pole still uses `ceil(period)` sample cells. The
spatial projection fixes the midpoint contact, not this inherited fractional
last-cell approximation. Coupled rest-stretch modes also remain. Residual
fundamental energy on the shortest extension keys is not claimed to vanish.

## Validation so far

`Acukoto.PlayingOwnership` uses only the public API. The current upstream
baseline fails 21 regression assertions across tremolo count/velocity, deferred key
ownership and left-of-ji release/staccato. The candidate passes; the original
limits were selected to distinguish those mechanisms, not fitted afterwards.

`Acukoto.PluckShape` checks 48 combinations of length, pluck point and width:
fixed endpoints, total applied force, the point-force Green function, removal
of odd modes, preservation of even modes and no increase in deformation
energy. Endpoint force error is at most 1.47e-5; residual odd-mode amplitude
normalized by length is at most 1.27e-17.

The final acoustic candidate was clean-built at 07:52 UTC with GCC 14.2,
Release `-O3 -DNDEBUG`. All 13 unit/audio tests pass without changing any
existing test limit; the strict C++17 `-Wall -Wextra -Werror -fno-exceptions`
target also builds. Clang 19.1.7 ASan/UBSan passes Engine, HandContact,
PlayingOwnership, PluckShape and LoopPair; the existing 120-second libFuzzer
run completes without a finding. CPU-budget and optimization qualification
follow this acoustic freeze, separately from these results.

## Cleanup

Removed the unused lyre sharping-lever pitch slot and boolean argument,
write-only generalized-velocity history, obsolete sampled-harmonic scratch
buffer, unused scheduled-gesture counter and superseded Gaussian helpers. None of the removed state was a
calibration or runtime control.

## Ordinary keyboard survey

Two seconds per reset, seeded, dry note, 64-frame blocks, native engine;
44.1/48/96 kHz × two presets × velocities 0.15/0.3/0.7/1.0. Each case
covers all twelve body keys and all 59 pitched keys (MIDI 38–108): **1,704
conditions per version**, plus a complete 0–127 classification at 48 kHz.
Keys 0–20 and 109–127 are unused. Keys 21–37 are technique controls,
including the two audible standalone glissandi on 34/35; their silence when
there is no string to act on is intentional. Body strikes are assessed
separately from pitched strings.

`Tools/KeyboardSurvey.cpp` writes raw stereo float32, a manifest and native
RMS/peak metrics. `Tools/measure_keyboard_survey.py` scores them without mono
cancellation. The first 0.6 s is resampled to 48 kHz before BS.1770 K weighting;
native RMS/peak retain their original rate. Early fundamental decay is a
local 0.14–0.74 s fit, with residual, not a claimed T60 for beating modes.
The per-case bounds are in [the survey CSV](realism-keyboard-2026-11-09.csv).

All samples are finite. Each body and pitched key becomes louder at every
tested velocity step. The largest candidate change from the baseline is
**0.0003464 dB** in K-weighted level, **0.00002504** in peak sample and
**0.0003657 dB/s** in the local decay slope. Ordinary calibration and decay
are effectively unchanged. Every pitched key's final half-second is at
least 19.9 dB below its first 50 ms; no late-growing note appears in this
two-second observation. Longer decay checks follow separately.

At 48 kHz the thirteen native strings span 1.17/1.29 dB at velocity 0.7
(Hirajoshi/Hankumoijoshi), 2.30/3.22 dB at 0.3 and 1.13/1.22 dB at 1.0.
At very soft 0.15 they span 5.17/6.53 dB. Across the entire artificial
extension, medium/hard spread is 2.02–3.24 dB across the tested rates and
presets, with no adjacent step over 2.42 dB. The inherited soft upper-key
falloff remains: 14.82–15.83 dB spread at 0.15 and 8.28–9.33 dB at 0.3,
including an adjacent step up to 3.86 dB. This is not called an even response
at every velocity. The existing measured velocity-dependent slip and the
owner's 80% level correction at velocity 0.7 are preserved; flattening the
extension at every velocity would be a separate calibration choice.

A separate quiet-edge scan covers MIDI velocities 1/127 and 2/127 on all
128 keys of both presets at 48 kHz. Every sample is finite; the same 73
keys intentionally sound, and every body/pitched key increases in level.
Pitched peaks span 4.87e-5–2.74e-4 at velocity 1 and
1.28e-4–7.24e-4 at velocity 2. Their level rises about 8.43 dB, as the
existing force law `v^1.4` predicts. Body strikes have a documented minimum
arrival speed of 0.02 m/s, so their quiet edge is intentionally much louder:
velocity-1 peaks span 0.0107–0.0425 and the next step adds only about 0.26 dB.
That authored minimum is unchanged, not presented as a flat response across
the percussion and pitched regions.

## Changed techniques, tails and output guard

The octave-harmonic survey covers all 59 pitched keys, both presets and
44.1/48/96 kHz at velocity 0.7 (354 conditions per version). Every condition
now puts the octave above the residual fundamental; on the thirteen native
strings the margin is at least **28.87 dB**. On the artificial extension,
55–57 keys per case exceed 20 dB. The weakest is Hirajoshi C8 at 44.1 kHz,
only **0.88 dB**: the shortened extension still exposes discretization and
coupled-rest limitations. Largest harmonic K-level change is 2.20 dB;
maximum candidate peak across this matrix is 0.2701.

Bare-finger plucks, whose 8 mm contact makes the endpoint error larger,
were checked on all 59 keys at 48 kHz, both presets and velocities
0.15/0.7/1.0 (354 conditions per version). The correction reaches 1.496 dB
at C8; all outputs remain finite and the largest peak is 0.4219. Native
tsume plucks are effectively unchanged as the ordinary matrix above shows.
An independent Gaussian-force Fourier oracle also checks the helper against
the static Green-function identity, including contacts 20% of string length:
maximum absolute modal-coefficient error 1.17e-15 (2.58e-12 relative above
1e-9). This is a continuum-shape check, not a claim about the complete
sampled waveguide or an ear judgement.

Twenty-second held notes at velocity 1, all twelve body keys and 59 pitched
keys of both presets, show no lingering loud tail. The loudest 15–20 s RMS
is D3, **−175.71 dBFS** on Hirajoshi and **−175.30 dBFS** on Hankumoijoshi.
No output normalization or release hand was used to force these decays.

Separate full-keyboard Performer scans at 48 kHz and velocities 0.7/1.0
include the shipping guard. Its 96-frame latency is trimmed before comparison
with the native engine. At velocity 0.7 every pitched note passes bit for bit;
one body hit per preset gets at most 0.034 dB instantaneous gain reduction.
At velocity 1, maximum native-string reduction is 0.164/0.282 dB and maximum
pressed/extended-key reduction is 0.241/0.257 dB. Raw pitched peaks reach
0.633/0.843. The intentionally unlimited raw body hits reach 1.783/1.773;
the Performer holds both at 0.841395 (−1.5 dBFS), with up to 6.53/6.48 dB
instantaneous reduction. Ordinary differences are not being concealed by
the guard. Values separated by `/` are Hirajoshi/Hankumoijoshi.

## Broader acoustic model audit

The existing radiation field emits more acoustic power than the prescribed
mechanical losses can supply in several low body modes. A physically
derived passive reaction was prototyped separately and measured, including
coherent wall/hole interference and uncertainty in empirical cavity loss.
It materially changes lower-note spectra and tails and fails unchanged
level targets; a complete, consistent loading model was not established.
The production body remains unchanged and this discrepancy is explicitly
open. See [the investigation and rejected prototype evidence](acoustic-budget-investigation-2026-11-09.md).

## Remaining work

The sound/ownership work is frozen for profiling. Qualify CPU changes against
that exact source and output, run the performance gate in the coordinated
quiet window, and record final CPU/audio equivalence separately. The accepted
11.06-second Performer A/B is in the external `review/` directory, with the
unchanged upstream as A, candidate as B, whole-file RMS matching, raw levels,
score and separate key. No listening preference or real-time deadline
guarantee is inferred from these renders.
