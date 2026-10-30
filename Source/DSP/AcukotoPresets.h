// Acukoto's proof-of-concept instruments. Every value is a published
// measurement, a dimension typical of the instrument, or marked as
// estimated; none is a measurement of a particular koto.

#pragma once

#include "AcukotoEngine.h"

#include <cmath>

namespace acukoto
{

// Silk, the koto's original string: twisted filament, its tone generally held
// above tetron's and nylon's though it is replaced before each performance
// (Ando, J. Acoust. Soc. Jpn. (E) 10 (1989) 279). Raw silk's density is 1330
// kg/m^3. A twisted bundle bends far more easily than a solid rod of its
// diameter, as its filaments slide, so its bending modulus is no fibre's: it
// is fitted to the inharmonicity of 566 isolated notes in the professional
// recordings (Rokudan, Godan, Kudan, Kumoi Rokudan and Etsuko Chida's
// Omoigawa, whose strings are not stated), 4.15 GPa, the median (4.16) over
// the notes of B L^2 T / (pi^3 d^4 / 64) for this gauge and tension. One modulus
// sets B rising as the square of the pitch; the recordings' B rises more
// slowly, so the strings from G3 to E4 come out about 1.6 times less
// inharmonic than recorded and those from E-flat 5 up about 2.4 times more
// (Docs/validation/recording-comparison-2026-10-29.md).
// No loss modulus is published for a silk string, so it too is fitted, to the
// same recordings' decay of partials 4 to 8 on the strings from B-flat 4 to
// A5, where bending loss is the largest: 0.008 GPa sets the median log ratio
// of the model's decay rates to the recordings' to zero (the four recordings
// other than Omoigawa alone give 0.022 GPa; Omoigawa alone decays more slowly
// than this body allows with none). Nylon's loss tangent, its measured E'' of
// 0.25 GPa over its E' at this stress, 11.6 GPa (Woodhouse and Lynch-Aird,
// Acta Acustica 105 (2019) 516), would give 0.089 GPa here, and those
// partials died two to three times faster than recorded: the single modulus
// that overstates the top strings' stiffness overstates their bending loss
// with it. With a small constant loss for the friction between the twisted
// filaments (estimated).
inline constexpr StringMaterial silk { "silk", 1330.0, 4.15e9, 0.0, 1.0 / 20000.0, 0.008e9, true };

// Paulownia (kiri), the koto's wood, as Coaldrake measured it for a finite
// element model of a professional koto (JASA 148 (2020) 3153, Table I):
// 277 kg/m^3; E_L 5.6, E_R 0.63, E_T 0.26 GPa; G_LR 0.52, G_LT 0.37 GPa;
// Poisson's ratio 0.37 (Coaldrake's, assumed as for spruce).
inline constexpr WoodSpec paulownia { 277.0, 5.6e9, 0.63e9, 0.26e9, 0.52e9, 0.37e9, 0.37 };

namespace detail
{
// All thirteen strings share one gauge and about one tension, so a ji's
// distance from the ryukaku sets the pitch (Ando 1989). A string of 1.567
// g/m (1.225 mm of silk) at 216 N, about 22 kgf: the figures this model was
// built on, typical of a koto but not checked against one instrument.
// Its waves travel at sqrt(216 / 1.567e-3) = 371.3 m/s, so D3 speaks over
// 1.264 m and A5 over 0.211 m.
constexpr double waveSpeed = 371.27;

constexpr double lengthFor(int midiNote)
{
    double frequency = 440.0;
    for (int k = 69; k > midiNote; --k)
        frequency /= 1.0594630943592953;
    for (int k = 69; k < midiNote; ++k)
        frequency *= 1.0594630943592953;
    return waveSpeed / (2.0 * frequency);
}

constexpr StringSpec koto(int midiNote, double cents, double spread)
{
    StringSpec string { midiNote, lengthFor(midiNote), 1.225e-3 };
    string.cents = cents;
    string.centsSpread = spread;
    return string;
}

// Hirajoshi in D as professional players tune it. Ando (1989, Sec. 2.3,
// Fig. 4) describes the method: string 1 is set to D3; 2 is tuned a perfect
// fourth above it and 3 a perfect fifth; 4 a minor second above 3 and 6 a
// minor second above 5, by ear; 5 and 7 to 13 an octave above 1 and 2 to 8.
// Ando recorded six professional players of the Ikuta school tuning a
// professional koto to it, three times each (Sec. 3.1), and the cents are
// their average against equal temperament (Fig. 7, read from the figure to
// about half a cent): the fourths and fifths near pure, the octaves widening
// up the instrument as a piano's do, and the minor seconds near the
// Pythagorean 90 cents (Sec. 3.2(7)), so E-flat 4 sits 11.3 cents flat. The
// spreads are the standard deviation among the six (Fig. 8), largest at the
// minor seconds; string 1 is everyone's reference.
constexpr std::array<StringSpec, maxStrings> hirajoshi {{
    koto(50, 0.0, 0.0), koto(55, -1.7, 0.8), koto(57, 0.9, 1.8), koto(58, -5.1, 2.7),
    koto(62, 1.0, 2.0), koto(63, -11.3, 4.2), koto(67, -1.1, 2.0), koto(69, 2.1, 1.7),
    koto(70, -4.6, 2.8), koto(74, 2.0, 2.0), koto(75, -9.7, 4.3), koto(79, 2.2, 2.6),
    koto(81, 5.3, 2.1)
}};

// Hankumoijoshi: hirajoshi with string 8 lowered a semitone and string 9
// raised a whole tone (Ando 1989, Fig. 3). Ando measured hirajoshi alone, so
// these two follow its method (estimated): 8, A-flat 4, a minor second above
// 7 of the size the two minor seconds tuned directly averaged, 90.9 cents;
// 9, C5, a perfect fourth above 7. Their spreads add 7's to a minor second's
// own (4's and 6's over 3's and 5's, 2.9 cents) and to a fourth's (2's, 0.8).
constexpr std::array<StringSpec, maxStrings> hankumoijoshi {{
    koto(50, 0.0, 0.0), koto(55, -1.7, 0.8), koto(57, 0.9, 1.8), koto(58, -5.1, 2.7),
    koto(62, 1.0, 2.0), koto(63, -11.3, 4.2), koto(67, -1.1, 2.0), koto(68, -10.2, 3.5),
    koto(72, -3.1, 2.2), koto(74, 2.0, 2.0), koto(75, -9.7, 4.3), koto(79, 2.2, 2.6),
    koto(81, 5.3, 2.1)
}};

// The body. Coaldrake's koto is 1.83 m long (ICA 2019); a 1981 Yamada-style
// koto measured 1.84 m by 24.6 cm, 7.5 cm deep at its end and 13.6 cm in the
// middle, 7.5 kg, with two holes in its back and four struts inside (the
// struts are left out here). Estimated, then fitted with the wood above to
// the modes measured on Coaldrake's koto (JASA 2020, Table III: the first
// bending at 100 Hz, the first twist at 184-195 Hz and the second at
// 352-366): a top arched 4.5 cm across its width, the solid ends with their
// fittings 0.8 and 0.9 kg. The bending and twist fix the walls' stiffness
// over their mass but not their thickness: walls in proportion to a top of
// 22 mm, sides of 16 and a back of 8 fit them as well (100 and 241 Hz, 205
// and 348) as these do. The thickness is fitted instead to the professional
// recordings' fundamentals, which below the shell's breathing only the walls
// yielding to the ji radiate: with those thicker walls a string's
// fundamental came out a median 13.6 dB further below its second partial,
// over its first 250 ms, than in the recordings (A3 to E-flat 4 by 18 to 40
// dB), and with 0.85, 0.7, 0.6 and 0.5 of them 5.7, 0.5, 2.6 and 0.9 dB, by a
// POC script since replaced (Tools/measure_koto_notes.py --compare reads
// these walls 3.5 dB short, 2.2 against the four JpnMusic recordings). The
// thickest without the shortfall, 0.7: a top 15.4 mm thick, sides of 11.2 mm
// and a back board of 5.6 mm, the whole 5.2 kg (a 7.5 kg koto weighs its
// fittings, legs and struts too). The box then bends at 95 and 234 Hz and
// twists at 184, 195 and 320. As one shell (the top and sides carved from one log, the back
// glued under them), its section first breathes at 293 Hz, the top falling
// as the back rises and the sides bow out. The holes are estimated in place
// and fitted in size, with the walls yielding, to the cavity's two air modes
// Coaldrake identified at 85 and 157 Hz: round holes of 4.5 and 3.75 cm
// radius at 0.30 and 1.30 m give 86 and 146, the second held below 157 by
// the walls whatever the holes.
//
// The strings: the ryukaku 8 cm in from the head, the unkaku 1.50 m beyond it
// (estimated), the strings 16.5 mm apart. The ji 6 cm tall (Coaldrake), its
// feet 5.1 cm apart, of ivory, which makes the best ji (Ando 1989). A plastic
// ji of its shape weighs about 6 g (estimated), and its resonances on its
// feet are bounded by the recordings: rocking at 3.5 kHz it would take the
// upper strings' partials from 2.5 to 4.5 kHz away at hundreds of dB per
// second, where the professional recordings decay at 40 to 60; at 6 kHz, and
// heaving at 8, at 20 to 110. Ivory (1700-1930 kg/m^3 against a plastic's
// 1050-1200, a flexural modulus of 14 GPa against about 2.3) makes it 1.6
// times as heavy, 10 g, and its legs 6 times as stiff; in series with the top
// under its feet, estimated as stiff as the plastic legs, 2.7 times as stiff
// on the whole: rocking at 7.8 kHz, heaving at 10.3.
//
// Played seated on the floor, the koto lies with its tail on the floor and
// its head, the playing end, raised on two small legs (Britannica, "koto";
// the Smithsonian's and museum catalogues describe the same) or, as a koto
// guide has it, on a small pillow, "so that the instrument is balanced about
// 8cm above TATAMI-mat flooring" ("How to play the KOTO",
// www.asahi-net.or.jp/~NP5Y-HRUC/kt-play1.html): the back 8 cm over the
// floor at the head and an estimated 1 cm at the tail.
constexpr BodySpec kotoBody {
    1.83, 0.246, 0.235, 0.075, 0.136, 0.045,
    0.0154, 0.0112, 0.0056, paulownia,
    0.07, 1.76, 0.8, 0.9,
    {{ { 0.30, 0.045 }, { 1.30, 0.0375 } }},
    0.08, 1.50, 0.0165,
    0.06, 0.051, 0.010, 10300.0, 7800.0,
    0.08, 0.01
};
} // namespace detail

// Played with tsume, ivory picks on the thumb and two fingers (Ando 1989).
// The player plucks just beside the ryukaku, the fixed bridge being the
// reference for the hand (koto method books), so the tsume meets every
// string at one distance from it, not at one share of its length. The
// distance, 4.5 cm, is fitted with the tsume's release (AcukotoEngine.cpp) to
// the attacks of 724 isolated notes in eight professional recordings: their
// partials 1-8 fall a median 2.6, 6.0 and 8.0 dB per octave of partial
// number from G3 to E-flat 4, G4 to D5 and E-flat 5 to A5, and the model,
// through the Performer and measured the same way over eleven plucks of each
// string, 1.1, 5.4 and 10.3. No distance or release brings the top strings
// brighter than about 9.5 with the rest in place. It was 5 cm until 0.8.0,
// when the walls' highs radiated too strongly (AcukotoEngine.cpp,
// wallModes). One share of the length for every string, 0.075 as the CC0
// RutgerMuller sessions (Freesound 364807 and 365215) read by the same tool,
// gave 4.6, 3.7 and 5.4 at best, darkening every register alike
// (Docs/validation/attack-comparison-2026-10-30.md). outputGain is the free gain:
// 2.4 since the microphones moved from 0.5 m to 1.2 m, which kept the demos
// within about 2 dB of their earlier level, and 1.7 since the floor under
// the koto reflects what it radiates, which made both demos 2.8-3.2 dB
// louder, then 2.1 since the tsume's fitted release took 1.7-2.0 dB of
// their highs, then 1.4 since the pluck at a fixed distance, a larger share
// of the upper strings, made both demos 3.3-3.7 dB louder.
inline constexpr InstrumentSpec kotoHirajoshi {
    "Koto Hirajoshi", silk, 13, detail::hirajoshi, detail::kotoBody,
    Technique::Plectrum, 0.045, 0.3, 3.0, 0.25, 1.4, 0.0
};

inline constexpr InstrumentSpec kotoHankumoijoshi {
    "Koto Hankumoijoshi", silk, 13, detail::hankumoijoshi, detail::kotoBody,
    Technique::Plectrum, 0.045, 0.3, 3.0, 0.25, 1.4, 0.0
};

inline constexpr std::array<const InstrumentSpec*, 2> presets {{
    &kotoHirajoshi, &kotoHankumoijoshi
}};

} // namespace acukoto
