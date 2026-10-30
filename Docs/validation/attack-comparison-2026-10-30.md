# The attack against eight professional koto recordings, 2026-10-30

Acukoto 0.4.0, 0.5.0 and 0.6.0, Koto Hirajoshi, set against the first
moments of 724 isolated notes from G3 to A5 in eight professional koto
recordings. The tsume's release time in 0.5.0, and in 0.6.0 the pluck's
distance from the ryukaku with it, are fitted to these measurements
(`Source/DSP/AcukotoEngine.cpp`, `AcukotoEngine::pluck`, and
`Source/DSP/AcukotoPresets.h`). Neither the recordings nor the renders are
committed.

## The recordings

| Recording | Source | Isolated notes, G3-A5 |
| --- | --- | ---: |
| Rokudan, Godanginuta, Kudan, Kumoi Rokudan | JpnMusic, archive.org (`THIRD_PARTY_NOTICES.md`) | 141, 45, 32, 28 |
| Omoigawa | Etsuko Chida, archive.org (`THIRD_PARTY_NOTICES.md`) | 295 |
| Sakura (two performances) | shared by the owner for comparison | 9 (one has a single isolated note and is left out) |
| A solo koto programme, two hours | shared by the owner for comparison | 118 and 56 |

The owner's three files are treated as the professional recordings are:
fetched for analysis, never committed. The two-hour programme was measured
in six 5-minute excerpts from 0:30, 20:30, 40:30, 60:30, 80:30 and 100:30;
the first three and the last three measure alike and differently from each
other, so they are counted as two recordings. Rokudan here is the court
music album's "roku dan no shirabe", and Godanginuta the track of that name;
the 2026-10-29 comparison's Godan may have been another file.

## How it was measured

Each MP3 was decoded to mono 48 kHz float (`ffmpeg -ac 1 -ar 48000 -c:a
pcm_f32le`) and measured by `Tools/measure_koto_notes.py`, its protocol
unchanged. The model was measured two ways: `RenderAcukotoNotes 0` (velocity
0.7, the room off, `--dir`), as the 2026-10-29 comparison did, and each
string plucked three times 1.5 s apart at MIDI velocity 90 through
`RenderAcukotoMidi`, the Performer with its room and output stage, measured
as a recording is, onsets and all.

The attack's brightness is each note's **slope**: a line through its
partials 1-8's early levels (the peak of each partial's envelope over
10-60 ms) against log2 of the partial number, in dB per octave. Unlike any
one partial's level against the second, it does not swing with the partial
that happens to land on a body mode. The table gives each recording's median
slope, and the model's.

| | G3-E♭4 | G4-D5 | E♭5-A5 | G3-A5 |
| --- | ---: | ---: | ---: | ---: |
| Sakura (owner's) | — | -6.8 | — | -6.8 |
| Solo programme, first hour | -3.3 | -11.0 | -10.2 | -6.3 |
| Solo programme, second hour | -2.9 | -7.8 | — | -3.1 |
| Rokudan | +0.3 | -2.6 | -3.3 | -1.7 |
| Godanginuta | +0.6 | -5.3 | -8.0 | -5.0 |
| Kudan | -2.6 | -5.7 | — | -4.2 |
| Kumoi Rokudan | -2.3 | -3.1 | -6.9 | -3.3 |
| Omoigawa | -5.2 | -6.7 | -9.2 | -6.4 |
| **Median of the recordings** | **-2.6** | **-6.0** | **-8.0** | **-4.6** |
| 0.4.0, Performer, velocity 90 | -3.2 | -1.8 | -2.5 | -2.3 |
| 0.5.0, Performer, velocity 90 | -4.6 | -3.7 | -5.4 | -4.7 |
| 0.6.0, Performer, velocity 90 | -1.7 | -7.0 | -8.7 | -4.3 |
| 0.4.0, `RenderAcukotoNotes`, 0.7 | -2.8 | -1.2 | -3.4 | -2.0 |
| 0.5.0, `RenderAcukotoNotes`, 0.7 | -4.2 | -3.8 | -6.5 | -4.3 |

In seven of the eight recordings the attack falls faster with partial
number than 0.4.0's did; only Rokudan's is as bright. Played softly
(velocity 0.3) 0.4.0 still read -4.4 overall and -3.9 from G4 to D5, so the
recordings' darker attacks were not only softer playing.

## What was probed

Each change below was rendered with `RenderAcukotoNotes` at velocity 0.5 and
measured as above. Only the release moves the slope without costing what
the earlier fits secured.

| Change | Slope, G3-A5 | Fundamentals against the recordings' balance |
| --- | ---: | ---: |
| None (0.4.0) | -2.9 | 1.0 dB over |
| Tsume release 0.05 → 0.2 ms | -5.5 | 2.7 dB over |
| Tsume edge 2 → 8 mm | -3.0 | 1.0 dB over |
| Walls at their full-weight thickness (22, 16, 8 mm) | -4.0 | 3.8 dB short, D3 lost |
| Top 22 mm alone | -3.7 | 7.4 dB short |
| Sides and back thicker alone | -3.5 | 3.5 dB short |

A wider edge barely acts: even 8 mm is a small share of the shortest
string. Thicker walls darken G4-D5 by up to 10 dB but undo the walls' fit
to the fundamentals ([2026-10-29 decisions](../decisions.md)). The release
is the one estimated value on the attack's path. Swept through the
Performer at 0.10, 0.15, 0.20 and 0.25 ms, it set the slope from G3 to A5 at
-3.2, -4.0, -4.7 and -5.1 dB per octave; 0.20 ms meets the recordings'
-4.6. 0.5.0 ships it. The partials' decay rates do not move, and the
fundamentals sit 1.6 dB over the recordings' balance against the second
partial, where 0.4.0's sat 0.9.

## Where the tsume plucks (0.6.0)

0.5.0 left a tilt across the registers: one release darkens every string
alike, while the recordings darken with pitch (the 0.5.0 row above: 2.0 dB
per octave darker than their median from G3 to E♭4, 2.3 brighter from G4 to
D5, 2.6 brighter from E♭5 up). The pluck was then at one share of every
string's length, 0.075, as three CC0 sessions read. But a koto player plucks
just beside the ryukaku, the fixed bridge being the reference for the hand,
so the tsume meets every string at about one distance from it: a larger
share of a short upper string, whose attack then falls off faster with
partial number, and a smaller share of a long low one.

The distance and the release were swept together through the Performer
(each string three times at velocity 90, measured as above), and scored by
the summed squared difference from the recordings' three register medians:

| Distance from the ryukaku | Release | G3-E♭4 | G4-D5 | E♭5-A5 | G3-A5 | Error, (dB/oct)² |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.075 of the length (0.5.0) | 0.20 ms | -4.6 | -3.7 | -5.4 | -4.7 | 16.0 |
| 2.5 cm | 0.10 ms | -0.9 | -1.5 | -7.5 | -1.9 | 23.4 |
| 3.5 cm | 0.20 ms | -2.0 | -4.2 | -11.4 | -4.2 | 15.2 |
| 4.5 cm | 0.10 ms | -1.2 | -6.1 | -9.0 | -4.0 | 3.0 |
| 4.5 cm | 0.15 ms | -1.8 | -6.8 | -9.7 | -4.7 | 4.2 |
| **5 cm** | **0.10 ms** | **-1.7** | **-7.0** | **-8.7** | **-4.3** | **2.3** |
| 6 cm | 0.10 ms | -3.0 | -8.4 | -11.0 | -6.7 | 14.9 |
| Recordings' median | | -2.6 | -6.0 | -8.0 | -4.6 | |

A release that moved every register alike could bring the fixed share's
error no lower than about 13. 0.6.0 ships 5 cm and 0.10 ms: a share of 0.04
of D3's speaking length and 0.24 of A5's. The partials' decay rates do not
move. Measured note by note with `RenderAcukotoNotes` (velocity 0.7) as
above, the fundamentals sit 1.1 dB over the recordings' balance against the
second partial, where 0.5.0's sat 1.6.

## What is still off

- **The rise time is not a gap.** The tool reads the recordings' notes as
  rising from 10% to 90% in a median 40-55 ms, and the model's in 5-6 ms
  measured the same way. The recordings' earlier notes are still ringing at
  more than 10% of the new note's peak, so their 10% point falls at the
  onset and the rise reads as the time to the peak. Averaged band by band
  (under 1 kHz, 1-3 kHz, over 3 kHz) from the detected onset, the model's
  envelopes and the recordings' both peak 35-50 ms after it, the highest
  band first.
- **The low band sags after the attack.** From 40 to 150 ms after the
  onset, the model's band under 1 kHz falls 4.8 dB (4.5 in 0.6.0); the
  recordings' falls -0.7 to 3.2 dB. This is Known gaps' low fundamentals fading too soon,
  seen from the attack.
