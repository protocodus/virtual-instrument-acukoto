# Acukoto artifact and resonance audit — 2026-11-18

A search of 0.25.21 for clicks, zipper noise, aliasing, stuck or stray
resonances and controls that do not do what they say, by render and
measurement through the shipping path. It found one defect in the model and
one in the Performer; both are fixed in 0.25.22. Everything else measured
clean or is a designed behaviour the README already describes.

## Protocol

Every render plays `acukoto::Performer` (the output stage included) from an
event script in 64-frame blocks with the Rack device's control mapping and
defaults: Koto Hirajoshi, tsume, 4.5 cm, Release 100%, Sympathy 100%, Room
25%, Width 50%, 48 kHz at 1x unless a row says otherwise. The declared
latency is trimmed. The probes were off-tree scripts; the contract the fix
has to keep is in `Tests/EngineTests.cpp` (`checkRelease`).

Levels are RMS of the stereo mean unless a row says otherwise. "Off-note" is
the energy outside 25 cents of the played note's stretched partials.

## What was found

### Release left the long strings ringing (fixed)

Since 0.25.4 a let-go string was stopped by a point contact where the tsume
had plucked it, 4.5 cm from the ryukaku: 0.036 of D3's speaking length. A
point there sits beside the node every low partial has at the string's end,
so it took the high partials and left the fundamental. The control's own
description is an amplitude time constant of 0.03 x 200^Release seconds,
30 ms at 0.

Fundamental of the played string after key-up at Release 0 (Sympathy 0,
Room 0, key-up 0.3 s after the pluck), dB re its level at key-up:

| String | 0.25.21, +0.1 / +0.3 / +1.0 s | 0.25.22, +0.1 / +0.3 / +1.0 s |
| --- | --- | --- |
| D3 | -1.5 / -4.9 / -17.0 | -13.1 / -50.0 / -169.8 |
| A3 | -2.7 / -7.9 / -25.8 | -27.6 / -72.0 / -116.6 |
| D4 | -2.7 / -10.4 / -55.6 | -18.7 / -55.8 / -153.1 |
| G4 | -7.2 / -21.7 / -152.5 | -29.0 / -89.6 / -214.8 |
| A5 | -27.6 / -63.5 / -253.9 | -36.7 / -94.4 / -258.5 |

The rate asks -29, -87 and -290 dB. The same knob stopped A5 and left D3:
at Release 30% D3's fundamental stood 9.1 dB down a second after key-up
and at Release 60% 7.6, so the control did little there.

The string's own energy (`AcukotoEngine::stringEnergy`), both tunings:

| | 0.25.21 | 0.25.22 | The rate asks |
| --- | --- | --- | --- |
| Release 0, fall 0.15 s after key-up | 8.7 (D3) to 40.9 dB | 33.4 to 48.6 dB | 43.4 dB |
| Release 50%, under the held string 0.5 s after key-up | 1.8 (D3) to 13.8 dB | 10.1 to 10.8 dB | 10.2 dB |
| Release 70%, the same | 0.7 to 5.8 dB | 3.5 to 3.7 dB | 3.6 dB |

In 0.25.22 the playing hand lies on the stretch it plucked and takes every
partial at Release's rate (`updateDamping`), as the staccato keyswitch, the
thumb's rest, Mute and Sympathy already did. The strings that still fall
short of the rate at Release 0 stand on body modes, which ring on by
themselves and give some back: G3 on the box's 195 Hz twist (33.9 dB), E♭5
beside the 611 and 615 Hz modes (33.4 dB).

What a short note leaves, 0.3-0.8 s after its key-up at Release 0 with
Sympathy and Room at 0, dB re the note:

| String | 0.25.21 | 0.25.22 | Strongest of what is left in 0.25.22 |
| --- | --- | --- | --- |
| D3 | -35.1 | -61.3 | the 94 Hz body mode |
| G3 | -27.4 | -34.7 | the box's 195 Hz twist |
| A3 | -38.9 | -62.8 | the 94 Hz body mode |
| A♯3 | -39.7 | -53.8 | 94 Hz and 231 Hz |
| D4 | -21.1 | -59.8 | the 94 Hz body mode |
| D♯4 | -26.3 | -64.1 | the 94 Hz body mode |
| G4 | -41.3 | -65.4 | the 94 Hz body mode |
| A4 | -47.7 | -62.0 | the 94 Hz body mode |
| A♯4 | -51.6 | -63.9 | the 94 Hz body mode |
| D5 | -49.5 | -51.3 | the 94 Hz body mode |
| D♯5 | -38.4 | -40.6 | the 614 Hz body mode |
| G5 | -63.3 | -63.7 | the 94 Hz body mode |
| A5 | -60.3 | -70.7 | the 94 Hz body mode |

In 0.25.21 what was left was the played string's own fundamental (294 Hz
under D3 and D4, 196 under G3, 309 under D♯4); in 0.25.22 it is the body.

Release at 100% is untouched: both README demos render to the same bytes.
The point contact's code and its `Acukoto.HandContact` suite went with it;
nothing else used them.

### A latched technique was lost at a tuning change (fixed)

Each tuning plays through its own engine, and the latches lived in the
engine. Switching the tuning with, say, Staccato latched played the next
notes plain. `Performer::setInstrument` now hands the latches to the engine
that takes over; `checkRelease` holds a staccato latched on Hirajoshi to
stop a note on Hankumoijoshi by 20 dB in 0.2 s.

## What measured clean

- **Clicks.** Key-ups at every Release setting, every control stepped while
  a chord rings (volume, width, room, mute, sympathy, the wheels, pluck
  position and angle), a tuning switch and the sustain pedal: the largest
  jump of the 5-20 kHz band over the 25 ms before it equals the same render
  with no event (5.5 dB of the string's own variation), but for a key-up at
  Release 0, 15 dB at -78 dBFS, and a volume step, 7.8 dB at -83 dBFS.
- **Zipper noise.** Each control ramped over 0.6 s and over 50 ms under a
  held D4-A4-D5, one step per 64-frame block: the second difference folded
  on the block length shows no step, 1.01-1.07 times its median on the slow
  ramps and at most 1.18 on the fast ones.
- **Tails.** A five-note chord let ring: -78 dBFS at 6 s, -126 at 10 s,
  asleep at 16.8 s from -173 dBFS, no cliff before it, with Room at 0, 25%
  and 100%.
- **Rates and oversampling.** D3, D4, D5 and A5 at 44.1, 48, 88.2, 96 and
  192 kHz and at 2x and 4x: the fundamental within 0.01 cent, the level
  within 0.02 dB, the first eight partials within 2.7 dB of 48 kHz at 1x
  (within 0.2 dB on D3, 0.8 on D4). The keys two octaves above the strings differ more
  above 10 kHz, where a 14-sample loop cannot carry the silk's loss: A7's
  third partial, at 10.6 kHz, stands 15 dB under its fundamental at 1x and
  21 dB under at 4x. No component appears at 1x that is absent at 4x.
- **The output stage.** Sweeps across every string 25 ms apart, six
  thirteen-string chords, a string repeated 20 times a second and 26 pressed
  and moved-ji keys under the pedal, all at full velocity, both tunings, at
  1x and 4x: the peak is -1.50 dBFS, every sample finite.
- **The technique keys.** Each of the seventeen latched or held over six
  notes, then let go: finite, and a plain note played afterwards differs
  from one played without them only by the hand's own scatter (below).
- **The room.** Its tail on a body strike and on stopped notes is dense
  (kurtosis 2.7-3.1 from 0.4 s) and uncorrelated between the channels
  (-0.08 to +0.25); it falls about 70 dB/s.

## What is designed, and how large it is

- **The halo.** Sympathetic strings, the stretches beyond the ji and the
  body ring with every note. Held, at the defaults with Room at 0, the
  energy off the played note's partials is 18-29 dB under the whole in the
  first 0.3 s on the thirteen strings, and 15-42 dB under from 0.5 to 1 s.
  It comes within 10 dB of the whole only once the note has died away: on
  G5 from 1.5 s, at -74 dBFS, where the stretch beyond its own ji, tuned
  near 147 Hz, is the strongest of it.
- **Pressed keys waver.** The left hand's force fluctuates, so a pressed
  note carries sidebands about 4 Hz either side, 26-30 dB under the partial.
- **No two plucks are alike.** One key at one velocity, thirty times four
  seconds apart: a standard deviation of 2.6 dB on D4 and 2.3 on A5, the
  extremes 4.6 dB over and 4.2 under the mean.
- **Body modes ring on under a stopped string**, as the table above shows:
  94 Hz everywhere, 195 Hz under G3, 614 Hz under E♭5.

None of these was changed. Whether the halo is as strong as a koto's is the
README's open question about the light, strutless body.
