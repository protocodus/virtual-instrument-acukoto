// A clear zeroes only the part of each ring that can still be read: the lines
// within Loop::reach(), what is still to arrive of `pending`, the energy ring
// within the voice's longest round trip, the room's rings within their taps.
// A pluck on a sounding string keeps to the same part of the energy ring: the
// tsume scales it by what its contact left, a bare finger zeroes it.
// Four checks hold those bounds to the engine that plays:
//
//  - Nothing the controls reach asks a loop for a line as long as its
//    `longest`, or a voice for a round trip as long as its longestRoundTrip.
//    setLineDelay and the round trip's clamps hold both, so a bound drawn too
//    short would play a bent string sharp rather than read a stale sample.
//  - After a reset and after falling asleep, every slot within those bounds
//    is zero, and all of `pending`; and an engine whose rings then hold a
//    loud value everywhere else plays, bit for bit, as one whose rings are
//    zero there, which is the full clear.
//  - clearRing itself and one loop's clears, across the ring's end.
//  - A pluck on a sounding string, with the tsume and with a bare finger,
//    leaves every slot of the energy ring beyond the voice's longest round
//    trip as it was, and plays, bit for bit, the same whatever those hold.
//
// Include the implementation to reach the engine's rings; nothing but a friend
// declaration enters the shipping engine for it.
#include "EngineTestImplementation.h"
#include "DSP/AcukotoPresets.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// AcukotoEngine keeps its state private, and its type with it; this is the
// friend it names for its tests.
namespace acukoto
{
struct AcukotoEngineTestAccess
{
    static auto& state(AcukotoEngine& engine) { return *engine.state_; }
};
}

namespace
{
int failures = 0;
void check(bool condition, const char* message, double a = 0.0, double b = 0.0, double c = 0.0)
{
    if (!condition)
    {
        if (++failures <= 20)
            std::fprintf(stderr, "%s: %.12g %.12g %.12g\n", message, a, b, c);
    }
}

constexpr int mask = acukoto::delayCapacity - 1;
const double lowestRatio = std::pow(2.0, acukoto::lowestPitchBend / 12.0)
                         * std::pow(2.0, acukoto::lowestHandPress / 12.0);

void clearRingWraps()
{
    std::array<float, 16> ring;
    const auto zeros = [&] (int start, int count)
    {
        ring.fill(1.0f);
        acukoto::clearRing(ring, start, count);
        for (int k = 0; k < 16; ++k)
        {
            const bool inside = ((k - start) & 15) < count;
            check((ring[static_cast<std::size_t>(k)] == 0.0f) == inside, "clearRing slot", start, count, k);
        }
    };
    for (int start : { 0, 5, 13, 15, -3, 16, 37 })
        for (int count : { 0, 1, 3, 15, 16 })
            zeros(start, count);
    // More than the ring holds is the whole ring.
    ring.fill(1.0f);
    acukoto::clearRing(ring, 9, 40);
    for (float value : ring)
        check(value == 0.0f, "clearRing beyond the ring's size");
}

// One loop alone, its cursor near either end of the ring: silence() and
// reset() zero the line within reach() of the cursor they leave and no
// further, and all of a pluck still on its way.
void loopClearsWhatItSays()
{
    auto loop = std::make_unique<acukoto::Loop>();
    loop->longest = 300;
    loop->restLineDelay = 120.25;
    for (bool reset : { false, true })
        for (int write : { 0, 17, acukoto::delayCapacity - 5 })
            for (int ahead : { 0, 1, 9, 700 })
            {
                loop->line.fill(1.0f);
                loop->write = write;
                loop->pendingAhead = ahead;
                for (int k = 0; k < ahead; ++k)
                    loop->pending[static_cast<std::size_t>((write + k) & mask)] = 1.0f;
                if (reset)
                    loop->reset();
                else
                    loop->silence();
                check(loop->write == (reset ? 0 : write) && loop->pendingAhead == 0, "a loop's cursor after its clear", reset, write);
                for (int k = 0; k < acukoto::delayCapacity; ++k)
                {
                    const int behind = (loop->write - 1 - k) & mask;
                    check((loop->line[static_cast<std::size_t>(k)] == 0.0f) == (behind < loop->reach()),
                          "a loop's line after its clear (reset, cursor, slot)", reset, write, k);
                    check(loop->pending[static_cast<std::size_t>(k)] == 0.0f,
                          "a loop's pending pluck after its clear (reset, cursor, ahead)", reset, write, ahead);
                }
            }
}

// Every loop of every string, at each pitch its ji can give it, from the
// controls' lowest ratio up to 4.8 times the pitch (the pitch bend, the hand,
// a press and the tension glide together reach about 2.7): the line stays
// under `longest`, which setLineDelay would otherwise hold it to, and the
// round trip under longestRoundTrip.
void longestCoversTheControls(const acukoto::InstrumentSpec& instrument, double rate)
{
    auto engine = std::make_unique<acukoto::AcukotoEngine>();
    engine->prepare(rate);
    engine->setInstrument(instrument);
    auto& s = acukoto::AcukotoEngineTestAccess::state(*engine);
    for (int index = 0; index < engine->stringCount(); ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        const auto sweep = [&] (const auto& pitch, int key)
        {
            voice.normal.apply(pitch.planes[0]);
            voice.parallel.apply(pitch.planes[1]);
            voice.leftNormal.apply(voice.left[0]);
            voice.leftParallel.apply(voice.left[1]);
            for (int step = 0; step <= 40 * 16; ++step)
            {
                const double ratio = lowestRatio * std::pow(2.0, step / (12.0 * 16.0));
                voice.setLoopsRatio(ratio);
                for (const acukoto::Loop* loop : { &voice.normal, &voice.parallel, &voice.leftNormal, &voice.leftParallel })
                    check(loop->whole + loop->fraction < loop->longest - 0.5,
                          "a line reaches its longest (rate, key, ratio)", rate, key, ratio);
                check(acukoto::nearestInt(rate / (pitch.planes[0].frequency * ratio)) < voice.longestRoundTrip,
                      "a round trip reaches its longest (rate, key, ratio)", rate, key, ratio);
            }
            // And the reach is what the tap's reads need.
            for (const acukoto::Loop* loop : { &voice.normal, &voice.parallel, &voice.leftNormal, &voice.leftParallel })
                check(loop->reach() <= acukoto::delayCapacity && loop->reach() >= loop->whole + 2,
                      "a loop's reach", rate, key, loop->reach());
        };
        sweep(voice.pitch, -1);
        for (int key = acukoto::lowestKey; key <= acukoto::highestKey; ++key)
            if (s.extensionString[static_cast<std::size_t>(key)] == index)
                sweep(s.extension[static_cast<std::size_t>(key)], key);
    }
}

// Writes `value` into every slot a clear at this moment leaves untouched.
void fillUntouched(acukoto::AcukotoEngine& engine, float value)
{
    auto& s = acukoto::AcukotoEngineTestAccess::state(engine);
    for (int index = 0; index < engine.stringCount(); ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        for (acukoto::Loop* loop : { &voice.normal, &voice.parallel, &voice.leftNormal, &voice.leftParallel })
            for (int behind = loop->reach() + 1; behind <= acukoto::delayCapacity; ++behind)
                loop->line[static_cast<std::size_t>((loop->write - behind) & mask)] = value;
        for (int behind = voice.longestRoundTrip + 1; behind <= acukoto::delayCapacity; ++behind)
            voice.squares[static_cast<std::size_t>((voice.squaresIndex - behind) & mask)] = value * value;
    }
    auto& room = s.room;
    for (std::size_t index = 0; index < room.lines.size(); ++index)
        for (int behind = room.lengths[index] + 1; behind <= room.capacity; ++behind)
            room.lines[index][static_cast<std::size_t>((room.positions[index] - behind) & (room.capacity - 1))] = value;
    for (int behind = room.earlyReach + 1; behind <= room.earlyCapacity; ++behind)
        room.early[static_cast<std::size_t>((room.earlyPosition - behind) & (room.earlyCapacity - 1))] = value;
    for (std::size_t index = 0; index < room.diffusers.size(); ++index)
        for (int behind = room.diffuserLengths[index] + 1; behind <= room.diffuserCapacity; ++behind)
            room.diffusers[index][static_cast<std::size_t>((room.diffuserPosition - behind) & (room.diffuserCapacity - 1))] = value;
}

// Right after a clear: every slot within the bounds is zero, and all of `pending`.
void clearedIsZero(acukoto::AcukotoEngine& engine, const char* after)
{
    const auto nonZero = [] (const auto& ring, int cursor, int reach)
    {
        int count = 0;
        for (int behind = 1; behind <= reach; ++behind)
            count += ring[static_cast<std::size_t>((cursor - behind) & (static_cast<int>(ring.size()) - 1))] != 0.0f;
        return count;
    };
    auto& s = acukoto::AcukotoEngineTestAccess::state(engine);
    for (int index = 0; index < engine.stringCount(); ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        for (const acukoto::Loop* loop : { &voice.normal, &voice.parallel, &voice.leftNormal, &voice.leftParallel })
        {
            check(nonZero(loop->line, loop->write, loop->reach()) == 0, after, index, loop->write, loop->reach());
            check(nonZero(loop->pending, 0, acukoto::delayCapacity) == 0 && loop->pendingAhead == 0,
                  after, index, loop->pendingAhead);
        }
        check(nonZero(voice.squares, voice.squaresIndex, voice.longestRoundTrip) == 0 && voice.squaresSum == 0.0,
              after, index, voice.squaresIndex, voice.longestRoundTrip);
    }
    const auto& room = s.room;
    for (std::size_t index = 0; index < room.lines.size(); ++index)
        check(nonZero(room.lines[index], room.positions[index], room.lengths[index]) == 0, after, index);
    check(nonZero(room.early, room.earlyPosition, room.earlyReach) == 0, after);
    for (std::size_t index = 0; index < room.diffusers.size(); ++index)
        check(nonZero(room.diffusers[index], room.diffuserPosition, room.diffuserLengths[index]) == 0, after, index);
}

struct Player
{
    std::unique_ptr<acukoto::AcukotoEngine> engine { std::make_unique<acukoto::AcukotoEngine>() };
    const acukoto::InstrumentSpec& instrument;
    std::vector<float> out;

    Player(const acukoto::InstrumentSpec& spec, double rate) : instrument(spec)
    {
        engine->prepare(rate);
        engine->setInstrument(spec);
    }
    void run(int samples)
    {
        float left[64], right[64];
        while (samples > 0)
        {
            const int count = std::min(samples, 64);
            engine->process(left, right, count);
            out.insert(out.end(), left, left + count);
            out.insert(out.end(), right, right + count);
            samples -= count;
        }
    }
    void chord(float velocity)
    {
        for (int index = 0; index < instrument.stringCount; ++index)
            engine->noteOn(instrument.strings[static_cast<std::size_t>(index)].midiNote, velocity);
    }
    void lift()
    {
        for (int index = 0; index < instrument.stringCount; ++index)
            engine->noteOff(instrument.strings[static_cast<std::size_t>(index)].midiNote);
    }
    // Loud on every string for longer than a ring is long, under a moving
    // bend, so that no slot of any ring is left at zero.
    void fillTheRings()
    {
        acukoto::PlayingControls controls {};
        for (int block = 0; block < 12; ++block)
        {
            controls.pitchBend = -9.0 + 1.5 * block;
            engine->setControls(controls);
            lift();
            chord(1.0f);
            run(2048);
        }
    }
    // From the first sample after a clear the lines are at their longest:
    // every string bent to the controls' lowest ratio. Then up through every
    // key, pressed and moved-ji pitches with them, while the bend rises.
    void playLowThenEverything()
    {
        acukoto::PlayingControls controls {};
        controls.pitchBend = acukoto::lowestPitchBend;
        controls.handPress = acukoto::lowestHandPress;
        engine->setControls(controls);
        chord(1.0f);
        run(4096);
        const int lowest = instrument.strings[0].midiNote;
        for (int key = lowest; key <= acukoto::highestKey; ++key)
        {
            controls.pitchBend = acukoto::lowestPitchBend + 24.0 * (key - lowest) / (acukoto::highestKey - lowest);
            engine->setControls(controls);
            engine->noteOn(key, 0.9f);
            run(384);
            engine->noteOff(key);
        }
        run(4096);
    }
    void fallAsleep()
    {
        lift();
        acukoto::PlayingControls controls {};
        controls.mute = 1.0;
        controls.sympathy = 0.0;
        engine->setControls(controls);
        for (int block = 0; block < 10 * static_cast<int>(engine->sampleRate()) / 64 && !engine->idle(); ++block)
            run(64);
        check(engine->idle(), "the engine did not fall asleep");
    }
};

double squaresSum(acukoto::AcukotoEngine& engine)
{
    double sum = 0.0;
    auto& s = acukoto::AcukotoEngineTestAccess::state(engine);
    for (int index = 0; index < engine.stringCount(); ++index)
        sum += s.strings[static_cast<std::size_t>(index)].squaresSum;
    return sum;
}

// `loud` plays on with a loud value in every slot the clear left; `zeroed`
// with zeros there, as after a full clear.
void sameAsTheFullClear(Player& loud, Player& zeroed, const char* after)
{
    clearedIsZero(*loud.engine, after);
    fillUntouched(*loud.engine, 1000.0f);
    fillUntouched(*zeroed.engine, 0.0f);
    loud.out.clear();
    zeroed.out.clear();
    loud.playLowThenEverything();
    zeroed.playLowThenEverything();
    check(loud.out.size() == zeroed.out.size()
              && std::memcmp(loud.out.data(), zeroed.out.data(), loud.out.size() * sizeof(float)) == 0,
          after);
    double peak = 0.0;
    for (float value : zeroed.out)
        peak = std::max(peak, static_cast<double>(std::fabs(value)));
    check(peak > 0.01 && peak < 4.0, "the phrase after the clear sounds", peak);
    // The energy ring feeds the pitch only through the tension glide; its sum says it outright.
    check(squaresSum(*loud.engine) == squaresSum(*zeroed.engine), after, squaresSum(*loud.engine), squaresSum(*zeroed.engine));
}

void clearsLeaveNothingToRead(const acukoto::InstrumentSpec& instrument, double rate)
{
    Player loud(instrument, rate), zeroed(instrument, rate);
    for (Player* player : { &loud, &zeroed })
    {
        player->fillTheRings();
        // A reset with a pluck still on its way to the bridge.
        player->chord(1.0f);
        player->run(8);
        player->engine->reset();
    }
    sameAsTheFullClear(loud, zeroed, "after a reset");
    for (Player* player : { &loud, &zeroed })
    {
        player->fillTheRings();
        player->fallAsleep();
    }
    sameAsTheFullClear(loud, zeroed, "after falling asleep");
}

// Every slot of each string's energy ring beyond its longest round trip.
template <typename Visit>
void beyondTheRoundTrip(acukoto::AcukotoEngine& engine, Visit visit)
{
    auto& s = acukoto::AcukotoEngineTestAccess::state(engine);
    for (int index = 0; index < engine.stringCount(); ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        for (int behind = voice.longestRoundTrip + 1; behind <= acukoto::delayCapacity; ++behind)
            visit(voice.squares[static_cast<std::size_t>((voice.squaresIndex - behind) & mask)]);
    }
}

// Until 0.25.15 a pluck on a sounding string scaled (the tsume) or zeroed (a
// bare finger) all 64 KB of its energy ring, every string of a chord in one
// block, where only the longest round trip behind the cursor can be read.
void aRepluckKeepsToTheRoundTrip(const acukoto::InstrumentSpec& instrument, double rate, acukoto::Technique technique)
{
    Player marked(instrument, rate), zeroed(instrument, rate);
    constexpr float mark = 7.0f;
    for (Player* player : { &marked, &zeroed })
    {
        acukoto::PlayingControls controls {};
        controls.technique = technique;
        player->engine->setControls(controls);
        player->chord(1.0f);
        // Long enough for the chord's last stroke to have crossed to its string.
        player->run(16384);
        beyondTheRoundTrip(*player->engine, [&] (float& slot) { slot = player == &marked ? mark : 0.0f; });
        player->out.clear();
        // Every string still sounds, its key down: each pluck meets it at once.
        for (int index = 0; index < instrument.stringCount; ++index)
            player->engine->pluckString(index, 0.8f);
    }
    int moved = 0;
    beyondTheRoundTrip(*marked.engine, [&] (float& slot) { moved += slot != mark; });
    check(moved == 0, "a pluck on a sounding string changed its energy ring beyond the round trip (rate, slots)", rate, moved);
    for (Player* player : { &marked, &zeroed })
        player->run(8192);
    check(marked.out.size() == zeroed.out.size()
              && std::memcmp(marked.out.data(), zeroed.out.data(), marked.out.size() * sizeof(float)) == 0,
          "a pluck on a sounding string read its energy ring beyond the round trip", rate);
    check(squaresSum(*marked.engine) == squaresSum(*zeroed.engine) && squaresSum(*marked.engine) > 0.0,
          "the running energy after a pluck on a sounding string", squaresSum(*marked.engine), squaresSum(*zeroed.engine));
}
}

int main()
{
    clearRingWraps();
    loopClearsWhatItSays();
    for (const acukoto::InstrumentSpec* instrument : acukoto::presets)
        for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            longestCoversTheControls(*instrument, rate);
    clearsLeaveNothingToRead(*acukoto::presets[0], 48000.0);
    clearsLeaveNothingToRead(*acukoto::presets[1], 96000.0);
    // Moving a ji to shorten a string while suri-zume scrapes it must keep
    // every injected pending sample ahead of the cursor and cleared on reset.
    {
        Player scraped(*acukoto::presets[0], 48000.0);
        scraped.engine->noteOn(81, 0.8f);   // A5, string 12 (0.211 m)
        scraped.run(256);
        scraped.engine->noteOff(81);
        scraped.engine->noteOn(37, 0.8f);   // suri-zume (0.7 s, travels to ~0.11 m)
        scraped.run(8192);                  // mid-stroke near maximum travel
        scraped.engine->noteOn(108, 0.8f);  // C8 moved ji on string 12 (0.044 m < 0.11 m)
        scraped.run(2048);
        scraped.engine->reset();
        clearedIsZero(*scraped.engine, "reset after moved-ji during suri-zume");
    }
    for (acukoto::Technique technique : { acukoto::Technique::Plectrum, acukoto::Technique::Finger })
    {
        aRepluckKeepsToTheRoundTrip(*acukoto::presets[0], 48000.0, technique);
        aRepluckKeepsToTheRoundTrip(*acukoto::presets[1], 96000.0, technique);
    }
    if (failures != 0)
    {
        std::fprintf(stderr, "%d bounded-clear check(s) failed\n", failures);
        return 1;
    }
    std::printf("bounded clears: every line and round trip within its bound; a reset and a sleep play as the full clear; "
                "a pluck on a sounding string keeps to the round trip\n");
    return 0;
}
