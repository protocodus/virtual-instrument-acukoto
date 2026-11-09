// Reproducible keyboard survey: one reset, seeded, dry note per MIDI key,
// 64-frame blocks, factory playing controls and no per-file gain. The raw
// engine is the default; --performer includes the shipping output guard and
// trims its declared latency so both paths have the same observation window.
// Writes metrics.csv, manifest.json and optional native-endian interleaved
// stereo float32 key-NNN.f32 files. No timing assertions or fitted thresholds.
// Tools/measure_keyboard_survey.py measures those files and compares surveys.
#include "DSP/AcukotoEngine.h"
#include "DSP/AcukotoPerformer.h"
#include "DSP/AcukotoPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct Options
{
    std::string output;
    int preset = 0, rate = 48000, style = -1, first = 0, last = 127;
    double velocity = 0.7, seconds = 2.0;
    bool performer = false, audio = true;
};

Options options(int argc, char** argv)
{
    if (argc < 2)
        throw std::runtime_error("usage: AcukotoKeyboardSurvey output-directory [--preset 0|1] [--rate Hz] "
                                 "[--velocity 0..1] [--seconds 0.1..30] [--style key] [--first key] "
                                 "[--last key] [--performer] [--no-audio]");
    Options out;
    out.output = argv[1];
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--performer") { out.performer = true; continue; }
        if (arg == "--no-audio") { out.audio = false; continue; }
        if (++i == argc)
            throw std::runtime_error("missing value for " + arg);
        const std::string value = argv[i];
        if (arg == "--preset") out.preset = std::stoi(value);
        else if (arg == "--rate") out.rate = std::stoi(value);
        else if (arg == "--velocity") out.velocity = std::stod(value);
        else if (arg == "--seconds") out.seconds = std::stod(value);
        else if (arg == "--style") out.style = std::stoi(value);
        else if (arg == "--first") out.first = std::stoi(value);
        else if (arg == "--last") out.last = std::stoi(value);
        else throw std::runtime_error("unknown option " + arg);
    }
    if (out.preset < 0 || out.preset >= acukoto::Performer::instrumentCount
        || acukoto::Performer::usableRate(out.rate) != out.rate
        || !std::isfinite(out.velocity) || out.velocity < 0.0 || out.velocity > 1.0
        || !std::isfinite(out.seconds) || out.seconds < 0.1 || out.seconds > 30.0
        || out.style < -1 || out.style > 37 || (out.style >= 0 && out.style < 21)
        || out.first < 0 || out.last > 127 || out.first > out.last)
        throw std::runtime_error("invalid survey range");
    return out;
}

double rmsDb(const std::vector<float>& left, const std::vector<float>& right, int first, int last)
{
    double sum = 0.0;
    for (int i = first; i < last; ++i)
        sum += static_cast<double>(left[static_cast<std::size_t>(i)]) * left[static_cast<std::size_t>(i)]
             + static_cast<double>(right[static_cast<std::size_t>(i)]) * right[static_cast<std::size_t>(i)];
    return 10.0 * std::log10(sum / (2.0 * std::max(1, last - first)) + 1.0e-30);
}
}

int main(int argc, char** argv)
{
    try
    {
        const Options o = options(argc, argv);
        std::filesystem::create_directories(o.output);
        std::unique_ptr<acukoto::AcukotoEngine> engine;
        std::unique_ptr<acukoto::Performer> performer;
        auto controls = acukoto::Performer::defaultControls(o.preset);
        controls.room = 0.0;
        if (o.performer)
        {
            performer = std::make_unique<acukoto::Performer>();
            performer->prepare(o.rate);
            performer->setInstrument(o.preset);
            performer->setControls(controls);
        }
        else
        {
            engine = std::make_unique<acukoto::AcukotoEngine>();
            engine->prepare(o.rate);
            engine->setInstrument(*acukoto::presets[static_cast<std::size_t>(o.preset)]);
            engine->setControls(controls);
        }
        const int latency = performer ? performer->latency() : 0;
        const int frames = static_cast<int>(std::lround(o.seconds * o.rate));
        std::vector<float> left(static_cast<std::size_t>(frames + latency));
        std::vector<float> right(left.size()), interleaved(static_cast<std::size_t>(2 * frames));
        std::ofstream manifest(o.output + "/manifest.json");
        manifest << std::setprecision(12) << "{\n  \"sample_rate\": " << o.rate << ",\n  \"preset\": " << o.preset
                 << ",\n  \"velocity\": " << o.velocity << ",\n  \"frames\": " << frames
                 << ",\n  \"style_key\": " << o.style << ",\n  \"block_frames\": 64,\n  \"path\": \""
                 << (performer ? "performer" : "engine") << "\",\n  \"latency_trimmed\": " << latency
                 << ",\n  \"sample_format\": \"native-endian-float32-stereo\"\n}\n";
        if (!manifest) throw std::runtime_error("cannot write manifest");
        std::ofstream csv(o.output + "/metrics.csv");
        csv << "key,peak,rms_db,attack_rms_db,tail_rms_db\n" << std::setprecision(12);
        double maximum = 0.0;
        for (int key = o.first; key <= o.last; ++key)
        {
            if (performer) performer->reset(); else engine->reset();
            const auto note = [&] (int midi)
            {
                if (performer) performer->noteOn(midi, static_cast<float>(o.velocity));
                else engine->noteOn(midi, static_cast<float>(o.velocity));
            };
            if (o.style >= 0) note(o.style);
            note(key);
            for (int i = 0; i < frames + latency; i += 64)
            {
                const int count = std::min(64, frames + latency - i);
                if (performer) performer->process(left.data() + i, right.data() + i, count);
                else engine->process(left.data() + i, right.data() + i, count);
            }
            double peak = 0.0;
            for (int i = 0; i < frames; ++i)
            {
                const auto source = static_cast<std::size_t>(i + latency);
                const double a = left[source], b = right[source];
                if (!std::isfinite(a) || !std::isfinite(b)) throw std::runtime_error("nonfinite key " + std::to_string(key));
                peak = std::max({ peak, std::abs(a), std::abs(b) });
                interleaved[static_cast<std::size_t>(2 * i)] = left[source];
                interleaved[static_cast<std::size_t>(2 * i + 1)] = right[source];
            }
            maximum = std::max(maximum, peak);
            csv << key << ',' << peak << ',' << rmsDb(left, right, latency, latency + frames) << ','
                << rmsDb(left, right, latency, latency + std::min(frames, o.rate / 20)) << ','
                << rmsDb(left, right, latency + frames * 3 / 4, latency + frames) << '\n';
            csv.flush();
            if (!csv) throw std::runtime_error("cannot write metrics");
            if (o.audio)
            {
                char name[32];
                std::snprintf(name, sizeof name, "/key-%03d.f32", key);
                std::ofstream audio(o.output + name, std::ios::binary);
                audio.write(reinterpret_cast<const char*>(interleaved.data()),
                            static_cast<std::streamsize>(interleaved.size() * sizeof(float)));
                if (!audio) throw std::runtime_error("cannot write audio");
            }
        }
        std::cout << "surveyed " << o.last - o.first + 1 << " keys; " << (performer ? "performer" : "engine")
                  << " peak " << maximum << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
