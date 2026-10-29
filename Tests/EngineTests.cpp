// The Performer's contracts: a render is deterministic and does not depend on
// how the host splits it into blocks, every sample is finite at the host
// rates, a pluck dies away, and the palm across the strings stops it sooner.

#include "DSP/AcukotoPerformer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace
{
int failures = 0;

void check(bool condition, const char* what, double value)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s (%.6g)\n", what, value);
        ++failures;
    }
}

struct Note
{
    double seconds;
    int note;
    float velocity;
};

// A run up the strings, an awase and a pressed note: every string, the
// body and a press.
const std::vector<Note> phrase { { 0.00, 50, 0.7f }, { 0.15, 55, 0.6f }, { 0.30, 57, 0.6f }, { 0.45, 58, 0.6f },
                                 { 0.60, 62, 0.6f }, { 0.75, 63, 0.6f }, { 0.90, 67, 0.7f }, { 1.05, 69, 0.6f },
                                 { 1.20, 70, 0.6f }, { 1.35, 74, 0.6f }, { 1.50, 75, 0.6f }, { 1.65, 79, 0.6f },
                                 { 1.80, 81, 0.6f }, { 2.20, 62, 0.9f }, { 2.20, 74, 0.8f }, { 2.80, 72, 0.7f } };

// Renders `phrase` (or one note) for `seconds` in blocks that cycle through
// `blocks`, notes at their sample, the left and right channels interleaved.
std::vector<float> render(acukoto::Performer& performer, const std::vector<Note>& notes, double seconds,
                          const std::vector<int>& blocks)
{
    performer.reset();
    const double rate = performer.engine().sampleRate() / performer.oversampling();
    const auto total = static_cast<std::size_t>(seconds * rate);
    std::vector<float> left(total), right(total);
    std::size_t done = 0, next = 0, cycle = 0;
    while (done < total)
    {
        while (next < notes.size() && static_cast<std::size_t>(notes[next].seconds * rate) <= done)
        {
            performer.noteOn(notes[next].note, notes[next].velocity);
            ++next;
        }
        std::size_t until = total;
        if (next < notes.size())
            until = std::min(until, static_cast<std::size_t>(notes[next].seconds * rate));
        const auto frames = std::min<std::size_t>(static_cast<std::size_t>(blocks[cycle++ % blocks.size()]),
                                                   std::max<std::size_t>(until - done, 1));
        performer.process(left.data() + done, right.data() + done, static_cast<int>(frames));
        done += frames;
    }
    std::vector<float> out(2 * total);
    for (std::size_t i = 0; i < total; ++i)
    {
        out[2 * i] = left[i];
        out[2 * i + 1] = right[i];
    }
    return out;
}

double rmsDb(const std::vector<float>& x, double rate, double from, double to)
{
    const auto a = 2 * static_cast<std::size_t>(from * rate), b = 2 * static_cast<std::size_t>(to * rate);
    double sum = 0.0;
    for (std::size_t i = a; i < b; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(sum / static_cast<double>(b - a) + 1e-30);
}

bool finite(const std::vector<float>& x)
{
    return std::all_of(x.begin(), x.end(), [] (float v) { return std::isfinite(v); });
}
} // namespace

int main()
{
    constexpr double rate = 48000.0;
    auto performer = std::make_unique<acukoto::Performer>();
    performer->prepare(rate);
    performer->setControls(acukoto::Performer::defaultControls(0));

    const auto reference = render(*performer, phrase, 4.0, { 256 });
    check(finite(reference), "a non-finite sample at 48 kHz", 0.0);
    check(rmsDb(reference, rate, 0.0, 4.0) > -60.0, "the phrase is silent", rmsDb(reference, rate, 0.0, 4.0));

    const auto again = render(*performer, phrase, 4.0, { 256 });
    check(std::memcmp(reference.data(), again.data(), reference.size() * sizeof(float)) == 0,
          "a second render differs", 0.0);

    // A host may call with any block size, and change it every call.
    const auto split = render(*performer, phrase, 4.0, { 1, 7, 64, 333, 2, 256, 129 });
    double worst = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i)
        worst = std::max(worst, static_cast<double>(std::abs(reference[i] - split[i])));
    check(worst == 0.0, "the render depends on the block sizes", worst);

    // A pluck dies away: 4-5 s after it, at least 30 dB under its first half
    // second.
    const auto single = render(*performer, { { 0.0, 67, 0.7f } }, 5.0, { 256 });
    const double early = rmsDb(single, rate, 0.0, 0.5), late = rmsDb(single, rate, 4.0, 5.0);
    check(early - late >= 30.0, "a pluck does not die away", early - late);

    // The palm across the strings stops a note sooner.
    acukoto::PlayingControls muted = acukoto::Performer::defaultControls(0);
    muted.mute = 1.0;
    performer->setControls(muted);
    const auto damped = render(*performer, { { 0.0, 67, 0.7f } }, 5.0, { 256 });
    check(rmsDb(damped, rate, 1.0, 2.0) < rmsDb(single, rate, 1.0, 2.0) - 10.0, "the mute does not damp",
          rmsDb(single, rate, 1.0, 2.0) - rmsDb(damped, rate, 1.0, 2.0));

    // Every host rate, with the oversampling a front end offers.
    for (const double hostRate : { 44100.0, 96000.0 })
        for (const int factor : { 1, 2 })
        {
            auto other = std::make_unique<acukoto::Performer>();
            other->prepare(hostRate, factor);
            other->setControls(acukoto::Performer::defaultControls(1));
            other->setInstrument(1);
            const auto out = render(*other, phrase, 3.5, { 256 });
            check(finite(out), "a non-finite sample", hostRate);
            check(rmsDb(out, hostRate, 0.0, 3.5) > -60.0, "silent at this rate", hostRate);
        }

    if (failures == 0)
        std::printf("Acukoto.Engine: deterministic, block-size invariant, finite at every rate, decays and mutes\n");
    return failures == 0 ? 0 : 1;
}
