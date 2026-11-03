# Even scale and the 0.16.0 review, 2026-11-03

This change starts from 0.15.2, commit `eb15eb9`. It sets each string's
`touch` to take its string 80% of the way to its scale's mean loudness, removes the silent
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
| Koto Hirajoshi | 6.89 dB | 1.38 dB | B♭3 -3.32 dB | E♭5 +3.57 dB |
| Koto Hankumoijoshi | 8.59 dB | 1.71 dB | D3 -3.39 dB | C5 +5.20 dB |

(The last two columns are 0.15.2's.) The same offsets hold at other
velocities: before the change the spread was 8.0 and 10.0 dB at velocity 0.3
and 6.8 and 8.5 dB at 1.0. After it, with the touches calibrated at 0.7 only,
it is 2.5 and 3.3 dB at 0.3 and 1.3 and 1.7 dB at 1.0.

## The change

`koto()` takes the touch, and the presets carry the values the tool prints:
each string's level without its touch, 80% of the way, in dB, to its scale's
mean, applied to the force (the scale's average touch in dB is zero). The 80%
is the owner's, chosen after a lettered set of the full correction and half of
it (`Docs/decisions.md`). They differ between the two
presets: moving strings 8 and 9 shifts every string's level a little (D3 sits
0.6 dB lower in Koto Hankumoijoshi with the same string and ji); the cause was
not traced.

| string | D3 | G3 | A3 | B♭3 | D4 | E♭4 | G4 | A4/A♭4 | B♭4/C5 | D5 | E♭5 | G5 | A5 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Hirajoshi | 1.254 | 0.974 | 1.034 | 1.358 | 1.099 | 0.797 | 0.928 | 0.976 | 1.043 | 0.764 | 0.720 | 1.133 | 1.132 |
| Hankumoijoshi | 1.367 | 0.993 | 1.068 | 1.336 | 1.172 | 0.876 | 0.912 | 1.054 | 0.619 | 0.806 | 0.753 | 1.216 | 1.145 |

(Strings 8 and 9 are A4 and B♭4 in Hirajoshi, A♭4 and C5 in Hankumoijoshi.)

`Acukoto.Levels` (audio tier) fails when a string stands more than 0.5 dB from
the level its touch is meant to give it, at velocity 0.7. The engine, the Performer and the Rack wrapper's interface are
unchanged; the Rack submodule pin was not advanced.

## Effect on the demos

The two README demos (`RenderAcukotoPresetDemos`, Performer, output stage),
before and after:

| demo | RMS | peak |
| --- | --- | --- |
| Koto Hirajoshi | -31.11 to -31.62 dBFS | -10.18 to -10.79 dBFS |
| Koto Hankumoijoshi | -28.57 to -30.07 dBFS | -8.20 to -9.28 dBFS |

They lean on the loud strings, so they come out lower; no preset's
`outputGain` was changed.

## Not claimed

No listening verdict is claimed. The owner chose 80% of the full correction
after a lettered set (A the shipping 0.15.2, B the full correction, C half of
it) and did not say whether by ear, so `Docs/decisions.md` does not record it
as chosen by ear. The target is a level, not a recorded koto's balance: no
per-note level was measured on the recordings, and a real koto is not even.

## Low notes sound buzzy: measured, not changed

The owner asked why the low notes sound like buzzing, not koto. Koto Hirajoshi,
each key alone at velocity 0.7, engine alone, room off (the 10-250 ms window,
the partials within 3% of each harmonic, dB re the strongest of the first 12):

| key | f0 | fundamental | second partial | strongest | share of energy over 1.5 kHz |
| --- | --- | --- | --- | --- | --- |
| C2 | 65 Hz | -47 dB | -44 dB | 8th | 11% |
| B2 | 124 Hz | -41 dB | -23 dB | 10th | 10% |
| C3 | 131 Hz | -44 dB | -23 dB | 4th | 9% |
| D3 (string 1) | 147 Hz | -30 dB | -8 dB | 5th | 14% |
| E3 | 165 Hz | -28 dB | 0 dB | 2nd | 31% |
| G3 (string 2) | 196 Hz | -4 dB | -1 dB | 6th | 20% |
| D5 (string 10) | 587 Hz | 0 dB | -1 dB | 1st | 4% |

Below G3 the fundamental, and below D3 the second partial too, is missing, and
the sound's centre of energy stays near 1 kHz whatever the pitch (0.9-1.4 kHz
from C2 to B♭3, 0.9 kHz at D5). Held for 1.5 s, D3 is purely harmonic (peaks at
2.00, 3.00, 4.00 ... times f0, none between them) and its amplitude does not
beat, so the sound is not a rattle: it is a comb of partials 3-10 with no
body under it, which reads as buzz. The recordings' D3 has its fundamental
level with the second partial (-0.6 dB), against -22 dB here. The cause is the
body: these fundamentals can only leave through the walls yielding to the ji
(README, Known gaps), and D3's ji stands 1.6 cm inside the side wall. The keys
below D3 lie past the strings: a moved ji makes them and no koto plays them
(README, Known gaps).
The new touches raise D3 and B♭3 by 2.0 and 2.7 dB; they make this timbre
louder, not different. A fix needs a body that radiates 100-200 Hz from the
ji, which twelve body variants did not give (Known gaps); no change was made.

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
