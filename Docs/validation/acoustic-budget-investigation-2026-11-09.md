# Acoustic radiation and mechanical loss — 2026-11-09

Investigation of upstream `9d7bda9` (0.25.4), also present in the accepted
0.25.5 attack/ownership candidate. **No radiation, modal Q, touch table or
test tolerance was changed in production.** The experiment below remains
outside the checkout. It identifies a real inconsistency and quantifies a
possible reaction mechanism; it does not establish a complete replacement
soundbox or a listening preference.

## What the diagnostic establishes

The body's mass-normalized modal coordinate obeys
`q'' + (omega/Q) q' + omega² q = F`. At unit RMS modal velocity its
mechanical dissipation is `R_mech = omega/Q` watts. The existing surface and
aperture sources give far-field pressure
`p(n,r) = rho omega W(n)/(4 pi r)`, so their integrated radiation resistance
is `R_rad = rho omega² integral(|W|² dOmega)/(16 pi² c)`.
Both resistances have units s⁻¹; multiply by squared mass-normalized RMS
velocity for watts. Peak rather than RMS convention introduces the same
factor of one half in both powers. Independent closed-form tests cover a
monopole, coherent and opposed pairs, image-source interference and the
hemisphere integral. No missing normalization or factor of two was found.

The current loss is assigned from supports, box, walls, cavity and ji. The
walls' Q=60 explicitly represents paulownia's internal material loss.
The cavity's Q=20 comes from an empirical air resonance and may already
include aperture radiation. The production radiation field includes a
rigid-floor image, but its corresponding mechanical reaction is absent.

For Hirajoshi's 299.167 Hz mode (Q=53.1199):

| Quantity | Resistance, s⁻¹ |
| --- | ---: |
| Entire prescribed mechanical loss | 35.3864 |
| Its empirical cavity-loss share | 3.4569 |
| Free-field total radiation | 44.2031 |
| Free-field wall radiation alone | 39.7393 |
| Floor-inclusive wall radiation | 73.3483 |
| Floor-inclusive holes alone | 0.5845 |
| Coherent wall/hole interference | 7.7459 |
| Floor-inclusive total radiation | 81.6787 |

These use 64×128 equal-area angular directions on the exact production
source grid. Refining 32×64 to 64×128 changes floor-inclusive total power
by 0.026%. Wall radiation alone exceeds all prescribed loss even without
the floor. Neither uncertain cavity overlap nor removal of the floor can
explain away this deficit. The floor remains an existing model assumption,
not a measured acoustic load of a real floor or tatami.

A wider 20–1200 Hz scan covers 70 modes per preset. Seven of 140 exceed
prescribed total loss: both ~299 Hz modes (2.31 times), both ~343 Hz modes
(1.04), both ~564 Hz modes (1.17), and Hirajoshi's ~338 Hz mode (1.002).
The borderline modes were refined separately. The wide scan also flags
angular-resolution uncertainty on other modes; it is not a high-frequency
or complete multimode energy certificate. Microphone directivity, output
gain, room and the output guard do not enter this calculation.

## Bounded reaction experiment

In an isolated source copy, the 298–300 Hz mode receives a positive viscous
reaction `F_rad = -R_rad q'`, using its coherent floor-inclusive radiation
resistance at resonance. This is causal and dissipative; at resonance it
assigns the emitted acoustic power a corresponding mechanical loss. It is
a narrow-band diagonal approximation, retaining the mode's geometry,
frequency, radiation amplitudes and internal wood loss.

Two variants bracket possible overlap with the empirical cavity loss:

- Retain all existing damping and add `R_rad`.
- Credit the entire existing cavity allocation before adding `R_rad`.

Neither endpoint claims to identify actual cavity internal friction. They
preserve coherent interference rather than summing wall and hole powers
as though they were unrelated. Hirajoshi Q becomes 16.057 or 16.546; the
credited uncertainty changes the answer by only about 3%. The emitted
power then accounts for about 70–72% of total modal loss. A production law
would have to apply consistently to the other relevant modes, including
their cross-mode radiation coupling and reactive loading. Selecting this
one band is a diagnostic isolation, not an admissible production fix.

## Audible consequences and unchanged gates

Both variants pass the existing tuning test without changing its limits.
They fail the existing level test: the retained-damping variant misses a
Hirajoshi string target by 0.56 dB and body-hit targets by up to
3.13/3.44 dB (Hirajoshi/Hankumoijoshi). Crediting cavity damping still misses
the body-hit targets by 3.06/3.36 dB. No target, tolerance or touch was
rewritten to admit either result.

At Hirajoshi D4, velocity 0.7, the retained-damping variant changes total
K-weighted level by only −0.136 dB, but its H2–H1 balance changes from
−4.71 to +3.93 dB. The local fundamental decay fit changes from 3.82 to
12.68 dB/s, and the 3.75–5 s stereo tail falls from −81.62 to −91.64 dBFS.
The neighboring E-flat4 tail falls by 5.87 dB. These local slopes include
coupled-mode behavior and are not asserted to be measured string T60s.
Small whole-note level change therefore does not imply preserved timbre
or sustain. Existing recording comparisons already report uneven lower
fundamental decay; no new recording comparison or by-ear verdict was
available to establish improvement.

## Artifacts and disposition

All diagnostic inputs, CSVs, source copies, builds and raw surveys are in
`/workspace/scratch/acukoto-2026-11-09/`. In particular:

- `radiation-components.csv`: wall, hole and coherent total power at
  16/32/64 angular resolutions.
- `baseline/power-20-1200.*` and `baseline/power-*-refined*.csv`: wide and
  refined modal budgets.
- `radiation-prototype/`: both cavity-credit variants, mobility sweeps,
  unchanged tuning/level logs and five-second native-note audio.
- `radiation-review/`: shipping-path A/B against untouched `9d7bda9`, with
  only the reaction mechanism changed in B; separate key, original MIDI,
  raw levels and whole-file RMS matched WAVs. This experiment is excluded
  from the accepted 0.25.5 review pair.

The positive diagonal reaction is useful evidence for the missing mechanism,
but the present acoustic approximation does not yet support a complete,
qualified causal loading model. The production budget discrepancy remains
explicitly open. Output EQ, radiation capping and a special Q for one
troublesome frequency would conceal rather than resolve it.
