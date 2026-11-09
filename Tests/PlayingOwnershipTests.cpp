// Audible ownership contracts: a tremolo has one right hand per string;
// overlapping deferred strokes consume one key-up each; a left-of-ji pluck
// is stopped on the stretch that was played. Public engine API only.
#include "DSP/AcukotoEngine.h"
#include "DSP/AcukotoPerformer.h"
#include "DSP/AcukotoPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
constexpr int rate = 48000;
int failures = 0;

void check(bool okay, const char* message, double value)
{
    if (!okay)
    {
        std::fprintf(stderr, "FAIL %s: %.9g\n", message, value);
        ++failures;
    }
}

void advance(acukoto::AcukotoEngine& engine, int frames)
{
    float left[64], right[64];
    while (frames > 0)
    {
        const int count = std::min(frames, 64);
        engine.process(left, right, count);
        frames -= count;
    }
}

bool different(const acukoto::AcukotoEngine::PluckReport& a,
               const acukoto::AcukotoEngine::PluckReport& b)
{
    return a.force != b.force || a.position != b.position || a.release != b.release;
}

void tremolo(acukoto::AcukotoEngine& engine)
{
    for (int retriggers : { 0, 1, 2, 3 })
    {
        engine.reset();
        engine.noteOn(22, 0.7f);
        engine.noteOn(62, 0.7f);
        auto previous = engine.lastPluck();
        int lateStrokes = 0;
        float left, right;
        for (int frame = 0; frame < rate; ++frame)
        {
            for (int k = 1; k <= retriggers; ++k)
                if (frame == k * rate / 10)
                    engine.noteOn(62, 0.7f);
            engine.process(&left, &right, 1);
            const auto current = engine.lastPluck();
            if (different(current, previous) && frame >= rate / 2)
                ++lateStrokes;
            previous = current;
        }
        // The measured 58 +/- 5.4 ms hand gives about nine strokes in a
        // half-second. Allow its timing variation; a duplicated stream gave
        // 18, 25 or 34 after one, two or three retriggers.
        check(lateStrokes >= 6 && lateStrokes <= 12,
              "retrigger multiplies a tremolo's hands", lateStrokes);
        for (int k = 0; k <= retriggers; ++k)
            engine.noteOff(62);
        previous = engine.lastPluck();
        advance(engine, rate / 5);
        check(!different(previous, engine.lastPluck()), "tremolo continues after the final key-up", retriggers);
    }

    engine.reset();
    engine.noteOn(22, 0.7f);
    engine.noteOn(62, 1.0f);
    const double hardForce = engine.lastPluck().force;
    advance(engine, rate * 3 / 10);
    engine.noteOn(62, 0.2f);
    double loudest = 0.0;
    float left, right;
    for (int frame = 0; frame < rate / 2; ++frame)
    {
        engine.process(&left, &right, 1);
        loudest = std::max(loudest, engine.lastPluck().force);
    }
    // Force follows v^1.4; 0.2^1.4 = 0.105. A generous 0.25 allows hand
    // scatter while rejecting a stale full-velocity tremolo stream.
    check(loudest < 0.25 * hardForce, "soft tremolo retrigger retains hard strokes", loudest / hardForce);

    // Releasing and re-pressing before the old timer is due is a fresh
    // attack too. It must not revive the first hand when that timer fires.
    engine.reset();
    engine.noteOn(22, 0.7f);
    engine.noteOn(62, 1.0f);
    advance(engine, rate / 100);
    engine.noteOff(62);
    engine.noteOn(62, 0.2f);
    const double softForce = engine.lastPluck().force;
    loudest = 0.0;
    for (int frame = 0; frame < rate / 5; ++frame)
    {
        engine.process(&left, &right, 1);
        loudest = std::max(loudest, engine.lastPluck().force);
    }
    check(loudest < 2.0 * softForce, "a pre-release tremolo timer revives on a new key-down", loudest / softForce);
}

void deferredKeys(acukoto::AcukotoEngine& engine)
{
    for (bool alreadySounding : { false, true })
    {
        engine.reset();
        if (alreadySounding)
        {
            engine.noteOn(63, 0.7f);
            advance(engine, rate / 10);
        }
        engine.noteOn(62, 0.7f);
        engine.noteOn(63, 0.7f); // the next string of kakite, about 9 ms later
        if (!alreadySounding)
            engine.noteOn(63, 0.7f);
        advance(engine, rate / 500);
        engine.noteOff(63);
        advance(engine, rate / 50);
        const int string = engine.stringForNote(63);
        check(engine.stringActivity(string).held,
              "one key-up consumes two overlapping deferred strokes", alreadySounding);
        engine.noteOff(63);
        check(!engine.stringActivity(string).held, "final deferred key-up leaves a held note", alreadySounding);
    }

    // Oshi-awase gives a MIDI key two string owners, one of them deferred.
    // Replaying the deferred owner's earlier key-up must not broadcast a
    // second release to its already-sounding companion.
    for (int presses : { 2, 3 })
        for (int earlyOffs : { 1, presses })
        {
            engine.reset();
            engine.noteOn(30, 0.7f);
            for (int k = 0; k < presses; ++k)
                engine.noteOn(63, 0.7f);
            advance(engine, rate / 500);
            for (int k = 0; k < earlyOffs; ++k)
                engine.noteOff(63);
            advance(engine, rate / 50);
            const int upper = engine.stringForNote(63);
            for (int string : { upper - 1, upper })
                check(engine.stringActivity(string).held == (earlyOffs < presses),
                      "a deferred key-up releases another oshi-awase owner", string);
            for (int k = earlyOffs; k < presses; ++k)
                engine.noteOff(63);
            for (int string : { upper - 1, upper })
                check(!engine.stringActivity(string).held, "oshi-awase keeps a released owner", string);
        }

    for (bool releaseNew : { false, true })
    {
        engine.reset();
        engine.noteOn(62, 0.7f);
        engine.noteOn(63, 1.0f); // a hard kakite contact still on its way
        advance(engine, rate / 500);
        engine.noteOn(64, 0.2f); // oshide takes over the same physical string
        const int string = engine.stringForNote(63);
        const auto newPluck = engine.lastPluck();
        advance(engine, rate / 1000);
        if (releaseNew)
            engine.noteOff(64);
        advance(engine, rate / 50);
        check(!different(newPluck, engine.lastPluck()),
              "a displaced deferred note reclaims the string", engine.lastPluck().force / newPluck.force);
        check(engine.stringActivity(string).held != releaseNew,
              "a displaced deferred note changes the current key owner", releaseNew);
        engine.noteOff(63);
        check(engine.stringActivity(string).held != releaseNew,
              "the displaced key-up releases the current owner", releaseNew);
        engine.noteOff(64);
    }
}

double leftTail(acukoto::AcukotoEngine& engine, int key, double release, bool staccato, bool pedal)
{
    auto controls = acukoto::Performer::defaultControls(0);
    controls.room = 0.0;
    controls.release = release;
    // Default sympathy must stay on: damping it would mask the missing
    // contact once the speaking stretch's energy fell below its threshold.
    engine.setControls(controls);
    engine.reset();
    engine.noteOn(26, 0.7f);
    if (staccato)
        engine.noteOn(29, 0.7f);
    engine.noteOn(key, 0.7f);
    advance(engine, rate / 2);
    engine.setSustain(pedal);
    engine.noteOff(key);
    std::vector<float> left(rate), right(rate);
    engine.process(left.data(), right.data(), rate);
    double sum = 0.0;
    for (int i = rate / 2; i < rate; ++i)
        sum += static_cast<double>(left[i]) * left[i] + static_cast<double>(right[i]) * right[i];
    return 10.0 * std::log10(sum / rate + 1.0e-30);
}

std::vector<float> afterLeftSleeps(acukoto::AcukotoEngine& engine, double release)
{
    auto controls = acukoto::Performer::defaultControls(0);
    controls.room = 0.0;
    controls.release = 0.0;
    engine.setControls(controls);
    engine.reset();
    engine.noteOn(26, 0.7f);
    engine.noteOn(81, 0.001f);
    advance(engine, rate / 100);
    engine.noteOff(81);
    for (int block = 0; !engine.idle() && block < rate * 20 / 256; ++block)
        advance(engine, 256);
    check(engine.idle(), "quiet left pluck does not let the engine sleep", 0.0);
    engine.noteOn(26, 0.7f); // return to the speaking stretch
    controls.release = release;
    engine.setControls(controls);
    engine.noteOn(69, 0.7f);
    std::vector<float> left(rate / 2), right(rate / 2);
    engine.process(left.data(), right.data(), rate / 2);
    left.insert(left.end(), right.begin(), right.end());
    return left;
}
} // namespace

int main()
{
    auto engine = std::make_unique<acukoto::AcukotoEngine>();
    engine->prepare(rate);
    engine->setInstrument(acukoto::kotoHirajoshi);
    engine->setControls(acukoto::Performer::defaultControls(0));
    tremolo(*engine);
    deferredKeys(*engine);
    for (int key : { 50, 62, 81 })
    {
        const double ringing = leftTail(*engine, key, 1.0, false, false);
        const double stopped = leftTail(*engine, key, 0.0, false, false);
        const double staccato = leftTail(*engine, key, 1.0, true, false);
        const double held = leftTail(*engine, key, 0.0, false, true);
        std::printf("left-of-ji key %d: Release %.3f dB, Staccato %.3f dB, pedal %.3f dB\n",
                    key, stopped - ringing, staccato - ringing, held - ringing);
        check(stopped < ringing - 0.5, "Release misses the plucked rest stretch", stopped - ringing);
        check(staccato < ringing - 8.0, "Staccato misses the plucked rest stretch", staccato - ringing);
        check(std::abs(held - ringing) < 0.01, "pedal does not hold a left-of-ji pluck", held - ringing);
    }
    // All prior waves are gone and the new note is held. Release has
    // nothing to stop, so changing it must not leave a phantom finger on
    // the former left pluck and alter that string's sympathetic response.
    const auto stopped = afterLeftSleeps(*engine, 0.0), free = afterLeftSleeps(*engine, 1.0);
    check(stopped == free, "a sleeping left pluck leaves a release hand on its rest stretch", 0.0);
    return failures == 0 ? 0 : 1;
}
