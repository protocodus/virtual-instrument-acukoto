// How evenly each preset's strings sound. The player evens the scale by what
// is heard (StringSpec::touch), so each string's touch is the one that brings
// its level to the preset's mean.
//
//   AcukotoLevels [--velocity 0-1]
//       prints each string's level, the preset's spread and the touch that
//       would level it
//   AcukotoLevels --check [--limit dB] [--velocity 0-1]
//       exits 1 when a preset's loudest string stands more than the limit
//       (default 0.5 dB) over its quietest
//
// Protocol. Each string is plucked alone at one velocity (default 0.7) from a
// reset engine at 48 kHz, 10 ms into the render, with the preset's default
// controls and the room off. Its level is the ITU-R BS.1770-4 K-weighted mean
// square of both microphones over the 0.6 s after the pluck, in dB. Every
// string starts from the same random state, so the pluck's per-note variation
// is the same for all. This is the engine alone, before the Performer's output
// stage: what is measured is the strings and the body. The pluck's force is
// linear in the touch to within 0.1 dB of level, so the printed touch lands
// the string on the mean; run again to confirm.

#include "../Source/DSP/AcukotoEngine.h"
#include "../Source/DSP/AcukotoPerformer.h"
#include "../Source/DSP/AcukotoPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{

constexpr int sampleRate = 48000;
constexpr std::size_t lead = sampleRate / 100;
constexpr std::size_t window = sampleRate * 6 / 10;

struct Biquad
{
    double b0, b1, b2, a1, a2, z1 { 0.0 }, z2 { 0.0 };
    double run(double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// The two stages of BS.1770-4's K-weighting at 48 kHz: a high shelf, then a
// high pass.
double kWeightedLevel(const std::vector<float>& left, const std::vector<float>& right)
{
    Biquad shelf[2] { { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 },
                      { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 } };
    Biquad highPass[2] { { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 },
                         { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 } };
    double sum = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        const double a = highPass[0].run(shelf[0].run(left[i]));
        const double b = highPass[1].run(shelf[1].run(right[i]));
        if (i >= lead)
            sum += a * a + b * b;
    }
    return -0.691 + 10.0 * std::log10(sum / static_cast<double>(left.size() - lead) + 1e-30);
}

} // namespace

int main(int argc, char** argv)
{
    bool check = false;
    double velocity = 0.7, limit = 0.5;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--check") == 0)
            check = true;
        else if (std::strcmp(argv[i], "--velocity") == 0 && i + 1 < argc)
            velocity = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--limit") == 0 && i + 1 < argc)
            limit = std::atof(argv[++i]);
        else
        {
            std::fprintf(stderr, "usage: AcukotoLevels [--check] [--limit dB] [--velocity 0-1]\n");
            return 2;
        }
    }

    int failures = 0;
    auto engine = std::make_unique<acukoto::AcukotoEngine>();
    engine->prepare(sampleRate);
    for (int preset = 0; preset < acukoto::Performer::instrumentCount; ++preset)
    {
        const acukoto::InstrumentSpec& spec = *acukoto::presets[static_cast<std::size_t>(preset)];
        engine->setInstrument(spec);
        acukoto::PlayingControls controls = acukoto::Performer::defaultControls(preset);
        controls.room = 0.0;

        std::vector<double> levels;
        for (int index = 0; index < spec.stringCount; ++index)
        {
            std::vector<float> left(lead + window), right(lead + window);
            engine->reset();
            engine->setControls(controls);
            engine->process(left.data(), right.data(), static_cast<int>(lead));
            engine->noteOn(spec.strings[static_cast<std::size_t>(index)].midiNote, static_cast<float>(velocity));
            engine->process(left.data() + lead, right.data() + lead, static_cast<int>(window));
            levels.push_back(kWeightedLevel(left, right));
        }
        double mean = 0.0;
        for (const double level : levels)
            mean += level / static_cast<double>(levels.size());
        const double spread = *std::max_element(levels.begin(), levels.end()) - *std::min_element(levels.begin(), levels.end());

        std::printf("%s, velocity %.2f: spread %.2f dB about a mean of %.2f dB\n", spec.name, velocity, spread, mean);
        for (int index = 0; index < spec.stringCount; ++index)
        {
            const auto& string = spec.strings[static_cast<std::size_t>(index)];
            const double level = levels[static_cast<std::size_t>(index)];
            std::printf("  note %3d  %7.2f dB  %+5.2f  touch %.3f -> %.3f\n", string.midiNote, level, level - mean,
                        string.touch, string.touch * std::pow(10.0, (mean - level) / 20.0));
        }
        if (check && spread > limit)
        {
            std::printf("FAIL %s: the scale is %.2f dB uneven, over the %.2f dB limit\n", spec.name, spread, limit);
            ++failures;
        }
    }
    if (check)
        std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
