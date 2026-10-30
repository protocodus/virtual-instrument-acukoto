# Acukoto

Acukoto is a physically modelled koto (箏): thirteen silk strings, each raised on its own movable ivory ji, over a hollow paulownia body built as the instrument is made, and plucked with ivory tsume. The koto has been at the heart of Japanese music for centuries, and Acukoto tries to earn its name by modelling it from what has been measured of the instrument and its materials, and by fitting what has not to five professional koto recordings. It is DSP only for now: a JUCE-free C++17 engine with a demo renderer and analysis tools. There is no VST3, AU, CLAP or standalone build yet.

## Audio demos

The main build renders these demos once Acukoto has a plug-in to build. Until then, `RenderAcukotoPresetDemos` renders them locally from the phrases in `Docs/audio/midi/` (see Build).

<!-- audio-demos-begin -->
<!-- audio-demos-end -->

## Version history

### 0.5.0 — 2026-10-30 — The tsume's attack, fitted to recordings

The tsume now lets the string go over 0.20 ms, where it took 0.05. The time is fitted to the first 60 ms of 724 isolated notes in eight professional koto recordings: their partials fall a median 4.6 dB per octave of partial number, and the model's now 4.7, where they fell 2.3. At the same loudness, a demo's 4 and 8 kHz octaves sit 6-8 dB lower and the bands to 500 Hz about 2 dB higher. The decay of the partials is unchanged ([comparison](Docs/validation/attack-comparison-2026-10-30.md)).

### 0.4.0 — 2026-10-30 — On the floor, with a living left hand

- **The koto lies on the floor.** It is played seated, its tail on the floor and its head raised on two small legs, and the floor now reflects what the body radiates. The back and the two holes face it a few centimetres away, so below a few hundred hertz they sound about twice as strong; above, the top's reflection adds a comb starting near 1 kHz. Against the demos, the bands to 500 Hz rise 1-2 dB against the rest at the same loudness. The fundamentals now sit a median 0.9 dB over the five professional recordings' balance against the second partial, where they sat 1.3 dB over.
- **A pressed note wavers.** A hand cannot hold its force still, so a held oshide moves by about 3 cents RMS on a semitone and 7 on a whole tone, mostly slowly, as a steady finger's force varies by about 1.5%.
- **Legato is ato-oshi.** A note played before the last key is up, which that ringing string reaches by its press alone, now moves the press with the string ringing on instead of plucking it again; playing the string's own note legato lets the press go.

### 0.3.0 — 2026-10-29 — The walls ring as paulownia does

Below 1 kHz, the walls' modes now lose energy at paulownia's own measured rate (Q 60 across the grain, Coaldrake), where they took a guitar's statistics (Q 23). D4, which sits on the shell's breathing at 293 Hz, rings about 6 dB louder two seconds after its pluck; the other low strings change by about 1 dB. The fundamentals now sit a median 1.3 dB over the five professional recordings' balance against the second partial.

### 0.2.0 — 2026-10-29 — Tuned as koto players tune, heard from in front

- **Tuning.** The strings are tuned as six professional players tuned hirajoshi in Ando's measurements. The fourths and fifths are near pure, the octaves widen up the instrument, and the minor seconds are narrow, so E♭4 sits 11 cents flat. Each instrument draws its own player's scatter about that.
- **No more beating on the top strings.** A string's two planes of vibration now share its length. Until now one plane was a string's diameter longer, so the top strings beat at 4-5 Hz and settled up to 13 cents flat. A press up a whole tone now lands within a cent of its note, where it fell about 13 cents short.
- **A new microphone pair.** Two cardioids in an ORTF pair, 1.2 m in front of the koto and 40° above it, replace the two microphones 0.5 m over the top. Each body mode reaches each microphone with its own phase. Against a recording of Sakura, the left-right correlation by octave band now falls with frequency as the recording's does. The fundamentals now fall a median 0.5 dB short of the five professional recordings' balance against the second partial, where they fell 3.5 dB short.

### 0.1.1 — 2026-10-29 — A host rate Acukoto cannot run at plays at 48 kHz

A sample rate outside 22.05-192 kHz, or one that is not a number, now runs the whole instrument at 48 kHz, as the engine already did. The output stage no longer takes such a rate alone. Rates a host offers are unchanged.

### 0.1.0 — 2026-10-29 — A koto, strung and built as one is made

This is the first version. It was forked from Aculyre 0.6.0's lyre engine and rebuilt as a koto: thirteen silk strings on movable ivory ji, over a hollow paulownia body with two holes in its back, played with ivory tsume. It has two tunings, hirajoshi and hankumoijoshi.

Three values are fitted to 566 isolated notes from five professional recordings: the silk's stiffness, the silk's bending loss and the body's wall thickness. Over a note's first 250 ms, the strings' fundamentals fall a median 3.5 dB short of the recordings' balance against the second partial, and 2.2 dB short of the four that sound alike. On the top three strings, the fifth to eighth partials die at 42-81 dB/s. The four recordings that sound alike give 52-68 dB/s, and the fifth gives 15-35 ([comparison](Docs/validation/recording-comparison-2026-10-29.md)).

## Downloads

<!-- build-download-begin -->
No downloads yet: Acukoto is DSP only, with no plug-in to build.
<!-- build-download-end -->

---

## How it works

The engine is Aculyre's dual-polarisation stiff-string waveguide and modal body, rebuilt around the koto's construction (`Source/DSP/AcukotoEngine.cpp`, the instruments in `Source/DSP/AcukotoPresets.h`). Every value there is one of three things: a published measurement, a dimension typical of the instrument, or a fit, marked as such. None is a measurement of one particular koto.

- **Thirteen open strings, each on its own ji.** The strings are numbered from the far side, ichi (1) to jū (10), then to, i and kin. Each runs from the ryukaku, the fixed bridge at the player's end, over its ji to the unkaku at the far end. Each polarisation of a string is two waveguides: the speaking length from the ryukaku to the ji, and the rest from the ji to the unkaku. The ji stands on the top on its two feet: it heaves with the string's motion normal to the top and rocks with its motion across it. So every string drives the body under its own ji, and through the body every other string rings in sympathy.
- **Silk strings.** All thirteen share one gauge and about one tension, so the ji's place alone sets the pitch (Ando, J. Acoust. Soc. Jpn. (E) 10 (1989) 279). The gauge is 1.225 mm of silk, at a raw silk density of 1330 kg/m³ (1.567 g/m), held at about 216 N (22 kgf). D3 speaks over 1.26 m and A5 over 0.21 m.
  - A twisted silk bundle bends far more easily than a solid rod, as its filaments slide, so its bending modulus is fitted: 4.15 GPa, the median over the recordings' notes of the stiffness their inharmonicity implies.
  - No loss modulus is published for silk either. It is fitted to the decay of the recordings' fourth to eighth partials on the strings from B♭4 to A5, where bending loss is largest: 0.008 GPa. Nylon's loss would make those partials die two to three times faster than recorded.
  - Air damping follows Woodhouse and Lynch-Aird's fit to measured polymer and gut strings (Acta Acustica 105 (2019) 516).
- **Ivory ji.** A ji is 6 cm tall (Coaldrake) with its feet 5.1 cm apart. Ivory, which makes the best ji (Ando 1989), puts it at about 10 g with legs far stiffer than a plastic ji's. On its feet it heaves at 10.3 kHz and rocks at 7.8 kHz. Lower resonances are ruled out by the recordings: a ji rocking at 3.5 kHz would take the upper strings' partials from 2.5 to 4.5 kHz away hundreds of times faster than the recordings' 40-60 dB/s.
- **A paulownia body built as a koto is.** The wood is kiri with the properties Coaldrake measured for a finite-element model of a professional koto (JASA 148 (2020) 3153, Table I). The body is 1.83 m long.
  - The top is the arched outside of the log, with its grain along the length. The top and sides are one carved shell, and the back board is glued under them with its grain across. The ends are solid, and two holes are cut in the back.
  - The box bends at 95 and 234 Hz and twists at 184, 195 and 320 Hz, against Coaldrake's measured 100, 184-195 and 352-366 Hz.
  - Those modes fix the walls' stiffness over their mass but not their thickness, so the thickness is fitted to the recordings' fundamentals. The fit gives a top of 15.4 mm, sides of 11.2 mm and a back of 5.6 mm, with the body weighing 5.2 kg.
  - The shell's section first breathes at 293 Hz. The air in the long cavity resonates through holes of 4.5 and 3.75 cm radius, at 86 and 146 Hz, against the 85 and 157 Hz Coaldrake identified.
  - Below 1 kHz, the walls' modes are damped at paulownia's own Q across the grain, 60, and the box's bending and twist likewise; Coaldrake measured 140 along the grain. What the joints and radiation add is not measured on a koto. Above 2 kHz, where the modes overlap, and for the air modes, the damping comes from the Q spread of Acustra's measured guitar bridge.
- **Played with tsume.** The ivory picks on the thumb and two fingers have an estimated rounded edge of 2 mm, released over 0.20 ms, fitted to the attacks of 724 notes in eight professional recordings ([comparison](Docs/validation/attack-comparison-2026-10-30.md)). A harder stroke slips off faster and leaves brighter, as harpists' strings do (Chadefaux et al., JASA 131 (2012) 844). The pluck lands 0.075 of the speaking length from the ryukaku. That is fitted to the first 60 ms of 109 isolated notes from three CC0 koto sessions, whose medians by register fall at 0.073-0.080. Each pluck draws its own force, place and angle within the spreads Chadefaux measured on harpists.
- **Oshide.** A note one or two semitones above a string's own note, which no string is tuned to, presses the nearest string below it behind its ji before the pluck. The press raises the string's tension and so its pitch, and the note's key-up lets the press go. Played legato, while the key of the ringing string is still down, the same note is ato-oshi instead: the press moves over about 80 ms and the string is not plucked again, and the string's own note played legato lets the press go. No hand holds a force still: the press's force fluctuates by 1.5% (young adults hold 1-2% in steady finger contractions, Galganski, Fuglevand and Enoka 1993), low-passed at an estimated 5 Hz, and the added tension by twice that. `RenderAcukotoPhrase` also plays yuri (a pulsing press) from a script.
- **Tuned by ear, as players tune.** Ando recorded six professional players of the Ikuta school tuning hirajoshi on one koto, three times each (Sec. 3.1, Figs. 7 and 8). Each string sits at their mean offset from equal temperament. The fourths and fifths are near pure, the octaves widen up the instrument, and the minor seconds sit near the Pythagorean 90 cents, so E♭4 is 11.3 cents flat. Each instrument then draws its own scatter about that from the players' spread, the same every time it is set up. Hankumoijoshi's two retuned strings follow the same method; Ando measured hirajoshi only, so they are estimated.
- **On the floor.** Played seated, the koto lies with its tail on the floor and its head raised on two small legs (estimated 8 cm under the back at the head, 1 cm at the tail; no leg height is published). The floor reflects as a rigid plane: every radiating patch is heard again from its image under it.
- **Microphones and room.** An ORTF pair stands 1.2 m in front of the koto and 40° above it, with the head on the listener's left: two cardioids 17 cm apart with 110° between them. Each mode's radiation over the box's surface and from its holes is summed at each microphone as free-field monopoles, with each patch's own delay. The pair is authored, then set beside a recording of Sakura (see Known gaps). A small room, authored rather than measured, is the space the demos are heard in. Width sets the stereo image, from mono to twice the pair's spread.
- **Playing.** A note plucks the string tuned to it. Its key-up lifts the hand and the string rings on, as a koto's does, and CC64 holds every string. A palm laid across the strings (`PlayingControls::mute`) stops them. The Performer runs at 1, 2 or 4 times the host rate and ends in the portfolio's shared output stage (`Source/DSP/OutputStage.h`).

### Presets

| # | Preset | Tuning, strings 1-13 | Playing |
| --- | --- | --- | --- |
| 1 | Koto Hirajoshi | Hirajoshi in D: D3 G3 A3 B♭3 D4 E♭4 G4 A4 B♭4 D5 E♭5 G5 A5 | Tsume |
| 2 | Koto Hankumoijoshi | Hankumoijoshi: hirajoshi with 8 on A♭4 and 9 on C5 | Tsume |

## Known gaps

- **The tuning is one school's.** It is the Ikuta school players' tuning that Ando measured. In the five professional recordings, E♭4 and E♭5 sit 14-27 cents flat and pressed notes 11-18 cents sharp. A recording of Sakura is tuned near equal temperament, within 6 cents.
- **The low fundamentals fade too soon.** D3, G3 and D4 sit on the body's own resonances: the 146 Hz air mode, the 195 Hz twist Coaldrake measured, and the shell's breathing at 293 Hz. Each lends its fundamental to the body, so the fundamental falls about 17 dB in its first half second and then decays as recorded. Measured from 80 ms, G3's fundamental dies about twice as fast as in the five recordings and D4's 1.7 times. Air damping alone, by Woodhouse and Lynch-Aird's measured law, takes 7-9 dB/s of the recordings' 10-12 dB/s, which leaves the body little room. A real koto does this too, on the strings that meet its modes, but these are the model's modes, not a measured koto's.
- **The attack's brightness tilts the wrong way across the registers.** One release time darkens every string alike, while the recordings darken with pitch. From G3 to E♭4 the attack is now 2.0 dB per octave of partial number darker than the eight recordings' median, from G4 to D5 2.3 brighter and from E♭5 up 2.6 brighter. The walls, fitted thin for the fundamentals and without their struts, are the likeliest cause.
- **The low band sags after the attack.** From 40 to 150 ms after a pluck, the band under 1 kHz falls 4.8 dB, where the recordings' falls 3 dB at most, so a note's first peak stands higher over what follows than recorded.
- **Single notes vary from the recordings.** Each partial lands on or between body modes, so one string's balance of fundamental against second partial can sit up to 15 dB either side of the recordings' median, D3 aside. A3, B♭3 and E♭5 are thin, and G3 and G5 full. A real koto does this too, but at other notes.
- **D3, string 1, sounds thin.** Its fundamental sits 27 dB further under its second partial than in the three recorded D3 notes, as that partial sits on the shell's breathing at 293 Hz.
- **The silk's loss lies between two kinds of recording.** Against the four recordings that sound alike, the third to eighth partials die 1.1-1.3 times more slowly than recorded (median over the strings); against Omoigawa, faster. The fitted loss sits between them.
- **The silk's stiffness is one number.** One bending modulus makes the inharmonicity rise as the square of the pitch. The recordings' rises more slowly, so the strings from G3 to E4 are about 1.6 times less inharmonic than recorded, and those from E♭5 up about 2.4 times more.
- **The floor's air is not on the holes.** The floor a few centimetres under the holes would load them and lower the air modes; they stay fitted to the 85 and 157 Hz Coaldrake identified, and the floor is taken as rigid, not as tatami.
- **The body is lighter than a real koto.** It weighs 5.2 kg; a measured koto with its fittings, legs and struts weighs 7.5 kg. The four struts inside are left out, and the wall thickness is fitted, not measured.
- **Only the techniques a MIDI note can name.** The MIDI map plays plucks, oshide and, legato, ato-oshi; yuri is reached only through `RenderAcukotoPhrase`. Hiki-iro, sukui-zume, glissandi and scraped strokes are not modelled.
- **It is heavy on the CPU.** A demo takes 43-49% of one 2.1 GHz Xeon core over runs, and its worst 256-sample block takes up to 1.9 times the block's 5.3 ms. These are development-machine figures, not a DAW's or Reason's.
- **Aculyre's pickup is still in the engine.** Its piezo path and sharping-lever state are not used by any preset and do not sound.
- The demo levels are not yet calibrated to Reason's meter.
- There is no plug-in, editor, screenshot, main build or release yet.

## Build

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DACUKOTO_REQUIRE_PYTHON_TESTS=ON
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp -L '^(unit|audio|perf)$' --output-on-failure
./build-dsp/RenderAcukotoPresetDemos --jobs 2 Docs/audio/midi <out-dir>
./build-dsp/RenderAcukotoMidi <preset 0-1> <in.mid> <out.wav> [--dry] [--no-sympathy] [--tail s] [--rate hz]
./build-dsp/RenderAcukotoMidi --report <preset 0-1>
./build-dsp/RenderAcukotoNotes <preset 0-1> <out-dir> [--velocity 0-1] [--seconds s]
./build-dsp/RenderAcukotoPhrase <preset 0-1> <script.txt> <out.wav>
python3 Tools/measure_koto_notes.py --dir <notes-dir> > model.json
python3 Tools/measure_koto_notes.py --compare model.json <recording.json> ...
```

CMake 3.22 and a C++20 compiler build the engine. `AcukotoDSPCxx17` compiles it as C++17 with `-Werror -fno-exceptions`, as the Rack toolchain does. `--report` prints each string's tension, inharmonicity, losses and dispersion design and the body's modes. `RenderAcukotoNotes` renders every string alone, for measuring against recorded notes. `RenderAcukotoPhrase` plays a phrase script by string number, with the left hand's presses. `Tools/measure_koto_notes.py` measures recorded and rendered notes alike and needs NumPy, SciPy and soundfile.

## Licensing

Acukoto is source-available under the PolyForm Noncommercial License 1.0.0 with an additional permission for official builds; see [LICENSE](LICENSE). The published measurements and the values fitted to recordings it uses are attributed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No recording is distributed with it.
