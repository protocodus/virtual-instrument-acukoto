# The attack's highs against four professional recordings — 2026-11-07

Acukoto 0.21.0. The owner asked for attacks that sound natural and physical.
This compares the first 200 ms of isolated notes in four professional
recordings with the model, and fits the silk's high-frequency loss to them.
Neither the recordings nor the renders are committed.

## Material

Recordings, decoded to mono 48 kHz float with ffmpeg:

- Rokudan ("roku dan no shirabe"), Godanginuta and Kudan from JpnMusic's
  *A Happy New Year from Japan (The Soul of Koto)* (archive.org,
  `THIRD_PARTY_NOTICES.md`)
- the owner's two-hour solo programme, five minutes from 30:30

The model is Koto Hirajoshi through `RenderAcukotoMidi`, the Performer with
its room and output stage. Each of the 13 strings is plucked twice, 1.5 s
apart, at velocities 0.5, 0.7 and 0.9.

## Tools

- `Tools/measure_attacks.py` takes `measure_koto_notes.py`'s isolated
  onsets, refines them on the 1.5-12 kHz energy and measures the attack per
  note. Its protocol is in its header.
- `Tools/measure_attack_highs.py` gives each band's decay from 20 to 200 ms,
  per register.

Only notes with f0 from 180 to 950 Hz are counted.

## What differed

The decay over 20-200 ms after the onset, in dB/s, as the median of each
register's medians:

| | 1-2 kHz | 2-4 kHz | 4-8 kHz | 8-14 kHz |
| --- | ---: | ---: | ---: | ---: |
| recordings (four, three registers each) | 40 | 68 | 118 | ≥118 |
| 0.20.0 | 48 | 49 | 68 | 105 |
| **0.21.0** | **65** | **61** | **112** | **215** |

The recordings' 8-14 kHz band falls 24-35 dB from the first 5 ms to 150-200
ms, and is then 1-11 dB over the level before the onset. Their measured rate
in that band is therefore set by their floor, a lower limit. The model's
fell 19 dB in 0.20.0 and falls 39 now.

In 0.20.0 the attack's first 3 ms already held as much 6-14 kHz against 1-3
kHz as the recordings' did (-13 dB against -11 to -22). The difference was
afterwards: the model's highs lingered. With them gone, the first 3 ms stand
out at 6-14 kHz over 15-30 ms by a median 3.7 dB, against the recordings'
2.8-5.4. In 0.20.0 it was 1.9.

## Candidates

Each candidate was rendered and measured the same way. The H1-8 column gives
partials 1-8's decay over a recorded note's length (`measure_koto_notes.py
--compare`, model over recorded, log2, median over partials 1-8, from
`RenderAcukotoNotes`). 0.20.0 sits at 0.15, -0.15, 0.48, -0.29, -0.20, 0.00,
-0.04 and 0.44.

| Change | 2-4 kHz | 4-8 kHz | 8-14 kHz | H1-8 |
| --- | ---: | ---: | ---: | --- |
| constant loss angle 2e-4 (was 5e-5) | 56-79 | 88-96 | 144-147 | not measured |
| constant loss angle 3.5e-4 | 65-90 | 109-118 | 186-188 | 0.35 to 0.84, all faster |
| loss modulus 0.016, 0.03, 0.06 GPa (was 0.008) | 44-79 | 70-102 | 125-228 | within 0.2 of 0.20.0 |
| normal share 0.1, 0.5, 0.7 (was 0.3) | 25-74 | 43-84 | 94-111 | mixed |
| **b3 omega^2, 4e-9 s** | **53-82** | **108-114** | **201-215** | **0.20, -0.07, 0.54, -0.20, -0.11, 0.26, 0.23, 0.67** |
| b3 omega^2, 2e-9 s | 46-78 | 87-93 | 166-172 | +0.0-0.1 |
| b3 omega^2, 6e-9 s | 58-80 | 125-134 | 241-259 | +0.1-0.3 |

A constant loss angle large enough for the bands takes every partial 1.3 to
1.8 times too fast. The bending loss reaches 4-8 kHz only on the top
strings, and the normal share does not move the bands.

The b3 omega^2 term (Chaigne and Askenfelt, JASA 95 (1994) 1112) leaves
partials 1-5 within 3-6% of 0.20.0 and takes partials 6-8 15-17% faster. At
4e-9 s it meets the 2-4 and 4-8 kHz bands and the 8-14 kHz floor bound.

## Variety, not yet addressed

The attacks of one performance vary far more than the model's. Interquartile
ranges per recording against the model at three velocities:

| | click | early 2.5-6 kHz drop |
| --- | ---: | ---: |
| recordings | 2.2-9.2 dB | 3.0-17.8 dB |
| model, 0.21.0 | 1.7 dB | 2.4 dB |

The recordings mix dynamics, strokes and context that this one-stroke test
does not, so the gap is not yet attributed to a mechanism.
