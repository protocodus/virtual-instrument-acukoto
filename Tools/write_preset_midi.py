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

    python3 Tools/write_preset_midi.py           # write the sources, remove stale ones
    python3 Tools/write_preset_midi.py --check   # fail unless the committed files match
"""

import argparse
import pathlib
import re
import struct
import sys

PPQ = 480
ROOT = pathlib.Path(__file__).resolve().parent.parent
MIDI_DIR = ROOT / "Docs/audio/midi"

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


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail unless the committed files match")
    args = parser.parse_args()
    expected = files()
    found = {path.name: path.read_bytes() for path in MIDI_DIR.glob("*.mid")}
    if args.check:
        stale = sorted(set(found) ^ set(expected) | {name for name in expected if found.get(name) != expected[name]})
        if stale:
            sys.exit(f"Docs/audio/midi differs from {pathlib.Path(__file__).name} in: {', '.join(stale)}")
        return
    MIDI_DIR.mkdir(parents=True, exist_ok=True)
    for name in set(found) - set(expected):
        (MIDI_DIR / name).unlink()
    for name, data in expected.items():
        (MIDI_DIR / name).write_bytes(data)
        print(MIDI_DIR / name)


if __name__ == "__main__":
    main()
