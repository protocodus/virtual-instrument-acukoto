# Against five professional koto recordings, 2026-10-29

Acukoto 0.1.0, Koto Hirajoshi. Every string was plucked alone and measured
the same way as the isolated notes of five professional recordings. These
measurements are what the silk's stiffness and bending loss and the walls'
thickness are fitted to (`Source/DSP/AcukotoPresets.h`). Neither the
recordings nor the renders are committed.

## The recordings

| Recording | Source, licence | Isolated notes | With a usable B |
| --- | --- | ---: | ---: |
| Rokudan | JpnMusic, *A Happy New Year from Japan (The Soul of Koto)*, CC BY-NC-SA 4.0 | 142 | 131 |
| Godan (Godanginuta) | the same | 192 | 175 |
| Kudan | the same | 32 | 26 |
| Kumoi Rokudan | the same | 30 | 25 |
| Omoigawa | Etsuko Chida, *Shsk'h Vol. 02 – Koto Kumiuta*, CC BY-NC 3.0 US | 313 | 209 |

Links are in `THIRD_PARTY_NOTICES.md`. Each MP3 was decoded to mono 48 kHz
32-bit float (`ffmpeg -ac 1 -ar 48000 -c:a pcm_f32le`). A usable B is one fitted
between 0 and 0.003, on a note between D3 and A5. The players, instruments,
strings, rooms and microphones are not stated. The four JpnMusic recordings
sound alike; Omoigawa's notes ring far longer.

## How it was measured

`Tools/measure_koto_notes.py` holds the protocol at its head. The recordings
were measured with its default 0.6 s gap; the model's notes with
`RenderAcukotoNotes 0 <dir>` (velocity 0.7, 8 s, the preset's controls, the
room off, the engine alone at 48 kHz) and `--dir`.

```sh
python3 Tools/measure_koto_notes.py rokudan.wav > rokudan.json      # and the others
./build-dsp/RenderAcukotoNotes 0 notes/
python3 Tools/measure_koto_notes.py --dir notes/ > model.json
python3 Tools/measure_koto_notes.py --summary recordings rokudan.json godan.json ...
python3 Tools/measure_koto_notes.py --compare model.json rokudan.json godan.json ...
```

## Inharmonicity

The silk's bending modulus is the median, over the 566 notes with a usable B,
of the modulus each one's B implies for the model's gauge, tension and
length: 4.16 GPa, shipped as 4.15. One modulus sets B rising as the square
of the pitch, and the recordings' rises more slowly. By register, the
modulus the recorded notes imply is 6.84 GPa from G3 to E♭4 (221 notes),
2.49 from G4 to D5 (278) and 1.73 from E♭5 to A5 (64): the model's B is
about 1.6 times too small, near right and about 2.4 times too large. The
medians of B itself, over every isolated note in each register:

| Register (nearest MIDI note) | Model B | Recorded B, median [quartiles] | Recorded notes |
| --- | ---: | ---: | ---: |
| 38-54 (D3) | 1.8e-5 | 6.4e-5 [4.6e-5, 1.1e-4] | 17 |
| 55-64 (G3-E♭4) | 4.3e-5 | 8.1e-5 [5.7e-5, 1.0e-4] | 278 |
| 65-74 (G4-D5) | 1.2e-4 | 7.4e-5 [4.8e-5, 1.5e-4] | 326 |
| 75-90 (E♭5-A5) | 2.1e-4 | 1.3e-4 [8.2e-5, 2.8e-4] | 83 |

## The fundamental against the second partial

The balance is the fundamental's level less the second partial's over a
note's first 10-250 ms. The model's walls were fitted so that its
fundamental falls short of the recordings' balance by as little as the
thickest walls allow (`Docs/decisions.md`). Each model string is compared with
the recorded notes nearest its MIDI note:

| String | Note | Model | Recorded median (all five) | Notes | Recorded median (JpnMusic) |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | D3 | -16.7 | -0.6 | 3 | — |
| 2 | G3 | -7.9 | -4.9 | 42 | -3.3 |
| 3 | A3 | -8.3 | -2.9 | 15 | -2.8 |
| 4 | B♭3 | -6.1 | -7.7 | 2 | -10.3 |
| 5 | D4 | +4.1 | +3.8 | 132 | +4.7 |
| 6 | E♭4 | +3.0 | +6.5 | 37 | +9.9 |
| 7 | G4 | -0.8 | +2.8 | 96 | -1.6 |
| 8 | A4 | -9.3 | +2.0 | 54 | +1.8 |
| 9 | B♭4 | -13.8 | +3.2 | 13 | +2.8 |
| 10 | D5 | +9.1 | -4.2 | 75 | -5.1 |
| 11 | E♭5 | +7.8 | +4.2 | 32 | +3.0 |
| 12 | G5 | +5.3 | -7.4 | 19 | -8.9 |
| 13 | A5 | -2.2 | +1.8 | 11 | +1.8 |

All values are in dB. The model's fundamental falls a median 3.5 dB short of
all five recordings (2.2 dB short of the four JpnMusic recordings). Single
strings sit up to 17 dB either side of the recordings, as each partial lands
on or between the body's modes: D3, A4 and B♭4 are thin, and D5 and G5 are
full. The recordings' own spread at one note is wide too.

During the POC, an earlier script read the same comparison over strings 1-13
as a 13.6 dB median shortfall with walls 1/0.7 as thick, and 0.5 dB with
these walls. That script measured differently and is not kept; the figures
above replace it.

## Decay of the partials

The log2 ratio of the model's decay rate to the recordings' median, partial
by partial, median over the thirteen strings (positive: the model dies
faster):

| Against | H1 | H2 | H3 | H4 | H5 | H6 | H7 | H8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| All five | +0.37 | +0.28 | -0.17 | +0.02 | -0.21 | -0.07 | -0.05 | +0.01 |
| JpnMusic | +0.16 | +0.04 | -0.36 | -0.19 | -0.32 | -0.27 | -0.31 | -0.19 |
| Omoigawa | +0.78 | +0.33 | +0.49 | +0.02 | +0.25 | +0.15 | +0.75 | +0.42 |

On the top three strings, the fifth to eighth partials die at 42-81 dB/s,
against 52-68 dB/s in the four JpnMusic recordings and 15-35 in Omoigawa.
The silk's loss modulus, 0.008 GPa, sets the median log ratio of partials 4
to 8 on strings B♭4 to A5 to zero over all five; it was swept from 0.006 to
0.089 GPa with a variant build not kept in the repository. The four JpnMusic
recordings alone give 0.022 GPa. Omoigawa decays more slowly than this body
allows even with no bending loss.

## The attack

The recordings' notes rise from 10% to 90% of their envelope's peak in a
median 39-44 ms by register; the model's rise in 1-3 ms. The recordings'
rooms and microphones lengthen their rise, and how much has not been
separated from the pluck.

## The pluck point

Fitting the plectrum shape to each note's partials 1-15 over its first 60 ms
gives the recordings 0.060-0.068 of the speaking length from the bridge in the
middle and upper registers (D3 is unreliable, 0.16). The model, plucked at
0.075, reads 0.027-0.059: the body's modes colour the early partials enough
to move this fit, so it does not read the model's own pluck point back.
`AcukotoPresets.h` takes 0.075 from the CC0 RutgerMuller sessions instead,
whose medians by register fall at 0.073-0.080.
