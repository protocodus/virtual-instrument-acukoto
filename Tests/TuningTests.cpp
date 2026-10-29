// Every string of every tuning sounds the pitch it is tuned to, and that
// pitch is its note give or take a hand's tuning.
//
// Each string is plucked alone through the Performer at the preset's own
// controls, 48 kHz and 256-sample blocks. Its fundamental is the peak of the
// Hann-windowed spectrum over 1-2 s, the pitch a tuner settles on, searched
// in 1-cent steps within 40 cents of the target and refined by a parabola
// through the log magnitudes. A koto is tuned by ear, string by string, so
// each string is designed a few cents from equal temperament
// (AcukotoEngine.cpp, the hand-tuned spread, 4.1 cents' standard deviation):
// that pitch must lie within tunedCents of its note (at most 9.5 cents,
// Hirajoshi's G5, when this test was written on 2026-10-29). A string's two
// planes of vibration sound a few cents apart, the one normal to the top
// pulled by its ji's heave, and the window reads whichever is louder in it,
// so the string must sound within soundedCents of its designed pitch (at
// most 9.8 cents flat, Hankumoijoshi's A5, then).

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
constexpr double soundedCents = 12.0;
constexpr double tunedCents = 10.0;
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

// Cents of the fundamental over 1-2 s from `target` (Hz).
double centsOff(const std::vector<float>& x, double target)
{
    const auto start = static_cast<std::size_t>(1.0 * rate), length = static_cast<std::size_t>(1.0 * rate);
    int best = -40;
    double bestLevel = -1.0;
    std::vector<double> levels;
    for (int cents = -40; cents <= 40; ++cents)
    {
        const double level = magnitude(x, start, length, target * std::pow(2.0, cents / 1200.0));
        levels.push_back(std::log(level + 1e-30));
        if (level > bestLevel)
        {
            bestLevel = level;
            best = cents;
        }
    }
    if (best == -40 || best == 40)
        return best;
    const auto i = static_cast<std::size_t>(best + 40);
    const double a = levels[i - 1], b = levels[i], c = levels[i + 1];
    return best + 0.5 * (a - c) / (a - 2.0 * b + c);
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
            const int note = spec.strings[static_cast<std::size_t>(string)].midiNote;
            const double designed = performer->engine().stringReport(string).frequency;
            const double tuned = 1200.0 * std::log2(designed / (440.0 * std::pow(2.0, (note - 69) / 12.0)));
            const double sounded = centsOff(renderNote(*performer, note), designed);
            std::printf("%s string %2d note %d: tuned %+.2f cents, sounds %+.2f from that\n", spec.name, string + 1,
                        note, tuned, sounded);
            check(std::abs(tuned) <= tunedCents, "string tuned off its note", tuned);
            check(std::abs(sounded) <= soundedCents, "string sounds off its tuning", sounded);
        }
    }
    if (failures == 0)
        std::printf("Acukoto.Tuning: every string tuned within %.0f cents and sounding within %.0f of that\n",
                    tunedCents, soundedCents);
    return failures == 0 ? 0 : 1;
}
