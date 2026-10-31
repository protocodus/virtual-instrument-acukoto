# The attack against eight professional koto recordings, 2026-10-30

Acukoto 0.4.0 to 0.9.0, Koto Hirajoshi, set against the first
moments of 724 isolated notes from G3 to A5 in eight professional koto
recordings. The tsume's release time in 0.5.0, and in 0.6.0 the pluck's
distance from the ryukaku with it, are fitted to these measurements, and
both were fitted again in 0.8.0
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

## How far each pluck strays (0.7.0)

0.6.0 scattered the 5 cm pluck by a harpist's share of the string's length
(Chadefaux et al., JASA 131 (2012) 844, Fig. 7: 0.012 of a 17.6 cm string
to 0.022 of a 97.5 cm one), 28 mm on D3 but 2.5 mm on A5. A hand held by the
ryukaku strays by about the same distance on every string, and a koto guide
puts the pluck 3-5 cm from the right end ("How to play the KOTO",
<https://www.asahi-net.or.jp/~NP5Y-HRUC/kt-play1.html>), so 0.7.0 scatters
it by an estimated 8 mm on every string, those 2 cm a little over two
standard deviations.

The recordings show what the share left wrong. A pluck 5 cm from the
ryukaku leaves its first notch at partial L / 5 cm: the fourth on A5, the
sixth on D5 and E♭5, the eighth on B♭4. Measured as the early level of the
partial nearest it less the mean of its two neighbours, each string plucked
eight times at velocity 90 through the Performer:

| | B♭4-D5 | E♭5 | G5-A5 |
| --- | ---: | ---: | ---: |
| The recordings (all eight, 185 notes) | -0.6 | +0.1 | +0.1 |
| A harpist's share (0.6.0) | -10.2 | — | -14.3 |
| 5 mm on every string | -5.8 | -8.7 | -6.9 |
| **8 mm on every string (0.7.0)** | **-3.0** | **-0.1** | **-6.0** |

The attack's slope by register does not move beyond the draw: -1.9, -7.3
and -9.5 dB per octave over these 104 plucks, against -1.5, -7.5 and -9.3
with the share. From pluck to pluck, partials 1-8 of one string vary by a
median 0.7, 2.3 and 3.9 dB by register with 8 mm, where the recordings' vary
by 1.2-7.2, 0.3-4.4 and 1.8-4.6 dB; theirs include the player's dynamics,
which these plucks at one velocity leave out.

## The walls' highs (0.8.0)

The walls' modes above 3 kHz are too dense to run one by one, so the body
keeps one mode of each run of alike modes, scaled by s to carry the run's
share of the top's motion (`Source/DSP/AcukotoEngine.cpp`, `wallModes`).
Until 0.8.0 that mode also radiated scaled. A run's modes radiate with
unrelated signs and add in power, N times one mode's, but one mode s times
as large, driven and radiating, radiates s^4 = N^2 times as much, so the
walls above 3 kHz radiated up to 10 dB too much. 0.8.0 keeps the scaled
drive, so the strings still lose to the kept mode what they lose to its run,
and radiates the mode with its own unscaled shape. The body keeps the same
356 modes, so the cost does not change.

A reference body with every wall mode to 12.5 kHz kept (1467 modes, offline
only) puts the README demos' sixth octaves from 3.2 to 5.6 kHz 5-11 dB under
0.7.0's at equal loudness, and 1-4 dB under 0.8.0's. Over the long-term
spectrum, in dB per octave (the nine recordings: both Sakura performances
count here, since no isolated notes are needed):

| | 2-5.6 kHz | 5.6-11 kHz |
| --- | ---: | ---: |
| The recordings, median (range) | -10 (-4.8 to -10.8) | -12 |
| 0.7.0's demos | +0.3 | -20 |
| The reference body's demos | -7.3 | |
| **0.8.0's demos** | **-3.4** | **-15** |

The pluck had been fitted with the excess in place, so its distance and
release were fitted again, each string plucked eleven times at velocity 90
through the Performer (the three plucks above and eight more), the error
summed against the recordings' medians above:

| Distance from the ryukaku | Release | G3-E♭4 | G4-D5 | E♭5-A5 | Error, (dB/oct)² |
| --- | ---: | ---: | ---: | ---: | ---: |
| 5 cm, 0.7.0's walls | 0.10 ms | -1.8 | -7.3 | -9.1 | 3.5 |
| 4.25 cm | 0.03 ms | -0.9 | -4.6 | -10.7 | 12.1 |
| 4.25 cm | 0.05 ms | -1.1 | -4.8 | -11.4 | 15.3 |
| **4.5 cm** | **0.03 ms** | **-1.1** | **-5.4** | **-10.3** | **7.9** |
| 4.5 cm | 0.05 ms | -1.2 | -5.9 | -11.0 | 11.0 |
| 4.75 cm | 0.03 ms | -1.2 | -7.1 | -10.1 | 7.6 |
| 4.75 cm | 0.05 ms | -1.3 | -7.4 | -10.7 | 10.9 |
| 5 cm | 0.03 ms | -1.4 | -8.1 | -9.9 | 9.5 |

4.5 and 4.75 cm tie within the draw; 0.8.0 ships 4.5 cm and 0.03 ms, whose
middle register sits nearer the recordings'. The fit is worse than 0.7.0's:
with the walls' highs radiated as they are, no distance or release from 3.5
to 5.5 cm and 0.03 to 0.10 ms brings the top strings brighter than about
-9.5 with the rest in place (What is still off). The reference body reads
alike there, so it is not the walls' thinning. The partials' decay rates do
not move.

## The walls' runs (0.9.0)

0.8.0 kept one wall mode of each run of nine above 3 kHz. 0.9.0 keeps as
many runs, so the body runs the same 356 modes, but each run's length grows
in proportion to its frequency: each candidate weighs 1/f and each run takes
an equal share of the weight still left, about four modes a run around 3-5
kHz and twelve toward 15 kHz (`Source/DSP/AcukotoEngine.cpp`, `wallModes`).
Over both README demos, set against the reference body with every wall mode
kept (with 0.8.0's pluck) and against the nine recordings' long-term
spectra, in dB per octave, and the 4.5-5.6 kHz sixth octaves' mean height
over a line through the 2-11 kHz spectrum:

| | 2-5.6 kHz | 5.6-11 kHz | 4.5-5.6 kHz bump |
| --- | ---: | ---: | ---: |
| The recordings, median | -10.0 | -12.0 | 0.1 |
| Runs of nine (0.8.0) | -3.4 | -15.2 | 5.1 |
| **Runs growing with frequency (0.9.0)** | **-5.2** | **-13.0** | **2.4** |
| Runs growing as frequency squared | -5.6 | -15.2 | 2.3 |
| The reference body | -6.0 | -14.3 | 2.6 |

From 5.6 to 11 kHz, 0.9.0's sixth octaves sit a mean 1.3 dB from the
reference's, where 0.8.0's sat 2.4; from 2.8 to 5.6 kHz both sit 1.9-2.0.
The reference body's attacks, eleven plucks of each string as below, fall
1.1, 6.5 and 9.9 dB per octave of partial number from G3 to E♭4, G4 to D5
and E♭5 to A5; 0.9.0's sit within 0.7 of them, and 0.8.0's within 1.1.

The pluck was swept again on these runs, eleven plucks of each string as
above:

| Distance from the ryukaku | Release | G3-E♭4 | G4-D5 | E♭5-A5 | Error, (dB/oct)² |
| --- | ---: | ---: | ---: | ---: | ---: |
| 4.5 cm, 0.8.0's runs | 0.03 ms | -1.1 | -5.4 | -10.3 | 7.9 |
| 4 cm | 0.03 ms | -0.9 | -5.4 | -10.4 | 9.0 |
| 4.25 cm | 0.03 ms | -1.0 | -6.1 | -9.7 | 5.5 |
| 4.5 cm | 0.02 ms | -1.1 | -6.8 | -9.1 | 4.1 |
| **4.5 cm** | **0.03 ms** | **-1.1** | **-6.9** | **-9.2** | **4.5** |
| 4.5 cm | 0.05 ms | -1.2 | -7.0 | -9.8 | 6.2 |
| 4.75 cm | 0.03 ms | -1.2 | -8.0 | -9.0 | 7.0 |
| 4.75 cm | 0.05 ms | -1.3 | -8.2 | -9.6 | 9.1 |
| 5 cm | 0.03 ms | -1.3 | -8.9 | -9.1 | 11.3 |

0.02 and 0.03 ms tie within the draw, and 0.02 ms would be quicker than
off a nail, so the pluck stays at 4.5 cm and 0.03 ms. The partials' decay
rates and the balance of fundamental against second partial do not move
(`RenderAcukotoNotes`, `--compare`: the median log2 ratios of partials 1-8
within 0.07 of 0.8.0's).

Tried and not shipped, at the same mode count: weighting each run's kept
mode's radiation by its members' energy shares moves the demos' spectrum by
under 1 dB; matching it to its members' radiated power, computed at setup
on the radiating grid, puts 5-6 kHz up to 6 dB over the reference body. A
wider scatter of the pluck's place than 0.7.0's 8 mm (10, 12 and 15 mm, on
0.8.0) steepens the middle strings' partials 8-15 toward the recordings'
(1.2 to 14.4 dB per octave at 12 mm, against their 18.4) but flattens the
low strings' (14.9 to 8.7) and the top strings' (20.5 to 16.0) away from
theirs.

## What is still off

- **The attacks follow the recordings' fall-off only roughly.** Partials
  1-8 fall 1.1, 6.9 and 9.2 dB per octave by register in 0.9.0, against the
  recordings' 2.6, 6.0 and 8.0, and no pluck swept fits all three more
  closely (The walls' runs, above).
- **The rise time is not a gap.** The tool reads the recordings' notes as
  rising from 10% to 90% in a median 40-55 ms, and the model's in 5-6 ms
  measured the same way. The recordings' earlier notes are still ringing at
  more than 10% of the new note's peak, so their 10% point falls at the
  onset and the rise reads as the time to the peak. Averaged band by band
  (under 1 kHz, 1-3 kHz, over 3 kHz) from the detected onset, the model's
  envelopes and the recordings' both peak 35-50 ms after it, the highest
  band first.
- **The upper strings' fundamentals fall fast at first.** From 40 to 150 ms
  after the onset, the model's band under 1 kHz falls 4.8 dB (4.5 in 0.6.0)
  over all its notes; the recordings' falls -0.7 to 3.2 dB. By register it
  is the upper strings' alone. Each note's fundamental, through a ±10% band,
  falls over that time:

  | | G3-E♭4 | G4-D5 | E♭5-A5 |
  | --- | ---: | ---: | ---: |
  | 0.6.0, Performer, velocity 90 | -4.1 | 3.1 | 6.9 |
  | Rokudan, Omoigawa, Kumoi Rokudan, Godanginuta, Kudan, solo programme from 20:30 | -1.5 to 1.3 | -1.4 to 3.0 | -0.6 to 4.0 |

  (0.4.0 and 0.5.0 read alike.) It is not the sympathetic strings: with them
  off the fall is the same. It is the plane parallel to the top. Plucked
  with nearly all its energy in that plane, E♭5's fundamental falls 9 dB in
  200 ms and 19 in 300, G5's 24 and A5's 9; plucked with nearly all of it
  normal to the top, the fundamental holds. A sideways pull at the ji's top,
  6 cm up, rocks it on its feet 5.1 cm apart, so the top under it is twisted
  as well as pushed; normal to the top the ji's mobility at the upper
  strings' fundamentals is only 0.001-0.006 m/s/N. The walls, fitted thin
  for the low fundamentals and without their struts, are the likeliest
  cause here too. Probed note by note through the engine (velocity 0.7, the
  room off, the same ±10% band), the walls at their full-weight thickness
  (22, 16 and 8 mm) cut the fall from 40 to 150 ms to 1.5 dB at E♭5, where
  it is 11.5, to 2.1 at D5 and B♭4 (3.3 and 2.5) and 3.3 at A5 (5.4); G5's
  is not helped (10.1, where 6.3). Those walls lose the low fundamentals
  (What was probed, above), so a fix has to hold both: the struts, or a
  measured thickness. Since the floor now lends the fundamentals about 1 dB,
  the thickness was scanned again between 0.70 and 0.85 of those walls
  (`RenderAcukotoNotes`, velocity 0.7, `--compare` against the five earlier
  recordings). Every thicker wall slows the fundamentals' decay toward the
  recordings' (the median rate from 1.39 times theirs at 0.70 to 1.15-1.26
  times), and the pooled balance stays within 0.0-4.7 dB of theirs, but each
  moves the box's modes under single notes: at 0.75-0.80 D4's fundamental,
  the most recorded note (107 notes), falls 23-33 dB under its second
  partial where the recordings' sits 3 dB over it, at 0.82-0.85 E♭4's 28-29
  dB under, and at 0.72 the fast fall moves to D5 (15.5 dB). None ships;
  the walls stay at 0.70. Nor is it the strings' damping by the air:
  halved, it brings the fundamentals' decay to the recordings' but leaves
  partials 2-8 ringing 1.4-1.9 times too long. Nor the body's own losses
  (0.8.0, `--compare` as above): the rests' Q at 30 (8) or the air modes'
  at 40 (20) moves neither G3's fundamental nor D4's; the box's at 140 (60),
  Coaldrake's along the grain, slows G3's (log2 ratio 0.76 to 0.54), and
  the walls' below 1 kHz doubled slows D4's (0.84 to 0.61). Each still
  leaves that fundamental decaying about 1.5 times as fast as recorded.
  (Over 2 s of each render; over a recorded note's 0.9 s, as
  `Tools/measure_koto_notes.py` now measures, 0.9.0's G3 and D4 decay 2.6
  and 2.2 times as fast: [recording comparison](recording-comparison-2026-10-29.md#over-a-recorded-notes-span-2026-10-31-090).)
- **The top strings' upper partials die fast.** On G5 and A5, partials 6-8
  (4.7-7.1 kHz) decay 1.2-6 times faster than in the recordings' 6-7 notes
  there. A ji rocking at 5.5 or 12 kHz (7.8) or damped at Q 60 (30) leaves
  them unchanged, and they decay as fast with every wall mode kept. The
  model's notes are measured with the room off, and a recording's room
  holds a fast-dying partial up, so part of this is likely the rooms.
