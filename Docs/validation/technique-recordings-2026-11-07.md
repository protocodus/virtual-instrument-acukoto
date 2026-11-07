# The koto's techniques, measured on Pro Musica Nipponia's demonstrations

2026-11-07, for 0.19.0's techniques on the free keys (`Source/DSP/AcukotoEngine.cpp`,
`TechniqueKey` and the constants after it).

## Recordings

Pro Musica Nipponia recorded the examples for Minoru Miki's *Composing for
Japanese Instruments* (University of Rochester Press, 2008), supervised by Miki;
the University of Rochester's library serves them as MP3. They were fetched for
measurement, decoded to 48 kHz mono and never committed. Each example opens with
its spoken number; the times below are from the file's start.

| Example | Technique | Measured |
| --- | --- | --- |
| 3.13 | Oshi-hanashi | B4 pressed from A4 (493 Hz) falls to 447 Hz over 0.9-1.0 s, starting 0.2 and 0.4 s after the pluck (7.83 s and 13.5 s), half the fall in the first 0.38 s |
| 3.14 | Yuri | G4 (394 Hz) on its third partial: steady for 0.3 s, then up to +78 cents (8.84 s), back to +15-20, up to +70 (9.39 s), back to +15-20, a last +24; peaks 0.55 s apart, never below the note |
| 3.16 | Tsuki-iro | the same G4: +120 cents 0.32 s after the pluck (8.21-8.30 s) and +145 cents 0.74 s after (14.53-14.62 s), each up in about 35 ms and back about 50 ms later |
| 3.18 | Muting | three muted notes are 9-12 dB darker (energy above 1.5 kHz against 80-1500 Hz) over their first 20-100 ms and 15-26 dB over 0.1-0.3 s than the notes of 3.14 and 3.16; their level falls about as fast |
| 3.23 | Right-hand techniques | a repeated note at 997 Hz from 49.1 s, nine strokes 50-66 ms apart, median 58 |
| 3.26 | Chirashi-zume | a burst at 7.22 s lasting about 30 ms, its noise from 2.4 to 4.8 kHz peaking near 3.8 kHz at -34 dB, then the string ringing at 422 Hz at -36 dB |
| 3.27 | Suri-zume | 0.7 s of noise (7.3-8.0 s) from 0.9 to 2.5 kHz at -40 to -46 dB, peaks near 1.1, 2.2 and 4.4 kHz, no pluck |
| 3.30 | Glissando | strings a median 55 ms apart (26.8 s, 11 onsets) and 62 ms (37.2 s, 27 onsets), strokes under 25 ms apart taken as one |

Pitch was tracked as the strongest STFT peak (2048 or 4096 points, 5 ms hop,
parabolic interpolation) in a band around one partial; onsets as peaks of the
1.5-8 kHz spectral flux. The muting comparison takes the energy above 1.5 kHz
against 80-1500 Hz in each window, the same measure as for the model below.

## The model against them

Rendered with `RenderAcukotoMidi` on Koto Hirajoshi at its defaults, 48 kHz.

- Yuri on G4: peaks +74 to +77 cents 0.53-0.55 s apart, troughs +14 to +16.
- Tsuki-iro on G4: +108 to +151 cents for about 60 ms (the 21 ms analysis
  window smears the push), back to the note within 85 ms.
- Oshi-hanashi: B4 on B♭4 pressed a semitone falls to B♭4 in about 1 s.
- A muted G4-B♭4-D5 line (E1): 16.5-18.9 dB darker than the same line open,
  the fifth partial 30.5 dB down at 0.1-0.3 s and the fundamental 4.5 dB.
- Tremolo: strokes every 58 ms (56-60 by the onset detector).
- Harmonic on G4: the fundamental 29.8 dB under the octave.
- Chirashi-zume on G5: the burst's 2.4-4.8 kHz band 2 dB over the note ringing
  after it, as in 3.26 (fitted: `chirashiForce`). Suri-zume on D5: 4-8 dB under
  that note's level (fitted to 3.27 against 3.26's note, taking the two as
  recorded at one gain: `suriForce`).

What the recordings do not settle is estimated beside each constant: where the
tsume plucks near and away from the ryukaku and left of the ji, the tsume's back
and corner, the fingertip's distance from the ji, and how far a scrape travels.
