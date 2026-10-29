#!/usr/bin/env python3
"""Measure isolated koto notes alike, in a recording or in RenderAcukotoNotes
renders, so the model and the recordings are compared by one protocol.

    measure_koto_notes.py recording.wav [--min-gap 0.6] > notes.json
    measure_koto_notes.py --dir renders/ > notes.json
    measure_koto_notes.py --summary <label> notes.json [notes.json ...]
    measure_koto_notes.py --compare model.json recording.json [...]

The protocol, which the silk's stiffness and loss and the walls' thickness in
Source/DSP/AcukotoPresets.h are fitted by:

- A recording's onsets are spectral-flux peaks over an adaptive threshold
  (a 1 s median plus 0.6 standard deviations). A note counts as isolated
  when the next onset is at least --min-gap seconds later (0.6 by default)
  and the last at least 0.25 s earlier, and its first 300 ms are above
  -50 dBFS RMS. A render's single note starts where it first passes 1% of
  its peak.
- f0 is a harmonic sum over 70-1400 Hz on 30-350 ms; for a render, whose
  file is named by its MIDI note, the spectrum's peak within 3% of that
  note, since the harmonic sum can take D3's weak fundamental for its
  octave. Partials 1-15 are the
  peaks within a quarter of f0 of where the running stiff-string fit puts
  them, on 50-450 ms, kept only 18 dB over the spectrum's median.
- B is the least-squares fit of (f_n / n)^2 = f0^2 (1 + B n^2) to partials
  1-12, with at least five found.
- Each partial's level is a heterodyne envelope (40 ms Hann, 10 ms hop). Its
  decay is the slope of a line through it from 80 ms to the next onset, at
  most 2 s, while it stays 12 dB over its own floor.
- The early levels are each partial's peak over 10-60 ms. The pluck point is
  the beta whose plectrum shape, |sin(n pi beta)| / n^2, best fits partials
  1-15 of them (median offset removed, errors capped at 20 dB).
- The centroid is that of the first 250 ms; the rise is the 10-90% time of
  the Hilbert envelope (1 ms smoothing) over the first 100 ms.
- The balance is the fundamental's envelope less the second partial's,
  averaged over 10-250 ms.

--summary prints the medians by register (each note's nearest MIDI note in
38-54, 55-64, 65-74 or 75-90):
B and its quartiles, the decay of partials 1-8, the pluck point, the
centroid over f0, the rise, and the early levels of partials 1-10 against
the second. --compare sets each model note beside the recorded notes
nearest the same MIDI note: the recordings' median balance less the model's
(the fundamental's shortfall), and the log2 ratio of the model's decay rates
to the recordings' median, partial by partial.

Recordings stay outside the checkout (CLAUDE.md, Acukoto specifics). Needs
NumPy, SciPy and soundfile.
"""
import json
import math
import sys
from pathlib import Path

import numpy as np
import soundfile
from scipy import signal

REGISTERS = [(38, 54), (55, 64), (65, 74), (75, 90)]


def mono(path):
    x, rate = soundfile.read(path, dtype="float64", always_2d=True)
    return x.mean(axis=1), rate


def onsets(x, rate, hop=256, n=2048):
    _, t, z = signal.stft(x, rate, nperseg=n, noverlap=n - hop, boundary=None)
    mag = np.log1p(1000 * np.abs(z))
    flux = np.concatenate([[0], np.maximum(0, np.diff(mag, axis=1)).sum(axis=0)])
    w = int(rate / hop)
    threshold = signal.medfilt(flux, 2 * (w // 2) + 1) + 0.6 * np.std(flux)
    peaks, _ = signal.find_peaks(flux, height=threshold, distance=int(0.06 * rate / hop))
    return t[peaks] - (n / 2) / rate


def f0_estimate(seg, rate, nominal=None):
    n = 1 << 16
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    if nominal is None:
        candidates = np.geomspace(70, 1400, 3000)
        logspec = np.log(spec + 1e-9 * spec.max())
        score = np.zeros_like(candidates)
        for h in range(1, 7):
            index = np.clip(np.round(candidates * h * n / rate).astype(int), 0, len(spec) - 1)
            score += logspec[index] / h ** 0.3
        nominal = candidates[np.argmax(score)]
    k = int(round(nominal * n / rate))
    lo, hi = int(k * 0.97), int(k * 1.03) + 1
    return (lo + np.argmax(spec[lo:hi])) * rate / n


def partial_track(x, rate, start, end, f0, count=15):
    """Partial frequencies over 50-450 ms, NaN where a partial is not found."""
    seg = x[int((start + 0.05) * rate):int(min(end, start + 0.45) * rate)]
    if len(seg) < int(0.2 * rate):
        return None
    n = 1 << 17
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    floor = np.median(spec)
    freqs = []
    guess = 0.0
    for p in range(1, count + 1):
        centre = p * f0 * math.sqrt(1 + guess * p * p)
        if centre > 0.45 * rate:
            break
        lo = int((centre - 0.25 * f0) * n / rate)
        hi = int((centre + 0.25 * f0) * n / rate) + 1
        k = lo + int(np.argmax(spec[lo:hi]))
        if spec[k] < 8 * floor:
            freqs.append(np.nan)
            continue
        a, b, c = np.log(spec[k - 1:k + 2] + 1e-30)
        freqs.append((k + 0.5 * (a - c) / (a - 2 * b + c)) * rate / n)
        if p >= 3:
            ns = np.array([q for q in range(1, p + 1) if not np.isnan(freqs[q - 1])])
            if len(ns) >= 3:
                fs = np.array([freqs[q - 1] for q in ns])
                a = np.vstack([np.ones_like(ns), ns ** 2]).T
                c0, c1 = np.linalg.lstsq(a, (fs / ns) ** 2, rcond=None)[0]
                guess = max(0.0, c1 / c0)
    return np.array(freqs)


def fit_b(freqs, at_least=5):
    ns = np.arange(1, len(freqs) + 1)
    ok = ~np.isnan(freqs) & (ns <= 12)
    if ok.sum() < at_least:
        return None, None
    a = np.vstack([np.ones(ok.sum()), ns[ok] ** 2.0]).T
    c0, c1 = np.linalg.lstsq(a, (freqs[ok] / ns[ok]) ** 2, rcond=None)[0]
    return math.sqrt(c0), c1 / c0


def envelopes(x, rate, start, end, freqs, hop=0.01, window=0.04):
    seg = x[int(start * rate):int(end * rate)]
    t = np.arange(len(seg)) / rate
    kernel = np.hanning(int(window * rate))
    kernel /= kernel.sum()
    step = int(hop * rate)
    out = []
    for f in freqs:
        if np.isnan(f):
            out.append(None)
            continue
        env = np.abs(np.convolve(seg * np.exp(-2j * np.pi * f * t), kernel, mode="same"))[::step] * 2
        out.append(20 * np.log10(env + 1e-12))
    return np.arange(0, len(seg), step) / rate, out


def decay_rate(times, env, t0, t1):
    sel = (times >= t0) & (times <= t1)
    if sel.sum() < 8:
        return None
    keep = env[sel] > np.min(env) + 12
    if keep.sum() < 8:
        return None
    return -np.polyfit(times[sel][keep], env[sel][keep], 1)[0]


def pluck_point(levels):
    ns = np.arange(1, len(levels) + 1)
    ok = ~np.isnan(levels)
    if ok.sum() < 8:
        return None
    best, best_error = None, np.inf
    for beta in np.linspace(0.01, 0.5, 491):
        model = 20 * np.log10(np.abs(np.sin(ns * np.pi * beta)) / ns ** 2 + 1e-4)
        diff = levels[ok] - model[ok]
        diff -= np.median(diff)
        error = np.mean(np.minimum(np.abs(diff), 20))
        if error < best_error:
            best, best_error = beta, error
    return best


def measure(x, rate, start, end, nominal=None):
    f0 = f0_estimate(x[int((start + 0.03) * rate):int(min(end, start + 0.35) * rate)], rate, nominal)
    freqs = partial_track(x, rate, start, end, f0)
    if freqs is None:
        return None
    fitted, b = fit_b(freqs)
    times, envs = envelopes(x, rate, max(0, start - 0.02), min(end, start + 2.2), freqs)
    times = times - 0.02
    decays = [None if e is None else decay_rate(times, e, 0.08, min(end - start, 2.0) - 0.05) for e in envs]
    early = np.array([np.nan if e is None else float(np.max(e[(times >= 0.01) & (times <= 0.06)]))
                      for e in envs])
    first = (times >= 0.01) & (times <= min(0.25, end - start))
    balance = (float(np.mean(envs[0][first] - envs[1][first]))
               if len(envs) > 1 and envs[0] is not None and envs[1] is not None and first.any() else None)
    head = x[int(start * rate):int((start + 0.25) * rate)]
    spec = np.abs(np.fft.rfft(head * np.hanning(len(head))))
    centroid = float((spec * np.fft.rfftfreq(len(head), 1 / rate)).sum() / spec.sum())
    env = np.abs(signal.hilbert(x[int(max(0, start - 0.01) * rate):int((start + 0.1) * rate)]))
    env = np.convolve(env, np.ones(48) / 48, mode="same")
    rise = (np.argmax(env > 0.9 * env.max()) - np.argmax(env > 0.1 * env.max())) / rate
    f0 = fitted or f0
    return {
        "start": round(start, 3), "end": round(end, 3), "f0": f0, "midi": 69 + 12 * math.log2(f0 / 440.0),
        "B": b, "decay": decays, "early": [None if np.isnan(v) else v for v in early],
        "pluck": pluck_point(early - np.nanmax(early)), "centroid": centroid, "rise": rise,
        "balance": balance,
    }


def column(notes, key, count):
    return np.array([[n[key][k] if k < len(n[key]) and n[key][k] is not None else np.nan for k in range(count)]
                     for n in notes], float)


def summary(label, paths):
    notes = [note for path in paths for note in json.loads(Path(path).read_text())]
    print(f"== {label}: {len(notes)} notes")
    for lo, hi in REGISTERS:
        sel = [n for n in notes if lo <= round(n["midi"]) <= hi]
        if not sel:
            continue
        b = [n["B"] for n in sel if n["B"] is not None and 0 < n["B"] < 0.003]
        decay = np.nanmedian(column(sel, "decay", 8), axis=0)
        early = column(sel, "early", 10)
        early = np.nanmedian(early - early[:, [1]], axis=0)
        plucks = [n["pluck"] for n in sel if n["pluck"] is not None]
        b_text = f"{np.median(b):.2e} [{np.percentile(b, 25):.1e}, {np.percentile(b, 75):.1e}]" if b else "-"
        print(f" MIDI {lo}-{hi}: n={len(sel)}  B {b_text}  pluck {np.median(plucks):.3f}"
              f"  centroid/f0 {np.median([n['centroid'] / n['f0'] for n in sel]):.1f}"
              f"  rise {np.median([n['rise'] * 1000 for n in sel]):.1f} ms")
        print(f"   decay H1-8 dB/s {np.round(decay, 1)}")
        print(f"   early H1-10 dB re H2 {np.round(early, 1)}")


def compare(model_path, paths):
    model = json.loads(Path(model_path).read_text())
    recorded = [note for path in paths for note in json.loads(Path(path).read_text())]
    print("note  balance model/recorded  decay H1-8, model over recorded (log2)")
    shortfalls, ratios = [], []
    for note in sorted(model, key=lambda n: n["midi"]):
        midi = round(note["midi"])
        same = [n for n in recorded if round(n["midi"]) == midi]
        if not same:
            continue
        balances = [n["balance"] for n in same if n.get("balance") is not None]
        decay = np.nanmedian(column(same, "decay", 8), axis=0)
        ratio = np.log2(column([note], "decay", 8)[0] / decay)
        ratios.append(ratio)
        line = f"{midi:4d}  "
        if balances and note.get("balance") is not None:
            shortfalls.append(np.median(balances) - note["balance"])
            line += f"{note['balance']:+6.1f} / {np.median(balances):+6.1f} dB (n={len(same)})"
        print(line.ljust(40), np.round(ratio, 2))
    if shortfalls:
        print(f"median shortfall of the fundamental against the second partial: {np.median(shortfalls):.1f} dB")
    print(f"median log2 decay ratio, H1-8: {np.round(np.nanmedian(np.array(ratios), axis=0), 2)}")


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if args[0] == "--summary":
        summary(args[1], args[2:])
        return
    if args[0] == "--compare":
        compare(args[1], args[2:])
        return
    results = []
    if args[0] == "--dir":
        for path in sorted(Path(args[1]).glob("*.wav"), key=lambda p: int(p.stem)):
            x, rate = mono(path)
            start = np.argmax(np.abs(x) > 0.01 * np.abs(x).max()) / rate
            result = measure(x, rate, start, len(x) / rate, 440.0 * 2 ** ((int(path.stem) - 69) / 12))
            if result:
                result["file"] = path.name
                results.append(result)
    else:
        gap = float(args[args.index("--min-gap") + 1]) if "--min-gap" in args else 0.6
        x, rate = mono(args[0])
        times = onsets(x, rate)
        for i, t in enumerate(times):
            following = times[i + 1] if i + 1 < len(times) else len(x) / rate
            previous = times[i - 1] if i > 0 else -10
            if following - t < gap or t - previous < 0.25:
                continue
            if np.sqrt(np.mean(x[int(t * rate):int((t + 0.3) * rate)] ** 2)) < 0.003:
                continue
            result = measure(x, rate, t, following)
            if result and result["B"] is not None:
                results.append(result)
    json.dump(results, sys.stdout, indent=1, default=float)


if __name__ == "__main__":
    main()
