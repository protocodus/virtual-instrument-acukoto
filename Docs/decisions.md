# Decisions

Choices the measurements could not close, newest first: what was decided,
what each candidate was, which the owner chose and what that licensed. A
choice made by ear is recorded as made by ear.

Comments in the engine that cite `Docs/decisions.md` with a date before
2026-10-29 refer to Aculyre's log at 0.6.0, the lyre engine Acukoto was
forked from (<https://github.com/protocodus/virtual-instrument-aculyre>).

## 2026-10-29 — Both planes of a string share its length

**Decided:** whether a string's plane of vibration parallel to the top is
longer than the plane normal to it. Two formulations were each derivable:

- **A**, the shipping 0.1.1: the parallel plane one string diameter (1.225
  mm) longer, the end correction Aculyre carried over from the guitar
  (Woodhouse, Acta Acustica 90 (2004) 945, Sec. 4.3). On the short top
  strings it puts that plane 7-10 cents below the normal one, so each top
  note beats at 4-5 Hz and settles up to 13 cents flat once the normal plane
  has given its energy to the body.
- **B**, no end correction: both planes share the string's length, and only
  the ji's pull on each separates them. No koto measurement supports the
  correction, and Acustra's owner removed the same correction by ear for high
  guitar notes (Acustra `Docs/decisions.md`, 2026-09-24, set 4).

**Test:** Koto Hirajoshi with tsume, strings 10 to 13 alone, then a slow
phrase over them. The script, preset, controls, 48 kHz rate and blocks of at
most 64 samples were identical, rendered through the engine and the
portfolio output stage. Both were set to -31 dBFS whole-file RMS before the
stage, which kept them below its limiter; after it A read -31.10 dBFS and B
-31.11, so no further trim was applied. It was not committed.

**Chosen:** B, adopted under the owner's standing rule that the candidate
wins unless the owner says otherwise; the owner had not yet listened.

**Licensed:** the shared length (`designPlanes` in
`Source/DSP/AcukotoEngine.cpp`) and the tighter sounded-pitch limit in
`Acukoto.Tuning`, 6 cents where it was 12. A measured koto string's
polarisations would reopen it.

## 2026-10-29 — The walls are thinner than a measured koto's weight suggests

**Decided:** how thick the top, sides and back are. Coaldrake's measured
bending (100 Hz) and twist (184-195 and 352-366 Hz) fix the walls' stiffness
over their mass, not their thickness, so two candidates fit them equally:

- **A**, walls in proportion to a top of 22 mm, sides of 16 mm and a back of
  8 mm, with holes of 3.0 and 2.5 cm radius (air modes 82 and 151 Hz). The
  body then weighs nearer a measured koto's 7.5 kg. Below the shell's
  breathing, a string's fundamental sat a median 13.6 dB further under its
  second partial than in the recordings, and A3 to E♭4 sat 18-40 dB further
  under it.
- **B**, those walls at 0.7 of the thickness (15.4, 11.2 and 5.6 mm), the
  thickest that removes that shortfall (about 2 dB), with holes of 4.5 and
  3.75 cm refitted to Coaldrake's air modes (86 and 146 Hz). The body weighs
  5.2 kg.

**Test:** strings 1 to 5 alone, a rising run over them, then 1 and 5
together, on Koto Hirajoshi with tsume, velocity 0.7-0.9. The script, seed,
48 kHz rate, 64-sample blocks and controls were identical, rendered through
the engine and the portfolio output stage. The pair was level-matched on
whole-file RMS, with A trimmed by -0.22 dB, both at -21.98 dBFS. It was not
committed.

**Chosen:** B. It was adopted under the owner's standing rule that the
candidate wins unless the owner says otherwise. The owner heard it in the
round 2 demos and asked for the repository to be made from them.

The shortfalls above were read by a POC script that is no longer kept. With
`Tools/measure_koto_notes.py --compare`, B's fundamentals fall a median
3.5 dB short of all five recordings and 2.2 dB short of the four JpnMusic
ones ([comparison](validation/recording-comparison-2026-10-29.md)).

**Licensed:** the fitted thickness and the lighter body (Known gaps). A
measured wall thickness or the struts would reopen it.

## 2026-10-29 — Silk strings and ivory ji

**Decided:** the materials. The owner asked for the koto's original
materials, so the strings are silk and the ji ivory, as the tsume already
were. Tetron (polyester) and nylon, the strings most koto are strung with
today, are not modelled. Silk's density is published; its bending and loss
moduli are fitted to the recordings (`silk` in `Source/DSP/AcukotoPresets.h`),
which is a measurement decision and not a choice by ear.
