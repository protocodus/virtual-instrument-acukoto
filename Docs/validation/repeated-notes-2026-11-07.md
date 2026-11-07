# Repeated notes against one player's recordings — 2026-11-07

Acukoto 0.20.0. The owner heard repeated notes as a machine's. This compares
how a repeated note's strokes vary in Pro Musica Nipponia's demonstrations for
Miki's *Composing for Japanese Instruments* (University of Rochester, 2008;
fetched for analysis, never committed) with how they varied in 0.19.0 and
vary now.

## Protocol

`Tools/measure_repeated_notes.py <wav> <from s> <to s> [shortest gap s]`
finds onsets by spectral flux at 1.5-8 kHz (512-sample frames, 64-sample hop)
and, for each, the level over 3 ms before to 25 ms after it and the spectral
centroid of the 30 ms after it. A run is consecutive onsets on one pitch
(within 40 cents). It prints each run's level and centroid spread (standard
deviation), the even against odd strokes' mean level and centroid, and the
lag-1 correlations.

The model renders a repeated G4 (0.08, 0.2 and 0.4 s apart), D5 (0.08 s) and
D4 (0.2 s) at velocity 0.7 on Koto Hirajoshi through `RenderAcukotoMidi`, and a
tremolo on G4 held 2 s.

The recordings carry mains hum whose 50 Hz period shows in the high band, so
onsets closer than about 20 ms cannot be timed from them: the stroke timing
below comes from the strokes' range measured in 0.19.0 (Example 3.23, nine
strokes 50-66 ms apart), not from this tool.

## Repeated notes

| | about 100 ms apart: level, centroid | 300-440 ms apart: level, centroid |
| --- | --- | --- |
| recordings (8 and 4 runs, medians) | 1.4 dB, 7.0% | 0.6 dB, 1.8% |
| recordings (range) | 1.1-2.1 dB, 1.3-13% | 0.2-1.0 dB, 1.1-3.3% |
| 0.19.0, every pluck drawn anew | 2.7-2.9 dB, 9.9-10% | 2.4-3.1 dB, 6.8-9.1% |
| memory 1 s | 2.0 dB, 7.3-7.4% | 1.7-2.1 dB, 4.3-5.1% |
| memory 3 s | 1.7 dB, 5.8-6.3% | 1.1-1.4 dB, 2.8-3.5% |
| **memory 10 s (0.20.0)** | **1.5-1.6 dB, 6.0-6.3%** | **0.6-0.9 dB, 2.2-2.8%** |
| identical plucks | 1.4-1.6 dB, 4.6-7.0% | 0.2-0.5 dB, 0.9-2.1% |

The recorded runs: 166449 at 0.0 s; 166462 at 17.8, 19.3 and 21.0 s; 166450
at 17.5 s; 166469 at 16.7, 22.0 and 53.6 s (about 100 ms); 166462 at 11.5 s,
166446 at 25.6 s, 166450 at 7.1 s and 166469 at 10.6 s (300-440 ms).

Identical plucks still vary 1.4-1.6 dB 80 ms apart: the new pluck meets the
last one still ringing, as a player's does. Their spread at 300-440 ms is under
the recordings', which is why the memory fades rather than holds. The memory
applies only while the hand stays on one string: a pluck after another
string's draws anew, so a phrase's notes spread as fitted in 0.7.0.

## Tremolo

| | intervals | back against front: centroid, level |
| --- | --- | --- |
| recordings (nine runs) | 50-66 ms (Example 3.23) | median 3% apart, either way; 0.9 dB |
| 0.19.0 | 57-59 ms (rounding only) | 25% darker; 0.5 dB |
| back with a 3 mm contact, slip x1.4 | | 10% |
| **0.20.0, back with the edge's contact** | **58 ms, 9.3% spread, lag-1 -0.22** | **1.4%; 0.2 dB** |

The nine recorded tremolos (166446 at 3.3 and 9.4 s, 166448 at 9.1, 10.5 and
18.7 s, 166449 at 3.4 s, 166469 at 43.6 s, 166462 at 3.2 s, 166463 at 19.3 s)
put even strokes against odd at 0.94-1.09 of each other's centroid, with no
consistent sign: within what the strokes' own spread gives by chance. The
model's back strokes, a rounder 4 mm back slipping off over twice the time
(estimated), stood 25% under. The back now meets the string with the edge.
