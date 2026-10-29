// Renders a koto phrase script through the engine and the portfolio's output
// stage to a 48 kHz 24-bit stereo WAV.
//
//   RenderAcukotoPhrase <preset> <script.txt> <output.wav>
//       [--technique finger|nail|plectrum] [--position m] [--room 0-1]
//       [--release 0-1] [--rms dBFS] [--tail s] [--rate hz]
//
// A script line is `<seconds> <command> <arguments>`, '#' starts a comment:
//   note <midi> <velocity 0-1>      the string tuned to it (oshide if needed)
//   off <midi>                      its key-up
//   pluck <string 1-13> <velocity>  a string by its number
//   press <string 1-13> <semitones> <seconds>   the left hand behind the ji
//   damp <0-1>                      the palm across the strings (mute)
//
// The level is set before the output stage: the whole render is scaled so its
// RMS is --rms dBFS (default -20, typical playing near -8 VU on Reason's
// meter), then the stage's guard compressor and limiter run as a Performer's
// do. The stage's look-ahead is trimmed off the start.

#include "../Source/DSP/AcukotoEngine.h"
#include "../Source/DSP/AcukotoPresets.h"
#include "../Source/DSP/OutputStage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct Event
{
    double time;
    std::string command;
    double a, b, c;
};

std::vector<Event> readScript(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("cannot read " + path);
    std::vector<Event> events;
    std::string line;
    while (std::getline(file, line))
    {
        if (const auto hash = line.find('#'); hash != std::string::npos)
            line.resize(hash);
        std::istringstream in(line);
        Event event { 0.0, {}, 0.0, 0.0, 0.0 };
        if (!(in >> event.time >> event.command))
            continue;
        in >> event.a >> event.b >> event.c;
        events.push_back(event);
    }
    std::stable_sort(events.begin(), events.end(), [] (const Event& x, const Event& y) { return x.time < y.time; });
    return events;
}

void writeWav24(const std::string& path, const std::vector<float>& left, const std::vector<float>& right, int rate)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot write " + path);
    const auto frames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = frames * 6u;
    const auto put32 = [&] (std::uint32_t v) { for (int i = 0; i < 4; ++i) file.put(static_cast<char>((v >> (8 * i)) & 0xff)); };
    const auto put16 = [&] (std::uint16_t v) { file.put(static_cast<char>(v & 0xff)); file.put(static_cast<char>(v >> 8)); };
    file.write("RIFF", 4);
    put32(36u + dataBytes);
    file.write("WAVEfmt ", 8);
    put32(16);
    put16(1);
    put16(2);
    put32(static_cast<std::uint32_t>(rate));
    put32(static_cast<std::uint32_t>(rate) * 6u);
    put16(6);
    put16(24);
    file.write("data", 4);
    put32(dataBytes);
    for (std::uint32_t i = 0; i < frames; ++i)
        for (const float value : { left[i], right[i] })
        {
            const auto sample = static_cast<std::int32_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * 8388607.0f));
            for (int k = 0; k < 3; ++k)
                file.put(static_cast<char>((sample >> (8 * k)) & 0xff));
        }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4)
        {
            std::cerr << "usage: RenderAcukotoPhrase <preset> <script.txt> <output.wav> [options]\n";
            return 2;
        }
        const int preset = std::clamp(std::atoi(argv[1]), 0, static_cast<int>(acukoto::presets.size()) - 1);
        const auto events = readScript(argv[2]);
        const std::string output = argv[3];
        const acukoto::InstrumentSpec& spec = *acukoto::presets[static_cast<std::size_t>(preset)];
        acukoto::PlayingControls controls {};
        controls.technique = spec.technique;
        controls.pluckPosition = spec.pluckPosition;
        controls.normalShare = spec.normalShare;
        controls.room = spec.room;
        controls.release = 1.0;
        double rms = -20.0, tail = 3.0;
        int rate = 48000;
        for (int i = 4; i + 1 < argc; i += 2)
        {
            const std::string option = argv[i], value = argv[i + 1];
            if (option == "--technique")
                controls.technique = value == "finger" ? acukoto::Technique::Finger
                                   : value == "nail" ? acukoto::Technique::Nail : acukoto::Technique::Plectrum;
            else if (option == "--position") controls.pluckPosition = std::atof(value.c_str());
            else if (option == "--room") controls.room = std::atof(value.c_str());
            else if (option == "--release") controls.release = std::atof(value.c_str());
            else if (option == "--rms") rms = std::atof(value.c_str());
            else if (option == "--tail") tail = std::atof(value.c_str());
            else if (option == "--rate") rate = std::atoi(value.c_str());
            else throw std::runtime_error("unknown option " + option);
        }

        auto engine = std::make_unique<acukoto::AcukotoEngine>();
        engine->prepare(rate);
        engine->setInstrument(spec);
        engine->setControls(controls);
        engine->reset();

        const double length = (events.empty() ? 0.0 : events.back().time) + tail;
        const auto frames = static_cast<std::size_t>(length * rate);
        std::vector<float> left(frames), right(frames);
        std::size_t next = 0, position = 0;
        while (position < frames)
        {
            while (next < events.size() && static_cast<std::size_t>(events[next].time * rate) <= position)
            {
                const Event& e = events[next++];
                if (e.command == "note") engine->noteOn(static_cast<int>(e.a), static_cast<float>(e.b));
                else if (e.command == "off") engine->noteOff(static_cast<int>(e.a));
                else if (e.command == "pluck") engine->pluckString(static_cast<int>(e.a) - 1, static_cast<float>(e.b));
                else if (e.command == "press") engine->press(static_cast<int>(e.a) - 1, e.b, e.c);
                else if (e.command == "damp")
                {
                    controls.mute = e.a;
                    engine->setControls(controls);
                }
                else throw std::runtime_error("unknown command " + e.command);
            }
            std::size_t until = frames;
            if (next < events.size())
                until = std::min(frames, static_cast<std::size_t>(events[next].time * rate));
            const auto count = static_cast<int>(std::min<std::size_t>(std::max<std::size_t>(until - position, 1), 64));
            engine->process(left.data() + position, right.data() + position, count);
            position += static_cast<std::size_t>(count);
        }

        double power = 0.0;
        for (std::size_t i = 0; i < frames; ++i)
            power += 0.5 * (double(left[i]) * left[i] + double(right[i]) * right[i]);
        power /= std::max<std::size_t>(frames, 1);
        double peak = 0.0;
        for (std::size_t i = 0; i < frames; ++i)
            peak = std::max({ peak, std::abs(double(left[i])), std::abs(double(right[i])) });
        // The tsume's attack is sharp (a crest factor of 22-27 dB over a
        // phrase), so the RMS target alone can drive the limiter hard and
        // duck the strings already sounding; the gain never puts the raw
        // peak more than 3.5 dB over the limiter's -1.5 dBFS ceiling.
        double gain = power > 0.0 ? std::pow(10.0, rms / 20.0) / std::sqrt(power) : 1.0;
        if (peak > 0.0)
            gain = std::min(gain, std::pow(10.0, 2.0 / 20.0) / peak);
        std::printf("crest factor %.1f dB\n", 20.0 * std::log10(peak / std::sqrt(power)));

        protocodus::OutputStage<2> stage;
        stage.Prepare(rate, protocodus::kVuGuard);
        const int delay = stage.DelaySamples();
        std::vector<float> outLeft(frames), outRight(frames);
        for (std::size_t i = 0; i < frames + static_cast<std::size_t>(delay); ++i)
        {
            std::array<float, 2> frame { i < frames ? static_cast<float>(gain * left[i]) : 0.0f,
                                         i < frames ? static_cast<float>(gain * right[i]) : 0.0f };
            stage.Process(frame);
            if (i >= static_cast<std::size_t>(delay))
            {
                outLeft[i - static_cast<std::size_t>(delay)] = frame[0];
                outRight[i - static_cast<std::size_t>(delay)] = frame[1];
            }
        }
        writeWav24(output, outLeft, outRight, rate);
        std::printf("%s: %.1f s, gain %.1f dB\n", output.c_str(), length, 20.0 * std::log10(gain));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
