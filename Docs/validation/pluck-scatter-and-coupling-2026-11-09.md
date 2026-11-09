# Pluck scatter, body-coupling CPU and keyboard survey — 2026-11-09

Acukoto 0.25.6 against 0.25.5 (`d8374b3`). Three source commits:

| Commit | Change | Output |
| --- | --- | --- |
| `e7c8b5b` | Removes code no preset reaches: the A/B realism switch (always on), the wound-string and guitar-material branches, the nail technique; drops the stale sharping-lever Known gap | byte-identical |
| `ebdb653` | The tsume's release time follows each pluck's force draw | audible, small |
| `9e75dc7` | Wider AVX2 body-coupling blocks, AVX2 junction product | byte-identical |

GCC 13.3, Release (`-O3`), on a shared 4-vCPU Intel Xeon @ 2.10 GHz
container (48 KB L1d, 2 MB L2 per core). Development-machine figures only,
never Reason or DAW CPU.

## A pluck that strays harder slips off faster

Each pluck draws its force from a 14% scatter (one harpist's displacement at
release, Chadefaux et al., JASA 131 (2012) 844, Fig. 8) and its release time
from a further one. The same paper puts the release velocity at about D^1.2
of the displacement and its scatter at about 25%. Until 0.25.6 the release
drew 20% on its own, independent of the force: a pluck that strayed harder
was louder but no brighter, which the D^1.2 law it already used for velocity
says it should be. Now 1.2 × 14% of the release's scatter follows the force's
own draw and the remaining sqrt(25² − 16.8²) = 18.5% is drawn alone.

Measured over 400 plucks on five strings of Koto Hirajoshi at velocity 0.7
(`AcukotoEngine::lastPluck()`, each string's mean removed):

| | release-time scatter | correlation with the force |
| --- | --- | --- |
| 0.25.5 | 20.5% | −0.02 |
| 0.25.6 | 25.2% | −0.66 |
| expected | 25.0% | −0.67 |

The force's own draw, the mean release time and the velocity law are
unchanged, so calibrated levels move only by each pluck's scatter (survey,
below). The unit, audio and perf tiers pass, including `Acukoto.Levels`.

## Body coupling: fewer instructions, the same bits

Callgrind (Koto Hirajoshi, `AcukotoCpuProbe --scenario dense`, 0.5 s, only
`AcukotoEngine::process` collected, line information from a `-g` build of the
same body code) put half the engine's instructions in
the modal body: its drive from the 26 bar coordinates (about 140M of 792M),
its pull back on them (about 138M) and the junction's two 26 × 26
matrix-vector products (73M). Three exact changes:

- the AVX2 drive takes 64 modes per block, then 32, then single groups of
  8 (was 32, then 8), halving the per-coordinate loop overhead;
- the AVX2 pull takes nine coordinates per pass over the bank, three passes
  instead of five (thirteen per pass spills registers inside the loop);
- the junction's two products run four outputs to an AVX2 vector behind the
  same dispatch as the modal bank, where they ran two to an SSE2 vector.

Every sum keeps its order and no multiply-add is fused. The probe's dense
and gesture scenarios on both presets at 1x and 2x oversampling, and both
README demos at 44.1, 48 and 96 kHz, are byte-identical to `ebdb653`; the
portable path (`-DACUKOTO_DISABLE_AVX2`) still compiles with `-Wall -Wextra
-Werror`. `Acukoto.ModalRadiation` now also checks the whole AVX2 modal step
(with and without a strike) at every bank size from 8 to 424 over three
evolving steps, and the junction product on 64 random matrices, against the
portable code bit for bit.

| `process()` instructions, 0.5 s dense | `ebdb653` | `9e75dc7` |
| --- | --- | --- |
| total | 791.6M | 716.6M (−9.5%) |
| modal step (`runModesAvx`) | 400.6M | 367.2M |
| junction products (inlined / `transposeApplyAvx`) | in `process` | 40.8M |

Native time did not move measurably. Over eight alternating pairs per
preset, one core pinned, 3 s each, the thread-CPU medians per 64-frame block
were 198.1 → 197.0 µs (Hirajoshi) and 202.8 → 200.3 µs (Hankumoijoshi),
−0.5% and −1.2%, inside this shared machine's run-to-run spread (183-266 µs);
the least-disturbed runs were about 5% lower. The body is bound by memory,
not arithmetic: per sample it streams its 26 × 360 participations twice
(37 KB), its states and coefficients (17 KB) and its radiation weights
(12 KB), about 69 KB against a 48 KB L1d. Further gains would need changes
that are not exact (storing the bank at a lower precision, or running the
upper modes at a lower rate), which change the sound and are a separate
decision. `Acukoto.CpuBudget` passes.

## Keyboard survey

`AcukotoKeyboardSurvey` at 48 kHz, 64-frame blocks, seeded, one dry note
per reset held 3 s, native engine before the output stage; both presets at
velocities 0.3, 0.7 and 1.0, all 128 keys. `Tools/measure_keyboard_survey.py`
scores them (K-weighted level over the first 0.6 s, partial balance over
0.08-0.58 s, the fundamental's local decay over 0.14-0.74 s with its fit
residual). An outlier is a key more than 4 dB, or 60%, from the median of
the three keys either side.

**Against 0.25.5**, the release scatter moves the 59 pitched keys' (D3-C8)
K-weighted level by at most 0.57 dB at velocity 0.7, 0.39 dB at 1.0 and
1.40 dB at 0.3, each largest on the artificial top keys (107, 108), and the
fundamental's decay by at most 0.14 dB/s. Every sample is finite and no
pitched key grows late: each one's last 0.75 s sits at least 19.9 dB under
its first 50 ms.

**Volume.** At velocity 0.7 the pitched keys span 2.43 dB (Hirajoshi) and
2.55 dB (Hankumoijoshi), the thirteen strings 1.15 and 1.25 dB, with no
adjacent step over 1.58 and 2.24 dB; at 1.0, 2.05 and 2.26 dB. At 0.3 they
span 7.3 and 7.9 dB, the top keys falling quiet (Known gaps). No key is a
level outlier at 0.7 or 1.0.

**Decay.** The same four keys stand out on both presets at every velocity,
as in 0.25.5; none is new:

| Key | fundamental decay, dB/s | neighbours | fit residual | cause |
| --- | --- | --- | --- | --- |
| F♯3 (54) | 21.7 | 8.8-9.1 | 0.5 dB | not traced (a key between the strings) |
| G3 (55) | 40.1 | 9.3 | 2.1 dB | on the 195 Hz twist, split (Known gaps) |
| D4 (62) | 3.7-3.8, slow | 12.1-12.5 | 0.8 dB | on the shell's 293 Hz breathing (`woodQ`) |
| F♯5 (78) | 74.3-75.1 | 33-39 | 3.5-3.9 dB | not traced (a key between the strings), beating |

A residual of 2-4 dB marks a beat, not an exponential decay, so those two
slopes are local. The partial balance shows the same body coincidences the
Known gaps list (*Single notes vary from the recordings*), and the pitch the
school's measured tuning: E♭4 13.5 and E♭5 19.6 cents under equal
temperament, Hankumoijoshi's A♭4 10.0 (E♭5 also splits on its body mode).
The twelve body strikes' native peaks at velocity 1.0 reach 1.78 before the
output stage, as in 0.25.5; the Performer's limiter holds them. Their
K-weighted level rises about 10 dB from velocity 0.7 to 1.0 where a
string's rises 4.6, because the key sets the hand's arrival speed
geometrically (0.02-0.5 m/s), so at 1.0 they stand 6-8 dB over the strings
while at 0.7 they sit level. That is the authored layout the Known gaps
already call unheard as a set, not a mistake, and it is left as it is.

## Low strings' fast-fading fundamentals: a listening test

The survey's largest decay outlier among the open strings is G3: its
fundamental falls 40 dB/s from 0.14 to 0.74 s where its neighbours fall
about 10, on the box's 195 Hz twist (Known gaps: *The low fundamentals fade
too soon*). The box's bending and twisting take an estimated Q of 60 for the
glued box, its back board, joints and rests, while the walls take
paulownia's own measured Q across the grain with nothing added for joints
(`woodQ`, chosen by ear on 2026-11-07). Coaldrake measured paulownia along
the grain at Q 140 (JASA 148 (2020) 3153, Table I), so 140 is the walls'
principle applied to the box. At 140, G3's fundamental falls 24 dB/s
instead of 40, F♯3 (pressed) 25.7 instead of 21.7 and F♯5 64 instead of 75;
the other keys move within 1 dB/s and their levels within 0.6 dB (velocity
0.7, both presets).

That is audible and the physics does not choose between the two pinnings,
so it went to the owner as a lettered A/B (A: shipping; key withheld from
the files), rendered through `RenderAcukotoMidi` and level-matched on
whole-file RMS. It is not committed, and nothing ships from it until the
owner chooses; `Docs/decisions.md` records the verdict then.
