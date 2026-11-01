# Prepared attacks and hiki-iro, 2026-11-01

This change starts from 0.14.0, commit `9756a71`. It corrects a prepared
pluck's initial wave, adds a deliberate downward hand gesture, and fixes
the low-note capacity exposed by that gesture. It does not refit the body,
silk, tuning, pluck position, loss or radiation parameters.

## Evidence and implementation

The [technique study](../research/koto-techniques-2026-11-01.md) records the
primary playing guides and fourteen new Pro Musica Nipponia reference
downloads. Two labelled hiki-iro examples lower a D4 by approximately
30–36 cents and recover. A continuous downward wheel gesture can reproduce
that contour; the one-semitone maximum is a conservative mapping supported
by written technique descriptions, not a fitted force law.

`PlayingControls::handPress` now accepts -1 to +3 semitones. The MIDI
renderer and Rack wrapper map the wheel to -1/+2, retaining the existing
upward range. Both directions act only on the last plucked string, damp
the non-speaking segment while the hand contacts it, transfer to the next
plucked string, and recover when the wheel returns to centre. Automatic
oshide and its owner-selected 103% reach are unchanged. Existing downward
wheel automation is intentionally audible in 0.15.0.

The first-trip defect is independently reproducible. An isolated-string
fixture makes the non-speaking segment long enough that none of its
reflections can return within 10 ms. A pre-held hand press and the same
master-tuning offset must then give the same initial speaking-string wave.
Before the correction, +1 and +2 semitones gave relative RMS errors of
3.24 and 4.69 dB; after including the hand ratio in the excitation period,
the waveforms match sample for sample. A -1-semitone pull also matches.
Automatic oshide is checked separately against its 103% tuning offset,
with allowance for its existing hand-force variation.

## Low-note safety and memory

The former 8192-sample storage could not hold A0 at 192 kHz after a
two-semitone downward master tune plus a one-semitone pull. ASan reproduced
a heap-buffer-overflow in the excitation fill. Master tuning down twelve
semitones without a hand gesture also reproduces the defect on the older
storage: the new gesture exposed a pre-existing boundary problem.

16384 samples cover the full supported range, including A0 with master
-12 and hand -1 at 192 kHz, requiring about 14,800 samples. Defensive upper
bounds also prevent an unsupported custom design from overflowing the
excitation or producing an excessive fractional delay. No supported pitch
is flattened to fit the buffer. Storage is allocated during construction;
the render and event paths allocate nothing.

Measured native state allocation rises from 9,426,064 to 13,325,456 bytes
per engine: an additional 7,798,784 bytes (7.4375 MiB) for the two-tuning
Performer. This is a real memory tradeoff, not a CPU optimization.

## Audio comparison and calibration regression

The external task evidence is under
`diagnostics/acukoto-realism-20261101/` in the Rack workspace. Its
`baseline-manifest.json` identifies the frozen 0.14.0 executables and
renders. Raw reference recordings remain outside both repositories.

`listening/A.wav` and `B.wav` play the same original 11.700-second MIDI
phrase through the baseline and candidate Performer at 48 kHz. It includes
a gentle downward pull, oshide, repeated notes, a post-pluck press and a
pre-held wheel. Whole-file RMS is matched to A, -35.1702 dBFS; B receives
+0.1764 dB. Peaks are -10.3734 and -10.3985 dBFS. These are phrase
measurements including gaps and tails, not Reason VU readings. The
mechanism key is separate in `listening/key.md`; the set is review evidence
and is not committed.

All thirteen dry open-string notes were compared against the frozen
baseline. The larger ring also widens the tuning solver's search interval,
so the files differ at floating-point roundoff: the largest sample
residual is 7.45e-9 full scale (about -162.6 dBFS); relative RMS residuals
range from -144.0 to -190.7 dB. This preserves the existing open-string
calibration to numerical precision. It does not claim improvement in its
documented decay or spectral-balance errors.

## Final native checks

AppleClang 21 Release passed all six CTest suites: Tuning, Engine,
DspFuzzReplay, DemoMidiSources, AudioDemos and CpuBudget. The dedicated
C++17 build passed with warnings as errors and exceptions disabled. The
ASan/UBSan build passed all four unit suites, including the new extreme
pitch, pending re-pluck and reset cases, in 487.57 seconds.

On an Apple M1 Max, the two default Performer demos at 48 kHz, 256-frame
blocks and three repeats took median realtime factors 0.22431 and 0.21789.
The reported median worst-block shares were 0.303 and 0.292 of the 5.33 ms
deadline. This is a native average-budget pass, not Reason CPU or an
exhaustive deadline-miss count. The baseline's corresponding factors were
0.23386 and 0.22198, but one baseline block exceeded its deadline while a
Python analysis was also running. The baseline is retained; no performance
improvement is attributed to this code change.

## Remaining limits

The downward gesture does not model a finger's frictional slip, measured
pulling force or different physical reach on each string. Body struts,
body drive through the ryukaku, and the changing non-speaking length of a
moved ji remain absent. Rest strokes, reverse strokes, scraping and
harmonics need distinct excitation/contact models and deliberate controls.
The new recordings support studying these next; they do not justify
inventing new body constants or automatically changing every keyboard
stroke. No koto-player listening verdict is claimed for this change.
