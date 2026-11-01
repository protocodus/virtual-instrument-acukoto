# The scanned body against the fitted one (2026-11-01)

Acukoto 0.14.0, preset Koto Hirajoshi. The owner asked whether the koto's
struts, left out of the model, would fill the thin A3 and B-flat 3. This
note records what a published scan of a koto shows, and what each piece of
it does to the model. None of it is adopted.

## What the scan shows

Coaldrake's ICA 2019 paper ("Measurement and modelling of the Japanese koto:
problems and solutions", Fig. 2) prints two CT cross-sections of her koto and
a plan view through its cavity. Read off the printed figure (about 0.65 mm a
pixel across a section, taking the section's width as 0.24 m, and 3.1 mm a
pixel along the plan view, taking the body's length as 1.83 m):

- **The walls.** The top is a thick arched slab over a nearly flat cavity
  ceiling: 21-24 mm thick near the side walls and 30 mm at the crown, about
  26 mm across its width. The sides are 16-18 mm and the back 11-12.5 mm.
  Both sections are about 81 mm deep, so they lie towards the ends, and the
  middle may differ. The model's fitted walls are 15.4, 11.2 and 5.6 mm.
- **The struts.** Four bars across the cavity, about 2 cm wide along the
  body, at 0.44, 0.80, 1.17 and 1.54 m from the head. The plan view shows
  where they are, not how tall they are or which walls they join. Air must
  pass them: Coaldrake's 85 Hz air mode spans the cavity, so they are not
  partitions.
- **The holes.** An oval of about 10 by 13 cm centred 0.21 m from the head,
  and a rounded trapezoid of about 10 by 11 cm 1.71 m from it. The cavity
  runs from 0.12 to 1.81 m. The model's holes are round, estimated in place
  at 0.30 and 1.30 m, and fitted in size to the air modes.

## What each does to the model

Every string was rendered alone (`RenderAcukotoNotes 0 --seconds 2`) through
a probe copy of the engine, and measured against the five classical
recordings by `Tools/measure_koto_notes.py --compare` (balance: the
fundamental less the second partial over 10-250 ms).

| Note | recorded | 0.14.0 | scanned walls | ties under the top | ties at mid-side | ties at top, middle and bottom |
| --- | --- | --- | --- | --- | --- | --- |
| D3 | -0.6 | -22.6 | -16.2 | -17.8 | -17.1 | -19.3 |
| G3 | -4.3 | -3.1 | -16.8 | -14.8 | -3.6 | -6.2 |
| A3 | -2.8 | -15.8 | -2.1 | -17.8 | -16.2 | -13.2 |
| B-flat 3 | -5.0 | -16.2 | -8.8 | -3.0 | -18.8 | -10.3 |
| D4 | +3.0 | +1.5 | -12.1 | -10.9 | -29.6 | -13.3 |
| E-flat 4 | +0.5 | +9.1 | -9.0 | -18.9 | +7.9 | -19.0 |
| G5 | +1.0 | +23.3 | +20.3 | +8.8 | +9.9 | +6.8 |
| mean error, every string | | 8.31 dB | 8.64 dB | 10.01 dB | 9.86 dB | 8.88 dB |
| D3 to B-flat 3 | | 11.85 dB | 8.15 dB | 11.18 dB | 11.10 dB | 9.07 dB |
| D4 to A5 | | 6.73 dB | 8.86 dB | 9.49 dB | 9.31 dB | 8.79 dB |

The struts are modelled as ties between the two side walls that keep them
the same distance apart. Each tie is wooden, 2 cm square with its grain
along it (1e7 N/m), or rigid (1e9 N/m); the table shows the wooden, and
rigid ties move each mean by under 1.3 dB. Ties at the
bottom of the sides change nothing, since the sides barely move apart
there.

- **The scanned walls** give A3 its fundamental and lift D3 by 6 dB. They
  also lift the box's first breathing from 299 to 502 Hz, off D4, and
  D4, E-flat 4 and G3 lose 14-18 dB. In this body they put the twist at 210
  and 221 Hz, where Coaldrake measured 184-195 Hz, and the fitted walls
  give 184 and 195. Heavier walls weigh the ends' fixed masses less.
- **The struts** stop the sides breathing apart at four places. The
  299 Hz breathing that gives D4 its fundamental rises to 356-415 Hz, and
  D4 or E-flat 4 loses 28-31 dB. The bending and the twist stay put, the
  first air mode rises from 85 to 88-90 Hz, and the second from 146 to
  151-152 Hz, nearer Coaldrake's 157. Ties under the top bring B-flat 3 within 2 dB of the
  recordings and G5 from 23 dB over them to 8. No height gives a lower
  mean error than the strutless body.
- **The holes where the scan puts them**, near the ends, give a first air
  mode of 60-73 Hz for any radius from 2.5 cm to the scanned size, where
  Coaldrake measured 85 Hz. The fitted holes give 85.

## What it says

The fitted body reproduces Coaldrake's measured modes and comes closest to
the recordings. Each scanned piece, put into it, breaks one of the two.
The model's body is one uniform section with its walls' modes running the
cavity's length. A real koto's section changes along the body, and its
struts join walls the scan does not show. Using the scan faithfully would
take a body whose section varies along its length, and the struts'
attachment, which the printed figure does not give. The thin A3 and B-flat 3
stay a known gap.
