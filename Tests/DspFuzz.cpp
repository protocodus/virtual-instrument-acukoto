// U4 input fuzzing of the front-end boundary, acukoto::Performer, with the
// shared Tests/FuzzHarness.h: from a fresh Performer every input, at most 64
// commands of raw and extreme playing controls (invalid enums and every
// float bit pattern included), notes of any number and velocity, sustain,
// instrument switches and resets, at any host rate and oversampling, each
// followed by a block of up to 256 frames. Every sample must stay finite and
// inside the frames asked for.
//
//   AcukotoDspFuzz -max_len=4096 -rss_limit_mb=4096 <corpus-dir>     (libFuzzer)
//   AcukotoDspFuzzReplay [-seed=N] [-runs=N] [file ...]               (any compiler)
//
// A crash leaves a crash-* reproducer; once fixed, commit it under
// Tests/Fixtures/fuzz/, where Acukoto.DspFuzzReplay replays it.

#include "DSP/AcukotoPerformer.h"

#include "FuzzHarness.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <vector>

namespace
{
constexpr int maxBlock = 256;

// prepare() designs both instruments' bodies, a few seconds, which would
// hold libFuzzer to a few inputs a minute. Prepare is deterministic, so the
// Performer prepared once at a rate and factor is reset and reused: reset()
// returns it to the state prepare() leaves. Every rate it cannot run at
// runs at the same one, so they share a Performer.
acukoto::Performer& preparedAt(double requested, int oversampling)
{
    const double rate = acukoto::Performer::usableRate(requested);
    struct Entry
    {
        std::uint64_t bits;
        int oversampling;
        std::unique_ptr<acukoto::Performer> performer;
    };
    static std::vector<Entry> cache;
    std::uint64_t bits;
    std::memcpy(&bits, &rate, sizeof bits);
    for (auto& entry : cache)
        if (entry.bits == bits && entry.oversampling == oversampling)
            return *entry.performer;
    auto performer = std::make_unique<acukoto::Performer>();
    performer->prepare(requested, oversampling);
    cache.push_back({ bits, oversampling, std::move(performer) });
    return *cache.back().performer;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    using namespace protocodus::fuzz;
    Input input { data, size };
    const double rate = input.Rate();
    acukoto::Performer& performer = preparedAt(rate, 1 << input.Pick(3));
    performer.setInstrument(0);
    performer.setSustain(false);
    acukoto::PlayingControls controls = acukoto::Performer::defaultControls(0);
    performer.setControls(controls);
    performer.reset();
    Sentinels<maxBlock> out;

    for (int command = 0; command < 64 && !input.Empty(); ++command)
    {
        switch (input.Pick(12))
        {
            case 0: controls.technique = static_cast<acukoto::Technique>(static_cast<int>(input.Byte()) - 64); break;
            case 1:
            {
                double* const fields[] { &controls.pluckDistance, &controls.normalShare, &controls.release,
                                         &controls.mute, &controls.sympathy, &controls.room, &controls.width,
                                         &controls.volume, &controls.pitchBend };
                *fields[input.Pick(std::size(fields))] = input.Scalar() * 4.0f - 1.0f;
                break;
            }
            case 2: controls.oshide = input.Byte() & 1; break;
            case 3: case 4: case 5:
                performer.noteOn(static_cast<int>(input.Byte()) - 64, input.Scalar());
                break;
            case 6: performer.noteOff(static_cast<int>(input.Byte()) - 64); break;
            case 7: performer.setSustain(input.Byte() & 1); break;
            case 8: performer.setInstrument(static_cast<int>(input.Byte()) - 64); break;
            default: break;
        }
        performer.setControls(controls);

        const auto frames = static_cast<int>(1 + input.Pick(maxBlock));
        out.Arm();
        performer.process(out.Left(), out.Right(), frames);
        out.Check(static_cast<std::size_t>(frames));

        if (input.Pick(64) == 0)
            performer.reset();
    }
    return 0;
}
