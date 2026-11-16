# Physical model audit, 2026-11-16

An audit of 0.25.18's model (`113c2bf`) for defects in its physics and its
numerics: the strings' loops and their fitted sections, the junction at the
ji, the modal body, the pluck, the radiation, the tuning, the Performer and
the Rack adapter's mapping. One defect was found and is changed in 0.25.19,
which the owner chose in A/B listening. Five more suspicions were measured
and are not defects.

A is 0.25.18 and B is 0.25.19 throughout.

## The finding: a string's two planes had two lengths

A string vibrates in two planes at the ji, one heaving it and one rocking
it, and the model runs a loop for each. The ji does not reflect either wave
exactly in phase: the body under it leads or lags the reflection by
`arg((1 - ZY)/(1 + ZY))`, and `setInstrument` shortens or lengthens each
loop's line so the fundamental lands on its tuning all the same.

0.25.18 solved that line for each plane against the plane's own lead. Away
from the body's modes the two leads agree and so did the lines. Beside a
mode that moves the ji in one plane more than the other, they did not, and
the planes were given two lengths. At 48 kHz the speaking stretches' lines
differed by:

| String | Planes' lines apart |
| --- | ---: |
| G3 (string 2) | 3.96 cents |
| E♭4 (string 6) | 3.12 |
| A4 (string 8) | 2.36 |
| G5 (string 12) | 1.39 |
| G4 (string 7) | 1.35 |
| C5 (Hankumoijoshi's string 9) | 5.46 |

and the rest stretches (ji to unkaku) of strings 2, 3 and 7 by 3.19, 1.38
and 1.13 cents. A line's length sets every partial of its loop, so each
partial above the fundamental sounded twice, that far apart, and beat,
though the lead it compensates exists only at the fundamental. That is
against what the model says of itself: the planes of a string share its
length (README, 0.7.1; `Docs/decisions.md`, 2026-10-29), and only the ji's
pull on each separates their pitches.

**The change.** Both planes take one line, the one that cancels the mean of
their two leads at the fundamental (`reflectionLead` in `setInstrument`),
for the speaking stretch, the rest stretch, a pressed string and a moved
ji's key alike. `Acukoto.LoopPair` now holds every string's two planes to
one line and one phase lag at 44.1, 48 and 96 kHz on both presets; it fails
432 times on 0.25.18. Two other single lines were tried and dropped: the
heaving plane's own lead for both sounds G3 6.3 cents sharp, over
`Acukoto.Tuning`'s 6-cent limit, and no compensation at all leaves every
string on a mode off its tuning. With the mean, `Acukoto.Tuning` passes
unchanged (G3 0.20 cents from its tuning; the worst string, Hankumoijoshi's
C5, 3.79).

### The partials

Each open string plucked alone at velocity 0.7 on the engine (no Performer
scatter, room off), 48 kHz, 4 s. For partials 2 to 6: the peaks within 12 dB
of the strongest, in cents from the stiff string's partial, in a
Hann-windowed spectrum of 0.1 to 3.9 s; and the RMS deviation, in dB, of
the partial's envelope (heterodyned, 12 Hz low-pass) from a straight decay
over 0.2 to 2.5 s, averaged over the five partials.

| String | A, partials' components | A, wobble | B, partials' components | B, wobble |
| --- | --- | ---: | --- | ---: |
| D3 | one each (the third two) | 1.6 dB | the same | 2.2 dB |
| G3 | two each, 3.6-4.8 cents apart | 4.2 | one each | 0.7 |
| A3 | one each | 0.7 | one each | 0.9 |
| B♭3 | one each | 0.8 | one each | 1.1 |
| D4 | one each | 2.4 | one each (the second two) | 2.5 |
| E♭4 | two on three of five, 2.9-3.0 apart | 3.0 | one each | 1.4 |
| G4 | two on two | 3.2 | two on two | 2.3 |
| A4 | two on four, 2.2-2.5 apart | 3.6 | two on three | 2.8 |
| B♭4 | two on two | 3.2 | two on one | 4.2 |
| D5 | two on three | 3.5 | two on three | 3.9 |
| E♭5 | two on one | 3.2 | one each | 3.1 |
| G5 | two on one | 4.5 | two on three | 3.4 |
| A5 | two on three | 6.3 | two on one | 7.2 |
| A♭4 (Hankumoijoshi) | two on four, 1.6-2.3 apart | 2.9 | two on one | 1.6 |
| C5 (Hankumoijoshi) | two on four, 3.7-5.4 apart | 3.1 | two on one | 2.6 |

The strings whose planes had two lengths (G3, E♭4, A4, C5, A♭4) lose the
doubling they got from it. The pairs that remain are another string
sounding in sympathy an octave or a fifth away (A4's with A3) or the ji's
pull at that partial, and are the model working as meant. On the strings
whose lines already agreed the wobble moves by a decibel either way, with
the re-levelled touches.

### The fundamentals

The same renders by the earlier audit's protocol
([Strings on body modes](consolidation-audit-2026-11-10.md#strings-on-body-modes)):
the peak of a Hann-windowed spectrum scanned in 0.5-cent steps within 40
cents of the string's designed pitch, in three windows after the pluck; a
second peak within 12 dB is given with its level under the first. Cents are
from the string's own tuning.

| Note | | 0.08-0.5 s | 0.5-1 s | 1-2 s |
| --- | --- | --- | --- | --- |
| G3 | A | −5.0 | −2.5 | +3.0 (and −17.5, 9 dB lower) |
| | B | −2.0 | 0.0 | +8.5 (and −10.5, 3 dB lower) |
| E♭4 | A | −2.0 | −0.5 | −0.5 |
| | B | +2.0 | +0.5 | +3.5 |
| A4 | A | 0.0 | 0.0 | −0.5 |
| | B | 0.0 | −6.0 | 0.0 |
| E♭5 | A | −10.0 (and +4.0, 6 dB lower) | −4.5 | −0.5 (and −11.0, 6 dB lower) |
| | B | −8.5 | −11.0 (and +0.5, 3 dB lower) | −2.0 |
| C5, Hankumoijoshi's string 9 | A | −1.0 (and +14.5, 5 dB lower) | +3.0 | 0.0 (and +11.0, 5 dB lower) |
| | B | +7.0 (and −9.0, 2 dB lower) | +2.5 | +2.5 (and +10.5, 7 dB lower) |
| A♭4, Hankumoijoshi's string 8 | A | 0.0 | 0.0 | 0.0 |
| | B | −1.5 | −1.0 | −0.5 |
| C5 pressed on Hirajoshi | A | −8.5 | −1.5 (and +11.5 and −21.0, 2 dB lower) | +9.0 (and −21.5, 6 dB lower) |
| | B | −9.0 (and +5.0, 2 dB lower) | +4.0 (and −20.5, 5 dB lower) | +9.0 (and −21.5, 4 dB lower) |

Every other string stays within 1.5 cents of its tuning in all three
windows in both, but B♭4, which reads +3.0 in the first with a component
16.5 under, 7-8 dB lower, in both.

So the change is a trade, and an audible one. With one length the planes
differ only by what the ji does to each at the fundamental, which is the
physical difference between them, and it shows where it should: the
fundamental of a string beside a body mode has two components a few cents
apart that beat slowly, and the pitch read off a short window moves more
than it did (G3, E♭4, A4, Hankumoijoshi's C5). In exchange the partials
above it stop beating at a rate the model had no reason for.

### What else moved

- **Levels.** A string's level over its first 0.6 s moved by up to 0.74 dB
  (G5 louder, G3 0.47 dB quieter), so the strings' touches and the key
  touches are refitted by `AcukotoLevels` until every string and key stands
  within 0.01 dB of its intended level; `Acukoto.Levels` failed before the
  refit (key 86, 0.61 and 0.76 dB). Without touches the strings stand
  6.1-7.2 dB apart, where they stood 6.1-7.0.
- **The release into sleep.** Hankumoijoshi's D4, held 6 s and released,
  leaves the instrument awake 8.03 s after its key-up, where it took 7.55 s,
  at levels under −170 dBFS. `Acukoto.Subnormals` checked for sleep at
  exactly 8 s; that wait is now 10 s, with the reason beside it. No
  subnormal operand or underflow was raised.
- **Tests.** The `unit` and `audio` tiers (18 suites), `Acukoto.CpuBudget`
  and the `AcukotoDSPCxx17` build pass on 0.25.19.

### The listening set

Koto Hirajoshi through the Performer, the room and the output stage
(`RenderAcukotoMidi 0 <phrase> <wav> --tail 3`), 48 kHz, 24-bit, the same
MIDI and default controls for both letters: the five strings alone (G3,
E♭4, G4, A4, G5), four intervals on them, and the README's hirajoshi demo
phrase. B is trimmed to A's whole-file RMS (+0.13, −0.15 and +0.06 dB); A is
untouched. The set and its key were handed to the owner and are not
committed. The owner chose B (`Docs/decisions.md`, 2026-11-16).

## Checked and not defects

| # | Suspicion | What was measured | Outcome |
| --- | --- | --- | --- |
| 1 | The fitted loss and dispersion sections drift with the sample rate. | The partials of every string at 44.1, 48, 88.2 and 96 kHz. | Pitches agree within 0.1 cents and decays within about 1 dB/s across the four rates. |
| 2 | The dispersion fit loses the stiff string's law in the high partials. | Partial pitches against `n f0 sqrt(1 + B n²)`. | Past the 12th partial, above about 5 kHz, the upper strings fall 10-25 cents flat (G4's 20th 11 cents, D5's 16th 19), the same at 48 and 96 kHz: the fit's stated limit, on partials 60-90 dB down. |
| 3 | The loss fit misses the stated loss law. | Partial decays against the law. | The fundamental is exact; partials 2-4 lose 5-10% less than the law (at worst 14%, E♭5's third at 44.1 kHz, 25.0 against 29.2 dB/s), within what a decay must change to be heard. |
| 4 | Tension modulation glides the pitch wrongly. | The pitch over a hard pluck's first second. | At the preset's 3 N the glide is under 0.05 cents on D3 and about 0.2 on A5. Nothing to hear, and no measurement to fit a larger one to. |
| 5 | An error in the junction's algebra, the modal step, the pluck, the radiation sums, the scrapes' delays, the Performer's decimator or the Rack adapter's mapping. | Read against their derivations and the tests that hold them. | Sound. The Known gaps the README already lists are not repeated here. |

Two things read and left as they are: `fixedEndRoundTripGain`, 0.9995, is
an authored loss at the fixed ends, about 18% of A5's own decay, and is
documented as one; and the lines are tuned by the fundamental alone, so
where the ji pulls a fundamental the line's correction carries to the
partials (D4's sit about 2 cents over), which is what a player tuning by
ear does to a string.
