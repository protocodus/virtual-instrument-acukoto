# Even scale and the 0.16.0 review, 2026-11-03

This change starts from 0.15.2, commit `eb15eb9`. It sets each string's
`touch` so that a preset's strings sound equally loud, removes the silent
piezo path inherited from Aculyre, and makes the engine cheaper to run
([benchmark](../benchmarks/cpu-bank-2026-11-03.md)). It refits no body, silk,
tuning, pluck or radiation parameter.

## What was found

Every string was plucked with the same force (`touch` 1.0 on all of them)
although `StringSpec::touch` exists so that the player evens the scale by what
is heard. The tsume meets every string 4.5 cm from the ryukaku and each
string's partials meet different body modes, so the strings differ in
loudness.

Protocol (`AcukotoLevels`, written at the head of `Tools/MeasureLevels.cpp`):
each string alone, velocity 0.7, from a reset engine at 48 kHz with the preset's
default controls and the room off; ITU-R BS.1770-4 K-weighted mean square of
both microphones over the 0.6 s after the pluck; engine alone, before the
Performer's output stage. Every string starts from the same random state.

| | 0.15.2 spread | 0.16.0 spread | quietest (re mean) | loudest (re mean) |
| --- | --- | --- | --- | --- |
| Koto Hirajoshi | 6.89 dB | 0.01 dB | B♭3 -3.32 dB | E♭5 +3.57 dB |
| Koto Hankumoijoshi | 8.59 dB | 0.01 dB | D3 -3.39 dB | C5 +5.20 dB |

The same offsets hold at other velocities: before the change the spread was
8.0 and 10.0 dB at velocity 0.3 and 6.8 and 8.5 dB at 1.0. After it, with the
touches calibrated at 0.7 only, it is 1.47 and 1.90 dB at 0.3 and 0.14 dB at 1.0.

## The change

`koto()` takes the touch, and the presets carry the values the tool prints
(the level of each string against its scale's mean, in dB, applied to the
force: the scale's average touch in dB is zero). They differ between the two
presets: moving strings 8 and 9 shifts every string's level a little (D3 sits
0.6 dB lower in Koto Hankumoijoshi with the same string and ji); the cause was
not traced.

| string | D3 | G3 | A3 | B♭3 | D4 | E♭4 | G4 | A4/A♭4 | B♭4/C5 | D5 | E♭5 | G5 | A5 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Hirajoshi | 1.326 | 0.967 | 1.042 | 1.465 | 1.126 | 0.753 | 0.911 | 0.970 | 1.054 | 0.714 | 0.663 | 1.169 | 1.168 |
| Hankumoijoshi | 1.478 | 0.991 | 1.086 | 1.436 | 1.220 | 0.848 | 0.891 | 1.068 | 0.550 | 0.764 | 0.701 | 1.277 | 1.185 |

(Strings 8 and 9 are A4 and B♭4 in Hirajoshi, A♭4 and C5 in Hankumoijoshi.)

`Acukoto.Levels` (audio tier) fails when a preset's spread exceeds 0.5 dB at
velocity 0.7. The engine, the Performer and the Rack wrapper's interface are
unchanged; the Rack submodule pin was not advanced.

## Effect on the demos

The two README demos (`RenderAcukotoPresetDemos`, Performer, output stage),
before and after:

| demo | RMS | peak |
| --- | --- | --- |
| Koto Hirajoshi | -31.11 to -31.74 dBFS | -10.18 to -10.94 dBFS |
| Koto Hankumoijoshi | -28.57 to -30.33 dBFS | -8.20 to -9.47 dBFS |

They lean on the loud strings, so they come out lower; no preset's
`outputGain` was changed.

## Not claimed

No listening verdict is claimed. Nobody has judged by ear whether an even
scale is better than the old one, or whether halfway is: the lettered set
(A the shipping 0.15.2, B this change, C half of B's correction in dB) is with
the owner and the choice is recorded in `Docs/decisions.md` when it is made.
The target is a level, not a recorded koto's balance: no per-note level was
measured on the recordings, and a real koto is not even.

## Findings not acted on

- **The keys past the strings are far more uneven than the strings.** Plucked
  alone at velocity 0.7 on Koto Hirajoshi (0.15.2), the 88 keys span
  16 dB, -34.4 to -18.3 dB, and C♯5 stands 9.8 dB over the strings' mean. A
  pressed or moved-ji key takes its string's touch, so the new touches do not
  level them. Leveling them takes a measured touch per key, a table the size
  of the keyboard; that is not an improvement without a new parameter set and
  was left for the owner to ask for.
- **The ji's feet are modelled across the top.** `BodySpec::bridgeFeet` is
  "the spread of its two feet across the top", and the README's explanation
  of the upper strings' fast-falling fundamentals (Known gaps) rests on it. I
  could not find a source that states which way a koto ji's feet lie, so
  nothing was changed and the direction is unverified.
- **`StringVoice::pitch[1]`, Aculyre's sharping-lever state, is never designed
  and no preset uses it.** It stays in the engine and is listed in Known gaps.

## Removed

The piezo path (members, mount filter, setup and reset code, `StringSpec::piezoTouch`
and `InstrumentSpec::piezoGain`) played nothing since the owner chose no piezo on
2026-11-01. Removing it left the demos byte-identical.
