// Supported low sample rates and high bent keys must remain finite in the
// raw physical model, before the Performer's output guard. The old second-
// order interpolation became unstable for C8 at 22.05 kHz and +12 semitones.
#include "DSP/AcukotoEngine.h"
#include "DSP/AcukotoPerformer.h"
#include "DSP/AcukotoPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <complex>
#include <memory>
#include <vector>

namespace
{
int failures = 0;
double peak = 0.0;
void inspect(float left, float right)
{
    for (float x : {left, right})
    {
        if (!std::isfinite(x) || std::abs(x) > 4.0f)
            ++failures;
        else
            peak = std::max(peak, std::abs(static_cast<double>(x)));
    }
}

// A narrow spectrum around the physical C8 bend catches a stable but
// incorrectly clamped/interpolated delay. Use the same six-cent tolerance
// as the established native-string tuning contract, with a short window
// because the artificial top extension decays quickly.
double peakCents(const std::vector<float>& audio, double rate, double target)
{
    constexpr double pi = 3.14159265358979323846;
    const int start = static_cast<int>(0.005 * rate), end = static_cast<int>(0.06 * rate);
    double largest = -1.0, centsAtPeak = 0.0;
    for (int halfCent = -24; halfCent <= 24; ++halfCent)
    {
        const double cents = 0.5 * halfCent;
        const auto step = std::polar(1.0, -2.0 * pi * target * std::pow(2.0, cents / 1200.0) / rate);
        std::complex<double> phase = 1.0, left = 0.0, right = 0.0;
        for (int i = start; i < end; ++i)
        {
            const double window = 0.5 - 0.5 * std::cos(2.0 * pi * (i - start) / (end - start));
            left += window * audio[static_cast<std::size_t>(2 * i)] * phase;
            right += window * audio[static_cast<std::size_t>(2 * i + 1)] * phase;
            phase *= step;
        }
        const double power = std::norm(left) + std::norm(right);
        if (power > largest) { largest = power; centsAtPeak = cents; }
    }
    return centsAtPeak;
}

std::vector<float> sweep(acukoto::AcukotoEngine& engine, int preset, double rate, int block)
{
    engine.reset();
    auto controls = acukoto::Performer::defaultControls(preset);
    controls.room = 0.0;
    controls.handPress = 3.0;
    engine.setControls(controls);
    engine.noteOn(108, 1.0f);
    // Events stay at absolute sample times under both callback partitions.
    // This crosses short-line tap and interpolation-order boundaries while
    // a wave is already circulating, then re-plucks and releases it.
    const int frames = static_cast<int>(rate * 0.6);
    std::vector<float> result(static_cast<std::size_t>(2 * frames));
    std::array<float, 64> left {}, right {};
    for (int at = 0; at < frames;)
    {
        if (at % 128 == 0)
        {
            const double phase = static_cast<double>(at) / frames;
            controls.pitchBend = 12.0 * (phase < 0.5 ? 2.0 * phase : 2.0 * (1.0 - phase));
            engine.setControls(controls);
            if (at == 128 * 20)
                engine.noteOn(108, 0.3f);
            if (at == 128 * 40 || at == 128 * 52)
                engine.noteOff(108);
        }
        const int count = std::min({block, frames - at, 128 - at % 128});
        engine.process(left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
        {
            inspect(left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
            result[static_cast<std::size_t>(2 * (at + i))] = left[static_cast<std::size_t>(i)];
            result[static_cast<std::size_t>(2 * (at + i) + 1)] = right[static_cast<std::size_t>(i)];
        }
        at += count;
    }
    return result;
}
}

int main()
{
    int cases = 0;
    for (double rate : {22050.0, 24000.0, 44100.0, 48000.0})
        for (int preset : {0, 1})
        {
            auto engine = std::make_unique<acukoto::AcukotoEngine>();
            engine->prepare(rate);
            engine->setInstrument(*acukoto::presets[static_cast<std::size_t>(preset)]);
            for (bool tension : {false, true})
            {
                engine->setTensionModulation(tension);
                for (int key : {96, 107, 108})
                    for (double bend : {0.0, 12.0})
                        for (double hand : {0.0, 3.0})
                        {
                            engine->reset();
                            auto controls = acukoto::Performer::defaultControls(preset);
                            controls.room = 0.0;
                            controls.pitchBend = bend;
                            controls.handPress = hand;
                            engine->setControls(controls);
                            engine->noteOn(key, 1.0f);
                            const int frames = static_cast<int>(rate * (key == 108 && bend == 12.0 ? 1.0 : 0.15));
                            std::array<float, 64> left {}, right {};
                            const bool checkPitch = rate == 22050.0 && key == 108 && bend == 12.0 && !tension;
                            std::vector<float> audio(checkPitch ? static_cast<std::size_t>(2 * frames) : 0);
                            const int before = failures;
                            for (int at = 0; at < frames; at += 64)
                            {
                                const int count = std::min(64, frames - at);
                                engine->process(left.data(), right.data(), count);
                                for (int i = 0; i < count; ++i)
                                {
                                    inspect(left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
                                    if (checkPitch)
                                    {
                                        audio[static_cast<std::size_t>(2 * (at + i))] = left[static_cast<std::size_t>(i)];
                                        audio[static_cast<std::size_t>(2 * (at + i) + 1)] = right[static_cast<std::size_t>(i)];
                                    }
                                }
                            }
                            if (checkPitch)
                            {
                                const double target = 440.0 * std::pow(2.0, (108.0 - 69.0 + bend + hand) / 12.0);
                                const double cents = peakCents(audio, rate, target);
                                if (std::abs(cents) > 6.0)
                                {
                                    ++failures;
                                    std::fprintf(stderr, "FAIL extreme pitch p%d hand%.0f cents%+.2f\n", preset, hand, cents);
                                }
                            }
                            if (failures != before)
                                std::fprintf(stderr, "FAIL raw range p%d rate%.0f key%d bend%.0f hand%.0f tension%d\n",
                                             preset, rate, key, bend, hand, tension);
                            ++cases;
                        }
                const auto a = sweep(*engine, preset, rate, 64);
                const auto b = sweep(*engine, preset, rate, 37);
                if (a != b)
                {
                    ++failures;
                    std::fprintf(stderr, "FAIL callback partition p%d rate%.0f tension%d\n", preset, rate, tension);
                }
            }
        }
    std::printf("Acukoto.RangeSafety: %d static cases, 16 live sweeps/partition pairs, raw peak %.8g, failures %d\n",
                cases, peak, failures);
    return failures == 0 ? 0 : 1;
}
