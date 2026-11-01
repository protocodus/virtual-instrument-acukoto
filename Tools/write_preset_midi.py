#!/usr/bin/env python3
"""Write the preset demo phrases to Docs/audio/midi/<NN>-<slug>.mid.

One phrase per preset in Source/DSP/AcukotoPresets.h, written for its
tuning. Each phrase's last event falls 5.5-9.5 s in, so with the demo
renderer's 2.5 s tail every demo is 8-12 s long.

The MIDI map, as the Performer plays it: a note plucks the string tuned to
it; a note one or two semitones above a string's, which no string is tuned
to, presses that string up to it behind its ji before the pluck (oshide).
A note's key-up lets the press go; the string rings on, as a koto's does.
CC64 holds every string.

Docs/audio/midi/songs/ holds longer public-domain pieces played with a
person's timing (Performance, below). They are not preset demos: render one
through the preset SONGS names, by its index in the bank, with

    build-dsp/RenderAcukotoMidi 0 Docs/audio/midi/songs/sakura-sakura.mid sakura-sakura.wav

    python3 Tools/write_preset_midi.py           # write the sources, remove stale ones
    python3 Tools/write_preset_midi.py --check   # fail unless the committed files match
"""

import argparse
import math
import pathlib
import random
import re
import struct
import sys

PPQ = 480
ROOT = pathlib.Path(__file__).resolve().parent.parent
MIDI_DIR = ROOT / "Docs/audio/midi"
SONG_DIR = MIDI_DIR / "songs"

# The strings of each tuning, 1 (the far side, lowest) to 13, as MIDI notes.
HIRAJOSHI = (50, 55, 57, 58, 62, 63, 67, 69, 70, 74, 75, 79, 81)
HANKUMOIJOSHI = (50, 55, 57, 58, 62, 63, 67, 68, 72, 74, 75, 79, 81)
# Beats a key is held: the hand leaves the string, which rings on.
KEY = 0.5


class Phrase:
    def __init__(self, bpm, strings):
        self.bpm = bpm
        self.strings = strings
        self.events = []  # (tick, order, bytes); note-offs sort first

    def ticks(self, beats):
        return round(beats * PPQ)

    def note(self, start, length, note, velocity):
        self.events.append((self.ticks(start), 1, bytes([0x90, note, velocity])))
        self.events.append((self.ticks(start + length), 0, bytes([0x80, note, 0])))

    def string(self, start, length, number, velocity):
        self.note(start, length, self.strings[number - 1], velocity)

    def smf(self):
        tempo = round(60_000_000 / self.bpm)
        track = bytearray()
        track += b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big")
        track += b"\x00\xff\x58\x04\x04\x02\x18\x08"
        last = 0
        for tick, _, data in sorted(self.events, key=lambda e: (e[0], e[1])):
            track += vlq(tick - last) + data
            last = tick
        track += b"\x00\xff\x2f\x00"
        return (b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
                + b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


def vlq(value):
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(out))


def koto_hirajoshi():
    # The opening of Sakura sakura, the melody every koto student learns in
    # hirajoshi, closed by an awase: the last note with its octave below.
    p = Phrase(96, HIRAJOSHI)
    line = [(7, 1), (7, 1), (8, 2), (7, 1), (7, 1), (8, 2),
            (7, 1), (8, 1), (9, 1), (8, 1), (7, 1), (8, 0.5), (7, 0.5), (6, 2)]
    beat = 0.25
    for index, (number, length) in enumerate(line):
        p.string(beat, KEY, number, 92 if index % 2 == 0 else 84)
        beat += length
    p.string(beat - 2, KEY, 1, 76)
    return p


def koto_hankumoijoshi():
    # A phrase that walks through the two strings hankumoijoshi retunes, 8 to
    # A-flat and 9 to C, down to an awase over the low strings.
    p = Phrase(120, HANKUMOIJOSHI)
    line = [(0.0, 5), (0.75, 6), (1.5, 7), (2.25, 8), (3.75, 7), (4.5, 9), (5.25, 10),
            (6.75, 9), (7.125, 8), (7.5, 7), (8.25, 6), (9.0, 5), (10.5, 10), (11.25, 11),
            (12.0, 12), (12.75, 11), (13.125, 10), (13.5, 9), (15.0, 8), (15.75, 7)]
    for index, (beat, number) in enumerate(line):
        p.string(0.5 + beat, KEY, number, 90 if index % 3 == 0 else 80)
    p.string(17.0, KEY, 5, 88)
    p.string(17.0, KEY, 1, 80)
    return p


# ---- Songs ---------------------------------------------------------------
#
# Longer public-domain pieces, played as a person plays them rather than as
# a sequencer does. The score is written in beats; Performance plays it:
# the tempo breathes along a curve of (beat, bpm) points, every onset lands
# a few milliseconds off the grid, a chord's notes are struck as one gesture
# and velocities wander a little around the line the score shapes. A fixed
# seed makes every run write the same bytes.


LEAD_IN = 0.3  # seconds


class Performance:
    def __init__(self, tempo, seed, timing=0.007, dynamics=4.0):
        self.tempo = sorted(tempo)  # [(beat, bpm)], linear between, held past the ends
        self.random = random.Random(seed)
        self.timing = timing        # seconds, the onset scatter's standard deviation
        self.dynamics = dynamics    # velocity steps, likewise
        self.events = []            # (seconds, order, bytes); note-offs sort first

    def bpm(self, beat):
        points = self.tempo
        if beat <= points[0][0]:
            return points[0][1]
        for (b0, t0), (b1, t1) in zip(points, points[1:]):
            if beat <= b1:
                return t0 + (t1 - t0) * (beat - b0) / (b1 - b0)
        return points[-1][1]

    def seconds(self, beat):
        """Clock time of a written beat: the integral of 60 / bpm, after a
        moment's silence before the first note."""
        total, previous = LEAD_IN, 0.0
        for b, _ in self.tempo[1:] + [(math.inf, 0)]:
            end = min(beat, b)
            if end > previous:
                t0, t1 = self.bpm(previous), self.bpm(end)
                slope = (t1 - t0) / (end - previous)
                total += (60.0 * (end - previous) / t0 if abs(slope) < 1e-9
                          else 60.0 / slope * math.log(t1 / t0))
                previous = end
            if beat <= b:
                break
        return total

    def scatter(self, scale=1.0):
        deviation = self.timing * scale
        return max(-2.5 * deviation, min(2.5 * deviation, self.random.gauss(0.0, deviation)))

    def velocity(self, value):
        return max(1, min(127, round(value + self.random.gauss(0.0, self.dynamics))))

    def at(self, beat, lean=0.0, scale=1.0):
        """A played onset: the written beat on the breathing clock, plus a
        deliberate lean in seconds and the hand's scatter."""
        return max(0.0, self.seconds(beat) + lean + self.scatter(scale))

    def message(self, seconds, data, order=1):
        self.events.append((seconds, order, bytes(data)))

    def play(self, onset, beats_held, beat, note, velocity, channel=0):
        """One note struck at `onset` seconds and let go `beats_held` written
        beats after `beat`, with the release scattered too."""
        release = max(onset + 0.04, self.seconds(beat + beats_held) + self.scatter(0.6))
        self.message(onset, (0x90 | channel, note, self.velocity(velocity)))
        self.message(release, (0x80 | channel, note, 0), 0)

    def smf(self):
        """A format 0 file at the piece's opening tempo, so it sits roughly on
        a DAW's bars; the performance's own time is in the ticks."""
        bpm = self.tempo[0][1]
        events = sorted(self.events, key=lambda e: (e[0], e[1]))
        # A key struck again before its last release lets go just before.
        struck = {}
        trimmed = []
        for index in range(len(events) - 1, -1, -1):
            seconds, order, data = events[index]
            key = (data[0] & 0x0F, data[1])
            if data[0] & 0xF0 == 0x90:
                struck[key] = seconds
            elif data[0] & 0xF0 == 0x80 and key in struck and seconds >= struck[key]:
                seconds = struck[key] - 0.004
            trimmed.append((seconds, order, data))
        events = sorted(trimmed, key=lambda e: (e[0], e[1]))
        ticks_per_second = PPQ * bpm / 60.0
        tempo = round(60_000_000 / bpm)
        track = bytearray(b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big"))
        last = 0
        for seconds, _, data in events:
            tick = max(last, round(seconds * ticks_per_second))
            track += vlq(tick - last) + data
            last = tick
        track += b"\x00\xff\x2f\x00"
        return (b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
                + b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


def breathe(bpm, phrases, swell=0.03, settle=0.07, last=0.22):
    """A tempo curve over phrases of (first beat, length in beats): each
    pushes on a little towards its middle and settles at its end; the last
    one broadens into the close."""
    points = []
    for index, (start, length) in enumerate(phrases):
        final = index == len(phrases) - 1
        # After a breath the hand takes the tempo up again within a beat.
        points += [(start if index == 0 else start + 0.75, bpm), (start + 0.55 * length, bpm * (1 + swell)),
                   (start + 0.85 * length, bpm), (start + length, bpm * (1 - (last if final else settle)))]
    return points


def sakura_sakura():
    """Sakura sakura, the Edo-period song every koto student learns, in
    hirajoshi: a sweep across the strings to open, the song with octave
    awase on its strong beats, then again an octave higher, the top B-flat
    a press on the thirteenth string, closed slowly on D. The time is
    elastic as the koto's is: each line broadens into its last note."""
    # (beat in bar, string, beats) in the low octave; a line is two bars.
    song = [
        [(0, 7, 1), (1, 7, 1), (2, 8, 2)], [(0, 7, 1), (1, 7, 1), (2, 8, 2)],
        [(0, 7, 1), (1, 8, 1), (2, 9, 1), (3, 8, 1)], [(0, 7, 1), (1, 8, 0.5), (1.5, 7, 0.5), (2, 6, 2)],
        [(0, 5, 1), (1, 4, 1), (2, 5, 1), (3, 6, 1)], [(0, 5, 1), (1, 5, 0.5), (1.5, 4, 0.5), (2, 3, 2)],
        [(0, 7, 1), (1, 8, 1), (2, 9, 1), (3, 8, 1)], [(0, 7, 1), (1, 8, 0.5), (1.5, 7, 0.5), (2, 6, 2)],
        [(0, 5, 1), (1, 4, 1), (2, 5, 1), (3, 6, 1)], [(0, 5, 1), (1, 5, 0.5), (1.5, 4, 0.5), (2, 3, 2)],
        [(0, 7, 1), (1, 7, 1), (2, 8, 2)], [(0, 7, 1), (1, 7, 1), (2, 8, 2)],
        [(0, 5, 1), (1, 6, 1), (2, 8, 0.5), (2.5, 7, 0.5), (3, 6, 1)], [(0, 5, 4)],
    ]
    # The octave a string's note sits in: below and above, by string.
    below = {3: None, 4: None, 5: 1, 6: None, 7: 2, 8: 3, 9: 4}
    above = {3: 8, 4: 9, 5: 10, 6: 11, 7: 12, 8: 13, 9: 13}  # 9 above is 13 pressed a semitone
    bars = len(song)
    intro = 4
    second = intro + 4 * bars - 2  # the second verse comes in over the first's last D
    p = Performance(breathe(66, [(0, intro)], last=0.15)
                    + breathe(66, [(intro + 8 * k, 8) for k in range(7)], swell=0.04, settle=0.14)
                    + breathe(72, [(second + 8 * k, 8) for k in range(7)], swell=0.04, settle=0.12, last=0.45),
                    seed=1888, timing=0.012, dynamics=5)

    def pluck(onset, beat, number, beats, velocity, press=0):
        note = HIRAJOSHI[number - 1] + press
        # A pressed note is held for its length, or the press lets go.
        p.play(onset, beats if press else KEY, beat, note, velocity)

    # Sararin: the tsume swept from the second string to the thirteenth,
    # quickening, then the lowest D with its octave.
    onset = p.at(0)
    for index, number in enumerate(range(2, 14)):
        pluck(onset + 0.075 * index - 0.0018 * index * index, 0, number, KEY, 62 + 2 * index)
    awase = p.at(2.5)
    pluck(awase, 2.5, 1, KEY, 84)
    pluck(awase + 0.018, 2.5, 5, KEY, 78)

    for start, octave, loud in ((intro, 0, 80), (second, 1, 88)):
        for bar, notes in enumerate(song):
            rise = 7 * math.sin(math.pi * ((bar % 2) * 4 + 2) / 8)
            for beat, number, beats in notes:
                at = start + 4 * bar + beat
                strong = beat in (0, 2)
                onset = p.at(at)
                velocity = loud + rise + (6 if strong else -4 if beat % 1 else 0)
                if octave:
                    high = above[number]
                    pluck(onset, at, high, beats, velocity, press=1 if number == 9 else 0)
                    if strong:  # awase: the octave below with it, a hair later
                        pluck(onset + 0.02, at, number, beats, velocity - 14)
                else:
                    pluck(onset, at, number, beats, velocity)
                    if strong and beats >= 2 and below[number]:
                        pluck(onset + 0.02, at, below[number], beats, velocity - 12)
    # The close: D in three octaves, rolled up slowly under the last note.
    end = second + 4 * bars - 1
    onset = p.at(end)
    for index, number in enumerate((1, 5, 10)):
        pluck(onset + 0.09 * index, end, number, KEY, 82 - 4 * index)
    return p


SONGS = {
    # file in Docs/audio/midi/songs/: (phrase, the preset it is written for)
    "sakura-sakura.mid": (sakura_sakura, "koto-hirajoshi"),
}


PHRASES = {
    "koto-hirajoshi": koto_hirajoshi,
    "koto-hankumoijoshi": koto_hankumoijoshi,
}


def preset_slugs():
    header = (ROOT / "Source/DSP/AcukotoPresets.h").read_text(encoding="utf-8")
    names = re.findall(r'inline constexpr InstrumentSpec \w+ \{\s*"([^"]+)"', header)
    return [re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") for name in names]


def last_event_seconds(phrase):
    return max(tick for tick, _, _ in phrase.events) / PPQ * 60 / phrase.bpm


def files():
    """{file name: bytes} of every source, after checking each phrase's limits."""
    slugs = preset_slugs()
    if sorted(slugs) != sorted(PHRASES):
        sys.exit(f"error: phrases {sorted(PHRASES)} do not match the bank {sorted(slugs)}")
    written = {}
    for number, slug in enumerate(slugs, 1):
        phrase = PHRASES[slug]()
        last = last_event_seconds(phrase)
        if not 5.5 <= last <= 9.5:
            sys.exit(f"error: {slug}'s last event is at {last:.2f} s, outside 5.5-9.5 s")
        playable = {n + press for n in phrase.strings for press in (0, 1, 2)}
        for _, _, data in phrase.events:
            if data[0] & 0xF0 == 0x90 and data[1] not in playable:
                sys.exit(f"error: {slug} plays note {data[1]}, which no string or press reaches")
        written[f"{number:02d}-{slug}.mid"] = phrase.smf()
    return written


def song_files():
    slugs = preset_slugs()
    written = {}
    for name, (build, preset) in SONGS.items():
        if preset not in slugs:
            sys.exit(f"error: song {name} names {preset}, which is not in the bank")
        written[name] = build().smf()
    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail unless the committed files match")
    args = parser.parse_args()
    stale = []
    for directory, expected in ((MIDI_DIR, files()), (SONG_DIR, song_files())):
        found = {path.name: path.read_bytes() for path in directory.glob("*.mid")}
        if args.check:
            stale += sorted(set(found) ^ set(expected) | {name for name in expected if found.get(name) != expected[name]})
            continue
        directory.mkdir(parents=True, exist_ok=True)
        for name in set(found) - set(expected):
            (directory / name).unlink()
        for name, data in expected.items():
            (directory / name).write_bytes(data)
            print(directory / name)
    if stale:
        sys.exit(f"Docs/audio/midi differs from {pathlib.Path(__file__).name} in: {', '.join(stale)}")


if __name__ == "__main__":
    main()
