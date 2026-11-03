# Body strikes, 2026-11-03

0.18.0 against 0.16.0 (`86028af`), by way of 0.17.0. The twelve keys under the
lowest string (D2 to C♯3), which played a moved ji until 0.16.0, are twelve
hits of a hand on the body, and the keys below them sound nothing
([research](../research/body-strikes-2026-11-03.md)). Nothing here is a
listening verdict on the layout, and no recording of a strike was compared.

## Contact

Each key struck alone on Koto Hirajoshi, the hand's compression and force traced
each sample in a copy of the engine with a print added. Contact is how long the
hand stays on the body; the peak force is the largest the hand pushes with.
The touch is each key's scale on the arrival speed (below).

| key | note | hand | place (fraction of the body from the head) | touch | contact at v 0.7 | peak force at v 0.7 |
| --- | --- | --- | --- | --- | --- | --- |
| 38 | D2 | palm | top 0.28 | 0.917 | 11.0 ms | 16 N |
| 39 | D♯2 | palm | top 0.62 | 0.737 | 11.6 ms | 12 N |
| 40 | E2 | palm | near side 0.45 | 1.492 | 9.6 ms | 30 N |
| 41 | F2 | palm | back 0.35 | 0.601 | 12.5 ms | 9 N |
| 42 | F♯2 | knuckle | top 0.22 | 1.010 | 3.8 ms | 16 N |
| 43 | G2 | knuckle | top 0.55 | 0.798 | 3.9 ms | 13 N |
| 44 | G♯2 | knuckle | near side 0.33 | 0.628 | 4.1 ms | 10 N |
| 45 | A2 | knuckle | back 0.58 | 0.403 | 4.9 ms | 5 N |
| 46 | A♯2 | rap | top 0.40 | 0.677 | 1.8 ms | 11 N |
| 47 | B2 | rap | top 0.76 | 0.700 | 1.8 ms | 12 N |
| 48 | C3 | rap | near side 0.72 | 0.711 | 1.8 ms | 13 N |
| 49 | C♯3 | rap | back 0.22 | 0.257 | 2.8 ms | 2 N |

The contact shortens as the speed rises, as a Hertzian contact does (time goes
as speed to the power -0.2): on D2 it is 14.3, 11.0 and 8.9 ms at velocities
0.3, 0.7 and 1.0, and the peak force 3.4, 15.8 and 49 N. The harder hands are
shorter by their mass and stiffness (about a third, and a sixth, of the palm's
time). The touches lower the arrival speed on the quieter keys, so the knuckle
and the rap last longer there (up to 4.9 ms and 2.8 ms) than the 3.8 ms and
1.65 ms of the lettered set the owner heard, which used one speed for every
key.

## Levels

Each key alone, from a reset engine at 48 kHz with Koto Hirajoshi's defaults and
the room off; the level is the ITU-R BS.1770-4 K-weighted mean square of both
microphones over the 0.6 s after the 10 ms lead, as `AcukotoLevels` measures
the strings (engine alone, before the Performer's output stage). The centre of
energy is the spectral centroid of the left channel over the first 0.3 s at
velocity 0.7. Four strings plucked the same way at velocity 0.7 stand at -28.5
(D3), -28.0 (G3), -28.3 (D4) and -27.5 dB (D5). Decay is the fall in level
between 0.1-0.2 s and 1.5-1.6 s after the strike. Peak is the engine's largest
sample at velocity 1.0 before the output stage.

| key | note | v 0.3 | v 0.7 | v 1.0 | peak at v 1.0 | centre of energy | decay |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 38 | D2 | -42.6 | -27.7 | -17.4 | -5.7 dBFS | 94 Hz | 48 dB |
| 39 | D♯2 | -42.3 | -27.3 | -16.8 | -5.2 dBFS | 93 Hz | 54 dB |
| 40 | E2 | -46.2 | -28.6 | -15.4 | +0.9 dBFS | 241 Hz | 35 dB |
| 41 | F2 | -42.4 | -26.9 | -15.6 | -1.3 dBFS | 102 Hz | 43 dB |
| 42 | F♯2 | -42.8 | -27.9 | -17.1 | +1.4 dBFS | 168 Hz | 30 dB |
| 43 | G2 | -40.2 | -27.4 | -17.6 | -2.6 dBFS | 105 Hz | 35 dB |
| 44 | G♯2 | -42.7 | -27.1 | -16.1 | -0.2 dBFS | 297 Hz | 37 dB |
| 45 | A2 | -40.3 | -26.1 | -15.1 | -0.3 dBFS | 217 Hz | 37 dB |
| 46 | A♯2 | -40.7 | -27.1 | -17.3 | +0.1 dBFS | 220 Hz | 27 dB |
| 47 | B2 | -40.7 | -27.1 | -17.4 | +2.5 dBFS | 313 Hz | 32 dB |
| 48 | C3 | -39.7 | -27.2 | -18.0 | +2.2 dBFS | 311 Hz | 33 dB |
| 49 | C♯3 | -38.0 | -25.0 | -15.8 | +1.9 dBFS | 299 Hz | 38 dB |

At velocity 0.7 the twelve stand 3.6 dB apart (mean -27.1 dB, against -28.1 for
the strings' mean on this preset); at 0.3 they stand 8.1 dB apart and at 1.0
2.9 dB. Without the touches (all 1.0) the same twelve stood 18.8 dB apart at 0.7:
the backs loudest (the rap's, C♯3, at -12.1 dB) and the palm on the near side
(E2) quietest (-33.5 dB).

`AcukotoLevels` takes the touches the way it takes the strings': each strike's
intended level is 80% of the way (the share the owner chose for the strings,
[decisions](../decisions.md)) from its level without a touch to the strings'
mean at the same velocity. Run after setting them (three rounds, on the
geometric mean of the two presets' touches since the table is shared), every
strike stands within 0.29 dB of its intended level on Koto Hirajoshi and
0.25 dB on Koto Hankumoijoshi; `--check` passes at its 0.5 dB limit on both.
A strike's level is not exactly linear in its speed (the contact lengthens as
it slows), which is why the touches took three rounds where the strings' take
one.

At velocity 1.0 the rap and knuckle keys peak over full scale in the engine
(up to +2.5 dBFS); the Performer's output stage holds them under -1.5 dBFS. At
velocity 0.7 the peaks stand from -16.1 to -7.5 dBFS, against -17.2 to -12.2 for
the four plucks: a hit is sharper than a note of the same level.

## What the tests check

`Acukoto.Engine` (`checkStrikes`): the keys from A0 to the one twelve under the
lowest string are free and render exact silence; each of the twelve under it
is finite and sounds (above -80 dB over the first 0.3 s), plucks no string,
and renders the same in blocks of 256 as in blocks of 1, 7, 64, 333, 2, 256
and 129; five of them, two to a hand, die away by 30 dB or more in 3 s and
stand at least 10 dB higher at velocity 0.9 than at 0.3; two places on the body
and two hands do not sound alike. `Acukoto.Tuning` reads every key from D3 up
at its pitch and requires the twelve under it to sound and the keys below them
to be silent. `Acukoto.Levels` also checks the twelve strikes against their
intended levels, within 0.5 dB, on both presets.

Two older tests changed with the behaviour. `Acukoto.Tuning` pitched the keys
below D3 (A0 and C2 among them); a hand has no pitch to read, so those keys
are now only required to sound or, below D2, to be silent. `Acukoto.Engine`'s
192 kHz case ran an extreme lowered note on key 21 to guard the delay buffer;
with A0's moved ji gone, the lowest string (D3) takes its place, which needs
about 2,800 cells against 14,800, and the buffer's size is unchanged.

## What stayed the same

The two demos render byte for byte as in 0.16.0, by both renderers:
`RenderAcukotoMidi` with `--tail 1` (SHA-256 prefixes `b4c460c253b6a136` for
Koto Hirajoshi and `4af93a8442a46a4d` for Koto Hankumoijoshi, both builds) and
`RenderAcukotoPresetDemos` (`53b0d17073585d46` and `89118d70be3c729a`): they use
no key below D3. `Acukoto.AudioDemos`, `Acukoto.Levels`, `Acukoto.Tuning`,
`Acukoto.Engine`, `Acukoto.DspFuzzReplay`, `Acukoto.DemoMidiSources` and
`Acukoto.CpuBudget` pass, and `AcukotoDSPCxx17` builds with `-Werror`; clang++
18.1 compiles the DSP with `-Wall -Wextra -Werror -fno-exceptions
-fno-builtin`.

## CPU

With no hand in contact a strike costs one branch per sample in `process()` and
two in `runModes`; during a contact (2 to 12 ms) it adds a 356-mode
multiply-add and a 356-mode dot product per sample and two table lookups. The
bank keeps twelve rows of per-mode displacement where 0.17.0 kept thirty.
`Acukoto.CpuBudget` passes. No timing was taken of the strikes themselves, and
the machine moves by more between identical runs than 0.17.0's alternating
benchmark could resolve, so no CPU difference is claimed.

## Not claimed

The sound is not compared with a recording of a hand on a koto: none is
available, and the source gives no force, contact time or place. The hands'
mass, stiffness and restitution, the speeds and the touches are estimates. The
owner chose the three hands by ear from a lettered set ([decisions](../decisions.md));
which hand and which place each key carries is the engine's layout, not yet
heard as a set. Every hand's patch is the same palm-sized area, though a knuckle
or a rap meets a smaller one.
