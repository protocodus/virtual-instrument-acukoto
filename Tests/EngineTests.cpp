// The Performer's contracts: a render is deterministic and does not depend on
// how the host splits it into blocks, every sample is finite at the host
// rates, a pluck dies away, the palm across the strings stops it sooner, a
// legato step to a pressed note is pressed, not plucked, with Legato on and
// unless it joins a chord, and plucked with Legato off, the left hand's
// press raises the last plucked string and leaves it for the next, a
// pressed note keeps its pitch after its key-up, a chord's pressed
// note leaves a held string alone when another will do, a key struck twice
// is let go at its last key-up, and Sympathy at 0 rests the whole of every
// string nobody plays.

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

struct Change
{
    double seconds;
    acukoto::PlayingControls controls;
};

// Renders `phrase` (or one note) for `seconds` in blocks that cycle through
// `blocks`, notes and control changes at their sample, the left and right
// channels interleaved.
std::vector<float> render(acukoto::Performer& performer, const std::vector<Note>& notes, double seconds,
                          const std::vector<int>& blocks, const std::vector<Change>& changes = {})
{
    performer.reset();
    const double rate = performer.engine().sampleRate() / performer.oversampling();
    const auto total = static_cast<std::size_t>(seconds * rate);
    std::vector<float> left(total), right(total);
    std::size_t done = 0, next = 0, cycle = 0, change = 0;
    while (done < total)
    {
        while (change < changes.size() && static_cast<std::size_t>(changes[change].seconds * rate) <= done)
            performer.setControls(changes[change++].controls);
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
        if (change < changes.size())
            until = std::min(until, static_cast<std::size_t>(changes[change].seconds * rate));
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

// The strongest component within `spread` (a share) of `frequency`, dB, in
// the left channel from `from` to `to` seconds, Hann-windowed.
double peakDb(const std::vector<float>& x, double rate, double from, double to, double frequency, double spread)
{
    const auto a = static_cast<std::size_t>(from * rate), b = static_cast<std::size_t>(to * rate);
    const double pi = 3.14159265358979323846;
    double best = 0.0;
    for (double f = frequency * (1.0 - spread); f <= frequency * (1.0 + spread); f += 0.25)
    {
        double re = 0.0, im = 0.0;
        for (std::size_t i = a; i < b; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(i - a) / static_cast<double>(b - a));
            const double phase = 2.0 * pi * f * static_cast<double>(i) / rate;
            re += w * x[2 * i] * std::cos(phase);
            im -= w * x[2 * i] * std::sin(phase);
        }
        best = std::max(best, re * re + im * im);
    }
    return 10.0 * std::log10(best + 1e-30);
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
    // well under 3. Legato off, the step is plucked, on E-flat 4 pressed a
    // semitone (15.8 dB over D4 alone when 0.12.0 was made), and D4 rings
    // on unpressed.
    performer->setControls(acukoto::Performer::defaultControls(0));
    const auto ringing = render(*performer, { { 0.0, 62, 0.7f } }, 0.7, { 256 });
    const auto plucked = render(*performer, { { 0.0, 62, 0.7f }, { 0.5, 64, 0.7f } }, 0.7, { 256 });
    check(rmsDb(plucked, rate, 0.5, 0.7) > rmsDb(ringing, rate, 0.5, 0.7) + 3.0, "Legato off, a step is not plucked",
          rmsDb(plucked, rate, 0.5, 0.7) - rmsDb(ringing, rate, 0.5, 0.7));
    check(!performer->engine().stringActivity(4).raised && performer->engine().stringActivity(5).raised,
          "Legato off, a step bends the held string", 0.0);
    acukoto::PlayingControls slurring = acukoto::Performer::defaultControls(0);
    slurring.legato = true;
    performer->setControls(slurring);
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
    performer->setControls(acukoto::Performer::defaultControls(0));

    // The left hand's press (a front end's pitch wheel) raises the last
    // plucked string and lets it go again: G4 pressed a semitone sounds
    // A-flat 4, which no string is tuned to, and back. Held through the next
    // pluck, it leaves G4 for that string, pressed before it is plucked:
    // B-flat 4 sounds B4. Sympathy 0 keeps the other strings still. When
    // 0.12.0 was made the new pitch stood 32 dB over the old, the old 43 dB
    // over the new once let go, and B4 63 dB over B-flat 4.
    acukoto::PlayingControls open = acukoto::Performer::defaultControls(0);
    open.sympathy = 0.0;
    acukoto::PlayingControls hand = open;
    hand.handPress = 1.0;
    const auto bent = render(*performer, { { 0.0, 67, 0.7f } }, 1.5, { 256 },
                             { { 0.0, open }, { 0.3, hand }, { 0.9, open } });
    const double up = peakDb(bent, rate, 0.5, 0.9, 415.3, 0.01) - peakDb(bent, rate, 0.5, 0.9, 392.0, 0.01);
    check(up > 20.0, "the hand's press does not raise the last string", up);
    const double down = peakDb(bent, rate, 1.1, 1.5, 392.0, 0.01) - peakDb(bent, rate, 1.1, 1.5, 415.3, 0.01);
    check(down > 20.0, "the hand's press does not let go", down);
    const auto moved = render(*performer, { { 0.0, 67, 0.7f }, { 0.3, 70, 0.7f } }, 0.8, { 256 },
                              { { 0.0, hand } });
    check(performer->engine().stringActivity(8).raised && !performer->engine().stringActivity(6).raised,
          "the hand's press does not move to the next string", 0.0);
    const double next = peakDb(moved, rate, 0.4, 0.8, 493.9, 0.01) - peakDb(moved, rate, 0.4, 0.8, 466.2, 0.01);
    check(next > 20.0, "the next string is not pressed before its pluck", next);
    performer->setControls(acukoto::Performer::defaultControls(0));

    // A pressed note keeps its pitch after its key-up while it rings (E4,
    // E-flat 4 pressed a semitone, up after 0.2 s), and the hand leaves the
    // string once the palm has stopped it.
    // Three seconds on, the string is still clearly heard (0.11.0 let the
    // press go 1.25 s after the key-up, about 32 dB down, and Recon heard
    // the drop).
    render(*performer, { { 0.0, 64, 0.7f }, { 0.2, 64, 0.0f } }, 3.2, { 256 });
    check(performer->engine().stringActivity(5).raised, "a key-up lets a ringing press go", 0.0);
    acukoto::PlayingControls palm = acukoto::Performer::defaultControls(0);
    palm.mute = 1.0;
    performer->setControls(palm);
    render(*performer, { { 0.0, 64, 0.7f }, { 0.2, 64, 0.0f } }, 2.0, { 256 });
    check(!performer->engine().stringActivity(5).raised, "a press outlasts its stopped string", 0.0);
    performer->setControls(acukoto::Performer::defaultControls(0));

    // A chord's pressed note leaves a string another key holds when the
    // next one down will do: with G4 and E-flat 4 held, E4 sounds on D4
    // pressed a whole tone, and E-flat 4 rings on unpressed.
    render(*performer, { { 0.0, 67, 0.7f }, { 0.0, 63, 0.7f }, { 0.5, 64, 0.7f } }, 0.7, { 256 });
    check(performer->engine().stringActivity(4).raised && performer->engine().stringActivity(4).plucked,
          "a chord's E4 is not D4 pressed", 0.0);
    check(!performer->engine().stringActivity(5).raised && performer->engine().stringActivity(5).held,
          "a chord's E4 takes a held E-flat 4", 0.0);

    // A key struck again before its key-up (two overlapping notes of one
    // pitch) is let go at its last key-up: with the hand stopping a let-go
    // string (Release 0), the first key-up leaves it ringing.
    acukoto::PlayingControls stopping = acukoto::Performer::defaultControls(0);
    stopping.release = 0.0;
    performer->setControls(stopping);
    render(*performer, { { 0.0, 67, 0.7f }, { 0.3, 67, 0.7f }, { 0.35, 67, 0.0f } }, 0.6, { 256 });
    check(performer->engine().stringActivity(6).held, "a key's first key-up lets its second press go", 0.0);
    render(*performer, { { 0.0, 67, 0.7f }, { 0.3, 67, 0.7f }, { 0.35, 67, 0.0f }, { 0.5, 67, 0.0f } }, 0.6, { 256 });
    check(!performer->engine().stringActivity(6).held, "a key's last key-up does not let it go", 0.0);

    // Sympathy 0 rests every string nobody plays along its whole length.
    // A D4 pressed up a whole tone by ato-oshi once had the stretch beyond
    // G5's ji, its second partial at D4's pitch, ringing on at D4 10 dB over
    // the pressed note 0.4-0.8 s after the press; resting, it falls under
    // the pressed note by more than 20 dB (22 when 0.11.2 was made).
    acukoto::PlayingControls resting = slurring;
    resting.sympathy = 0.0;
    performer->setControls(resting);
    const auto pressed = render(*performer, { { 0.0, 62, 0.7f }, { 0.4, 64, 0.7f } }, 1.2, { 256 });
    const double old = peakDb(pressed, rate, 0.8, 1.2, 294.1, 0.006), now = peakDb(pressed, rate, 0.8, 1.2, 330.0, 0.01);
    check(old < now - 20.0, "a resting string beyond its ji rings at a pressed note's old pitch", old - now);
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
        std::printf("Acukoto.Engine: deterministic, block-size invariant, finite at every rate, decays, mutes, slurs with Legato, presses by hand, holds presses, shares chords, counts keys and rests sympathy\n");
    return failures == 0 ? 0 : 1;
}
