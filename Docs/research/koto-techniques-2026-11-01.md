# Koto technique and recording study, 2026-11-01

## Scope and existing model

This study starts from Acukoto 0.14.0, not an uncalibrated plucked-string
patch. Its two-polarisation string waveguides, two string segments per
polarisation, individual ji, coupled modal body, radiation and floor model
have already been compared with the recordings in
[the attack study](../validation/attack-comparison-2026-10-30.md) and
[the low-note study](../validation/low-notes-2026-10-31.md).
The purpose here is to check playing gestures independently, with named
technique demonstrations, rather than further fit body constants to mixed
musical performances.

## Primary technique evidence

[Pro Musica Nipponia's koto guide](https://www.promusica.or.jp/english/koto.html)
distinguishes pitch preparation before a pluck, a press after a pluck,
release of a press, yuri, and hiki-iro. Hiki-iro pulls the string toward the
player's end behind the ji to lower the ringing pitch; it is an ornament,
not a general way to reach missing scale notes. The guide allows substantial
lowering in the middle register but says the extreme strings are harder to
lower. It also distinguishes ordinary and reverse thumb strokes, finger
pizzicato, near/far plucking positions, damping and harmonics. This supports
separate control of an individual string's left-hand gesture; it does not
supply a force-to-pitch law or a uniform register-wide lowering range.

[Naoko Kikuchi's performer guide](https://www.atlasensemble.nl/assets/files/instruments/Koto/Koto%20by%20Naoko%20Kikuchi.pdf)
explains that release timing changes the character of a press. It also
identifies an important unmodelled interaction: a thumb stroke can come to
rest on the adjacent string and stop it. That allows the left hand to
release a preceding pressed string silently. Its discussion of square and
rounded picks and of sweeping/scraping motions cautions against treating
all tsume strokes as identical. Acukoto's current generic tsume and finger
choices do not capture those distinctions.

[Craig Sapp's koto score representation](https://koto.sapp.org/kotospec/)
identifies hiki-iro as a delayed downward ornament of approximately a
semitone. It distinguishes this from a quick upward tsuki-iro and from
repeated press/release gestures. Together with the performer sources, this
supports a modest downward wheel range. It does not mean every hiki-iro
should descend a full semitone.

Names are not perfectly interchangeable between sources and traditions.
The model's existing name *ato-oshi* for a post-pluck press should be read
as its established API terminology; these sources also use *oshibiki* or
*oshi-hibiki* for that action.

## New reference recordings

[The University of Rochester repository](https://urresearch.rochester.edu/institutionalPublicationPublicView.action?institutionalItemId=29597)
offers the audio examples for Minoru Miki's *Composing for Japanese
Instruments*, performed and produced by Pro Musica Nipponia and supervised
by Miki. These are labelled technique demonstrations, providing a more
direct test of playing gestures than a commercial arrangement or producer
loop. The collection credits copyright 2008 Minoru Miki, all rights reserved.
Its repository licence grants distribution rights to the university; it
is not a Creative Commons licence for downstream reuse.

Fourteen publicly downloadable MP3s were fetched on 2026-11-01 into this
external analysis directory:

```text
/Users/vojta/Dev/reason-rack-extension/diagnostics/acukoto-realism-20261101/reference-miki/
```

`manifest.json` there records every source URL, original byte count,
SHA-256 and rights notice. `source-page.html` preserves the source listing.
The recordings and decoded WAV files remain outside the checkout and are
not samples used by the engine. No permission to redistribute them is
claimed. These measurements are analysis results, not a licence conclusion.

| Local MP3 | Original bytes | SHA-256 |
| --- | ---: | --- |
| `traditional-tunings.mp3` | 4349027 | `3f268abff9c80133f413f4abd835a5194cda4f23fe5758fa0b0ce883f93e9ff2` |
| `oshide.mp3` | 961997 | `9f77fc43fbf41202acbb29af9a7953870fd7c3f56b7737d5ed6cc5a32f1d8f7d` |
| `oshibiki.mp3` | 1204413 | `4d6fa04662a559e25d31f458d5a741cc0e417839bf73e312bd50aee7e422e3c3` |
| `ato-oshi.mp3` | 360652 | `1ef2ba29ddad6ee6c051c1d4c1fbb00242d8d052251e6eae5700f9e9b20d8a34` |
| `oshihanashi.mp3` | 826154 | `198a91e7e1c8ee89f4bdca0a2c8753da7213380222336953bf0b3a1c73693d52` |
| `yuri.mp3` | 448946 | `0842eac617b49df208701ca5e933276fa4b2ac40152770eecc72664da0f20397` |
| `hiki-iro.mp3` | 522089 | `dd1b23cf6d1421bfdbcb348060616152fad78c58faa824ea00696fe8f4b8b9e5` |
| `tsuki-iro.mp3` | 451558 | `9f860485e43d243b9a864736bfefa3115afca4b0995d375040f2720b273b723d` |
| `muting.mp3` | 386252 | `98c39c2507c084971e3ed0d218040f5ad1d5047981ffb14878fec55a9ae13d34` |
| `pizzicato.mp3` | 520521 | `f341afedfebfbce2190e1a53dbc2778d82db88a04afc3af49aad182bbe204d72` |
| `right-hand.mp3` | 1218519 | `38e17cb1f36275d938f1fede962ffd88da3f7999199fdc3ab30de85dc401a4b0` |
| `sukuizume.mp3` | 1225827 | `9887cc41a6dc3dbdbcf750dfd65e18a9574341d0ccf20ae5dd4073525e641472` |
| `kozume.mp3` | 251983 | `e2a0a32c585f53897cea9788b5bcc3b60279213ce3e39a158f816e09dbd2917a` |
| `playing-positions.mp3` | 499623 | `59bb3b4735a4b45fb3d0e62373e186e2dc22625b483e1c1c8e763f4d0390cff7` |

The files start with spoken labels. An automatic note extractor applied to
the entire file can mistake speech for koto notes. Exclude the introductions
before using any extracted attack, decay or inharmonicity statistic. The
traditional-tunings track also changes tuning; its notes are not all in
Acukoto's D hirajoshi.

## Held-out timbre check

The unchanged 0.14.0 baseline's thirteen dry string notes were measured with
`Tools/measure_koto_notes.py` against the new traditional-tunings recording.
After excluding its spoken introduction, 112 events remained; ten model
pitches overlapped, with only one example at some pitches. Across those
pitch medians, the model's fundamental-to-second-partial balance was
3.1 dB higher than the recording, but the difference varied widely: A3
and B-flat 3 remained about 7–8 dB thinner, while D5 was about 26 dB fuller.
Median log2 model/recording decay ratios for partials 1–8 were
`[-0.18, -0.39, -0.47, -0.64, -0.50, +0.04, -0.32, +0.04]`.

These findings are a useful independent check, not a new optimisation
objective. The track changes tuning, several comparisons have few notes,
and string material, bridge locations and recording conditions are not
matched. Its mixed deviations do not justify a broad EQ or decay change.
The external `baseline-vs-miki.txt` records the comparison; no model
parameter was fitted to this new recording. The playing-positions example
has notes roughly 0.16–0.27 seconds apart and cannot be analysed with the
same isolated-note decay protocol.

## Hiki-iro measurements

The two isolated demonstrations in `hiki-iro.mp3` start at approximately
8.57 and 15.81 seconds, after the spoken label. Both start near D4
(about 295 Hz). A 100 ms Hann-window STFT with a 10 ms hop was peak
interpolated separately around partials 1–4. The pitch trend is clearest in
partials 2 and 3; the fundamental and fourth have larger local deviations.
Relative to each note's early stable pitch, the second and third partials
show a downward excursion of roughly **30–36 cents**, followed by recovery.

| Pluck time in file | Approximate descent | Deepest part after pluck | Clear lowering begins |
| ---: | ---: | ---: | ---: |
| 8.57 s | 30–32 cents | 1.3–1.7 s | around 0.8 s |
| 15.81 s | 33–36 cents | 1.0–1.3 s | around 0.5–0.6 s |

The resolution, interference between string polarisations and MP3 source
make these approximate gesture measurements, not cent-accurate pitch
standards. There are only two demonstrations at one pitch. They do not
establish a population distribution, the maximum possible pull, or a law
for other string lengths. The external directory contains
`measure_gestures.py`, `hiki-iro-trajectories.json`, trajectory plots and
spectrograms so the estimate can be checked.

The useful implementation implication is a **continuous, deliberate,
string-local downward gesture**. A wheel with a one-semitone downward
maximum can reproduce these examples at about one-third of its travel;
that maximum comes from written technique descriptions, not a fit to
these two recordings. It should not be an automatic semitone drop after
every note. Applying the downward pitch ratio to the existing waveguides
is an approximation: no pulling force law, motion of the ji, or
register-dependent physical limit has been measured here.

## Coverage and next priorities

| Gesture or acoustic behaviour | Acukoto 0.14.0 | Evidence-led action |
| --- | --- | --- |
| Prepared oshide and post-pluck press | Note mapping and positive hand gesture | Check that a prepared note starts with the waveguide at the prepared pitch, including the excitation's first trip. |
| Hiki-iro | No downward hand gesture; negative wheel ignored | Add a restrained string-local downward gesture and test returning to centre and changing strings. |
| Yuri and press release | Positive wheel permits deliberate motion | Preserve player control; a fixed vibrato on every note is not justified. |
| Re-plucking a ringing string | Local contact added in 0.14.0 | Use the new named repeated-stroke examples for later validation; do not restore uniform damping without evidence. |
| Thumb rest stroke / non-rest stroke | No adjacent-string contact choice | A future explicit playing-style option should distinguish them. Applying damping to every keyboard note would incorrectly erase chords and finger strokes. |
| Reverse stroke, scraping, harmonics | Not explicitly modelled | Keep these as declared gaps until excitation/contact models and a deliberate performance mapping exist. |
| Tone across the register | Prior fitted string/body model | Preserve the existing low-note and attack comparisons as regression baselines. Technique data alone cannot validate new body parameters. |

The existing acoustic gaps remain material: low fundamentals decay too
quickly near body modes; upper-string early decay is too strong; the
single fitted silk stiffness misses the recorded register trend; the
light body omits internal struts; and the artificial keyboard range is
not a physical koto. Adding a downwards gesture and correcting a prepared
attack improve specific behaviours, not these independent timbre gaps.
A full claim of instrument fidelity still requires broader controlled
recordings with known string material, tuning, pluck position, geometry,
room and microphone placement, plus a koto player's listening assessment.
