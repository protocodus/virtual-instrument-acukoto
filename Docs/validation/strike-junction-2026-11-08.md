# Strike junction — 2026-11-08

Acukoto 0.25.2 corrects one connection in 0.25.1 (`ef9defe`): the current
hand force now participates in the string/body junction solve. This is a
numerical boundary condition correction, not a change to the estimated
Hertz/Hunt–Crossley hand, its patch, the body geometry, its Qs, the strings'
tension model, the microphone observer or the room.

## Connection and units

The strings send arriving velocity waves to each ji. Their net force is
`F_string = drive - Zg v`, where Zg includes both stretches at each port.
The body has a present-sample mobility Y0, a history term `past`, and a
direct mobility from the hand's patch to each ji, Y0_hand. Its velocity is

```text
(I + Y0 Zg) v = Y0 drive + past + Y0_hand F_hand.
```

Previously the final term was absent: the hand entered the modes after the
solve. Their actual port velocity then differed from the velocity reflected
into every string. Setup now precomputes Y0_hand from the same rounded
modal participation and bilinear feedthrough used by the production bank.
During contact, its force adds 26 terms to the existing solve. A sample
without a hand takes the previous sequence of arithmetic unchanged.

The hand's nonlinear integrator still advances compression from the previous
surface velocity. This correction does not claim a discrete total-energy
proof for that integrator, time-varying string tension, or the whole acoustic
model.

## Independent work observer

The dedicated `AcukotoStrikeEnergyTests` build enables
`ACUKOTO_ENERGY_DIAGNOSTICS`; normal DSP and Rack builds do not. It observes
actual modal velocity, then independently compares:

- velocity at each modal endpoint with the velocity reflected into its string;
- arriving minus departing wave power, `Z(a² - b²)` in watts, with the
  string force times actual endpoint velocity;
- the summed generalized modal input work with string plus hand work.

The report integrates these powers in joules and keeps absolute residuals,
so positive and negative errors cannot cancel in a passing test. It reports
work through ports, not stored modal energy or radiated acoustic energy.
The production bank accumulates its forces and pull in single precision;
the `2e-5` relative bounds allow that rounding, not an audible energy error.

There are 156 fixed cases: both presets, 44.1/48/96 kHz, all twelve strikes
at velocities 0.3 and 0.95, from rest or over ringing strings, plus no-strike
controls. Tension modulation is disabled to isolate the junction. The worst
relative residuals in the GCC 14.2 Release run were:

| Observation | Previous routing, instrumented | Corrected routing |
| --- | ---: | ---: |
| Maximum endpoint velocity error / maximum velocity | 0.0560809 | 6.396e-7 |
| Absolute string/body work residual / absolute string work | 7.938e-4 | 1.262e-7 |
| Absolute modal/source work residual / absolute source work | 4.480e-8 | 4.480e-8 |

Removing only the new hand term from an otherwise identical diagnostic
build makes the endpoint/work regressions fail. The already reciprocal
source-to-mode calculation passes both versions. Reset clears the observer.

The Release `Tuning`, `Engine`, `StrikeEnergy`, `DspFuzzReplay`,
`DemoMidiSources`, `AudioDemos`, `Levels`, `AcousticPowerBudget` and
`AcousticPowerReport` suites pass, as does `AcukotoDSPCxx17` with
`-Wall -Wextra -Werror -fno-exceptions`. These establish their named
contracts; they do not turn the outstanding acoustic budget into a pass.

## Acoustic power diagnostic

`AcukotoAcousticPower` builds the unchanged production body design with a
setup-only observation hook. It integrates the exact radiation patches
over a full sphere without a floor and an upper hemisphere with the
production rigid-floor image. No microphone directivity, finite microphone
distance, output gain, room or limiter enters. The report includes the
current inverse-Q contributions from supports, box, walls, cavity and ji.

The mechanical resistance of a mass-normalized mode is `omega / Q`;
both mechanical and acoustic resistance are multiplied by the same squared
modal RMS velocity to obtain power. The analytic tests independently cover
a monopole, coherent pair, opposed pair, image source, solid-angle
normalization and quadratic amplitude scaling.

```sh
build-dsp/AcukotoAcousticPower --min-hz 298 --max-hz 300 --polar 64 --csv power.csv --json power.json
```

Near 299.167 Hz the floor-inclusive acoustic power is about 2.31 times
the entire prescribed mechanical loss. A ratio above one is reported,
**not a passing physical budget**, but does not fail the diagnostic command:
the present task exposes this existing gap without changing the soundbox.
Invalid/nonfinite data or an inconsistent inverse-Q decomposition do fail.
Quadrature refinement is reported, and results changing by over 1% are
flagged for a finer grid. This is an isolated-mode audit, not a complete
multimode radiation-impedance matrix or a stability certificate.

## Listening evidence

The external review folder contains A (unchanged 0.25.1) and B (the corrected
junction), native float renders, a renderer, manifest and separate key. It
plays all twelve body hits, then strings with harder/softer hits, through
the shipping Performer, preset 0, its room and output stage, with identical
events, reset seeds, 48 kHz and blocks of at most 64 samples. No listening
preference is inferred from the numerical correction.

Whole-file stereo RMS is matched after the output stage: A needs no trim;
B needs +0.00021905 dB. Native RMS is -25.0566085/-25.0568276 dBFS. The
unmatched difference is -62.9858 dB relative to A's RMS; both pass through
the existing output limiter. Review WAVs are 24-bit PCM, and native 32-bit
float WAV/data are retained. The listening verdict remains pending.
