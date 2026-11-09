#!/usr/bin/env python3
"""Measure AcukotoKeyboardSurvey files without interpreting loudness as quality.

RMS and peak use stereo power; no mono cancellation. K weighting uses the
first 0.6 s, resampled to 48 kHz before the BS.1770-4 filters. Partial peaks
use 0.08–0.58 s, summed stereo FFT power, within 4% of each ET harmonic.
The fundamental's early decay fits 120 ms Hann-windowed complex amplitudes
centered at 0.14–0.74 s. This is a local slope, not a T60 where beating or
rest-stretch resonances dominate; the fit residual is reported alongside it.
No comparison to a recording, perceptual verdict or pass/fail threshold is
implied. The artificial keyboard extension is included and remains labelled
as such in the validation report. Requires NumPy and SciPy.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
from pathlib import Path

os.environ.setdefault("OPENBLAS_NUM_THREADS", "1")
os.environ.setdefault("OMP_NUM_THREADS", "1")

import numpy as np
from scipy import signal


def db(samples: np.ndarray) -> float:
    return float(10.0 * np.log10(max(1.0e-30, np.mean(samples * samples))))


def measure(directory: Path, requested_rate: int | None) -> list[dict]:
    manifest_path = directory / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    rate = manifest.get("sample_rate", requested_rate)
    if rate is None:
        raise ValueError("survey has no manifest; supply --rate for older scratch renders")
    if requested_rate is not None and requested_rate != rate:
        raise ValueError("--rate disagrees with the survey manifest")
    with (directory / "metrics.csv").open() as file:
        keys = [int(row["key"]) for row in csv.DictReader(file)]
    if not keys:
        raise ValueError("empty survey")
    k_rate = 48000
    k_sos = np.array([
        [1.53512485958697, -2.69169618940638, 1.19839281085285, 1, -1.69065929318241, .73248077421585],
        [1, -2, 1, 1, -1.99004745483398, .99007225036621],
    ])
    rows = []
    for key in keys:
        samples = np.fromfile(directory / f"key-{key:03d}.f32", dtype=np.float32).reshape(-1, 2).astype(np.float64)
        if "frames" in manifest and len(samples) != manifest["frames"]:
            raise ValueError(f"key {key}: incomplete render")
        if len(samples) < rate * .81 or not np.all(np.isfinite(samples)):
            raise ValueError(f"key {key}: need at least 0.81 s of finite stereo audio")
        # The fixed SOS below sees 48 kHz at every native rendering rate.
        divisor = math.gcd(rate, k_rate)
        k_samples = (signal.resample_poly(samples, k_rate // divisor, rate // divisor, axis=0)
                     if rate != k_rate else samples)
        weighted = signal.sosfilt(k_sos, k_samples, axis=0)
        row = dict(key=key, peak=float(np.abs(samples).max()), rms_db=db(samples),
                   k_db=float(-.691 + 10 * np.log10(max(1e-30, 2 * np.mean(weighted[:28800] ** 2)))),
                   attack_db=db(samples[:round(.05 * rate)]),
                   tail_db=db(samples[3 * len(samples) // 4:]))
        if row["peak"] > 1e-10 and 50 <= key <= 108:
            cut = samples[round(.08 * rate):round(.58 * rate)]
            nfft = 2 ** int(np.ceil(np.log2(len(cut) * 8)))
            spectrum = np.sum(np.abs(np.fft.rfft(cut * np.hanning(len(cut))[:, None], n=nfft, axis=0)) ** 2, axis=1)
            frequencies = np.fft.rfftfreq(nfft, 1 / rate)
            fundamental = 440 * 2 ** ((key - 69) / 12)
            powers, pitches = [], []
            for partial in range(1, 9):
                indices = np.flatnonzero((frequencies > partial * fundamental * .96)
                                         & (frequencies < partial * fundamental * 1.04)
                                         & (frequencies < rate * .48))
                if len(indices):
                    index = indices[np.argmax(spectrum[indices])]
                    powers.append(float(10 * np.log10(max(spectrum[index], 1e-30))))
                    pitches.append(float(frequencies[index]))
                else:
                    powers.append(None)
                    pitches.append(None)
            row.update(fundamental_hz=pitches[0], cents=float(1200 * np.log2(pitches[0] / fundamental)),
                       h2_minus_h1_db=powers[1] - powers[0] if powers[1] is not None else None)
            times, levels = [], []
            for time in np.arange(.14, .741, .04):
                cut = samples[round((time - .06) * rate):round((time + .06) * rate)]
                phase = np.exp(-2j * np.pi * pitches[0] * np.arange(len(cut)) / rate) * np.hanning(len(cut))
                amplitude = np.sum(np.abs(phase @ cut) ** 2)
                times.append(time)
                levels.append(10 * np.log10(max(amplitude, 1e-30)))
            slope, intercept = np.polyfit(times, levels, 1)
            row.update(fundamental_decay_db_s=float(-slope),
                       decay_fit_rms_db=float(np.sqrt(np.mean((np.array(levels) - np.polyval([slope, intercept], times)) ** 2))))
            cut = samples[:round(.05 * rate)]
            spectrum = np.sum(np.abs(np.fft.rfft(cut * np.hanning(len(cut))[:, None], axis=0)) ** 2, axis=1)
            frequencies = np.fft.rfftfreq(len(cut), 1 / rate)
            row["attack_centroid_hz"] = float((frequencies * spectrum).sum() / max(1e-30, spectrum.sum()))
        rows.append(row)
    (directory / "analysis.json").write_text(json.dumps(rows, indent=2) + "\n")
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--rate", type=int)
    parser.add_argument("--compare", type=Path, help="directory containing the matching baseline analysis.json")
    args = parser.parse_args()
    rows = measure(args.directory, args.rate)
    pitched = [row for row in rows if 50 <= row["key"] <= 108]
    print(f"{args.directory.name}: {len(rows)} keys, peak {max(row['peak'] for row in rows):.6f}")
    if pitched:
        print(f"Pitched-key K-weighted range: {min(row['k_db'] for row in pitched):.3f} to "
              f"{max(row['k_db'] for row in pitched):.3f} dB")
    if args.compare:
        baseline = {row["key"]: row for row in json.loads((args.compare / "analysis.json").read_text())}
        if set(baseline) != {row["key"] for row in rows}:
            raise ValueError("comparison key sets differ")
        for metric in ("k_db", "peak", "fundamental_decay_db_s", "h2_minus_h1_db"):
            pairs = [(row["key"], row[metric] - baseline[row["key"]][metric]) for row in rows
                     if row.get(metric) is not None and baseline[row["key"]].get(metric) is not None]
            if pairs:
                key, difference = max(pairs, key=lambda pair: abs(pair[1]))
                print(f"Largest {metric} change: {difference:+.6g}, key {key}")


if __name__ == "__main__":
    main()
