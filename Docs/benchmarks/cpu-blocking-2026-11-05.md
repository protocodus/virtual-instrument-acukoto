# CPU round: register blocking, 2026-11-05

0.18.1 against 0.18.0 (`90212a7`), five steps, every one bit-identical. The
numbers are development-machine figures (a shared 4-thread x86-64 VM, GCC
13.3.0, Release `-O3`, no `-march`), not Reason's or a DAW's CPU. Wall-clock
figures on this VM move by 10-20% between identical runs, so instruction
counts (callgrind) come first.

## How it was checked

- **Sound:** `RenderAcukotoPresetDemos` (both demos), `RenderAcukotoMidi` on
  `songs/sakura-sakura.mid`, on the Hankumoijoshi demo and on a phrase of all
  twelve body hits with plucks, each byte-identical to 0.18.0 after every
  step, with GCC 13.3 and with Clang 18.1.
- **Design:** an off-tree hash of every mode (frequency, Q, filter,
  participations, strikes, radiation, shares), the junction's two matrices
  and every loop design (pitch, left stretch and extension keys: losses,
  dispersion, rest line delay, lag table) after `setInstrument`, both presets
  at 44.1, 48 and 96 kHz: identical to 0.18.0 after steps 4 and 5.
- `Acukoto.Tuning`, `Engine`, `DspFuzzReplay`, `DemoMidiSources`,
  `AudioDemos` and `Levels` pass; `AcukotoDSPCxx17` builds with `-Werror`.

## Playing: `AcukotoEngine::process()`

Instructions in `process()`, inclusive, for the Hankumoijoshi demo through
`RenderAcukotoMidi` (callgrind, the same run each time):

| step | `process()` Ir | change |
| --- | --- | --- |
| 0.18.0 | 26.57 G | |
| 1. the force on the modes summed in registers | 23.52 G | -11.5% |
| 2. the modes' pull on the bar, six coordinates per pass | 20.84 G | -11.4% |
| 3. the junction's two matrix products vectorized | 19.76 G | -5.2% |
| total | | **-25.6%** |

1. `runModes` added each coordinate's share into every mode's drive in
   memory, 26 passes over the bank. A block of 32 modes now takes the 26
   coordinates in turn with its sums in registers (`driveModes`), adding them
   in the same order. The force sum went from 7.4 G to 4.5 G; a microbenchmark
   put blocks of 32 to 48 modes level, 8 and 16 slower.
2. The pull read every mode's state once per coordinate. Six coordinates now
   share each load (`pullOnBar`), each with its own eight lanes in the same
   order; four took 21.29 G, six 20.84 G.
3. `Y0 drive` and `(I + Y0 Zg)^-1 rhs` were strict reductions over j and ran
   scalar. The two matrices are kept transposed (`transposeApply`), so the 26
   outputs run side by side, each summed over j in order: 2.27 G to 1.20 G.

Wall clock, `RenderAcukotoPresetDemos --cpu --repeats 3`, alternating the
builds three times (realtime factor, median of three each; 0.15 is 15% of one
core):

| preset | 0.18.0 | 0.18.1 |
| --- | --- | --- |
| Koto Hirajoshi | 0.196, 0.172, 0.171 | 0.146, 0.161, 0.152 |
| Koto Hankumoijoshi | 0.185, 0.192, 0.202 | 0.140, 0.155, 0.134 |

## Loading: `Performer::prepare()`

Designing both presets at 48 kHz (an off-tree harness around
`Performer::prepare`):

| step | Ir | wall, best of 2, three runs |
| --- | --- | --- |
| 0.18.0 | 41.59 G | 2.56, 2.88, 3.07 s |
| 4. the ji tuning's mobility and inversions | 37.84 G | |
| 5. the wall modes' triangular solves | 36.98 G | 2.48, 2.57, 2.11 s |
| total | **-11.1%** | |

4. Setup finds each stretch's reflection phase at every bend ratio (5,044
   times per load), each time summing the complex mobility over 356 modes and
   inverting two 26 x 26 complex systems. The mobility's real and imaginary
   parts are summed apart (a complex times a real keeps them apart anyway),
   and Gauss-Jordan updates only the columns after the pivot: the eliminated
   columns reach nothing it returns.
5. `generalizedEigen`'s `L^-1 K` ran a column at a time, each row a strict
   reduction. It now substitutes in place a row of every column at a time,
   eight columns in registers, each subtracting its terms in order.

## What still costs

`process()` per 19.76 G: the pull 5.3 G and the force 4.5 G (their multiply-adds
now take 3.2 to 3.5 instructions per vector of four floats), the strings' loops
3.7 G, the mode filters 2.1 G, the four radiation sums 1.6 G, the junction
1.2 G. Loading: `symmetricEigen` 12.4 G, a third of it the QL rotations.

## Tried and dropped

- Fusing the four radiation sums into two passes or one: GCC 13 stopped
  vectorizing them (19.76 G became 22.9-23.6 G; a microbenchmark of one pass
  over four sums ran 3.2 times slower than four passes).
- Applying tql2's rotations four at a time with each element in registers:
  bit-identical, but 41.1 G against 37.0 G and slower on the clock.
