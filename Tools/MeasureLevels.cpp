// How evenly each preset's strings and body strikes sound. The player evens
// the scale by what is heard (StringSpec::touch). Left alone, the strings
// differ by 7-9 dB; the touches take 80% of the way, in dB, from each string's
// level without a touch to the mean of its scale (the owner's choice of
// 2026-11-03, Docs/decisions.md). The twelve body strikes (StrikeKey::touch)
// are taken the same share of the way to that mean of the strings: velocity
// 0.7 stands near a plucked note's level on every key.
//
//   AcukotoLevels [--velocity 0-1]
//       prints each string's and each strike's level, the level its touch is
//       meant to give it and the touch that would give it that
//   AcukotoLevels --check [--limit dB] [--velocity 0-1]
//       exits 1 when a string or a strike stands more than the limit (default
//       0.5 dB) from its intended level
//
// Protocol. Each string is plucked alone, and each strike struck alone, at one
// velocity (default 0.7) from a reset engine at 48 kHz, 10 ms into the
// render, with the preset's default controls and the room off. Its level is
// the ITU-R BS.1770-4 K-weighted mean square of both microphones over the
// 0.6 s after the pluck, in dB. Every string starts from the same random
// state, so the pluck's per-note variation is the same for all. This is the
// engine alone, before the Performer's output stage: what is measured is the
// strings and the body. The pluck's force is linear in the touch to within
// 0.1 dB of level, so a string's level without its touch is its level less the
// touch in dB, and the printed touch lands the string on its intended level;
// run again to confirm. A strike's arrival speed is linear in its touch, but
// its level only about so (the contact lasts longer at a lower speed): run it
// again until it holds.

#include "../Source/DSP/AcukotoEngine.h"
#include "../Source/DSP/AcukotoPerformer.h"
#include "../Source/DSP/AcukotoPresets.h"
#include "../Source/DSP/AcukotoStrikes.h"

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
// The share of the correction that would level every string.
constexpr double correction = 0.8;
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

// One key played alone: the level of the 0.6 s after it, both microphones.
double levelOfKey(acukoto::AcukotoEngine& engine, const acukoto::PlayingControls& controls, int midiNote,
                  double velocity)
{
    std::vector<float> left(lead + window), right(lead + window);
    engine.reset();
    engine.setControls(controls);
    engine.process(left.data(), right.data(), static_cast<int>(lead));
    engine.noteOn(midiNote, static_cast<float>(velocity));
    engine.process(left.data() + lead, right.data() + lead, static_cast<int>(window));
    return kWeightedLevel(left, right);
}

double spreadOf(const std::vector<double>& values)
{
    return *std::max_element(values.begin(), values.end()) - *std::min_element(values.begin(), values.end());
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
            levels.push_back(levelOfKey(*engine, controls, spec.strings[static_cast<std::size_t>(index)].midiNote,
                                        velocity));
        // Each string's level without its touch, and the intended level: the
        // correction's share of the way to the scale's mean.
        std::vector<double> plain, intended;
        double mean = 0.0;
        for (int index = 0; index < spec.stringCount; ++index)
        {
            plain.push_back(levels[static_cast<std::size_t>(index)]
                            - 20.0 * std::log10(spec.strings[static_cast<std::size_t>(index)].touch));
            mean += plain.back() / static_cast<double>(spec.stringCount);
        }
        for (const double level : plain)
            intended.push_back(level + correction * (mean - level));

        std::printf("%s, velocity %.2f: spread %.2f dB, %.2f dB without touches, %.2f dB intended\n", spec.name,
                    velocity, spreadOf(levels), spreadOf(plain), spreadOf(intended));
        double worst = 0.0;
        for (int index = 0; index < spec.stringCount; ++index)
        {
            const auto i = static_cast<std::size_t>(index);
            const auto& string = spec.strings[i];
            std::printf("  note %3d  %7.2f dB  intended %7.2f  %+5.2f  touch %.3f -> %.3f\n", string.midiNote,
                        levels[i], intended[i], levels[i] - intended[i], string.touch,
                        string.touch * std::pow(10.0, (intended[i] - levels[i]) / 20.0));
            worst = std::max(worst, std::abs(levels[i] - intended[i]));
        }
        if (check && worst > limit)
        {
            std::printf("FAIL %s: a string is %.2f dB from its intended level, over the %.2f dB limit\n", spec.name,
                        worst, limit);
            ++failures;
        }

        // The strikes under the lowest string, on the strings' mean level.
        int lowest = spec.strings[0].midiNote;
        double stringMean = 0.0;
        for (int index = 0; index < spec.stringCount; ++index)
        {
            lowest = std::min(lowest, spec.strings[static_cast<std::size_t>(index)].midiNote);
            stringMean += levels[static_cast<std::size_t>(index)] / static_cast<double>(spec.stringCount);
        }
        std::vector<double> struck, plainStruck, intendedStruck;
        for (std::size_t key = 0; key < acukoto::strikeKeys.size(); ++key)
        {
            struck.push_back(levelOfKey(*engine, controls, lowest - static_cast<int>(acukoto::strikeKeys.size())
                                                               + static_cast<int>(key), velocity));
            plainStruck.push_back(struck.back() - 20.0 * std::log10(acukoto::strikeKeys[key].touch));
            intendedStruck.push_back(plainStruck.back() + correction * (stringMean - plainStruck.back()));
        }
        std::printf("%s strikes, velocity %.2f: spread %.2f dB, %.2f dB without touches, %.2f dB intended, "
                    "strings' mean %.2f dB\n", spec.name, velocity, spreadOf(struck), spreadOf(plainStruck),
                    spreadOf(intendedStruck), stringMean);
        worst = 0.0;
        for (std::size_t key = 0; key < acukoto::strikeKeys.size(); ++key)
        {
            const double touch = acukoto::strikeKeys[key].touch;
            std::printf("  key %3d  %7.2f dB  intended %7.2f  %+5.2f  touch %.3f -> %.3f\n",
                        lowest - static_cast<int>(acukoto::strikeKeys.size()) + static_cast<int>(key), struck[key],
                        intendedStruck[key], struck[key] - intendedStruck[key], touch,
                        touch * std::pow(10.0, (intendedStruck[key] - struck[key]) / 20.0));
            worst = std::max(worst, std::abs(struck[key] - intendedStruck[key]));
        }
        if (check && worst > limit)
        {
            std::printf("FAIL %s: a strike is %.2f dB from its intended level, over the %.2f dB limit\n", spec.name,
                        worst, limit);
            ++failures;
        }
    }
    if (check)
        std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
