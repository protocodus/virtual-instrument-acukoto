# Decisions

Choices the measurements could not close, newest first: what was decided,
what each candidate was, which the owner chose and what that licensed. A
choice made by ear is recorded as made by ear.

Comments in the engine that cite `Docs/decisions.md` with a date before
2026-10-29 refer to Aculyre's log at 0.6.0, the lyre engine Acukoto was
forked from (<https://github.com/protocodus/virtual-instrument-aculyre>).

## 2026-11-08 — owner accepts the physical-model review candidates

The owner instructed: “use all candidates” after receiving the private
20-pair A/B review for Acustra, Aculele, Aculyre and Acukoto. This accepts
the B mechanisms in that review, including the combined physical/circuit,
output-calibration and stability fixes. It is the owner's selection, not an
inferred listening score or a claim of complete physical-model accuracy.

For this instrument the accepted combined DSP snapshot is `bb17121fdbd68ea734d08287e833bf91eba95fa0`.
Existing isolated A/B files and their separate keys remain frozen. Earlier
integration clips do not roll the engine back to their intermediate pins.
The rejected axial-FIR prototypes and analytical ADAA alternative were never
part of the listening candidates. Full axial-source anti-aliasing is a new
investigation; its numerical accuracy and runtime still require validation.

## 2026-11-07 — The fundamental loses what the model says

The string loss sections, Acustra's, are fitted from the second partial up
and reached the fundamental by extrapolation, so each note's fundamental
decayed at whatever the fit gave there: three to four times the constant
loss from E♭5 up, ten times on the keys from A7 up at 44.1 and 48 kHz, which
decayed twice as fast as at 96 kHz. The loop's gain now makes the
fundamental's loss exactly the air, viscous, constant and bending rates the
model gives it, as Acustra's own loop gain does for its strings: a
measurement decision, the model's own number
([measurements](validation/key-sweep-2026-11-07.md#sample-rate)). An A/B of
the keys from C6 to C8 was rendered for the owner (A 0.24.1, B 0.25.0,
level-matched on whole-file RMS, B trimmed -0.24 dB, not committed); B ships
under the owner's standing rule that the candidate wins unless the owner says
otherwise.

## 2026-11-07 — Each string at the players' mean, a moved ji at its key

By the owner's request for every key to be well tuned, each string now takes
Ando's six-player mean, without the scatter drawn about it from their spread,
and a moved ji's key its equal-tempered pitch, not its string's offset: a
measurement decision following the 2026-11-01 press decision (below). An A/B
was rendered for the owner (A the 0.23.0 engine, B this) and B ships under the
standing rule that the candidate wins; the renders are kept outside the
repository ([measurements](validation/key-sweep-2026-11-07.md#tuning)).

## 2026-11-07 — The keys take the strings' share

The pressed and moved-ji keys take the same 80% of the way to the strings'
mean as the strings and the hits (2026-11-03, below), by the owner's request
for every key to be even: a measurement decision, not a choice by ear
([measurements](validation/key-sweep-2026-11-07.md)).

## 2026-11-07 — Recordings may calibrate a commercial build

**Decided:** whether a commercial build may carry values fitted to recordings
licensed for non-commercial use (the JpnMusic and Etsuko Chida albums,
`THIRD_PARTY_NOTICES.md`). **The owner's decision:** yes. Using a recording
to calibrate the model is fine; the recordings stay outside the repositories
and are never copied into or shipped with the instrument. Not a by-ear
choice and not a measurement: a licensing decision by the owner.

## 2026-11-07 — The walls are damped as paulownia is above 2 kHz too

**Decided:** how fast the walls' modes above 2 kHz lose energy. In four
professional recordings a pluck's 1.5-6 kHz builds over 2-5 ms; the model's
answered at once, its walls there taking the median Q of Acustra's measured
guitar bridge, 10. Two pinnings from Coaldrake's measured paulownia (JASA 148
(2020) 3153, Table I) each put the 1.5-3 kHz build within the recordings'
range ([measurements](validation/attack-build-up-2026-11-07.md)):

- **A**, the shipping 0.21.0: Q 10 above 2 kHz, 60 below 1 kHz.
- **B**: Q 60, paulownia across the grain, at every frequency, as below 1
  kHz already. The 1.5-3 kHz band's first 1.5 ms sit 3.5 dB under the next
  4-12 ms, against the recordings' 3.6-4.6. The instrument radiates 2 dB more
  (5-6 dB at 2-8 kHz), with the first 250 ms' centroid nearer the recordings
  in three registers of four. The click falls from 3.8 to 2.5 dB, against the
  recordings' 4.1-8.8.
- **C**: Q 140, paulownia along the grain. The 1.5-3 kHz band sits 5.2 dB
  under; the click falls to 2.2 dB.

**Test:** Koto Hirajoshi with tsume, the thirteen strings once each 0.9 s
apart at velocity 0.7 and four of them again at 0.95, then the README demo
phrase (`Docs/audio/midi/01-koto-hirajoshi.mid`). Identical MIDI, 48 kHz,
the renderer's blocks and controls, through the Performer, its room and the
output stage; C kept B's touches. Level-matched to A on whole-file RMS: B
trimmed -2.04 and -2.03 dB, C -3.96 and -3.77 dB. Not committed.

**Chosen:** B, adopted first under the owner's standing rule that the
candidate wins unless the owner says otherwise, then confirmed by the owner
the same day ("go with candidate"). Chosen by ear.

**Licensed:** `woodQ` at 60 at every frequency in
`Source/DSP/AcukotoEngine.cpp`, and the strings' and hits' re-levelled
touches. A measured koto's mode damping would reopen it.

## 2026-11-07 — Note timing stays the MIDI's

The owner heard 0.20.0's repeated notes as still a little mechanical on a
grid-exact, one-velocity demo phrase. The same engine playing that phrase
with a player's timing (3% of each interval, Wing and Kristofferson's split)
and accents was offered as the evidence that the remainder is the input.
Asked whether the engine should scatter every note's time itself, at about
10 ms of added latency, the owner chose no: notes sound when the MIDI says,
and human timing belongs to the part. The engine's own timed strokes
(tremolo, glissandi) keep their hand's scatter.

## 2026-11-07 — The right hand's strokes, and techniques on the free keys

**Decided:** three changes to every phrase's playing, and what the keys A0 to
C♯2 do. The owner asked for twenty realism improvements grounded in research.

- The thumb's rest on the string below its stroke (Kikuchi), the stroke across
  two neighbouring strings (kakite) and the presses' minimum-jerk path were each
  rendered as an A/B against 0.18.1: A the shipping engine, B the change alone
  (ablation builds), level-matched on whole-file RMS (thumb A -0.37 dB, kakite B
  -0.12 dB, press B -0.02 dB). A fourth change, the twisted silk's axial
  modulus in the tension glide, moved pitch by under 0.1 cent and was kept as a
  correction, not heard. The sets were sent to the owner, not committed.
- **Chosen:** B in all three, under the owner's standing rule that the candidate
  wins unless the owner says otherwise. Not yet confirmed by ear.
- The seventeen techniques on A0 to C♯2 had no candidates to choose between: each
  is the guides' technique with the timing, shape or level measured on Pro
  Musica Nipponia's demonstrations ([measurements](validation/technique-recordings-2026-11-07.md))
  or an estimate where those do not reach. The layout of the keys is the
  engine's, not heard as a set.

**Licensed:** the keys A0 to C♯2 as techniques. Uchi-zume, keshi-zume and a
strike's crush were left out rather than given a contact the model cannot run.

## 2026-11-03 — The keys below the lowest string are a hand on the body

**Decided:** what the keys from A0 to C♯3 do. Until 0.16.0 each played the
lowest string with its ji moved to the key, a pitched string no koto has (the
owner's requirement of 2026-10-30). On 2026-11-03 the owner asked whether they
could carry "something that players can do on koto" instead, required every
technique and effect to be physically simulated, and chose to build them one at
a time, body strikes first.

The strike's contact is not settled by the model: the hand's mass, stiffness and
restitution are estimates, and a palm, a knuckle and a hard rap are each
defensible.

- **A**, the shipping 0.16.0: the moved ji.
- **B**, a palm: 0.3 kg, stiffness 1.36e6 N/m^1.5; contact 10.5 ms at velocity
  0.7, 8.6 ms at 1.0.
- **C**, a knuckle: 0.1 kg, 5.9e6; 3.8 and 3.2 ms.
- **D**, a hard rap: 0.05 kg, 2.4e7; 1.65 and 1.35 ms.

All three hands have a restitution of 0.4, the same speeds (0.02-0.5 m/s by
velocity) and the same places on the body.

**Test:** `places` (every key from A0 to C♯3, ascending, 0.45 s apart, velocity
90) and `groove` (eight bars of an arpeggio on the strings with strikes at C♯1,
C2 and A2) on Koto Hirajoshi, each through the Performer at its defaults and
48 kHz with `RenderAcukotoMidi`. C and D are copies of the source with the two
constants changed. Level-matched on whole-file RMS to A: B -7.99 and -4.56 dB, C
-10.73 and -5.01 dB, D -11.67 and -7.32 dB (places, then groove).

**Chosen** (by ear, after listening to the set): B, C and D, all three
("i like all, A sucks"). A is dropped: the keys below D3 no longer play a moved
ji. The owner then asked for the keys to carry "different hits (various places,
various hand positions, velocity = intensity)" and, on seeing that would fill
29 keys, limited it: "just one octave of percussion, keep other free for
future".

What that licensed, built as 0.18.0 and not yet heard as a set:

- The twelve keys under the lowest string (D2 to C♯3 on both presets) are twelve
  hits, four to each hand in order: the palm (B) on D2-F2, the knuckle (C) on
  F♯2-A2 and the rap (D) on A♯2-C♯3, each on the top's far edge twice, the near
  side once and the back once, at places 22% to 76% of the way along the body
  from the head, no two of a hand's mirroring each other about the middle. The
  keys A0 to C♯2 are free: they sound nothing, kept for the techniques to come.
  Which hand is on which key and which place is the engine's choice, not the
  owner's by ear, and open to change (`Source/DSP/AcukotoStrikes.h`).
- "Velocity = intensity": the twelve hits would stand 18.8 dB apart at one
  velocity (the back and the rap loudest), so each carries a touch on its
  arrival speed that takes 80% of the way to the strings' mean level, the share
  the owner chose for the strings (see "The scale is evened by touch" below).
  The 80% is that choice reused, not a new one for strikes. `AcukotoLevels`
  measures and checks them: all twelve within 0.3 dB of intended on both presets
  at velocity 0.7, 3.6 dB apart.
- The three hands are as heard, except that the touches lower the arrival speed
  of the harder hands, so a rap lasts a little longer than the 1.65 ms of D and
  a knuckle than the 3.8 ms of C. Heard files were level-matched by whole-file
  RMS, which scaled their levels in the same direction.

## 2026-11-03 — The scale is evened by touch

**Decided:** whether each string's touch is set so the scale sounds even.
Every touch was 1.0, so in 0.15.2 the strings plucked alone at velocity 0.7
stood 6.9 dB (Koto Hirajoshi) and 8.6 dB (Koto Hankumoijoshi) apart,
K-weighted over the first 0.6 s (`AcukotoLevels`); D3 and B♭3 sat 2.5-3.4 dB
under their scale's mean and E♭5 and D5 2.3-3.6 over, C5 of Koto
Hankumoijoshi 5.2.

- **A**, the shipping 0.15.2: every touch 1.0.
- **B**, the full correction: each string's touch brings its level to its
  scale's mean, in dB, so the scale's average touch is unchanged. Touches 0.55
  to 1.48.
- **C**, halfway: each touch the square root of B's, half of B's correction
  in dB.
- **D**, 80% of B's correction in dB: touches 0.62 to 1.37. D was not in the
  lettered set; it was rendered afterwards from the committed head.

**Test:** `scale-hirajoshi.mid` and `scale-hankumoijoshi.mid` (all thirteen
strings up and down, 0.3 s apart, velocity 90) on their own presets, and the
first 20 s of the Sakura demo on Koto Hirajoshi, each through the Performer
at its defaults and 48 kHz with `RenderAcukotoMidi`. C is a copy of the
source with the roots written into the presets. Level-matched on whole-file
RMS to A: B +0.77, +0.90 and +0.75 dB, C +0.49, +0.61 and +0.45 dB, D +0.69,
+0.82 and +0.65 dB (the three pieces in that order). Before the trim, peaks
fall from A to B by 2.3, 1.5 and 2.3 dB, to C by 1.0, 0.6 and 1.1, to D by 1.8,
1.1 and 1.8.

**Chosen:** D, by the owner's instruction after the A, B and C set was sent:
"use 80% of B correction". The owner did not say whether this was chosen by
ear, so it is not recorded as chosen by ear. D adds one number, the 80%, which
is the owner's, not a measurement's; each touch is otherwise the measurement
of one string. **Licensed:** the touches in `AcukotoPresets.h`, and
`AcukotoLevels` and `Acukoto.Levels` measuring each string against the level
D intends (`correction` in `Tools/MeasureLevels.cpp`). The target is a level,
not a recorded koto's balance; no per-note level was measured on the
recordings.

## 2026-11-01 — A press lands on its key's pitch

**Decided:** where a key's press takes its string. Since 0.11.2 a press was
103% of its interval from the string's own tuning, so a pressed note kept
that string's tuning: on Koto Hirajoshi the fifteen pressed keys, each
played alone, sat from 10.7 cents flat (E5, on the E♭5 string) to 12.3
sharp (G♯5, on G5).

- **A**, the shipping 0.15.0: 103 or 206 cents from the string's tuning.
- **B**, the key's own pitch in equal temperament, from wherever the string
  is tuned: E♭4, tuned 11.3 cents flat, is pressed 111.3 cents for E4.

**Chosen:** B, by the owner, asked whether a press should land exactly on
the note's pitch ("not sure, probably"), then told to make the change
without a lettered set; not by ear. B adds no number and retires the
owner's 3% of 2026-10-31. The same keys now sit within 3.1 cents of equal
temperament (`RenderAcukotoMidi --dry --no-sympathy`, each key held 1.5 s,
the fundamental over 0.3-1.2 s). The five professional recordings' pressed
notes sit 11-18 cents sharp, so this moves the D, G and A strings' pressed
notes away from them and the E♭ and B♭ strings' toward them.

**Licensed:** `keyPress` and `StringVoice::cents` in `AcukotoEngine.cpp`,
on the presses a key asks for (oshide and ato-oshi). The wheel's press and
pull stay semitones from the note's press, and `RenderAcukotoPhrase`'s
scripted presses keep the semitones their script gives.

## 2026-11-01 — No piezo pickup

**Decided:** whether Acukoto gets a piezo pickup, as Aculyre, the lyre it
was forked from, has. **Chosen:** no, by the owner. The piezo path left in
the engine from Aculyre stayed silent (README, Known gaps) until 0.16.0, which
removed it.

## 2026-11-01 — Overlapping keys pluck unless Legato is on

**Decided:** whether a legato step (one key held, the next one or two
semitones above its string) plays ato-oshi by default, now that
`PlayingControls::legato` (0.12.0) can turn it on or off.

- **Off by default**, as 0.12.0 ships: every key plucks unless Legato is
  on. On a keyboard notes overlap without a player meaning to slur: in a
  D4-D5 scale played with 30 ms overlaps, 4 of 15 notes were bent instead
  of plucked (0.11.1).
- **On by default**: a legato step slides the press without a new pluck
  unless Legato is turned off.

**Chosen:** Off by default, by the owner on a decision card (2026-11-01),
not by ear. Nothing changes in the engine. The Rack device's Oshide step
On, the default and every factory patch's setting, already plucks.

**Licensed:** `legato` false in `PlayingControls`'s defaults and in every
front end's default.

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

**Chosen:** C, by ear. It first shipped in 0.14.0 under the owner's
standing rule that a candidate wins over the baseline unless the owner says
otherwise; on 2026-11-01 the owner listened to the set and said C sounds
best. That choice licenses C's mechanism, the contact at the pluck point,
over the uniform stop and over no stop; it fits no number. The shipped code
reproduces C's render sample for sample on its 0.12.0 base. On 0.13.0 the soft A4's fall goes from 6.0 to 2.9 dB, D3's
from 2.4 to 0.1.

**Licensed:** the contact as the tsume's whole effect on a ringing string.
A by-ear verdict of B or A, or a measured re-pluck from a recording, would
reopen it.

## 2026-11-01 — The scanned struts, walls and holes stay out

**Decided:** whether the koto's struts, and the walls and holes Coaldrake's
CT scan shows (ICA 2019, Fig. 2), go into the body. The owner chose to look
for the struts after a gagaku koto's recordings, like the five classical
ones, gave A3 and B-flat 3 the fundamental the model lacks.

- **The struts**, as ties between the side walls at the four scanned places,
  under the top, at mid-side, at the bottom or all three, wooden or rigid.
  The scan does not show their height or which walls they join, so each
  form takes an attachment it does not give.
- **The scanned walls**: a top of 27 mm, sides of 17 and a back of 11.5,
  against the fitted 15.4, 11.2 and 5.6.
- **The scanned holes**, near the ends, with sizes refitted to the air modes.

**Measured, not by ear:** against the five recordings the mean balance
error is 8.31 dB with none of them, 8.3-10.0 with the struts and 8.64 with
the scanned walls. Ties at the bottom of the sides change nothing; every
other form takes D4 or E-flat 4 down 28-31 dB, by lifting the 299 Hz
breathing off it. The scanned walls fix A3 (-2.1 dB
against the recordings' -2.8) and lift D3 by 6 dB, but in this body they
put the twist at 210-221 Hz against Coaldrake's measured 184-195. The holes
at the scanned places give a first air mode of 60-73 Hz against her 85 Hz
([comparison](validation/scanned-body-2026-11-01.md)).

**Chosen:** none. The struts take an invented attachment, and the scanned
walls and holes break the measured modes the fitted body keeps, so no
listening test was run.

**Licensed:** nothing new. A body whose section varies along its length,
with the struts' attachment from a fuller source, would reopen it.

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

Superseded on 2026-11-01: a press lands on its key's pitch (above).

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

**Chosen:** B, adopted first under the owner's standing rule that the
candidate wins unless the owner says otherwise, then confirmed by the owner
the same day ("go with candidate"). Chosen by ear.

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

**Chosen:** B, adopted first under the owner's standing rule that the
candidate wins unless the owner says otherwise, then confirmed by the owner
the same day ("go with candidate"). Chosen by ear.

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
