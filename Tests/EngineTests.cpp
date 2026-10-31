// The Performer's contracts: a render is deterministic and does not depend on
// how the host splits it into blocks, every sample is finite at the host
// rates, a pluck dies away, the palm across the strings stops it sooner, a
// legato step to a pressed note is pressed, not plucked, unless it joins a
// chord, and a pressed note keeps its pitch after its key-up.

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
    float velocity;  // 0: the key comes up
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
            if (notes[next].velocity > 0.0f)
                performer.noteOn(notes[next].note, notes[next].velocity);
            else
                performer.noteOff(notes[next].note);
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

    // A legato step a held string reaches by its press is ato-oshi: the
    // string is pressed, not plucked again. A re-pluck stands far over the
    // ringing string (20.9 dB a second after the first pluck in the
    // listening set, Docs/decisions.md 2026-10-30); the press moves it by
    // well under 3.
    performer->setControls(acukoto::Performer::defaultControls(0));
    const auto ringing = render(*performer, { { 0.0, 62, 0.7f } }, 0.7, { 256 });
    const auto legato = render(*performer, { { 0.0, 62, 0.7f }, { 0.5, 64, 0.7f } }, 0.7, { 256 });
    check(rmsDb(legato, rate, 0.5, 0.7) < rmsDb(ringing, rate, 0.5, 0.7) + 3.0, "a legato step plucks again",
          rmsDb(legato, rate, 0.5, 0.7) - rmsDb(ringing, rate, 0.5, 0.7));
    check(performer->engine().stringActivity(4).raised, "a legato step does not press", 0.0);

    // In a chord, a key a held string's press would reach is plucked as a
    // note of its own: D4 and A4 held, E4 sounds on E-flat 4 pressed a
    // semitone, and D4 stays open.
    render(*performer, { { 0.0, 62, 0.7f }, { 0.0, 69, 0.7f }, { 0.5, 64, 0.7f } }, 0.7, { 256 });
    check(!performer->engine().stringActivity(4).raised, "a chord's note bends a held string", 0.0);
    check(performer->engine().stringActivity(5).raised && performer->engine().stringActivity(5).plucked,
          "a chord's pressed note is not plucked", 0.0);

    // A pressed note keeps its pitch after its key-up while it rings (E4,
    // E-flat 4 pressed a semitone, up after 0.2 s), and the hand leaves the
    // string once the palm has stopped it.
    render(*performer, { { 0.0, 64, 0.7f }, { 0.2, 64, 0.0f } }, 0.6, { 256 });
    check(performer->engine().stringActivity(5).raised, "a key-up lets a ringing press go", 0.0);
    acukoto::PlayingControls palm = acukoto::Performer::defaultControls(0);
    palm.mute = 1.0;
    performer->setControls(palm);
    render(*performer, { { 0.0, 64, 0.7f }, { 0.2, 64, 0.0f } }, 2.0, { 256 });
    check(!performer->engine().stringActivity(5).raised, "a press outlasts its stopped string", 0.0);
    performer->setControls(acukoto::Performer::defaultControls(0));

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

    // An audio reset leaves nothing of what was played: after a phrase that
    // bends, presses a string (oshide), moves a ji and leaves notes ringing,
    // a reset plays the next phrase sample for sample as a freshly prepared
    // Performer does, in both tunings, at 1x and 2x, with the room heard.
    for (int instrument = 0; instrument < acukoto::Performer::instrumentCount; ++instrument)
        for (const int oversampling : { 1, 2 })
        {
            const auto phrase = [instrument] (acukoto::Performer& performer, double bend)
            {
                acukoto::PlayingControls controls = acukoto::Performer::defaultControls(instrument);
                controls.room = 1.0;
                controls.pitchBend = bend;
                performer.setControls(controls);
                performer.setInstrument(instrument);
                std::vector<float> out;
                std::vector<float> l(64), r(64);
                for (int block = 0; block < 200; ++block)
                {
                    if (block == 0)
                        for (const int note : { 55, 62, 67, 61 })
                            performer.noteOn(note, 1.0f);
                    if (block == 80)
                    {
                        performer.noteOff(62);
                        performer.noteOn(64, 0.8f);
                    }
                    performer.process(l.data(), r.data(), 64);
                    out.insert(out.end(), l.begin(), l.end());
                    out.insert(out.end(), r.begin(), r.end());
                }
                return out;
            };
            auto fresh = std::make_unique<acukoto::Performer>();
            fresh->prepare(48000.0, oversampling);
            auto played = std::make_unique<acukoto::Performer>();
            played->prepare(48000.0, oversampling);
            phrase(*played, 1.5);
            played->reset();
            const auto a = phrase(*fresh, 0.0), b = phrase(*played, 0.0);
            double worst = 0.0;
            for (std::size_t i = 0; i < a.size(); ++i)
                worst = std::max(worst, static_cast<double>(std::abs(a[i] - b[i])));
            check(a == b, "a reset plays as a fresh instance, sample for sample (largest difference)", worst);
        }

    if (failures == 0)
        std::printf("Acukoto.Engine: deterministic, block-size invariant, finite at every rate, decays, mutes, slurs and holds presses\n");
    return failures == 0 ? 0 : 1;
}
