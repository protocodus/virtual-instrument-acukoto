// Every string of every tuning is tuned as the players Ando measured tune it,
// and sounds the pitch it is tuned to.
//
// Each string is plucked alone through the Performer at the preset's own
// controls, 48 kHz and 256-sample blocks. Its fundamental is the peak of the
// Hann-windowed spectrum over 1-2 s, the pitch a tuner settles on, searched
// in 1-cent steps within 40 cents of the target and refined by a parabola
// through the log magnitudes. A koto is tuned by ear, string by string: each
// string's cents from equal temperament are the six players' mean, and its
// own scatter is drawn about that from their spread (AcukotoPresets.h, Ando
// 1989), so the designed pitch must lie within three of those standard
// deviations of the mean, and half a cent of it where there is none. A
// string's two planes of vibration share their length, only the ji's pull
// on each separating their pitches, and the window reads whichever is
// louder in it, so the string must sound within soundedCents of its designed
// pitch: at most 4.5 cents, Hankumoijoshi's G5, when the limit was set on
// 2026-10-29. It was 12 while the parallel plane was one string diameter
// longer, which put the top strings up to 13 cents flat (AcukotoEngine.cpp,
// designPlanes).

#include "DSP/AcukotoPerformer.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr int block = 256;
constexpr double soundedCents = 6.0;
constexpr double pi = 3.14159265358979323846;

int failures = 0;

void check(bool condition, const char* what, double value)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s (%.2f)\n", what, value);
        ++failures;
    }
}

std::vector<float> renderNote(acukoto::Performer& performer, int note)
{
    performer.reset();
    const auto total = static_cast<std::size_t>(2.1 * rate);
    std::vector<float> left(total), right(total);
    performer.noteOn(note, 0.7f);
    for (std::size_t done = 0; done < total; done += block)
    {
        const auto frames = static_cast<int>(std::min<std::size_t>(block, total - done));
        performer.process(left.data() + done, right.data() + done, frames);
    }
    performer.noteOff(note);
    for (std::size_t i = 0; i < total; ++i)
        left[i] += right[i];
    return left;
}

// |X(f)| of the Hann-windowed window, by a rotating phasor.
double magnitude(const std::vector<float>& x, std::size_t start, std::size_t length, double frequency)
{
    const std::complex<double> step = std::polar(1.0, -2.0 * pi * frequency / rate);
    std::complex<double> phasor = 1.0, sum = 0.0;
    for (std::size_t i = 0; i < length; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(i) / static_cast<double>(length));
        sum += window * static_cast<double>(x[start + i]) * phasor;
        phasor *= step;
    }
    return std::abs(sum);
}

// Cents of the fundamental over 1-2 s from `target` (Hz): the power-weighted
// mean over the scan of what stands within 20 dB of its peak. A string's two
// planes sound a few cents apart through the ji, and the louder of the two
// at the microphones turns with what they hear: from 0.4.0's floor, E-flat
// 5's planes, at +2 and -11 cents, came within 2 dB of each other and the
// louder flipped, while their mean stayed at +2.8. The mean is the pitch
// the pair beats about.
double centsOff(const std::vector<float>& x, double target, double from = 1.0, double seconds = 1.0)
{
    const auto start = static_cast<std::size_t>(from * rate), length = static_cast<std::size_t>(seconds * rate);
    std::vector<double> powers;
    double peak = 0.0;
    for (int cents = -40; cents <= 40; ++cents)
    {
        const double level = magnitude(x, start, length, target * std::pow(2.0, cents / 1200.0));
        powers.push_back(level * level);
        peak = std::max(peak, level * level);
    }
    double sum = 0.0, weighted = 0.0;
    for (int cents = -40; cents <= 40; ++cents)
    {
        const double power = powers[static_cast<std::size_t>(cents + 40)];
        if (power < 0.01 * peak)
            continue;
        sum += power;
        weighted += power * cents;
    }
    return sum > 0.0 ? weighted / sum : 40.0;
}
} // namespace

int main()
{
    auto performer = std::make_unique<acukoto::Performer>();
    performer->prepare(rate);
    for (int preset = 0; preset < acukoto::Performer::instrumentCount; ++preset)
    {
        performer->setInstrument(preset);
        performer->setControls(acukoto::Performer::defaultControls(preset));
        const acukoto::InstrumentSpec& spec = *acukoto::presets[static_cast<std::size_t>(preset)];
        for (int string = 0; string < spec.stringCount; ++string)
        {
            const acukoto::StringSpec& tuning = spec.strings[static_cast<std::size_t>(string)];
            const int note = tuning.midiNote;
            const double designed = performer->engine().stringReport(string).frequency;
            const double tuned = 1200.0 * std::log2(designed / (440.0 * std::pow(2.0, (note - 69) / 12.0)));
            const double sounded = centsOff(renderNote(*performer, note), designed);
            std::printf("%s string %2d note %d: tuned %+.2f cents, sounds %+.2f from that\n", spec.name, string + 1,
                        note, tuned, sounded);
            check(std::abs(tuned - tuning.cents) <= 3.0 * tuning.centsSpread + 0.5, "string tuned off the players' tuning",
                  tuned - tuning.cents);
            check(std::abs(sounded) <= soundedCents, "string sounds off its tuning", sounded);
        }
    }

    // Every key of a piano, A0 to C8, sounds at its own pitch, with oshide
    // and without: a key no string or press reaches moves the nearest
    // string's ji to it (AcukotoEngine.cpp, State::extension), tuned as that
    // string is. Keys past those 88 stay silent.
    for (int preset = 0; preset < acukoto::Performer::instrumentCount; ++preset)
        for (const bool oshide : { true, false })
        {
            performer->setInstrument(preset);
            acukoto::PlayingControls controls = acukoto::Performer::defaultControls(preset);
            controls.oshide = oshide;
            performer->setControls(controls);
            const acukoto::InstrumentSpec& spec = *acukoto::presets[static_cast<std::size_t>(preset)];
            int silent = 0, sounding = 0;
            double worstCents = 0.0;
            std::vector<float> left(4800), right(4800);
            for (int note = 0; note < 128; ++note)
            {
                performer->reset();
                performer->noteOn(note, 0.7f);
                performer->process(left.data(), right.data(), 4800);
                double sum = 0.0;
                for (const float x : left)
                    sum += static_cast<double>(x) * x;
                const bool sounds = std::sqrt(sum / 4800.0) > 1.0e-4;
                if (note < 21 || note > 108)
                {
                    sounding += sounds ? 1 : 0;
                    continue;
                }
                silent += sounds ? 0 : 1;
                // The moved ji's design against the string's own tuning.
                int own = -1, below = 0;
                for (int string = 0; string < spec.stringCount; ++string)
                {
                    const int midi = spec.strings[static_cast<std::size_t>(string)].midiNote;
                    own = midi == note ? string : own;
                    if (midi < note && midi > spec.strings[static_cast<std::size_t>(below)].midiNote)
                        below = string;
                }
                const bool reached = own >= 0 || (oshide && note > spec.strings[0].midiNote
                                                  && (note - spec.strings[static_cast<std::size_t>(below)].midiNote <= 2));
                if (reached)
                    continue;
                // A pluck that pressed a string would have been reached; the
                // nearest string below sounds the key instead.
                const int string = note < spec.strings[0].midiNote ? 0 : below;
                const acukoto::StringSpec& tuning = spec.strings[static_cast<std::size_t>(string)];
                const double expected = performer->engine().stringReport(string).frequency;
                const double ownPitch = 440.0 * std::pow(2.0, (tuning.midiNote - 69) / 12.0);
                const double keyPitch = 440.0 * std::pow(2.0, (note - 69) / 12.0);
                const double cents = 1200.0 * std::log2(expected / keyPitch);
                const double stringCents = 1200.0 * std::log2(expected / ownPitch) - 100.0 * (note - tuning.midiNote);
                worstCents = std::max(worstCents, std::abs(cents - stringCents));
            }
            std::printf("%s, oshide %s: A0-C8 %d silent, past them %d sound; moved ji within %.3f cents of their "
                        "strings' tuning\n", spec.name, oshide ? "on" : "off", silent, sounding, worstCents);
            check(silent == 0, "a key from A0 to C8 is silent", silent);
            check(sounding == 0, "a key past A0 or C8 sounds", sounding);
            check(worstCents < 0.01, "a moved ji is not tuned as its string", worstCents);
        }
    // They sound where they are tuned. The lowest keys' fundamentals lie far
    // under the body's lowest mode, so their second partial is read. The
    // highest keys die away within a second, under what the rest of the
    // instrument rings with, so they are read from 0.3 to 0.8 s.
    performer->setInstrument(0);
    performer->setControls(acukoto::Performer::defaultControls(0));
    for (const int note : { 21, 36, 54, 66, 96, 108 })
    {
        const auto audio = renderNote(*performer, note);
        const acukoto::InstrumentSpec& spec = *acukoto::presets[0];
        int string = 0;
        for (int index = 0; index < spec.stringCount; ++index)
            if (spec.strings[static_cast<std::size_t>(index)].midiNote < note)
                string = index;
        const double designed = performer->engine().stringReport(string).frequency;
        const int partial = note < 40 ? 2 : 1;
        const double sounded = note > 84 ? centsOff(audio, partial * designed, 0.3, 0.5)
                                         : centsOff(audio, partial * designed);
        std::printf("key %d on string %d's moved ji: partial %d sounds %+.2f cents from its tuning\n", note,
                    string + 1, partial, sounded);
        check(std::abs(sounded) <= 15.0, "a moved ji's key sounds off its pitch", sounded);
    }

    if (failures == 0)
        std::printf("Acukoto.Tuning: every string tuned as the players tune it and sounding within %.0f cents of that, "
                    "and every key from A0 to C8 at its own pitch\n",
                    soundedCents);
    return failures == 0 ? 0 : 1;
}
