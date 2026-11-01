# Third-party notices

Acukoto's original source code is covered by the project's `LICENSE` file.
The published values and the values derived from recordings below are not
relicensed by Acukoto. No recording, sample or impulse response is
distributed with it, and the instrument loads none.

Acukoto's engine is forked from Aculyre 0.6.0
(<https://github.com/protocodus/virtual-instrument-aculyre>), whose string
loss sections and Thiran design are copied from Acustra 1.9.0
(<https://github.com/protocodus/virtual-instrument-acustra>). Both are
Protocodus's own code under the same licence.

## Koto construction and materials: published values only

Scalar values are taken from these publications and cited beside the code
that uses them (`Source/DSP/AcukotoPresets.h`,
`Source/DSP/AcukotoEngine.cpp`). No text, figure or table is reproduced.

- M. Ando, "Koto scales and tuning", J. Acoust. Soc. Jpn. (E) 10(5) (1989)
  279: the strings' common gauge and tension, the tunings, six professional
  players' tuning of hirajoshi and its spread (Figs. 7 and 8), and silk and
  ivory as the original materials.
- K. Coaldrake, "A finite element model of the Japanese koto constructed from
  computed tomography scans", J. Acoust. Soc. Am. 148 (2020) 3153:
  paulownia's density, elastic constants and Q (Table I), and the measured
  bending, twist and air modes (Table III). Coaldrake's ICA 2019 paper, "Measurement
  and modelling of the Japanese koto: Problems and solutions", gives the
  instrument's length, its ji's height and its internal struts.
- J. Woodhouse and N. Lynch-Aird, Acta Acustica 105 (2019) 516: the air
  damping of polymer and gut strings, and nylon's loss modulus, against which
  the fitted silk loss is compared.
- D. Chadefaux, J.-L. Le Carrou, B. Fabre and L. Daudet, J. Acoust. Soc. Am.
  131 (2012) 844, and D. Chadefaux, J.-L. Le Carrou and B. Fabre, J. Acoust.
  Soc. Am. 133 (2013) 2444: harpists' release velocities, their spread and
  the fingertip's slip, which set the pluck's release and its variation.
- C. Valette, in *Mechanics of Musical Instruments* (Springer, 1995): the
  air damping law.
- J. Dattorro, J. Audio Eng. Soc. 45(9) (1997): the room's input diffusers.

## Robert Mores guitar-measurement archive: derived statistics only

Creator: Robert Mores, Hamburg University of Applied Sciences. *Archive for
the acoustical documentation of classical Spanish guitars, flamenco guitars
and romantic guitars from private and public collections – bridge mobility*
(2021), <https://doi.org/10.5281/zenodo.4604577>, licensed
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

The body's mode damping above 2 kHz and the air modes' (`woodQ` and `airQ`
in `Source/DSP/AcukotoEngine.cpp`) are anchored on the spread of mode Qs in
Acustra's fit to the archive's guitar g21, as summary statistics: 5-36 above
2 kHz and 16-21 for the air modes. No table or response from the archive is included.

## Freesound recordings: derived values only

- RutgerMuller, koto sessions 364807 and 365215
  (<https://freesound.org/s/364807/>, <https://freesound.org/s/365215/>),
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/): the pluck
  point, fitted to the first 60 ms of 109 isolated notes.

## Professional koto recordings: measured values only, not distributed

The silk's bending modulus and loss modulus, the body's wall thickness and
the holes' size are fitted to scalar measurements (inharmonicity, partial
decay rates and partial balance) of 566 isolated notes in five recordings.
They are fetched for analysis only, kept outside the repository, and never
distributed with Acukoto.

- JpnMusic, *A Happy New Year from Japan (The Soul of Koto)*: Rokudan,
  Godan (Godanginuta), Kudan and Kumoi Rokudan.
  <https://archive.org/details/ahappynewyearfromjapanthesoulofkoto>, licensed
  [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).
- Etsuko Chida, *Shsk'h Vol. 02 – Koto Kumiuta* (2008): Omoigawa.
  <https://archive.org/details/Shskh_Vol02_-_Koto_Kumiuta-3048>, licensed
  [CC BY-NC 3.0 US](https://creativecommons.org/licenses/by-nc/3.0/us/).

Both licences are non-commercial. What Acukoto carries from them is a few
fitted numbers, each recorded with its method beside the code; whether such
values need the recordings' licence for a commercial build is for the owner
to decide before any release.

A performance of Sakura that the owner shared for comparison was measured
the same way, by its stereo, spectrum and tuning. The microphone pair was set
beside it, but nothing is fitted to it, and it is not distributed.

## Trademarks

Every product, company and instrument maker named in Acukoto's documentation
is named for identification only, and implies no endorsement.

## Technique demonstration recordings used for analysis, 2026-11-01

Minoru Miki, *Composing for Japanese Instruments*, audio examples performed
and produced by Pro Musica Nipponia, supervised by Miki, available from the
[University of Rochester repository](https://urresearch.rochester.edu/institutionalPublicationPublicView.action?institutionalItemId=29597).
The collection credits copyright 2008 Minoru Miki, all rights reserved.
Fourteen public downloads were retained outside the repository for analysis;
no audio is embedded in the engine or redistributed with it. The repository
licence is not a downstream sampling/redistribution licence. See
[the technique study](Docs/research/koto-techniques-2026-11-01.md) for provenance,
checksums, the observed hiki-iro gestures and the limits of the measurements.
