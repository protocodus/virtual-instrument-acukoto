# CPU ceiling plan, 2026-11-16

Standard U12 asks every product to keep each 256-frame block at 48 kHz under
10% of one core on the CI runner at its default settings, and a product above
it to keep a Known gap and this plan.

## Where Acukoto stands

CI run 43712337308 (`cpu.json`, ubuntu-24.04, AMD EPYC 9V74, gcc 13.3.0, 1x
oversampling, three timed renders after a warm-up):

| Workload | Real-time factor | Worst block |
| --- | --- | --- |
| 01 Koto Hirajoshi demo | 0.135 | 14.3% |
| 01 Koto Hirajoshi, stress phrase | 0.138 | 16.3% |
| 02 Koto Hankumoijoshi demo | 0.135 | 14.5% |
| 02 Koto Hankumoijoshi, stress phrase | 0.138 | 15.8% |

The worst block sits close to the mean, so the cost is the steady per-sample
work of thirteen strings and the body, not an occasional spike.

## Plan

1. Profile one stress render by stage (strings, ji/bridge coupling, body modal
   bank, radiation, room) with the shared renderer's `--cpu` and a sampling
   profiler; record the split here.
2. Take the exact-output optimisations first (vectorised modal bank, paired
   stereo filters, skipping silent strings and bands), checking every change
   bit for bit or by null test against the README demos, as 0.25.17 and 0.25.18
   did for about 10%.
3. Only then consider sound-changing savings (fewer body modes above the
   audible need, a cheaper radiation sum), each chosen by ear and recorded in
   `Docs/decisions.md`.
4. Re-measure on CI after each step; close this plan and the Known gap when the
   worst block stays under 10%.
