# Decisions

Choices the measurements could not close, newest first: what was decided,
what each candidate was, which the owner chose and what that licensed. A
choice made by ear is recorded as made by ear.

Comments in the engine that cite `Docs/decisions.md` with a date before
2026-10-29 refer to Aculyre's log at 0.6.0, the lyre engine Acukoto was
forked from (<https://github.com/protocodus/virtual-instrument-aculyre>).

## 2026-11-01 — The tsume stops only the stretch it touches

**Decided:** what the tsume does to a string still ringing when it plucks
it again. Until 0.14.0 it stopped 70% of the whole wave, every partial
alike (Acustra's picking-hand contact, as one step), and the second review
of 0.11.1 found that repeated notes choked.

- **A**, the shipping engine (0.12.0 when rendered): the uniform 70% stop.
  Its 0.3 is authored.
- **B**, no stop: the new pluck laid over the ringing string.
- **C**, a contact at the pluck point: the tsume holds the stretch between
  it and the ryukaku still, the rest rings on. The stretch is the pluck
  distance the presets already fit to the recordings, so C adds no number.

**Test:** `repeats.mid`, D4, A3 and G4 repeated, then A4 and D3 loud and
soft again 0.35 s later, on Koto Hirajoshi at its defaults through the
Performer and output stage at 48 kHz; the candidates were compile-time
switches in `pluck()`. Level-matched on whole-file RMS to A: B -1.48 dB,
C -0.27 dB. Over 50-300 ms after the soft A4, against the 200 ms before
it, the sound fell 6.3 dB in A and 3.3 dB in B and C; after D3's, 2.3 dB
against 0.2-0.3. B and C differed by under 0.5 dB in every band measured.
The set is in the project files (`acukoto/listening/2026-10-31-repluck/`),
not committed.

**Chosen:** C, not by ear. The owner had not listened; it was adopted under
the owner's standing rule that a candidate wins over the baseline unless
the owner says otherwise. Between the two candidates, C is the one that
models the tsume's contact rather than ignoring it, and costs nothing B
does not. The shipped code reproduces C's render sample for sample on its
0.12.0 base. On 0.13.0 the soft A4's fall goes from 6.0 to 2.9 dB, D3's
from 2.4 to 0.1.

**Licensed:** the contact as the tsume's whole effect on a ringing string.
A by-ear verdict of B or A, or a measured re-pluck from a recording, would
reopen it.

## 2026-10-31 — The arched top moves with the box's bending

**Decided:** whether the box's bending changes its volume. The owner found
the lower tones unrealistic. Against professional recordings, D3's
fundamental was 20 dB too weak and D4's died 2.2 times too fast. Before
0.13.0 the box's bending moved the top and the back together, so it radiated
nothing, where Coaldrake's acoustic camera found the first bending among a
koto's strongest radiators.

- **A**, the shipping 0.12.0.
- **B**, A with the radiation's quadrature corrected. This is a correction,
  decided by measurement against the model's full radiation sum
  ([comparison](validation/low-notes-2026-10-31.md)), and it ships either
  way.
- **C**, B with the arched top coupled to the bending: the top's longitudinal
  strain energy over its arch, with no new constants. It leaves out the
  top's own give along the body, so it is the coupling's upper bound.

The measurements split. C's balance of fundamental and second partial is
the closest of the three: mean error 9.92, 9.32 and 8.31 dB against the
five recordings, and 6.86, 6.59 and 6.47 dB early shape error against 18
tracks. Its decays over 18 tracks are the furthest: 1.48x, 1.59x and 1.66x.

**Chosen:** C, by ear. The owner listened to the lettered set
(`listening-tests/2026-10-31-low-notes/` in the project files, not
committed) and judged C better (2026-11-01). C had already shipped as
0.13.0 the night before, under the project's rule that a candidate wins
unless the owner says otherwise, so nothing changes in the engine. The
measurements above do not settle the choice by themselves: C is the
closest on balance and early shape and the furthest on decay.

**Licensed:** the arch's coupling and membrane stiffness in `designBody`, and
`PhaseNetwork` for the quadrature.

## 2026-10-31 — A press reaches 103% of its interval

**Decided:** how far a key's press raises its string. Pressed by the interval
alone, a pressed note took its open string's tuning: E4 and F4 sat 13 cents
flat of equal temperament, and B3, C4 and B4 9 cents flat, as E♭4 and the B♭
strings are tuned flat in Ando's players' hirajoshi. The five professional
recordings' pressed notes sit 11-18 cents sharp.

- **A**, the shipping 0.11.1: a press of exactly 100 or 200 cents.
- **B**, 103% of the interval: 103 or 206 cents.

**Chosen:** B, by the owner, asked for after the second koto player's review
(the Rack thread, 2026-10-31). No lettered set was rendered, and the number
is the owner's, not a fit to the recordings: pressed notes still sit short
of them on the E♭ and B♭ strings.

**Licensed:** `pressReach` in `AcukotoEngine.cpp`, on the presses a key asks
for (oshide and ato-oshi). `RenderAcukotoPhrase`'s scripted presses keep the
semitones their script gives.

## 2026-10-31 — A press holds while its string rings; chords are plucked

**Decided:** what the left hand does at a pressed note's key-up, and whether
a legato step inside a chord is ato-oshi. Playing the 0.10.1 Rack device, the
owner heard notes bend that they had not asked to bend: a pressed note fell
a semitone or a whole tone at its key-up while it still rang (Release at
100%, the sustain pedal too), and a note added to a chord bent a held string
a whole tone below it instead of sounding.

- **A**, the shipping 0.10.1: the key-up lets the press go, and any held,
  sounding string the new note's press reaches takes it as ato-oshi.
- **B**, the press holds while the string sounds and lets go once it has
  died away or is plucked again; a legato step is ato-oshi only while its
  string's key is the only key down, and a chord's new note is plucked.

**Chosen:** B, by the owner, from playing the Rack device in Reason; no
lettered set was rendered. It narrows 2026-10-30's choice below: ato-oshi
stays for a single line played legato.

**Licensed:** `AcukotoEngine::noteOff` no longer releasing a press, the
press's release in `process()` when its string has died away, and the
single-key condition on the legato branch of `AcukotoEngine::noteOn`.

## 2026-10-30 — A legato step to a pressed note is ato-oshi

**Decided:** what a note played legato does when the ringing string under
the held key reaches it by its press alone (a semitone or a whole tone up
from its own note, or back down to it). Both readings are a koto technique:

- **A**, the shipping 0.3.0: the string is plucked again with the press in
  place (oshide).
- **B**, the left hand moves the press over about 80 ms with the string
  ringing on (ato-oshi), and stepping back to its own note lets the press go.
  A note an open string sounds is still plucked there.

**Test:** Koto Hirajoshi, D4 to E4 and back, then B-flat 4 to C5 and back,
each next key down 0.2 s before the last comes up, through
`RenderAcukotoPhrase` and the portfolio output stage at 48 kHz, with only
this mechanism differing. Both were normalised to -20 dBFS RMS before the
stage; after it, A was trimmed -1.06 dB to B's whole-file RMS. It was not
committed.

**Chosen:** B, by ear: the owner listened to the three 2026-10-30 sets and
kept B.

**Licensed:** the legato branch of `AcukotoEngine::noteOn` and `pressGlide`.

## 2026-10-30 — A pressed note wavers as a hand's force does

**Decided:** whether a held oshide holds its pitch exactly. Two formulations:

- **A**, the shipping 0.3.0: the press is a constant pitch ratio.
- **B**, the pressing force fluctuates by 1.5%, within the 1-2% young adults
  hold in steady finger contractions at moderate force (Galganski, Fuglevand
  and Enoka, J. Neurophysiol. 69 (1993) 2108), low-passed at an estimated
  5 Hz. The press's added tension goes as its depth squared and its force as
  the depth, so the added tension fluctuates by twice the force's share:
  about 3 cents RMS on a semitone, 7 on a whole tone.

**Test:** Koto Hirajoshi, D4 open, then E4, F4, C5 and B4 pressed, each held
2.2 s, through `RenderAcukotoPhrase` and the output stage at 48 kHz, with
only this mechanism differing. Both were normalised to -20 dBFS RMS before
the stage and read -26.15 and -26.13 dBFS after it; no further trim. It was
not committed.

**Chosen:** B, by ear: the owner listened and kept B. The 5 Hz corner is
estimated, not measured on a koto player.

**Licensed:** `pressForceSpread` and `pressWaverHz` in
`Source/DSP/AcukotoEngine.cpp`. A measured koto player's press would reopen it.

## 2026-10-30 — The koto lies on the floor

**Decided:** whether the body radiates into free air or above the floor it is
played on. The koto is played seated with its tail on the floor and its head
raised on two small legs (Britannica, "koto"), so the back and its holes face
the floor a few centimetres away.

- **A**, the shipping 0.3.0: free-field radiation.
- **B**, a rigid floor, an estimated 8 cm under the back at the head and 1 cm
  at the tail: every radiating patch is heard again from its image. With
  `RenderAcukotoNotes` and `Tools/measure_koto_notes.py`, the strings'
  fundamentals sit a median 0.9 dB over the five professional recordings'
  balance against the second partial, where A's sit 1.3 dB over.

**Test:** the Koto Hirajoshi README demo (Sakura) through the Performer and
the output stage at 48 kHz. outputGain went from 2.4 to 1.7 to hold the
demos' level (B was 2.8-3.2 dB louder before), then B was trimmed +0.18 dB to
A's whole-file RMS, -30.99 dBFS. At equal RMS, B's octave bands to 500 Hz sit
1.0-1.9 dB over A's. It was not committed.

**Chosen:** B, by ear: the owner listened and kept B.

**Licensed:** `headClearance` and `tailClearance` in `BodySpec`, the image
paths in the radiation, and outputGain 1.7. A koto on a stand would be the
same mechanism with other clearances; the floor's load on the holes is left
out (Known gaps).

## 2026-10-29 — The walls are damped as paulownia is measured

**Decided:** how fast the walls' own modes lose energy below 1 kHz. Two
pinnings were each defensible, and the recordings do not choose between them:

- **A**, the shipping 0.2.0: Q 23, the lower quartile of Acustra's measured
  guitar bridge (g21), as Aculyre's lyre has it. Against the five
  professional recordings, the strings' fundamentals decay a median 1.36
  times as fast (log2 0.44) and fall 0.5 dB short of their balance against
  the second partial. D4, on the shell's breathing at 293 Hz, decays 2.3
  times as fast.
- **B**, Q 60, paulownia's own loss across the grain, the lower of the two
  Coaldrake measured (JASA 148 (2020) 3153, Table I; 140 along it). The
  fundamentals decay 1.34 times as fast (log2 0.42) and stand 1.3 dB over
  that balance; D4 decays 1.7 times as fast. Above 2 kHz both keep the
  guitar bridge's median.

Neither is a koto measurement: A is another instrument's, and B leaves out
what the joints and radiation add. Air damping alone takes 7-9 dB/s of the
recordings' 10-12 dB/s on G3 and D4, so the body's share is small either
way. Even every body Q tripled leaves G3 and D4 1.4-1.6 times too fast:
the rest is where those strings meet the body's modes.

**Test:** Koto Hirajoshi with tsume, strings 1 to 5 alone, 2.4 s apart, then
a slow phrase over them. The script, preset, controls, 48 kHz rate and
blocks of at most 64 samples were identical, rendered through the engine and
the portfolio output stage. Both were set to -31 dBFS whole-file RMS before
the stage, which kept them below its limiter (B was 1.4 dB louder before the
trim). Measured 1.5-2.3 s after each solo pluck, B's D4 sits 6.2 dB over A's,
G3 1.3 dB over, D3 and B-flat 3 1.3 and 1.0 dB under, and A3 level. It was not
committed.

**Chosen:** B, adopted under the owner's standing rule that the candidate
wins unless the owner says otherwise; the owner had not yet listened.

**Licensed:** `woodQ` at 60 below 1 kHz in `Source/DSP/AcukotoEngine.cpp`.
A measured koto's mode damping would reopen it.

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
