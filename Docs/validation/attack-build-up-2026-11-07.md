# How an attack builds, against four professional recordings — 2026-11-07

Acukoto 0.22.0. The owner asked for attacks that sound natural, physical and
varied. This measures how each band of an isolated note's first 12 ms builds
in four recordings and in the model, and pins the walls' damping above 2 kHz
to paulownia's own. Neither the recordings nor the renders are committed.

## Material

Recordings, decoded to mono 48 kHz float with ffmpeg:

- Rokudan, Godanginuta and Kudan from JpnMusic's *A Happy New Year from
  Japan (The Soul of Koto)* (archive.org, `THIRD_PARTY_NOTICES.md`)
- Pro Musica Nipponia's traditional tunings example for Minoru Miki's
  *Composing for Japanese Instruments* (University of Rochester, 2008), a
  studio recording of single plucks

The owner's solo programme, used in the attack-highs report, is left out:
some of its notes repeat sample for sample (two notes 140 s apart correlate
at 0.95 over 0.4 s), which a hand does not do.

The model is Koto Hirajoshi through `RenderAcukotoMidi`, the Performer with
its room and output stage: each of the 13 strings plucked twice at velocities
0.5, 0.7 and 0.9, 1.5 s apart, in an order that never plucks one string
twice running, so every pluck draws fresh deviations.

## Tools

- `Tools/measure_attacks.py` finds and refines the onsets (its header).
- `Tools/measure_attack_build_up.py` gives each band's time to come within
  3 dB of its peak and its first 1.5 ms against the next 4-12 ms (its
  header). Only notes with f0 from 180 to 950 Hz are counted.

## What differed

Each band's median first 1.5 ms against 4-12 ms, in dB (negative: the band
is still building), with the median time to 3 dB under its peak:

| | 0.2-0.8 kHz | 0.8-1.5 kHz | 1.5-3 kHz | 3-6 kHz | 6-12 kHz |
| --- | ---: | ---: | ---: | ---: | ---: |
| Miki, studio (108 notes) | -2.3, 1.5 ms | -5.3, 3.4 | -4.4, 2.4 | -1.8, 1.5 | +3.1, 0.9 |
| Godanginuta (46) | -3.9, 4.3 | -6.2, 5.4 | -4.4, 4.5 | -1.2, 2.4 | +2.8, 0.2 |
| Rokudan (144) | -2.1, 3.5 | -1.9, 1.8 | -3.6, 2.4 | -1.4, 1.9 | +1.5, 0.6 |
| Kudan (32) | -2.8, 6.4 | -4.5, 7.1 | -4.6, 4.4 | -1.5, 2.6 | +0.6, 1.8 |
| 0.21.0 | -4.3, 3.1 | -6.4, 3.9 | +0.9, 0.2 | +1.6, 0.2 | +2.7, 0.1 |
| **0.22.0** | **-4.2, 3.1** | **-7.5, 4.0** | **-3.5, 2.6** | **-0.4, 0.6** | **+1.2, 0.4** |

Below 1.5 kHz the model already built as the recordings do. From 1.5 to 6
kHz it answered at once, where every recording builds over 2-5 ms. The
string's release reaches the ji at once in every band, so what builds is the
body. Its modes above 2 kHz took the median Q of Acustra's measured guitar
bridge, 10: a mode of Q 10 at 2 kHz rings up within about 1.5 ms, one of Q 60
within about 10.

## Candidates

The walls' Q above 2 kHz (below 1 kHz it is already 60), each rendered and
measured the same way (30 and 140 before the touches were re-levelled):

| Q above 2 kHz | 1.5-3 kHz | 3-6 kHz | 6-12 kHz | click |
| ---: | ---: | ---: | ---: | ---: |
| 10 (0.21.0, the guitar's median) | +0.9, 0.2 ms | +1.6, 0.2 | +2.7, 0.1 | 3.8 dB |
| 30 | -2.2, 1.3 | +0.5, 0.3 | +2.1, 0.2 | 3.1 |
| **60, paulownia across the grain (Coaldrake), 0.22.0** | **-3.5, 2.6** | **-0.4, 0.6** | **+1.2, 0.4** | **2.5** |
| 140, paulownia along the grain (Coaldrake) | -5.2, 4.3 | -0.9, 0.8 | +0.7, 0.3 | 2.2 |

Coaldrake measured paulownia's Q at 60 across the grain and 140 along it
(JASA 148 (2020) 3153, Table I). Both sit within the recordings' range at 1.5-3
kHz; 60 is the walls' value below 1 kHz already.

What else moves with Q 60, against 0.21.0:

- The 2-4, 4-8 and 8-14 kHz bands still fall 63-83, 110-111 and 203-219
  dB/s over 20-200 ms (`measure_attack_highs.py`), as with 0.21.0's fit.
- Partials 1-8's decay over a recorded note's length, model over recorded,
  log2 (`measure_koto_notes.py --compare`, `RenderAcukotoNotes`): 0.22,
  -0.04, 0.46, -0.53, 0.26, 0.34, 0.32, 0.41, against 0.21.0's 0.22, -0.04,
  0.55, -0.19, -0.10, 0.26, 0.23, 0.68.
- The first 250 ms' centroid over f0, by register (D3 and below, G3-E4,
  F4-D5, D♯5 up): 13.5, 8.4, 4.4 and 2.4, against 0.21.0's 11.0, 7.5, 3.7
  and 2.1 and the attack-highs report's four recordings' 12.1, 8.1, 4.6 and
  3.7.
- The whole instrument radiates 2 dB more: the 2-8 kHz octaves 5-6 dB more,
  those to 1 kHz 0.3-1.1. The strings' and the hits' touches were re-levelled
  (`AcukotoLevels --check`).
- The click (6-14 kHz over the first 3 ms against 15-30 ms) falls from 3.8
  to 2.5 dB, where the four recordings give 4.1-8.8. The highs after the
  click now ring a little longer through the body. Something at the tsume's
  first touch is still missing (below).

## Variety and what is still missing

How much the attacks vary from note to note is unchanged by the body. The
interquartile range of the click is 2.0 dB (1.7 in 0.21.0), against the
three archive recordings' 5.1-9.2. Neither the notes' level nor their pitch
explains the recordings' spread (a regression on both leaves it within 0.7
dB of the raw range), so it lies in the plucks themselves.

All four recordings are MP3s. Coding the model's renders through LAME at
192 kb/s and measuring them again moves every band above by at most 0.5 dB
(0.21.0's 1.5-3 kHz stays at +0.7), so the codec does not make the
build-up. It does blur what happens before the peak: a search for a
separate tsume landing before the release, the peak's 6-14 kHz at least 25
dB over the floor earlier than 1.5 ms before it, finds one in 71-86% of the
recorded notes and 42% of the model's, which has none. That is not clean
enough to fit a landing to.

What is clear is how fast the highs fall in the first 30 ms. The 6-14 kHz
level against the 0.2-1.5 kHz level at 30-60 ms, median over the notes:

| | 0-3 ms | 3-8 | 8-15 | 15-30 | 30-60 | 60-120 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Miki, studio | -13.5 | -15.2 | -18.5 | -22.5 | -27.0 | -36.1 |
| Rokudan | -11.2 | -12.5 | -14.7 | -17.5 | -20.8 | -28.7 |
| 0.22.0, through MP3 | -15.2 | -14.6 | -15.9 | -17.5 | -21.1 | -27.0 |

The recordings' highs start 2-4 dB stronger and fall 6-9 dB by 15-30 ms;
the model's fall 2. Over 20-200 ms the rates agree, so what is missing is a
short component of the attack that dies within about 30 ms. The tsume's
angle (normal share 0.1 or 0.6 for 0.3) does not move it.
