// Renders one demo per factory preset (README G3) from the committed phrases in
// Docs/audio/midi/, through the Performer a front end plays, with the shared
// renderer in Tools/PresetDemoRenderer.h:
//
//   RenderAcukotoPresetDemos [--jobs N] Docs/audio/midi <out-dir>
//   RenderAcukotoPresetDemos [--jobs N] --check Docs/audio/midi
//   RenderAcukotoPresetDemos --cpu [--repeats N] [--json FILE] Docs/audio/midi
//
// The phrases play note on and off, which pluck and damp the string tuned to
// the note (oshide for a note a semitone or two above a string), and CC64.

#include "DSP/AcukotoPerformer.h"
#include "DSP/AcukotoPresets.h"
#include "PresetDemoRenderer.h"

#include <cstdint>
#include <memory>

namespace
{
namespace demos = protocodus::demos;

class PresetVoice final : public demos::Voice
{
public:
    PresetVoice(std::size_t preset, double sampleRate)
    {
        performer->prepare(sampleRate);
        performer->setInstrument(static_cast<int>(preset));
        performer->setControls(acukoto::Performer::defaultControls(static_cast<int>(preset)));
    }

    void Midi(const std::uint8_t* bytes, int size) override
    {
        if (size < 3)
            return;
        const int status = bytes[0] & 0xf0;
        if (status == 0x90 && bytes[2] > 0)
            performer->noteOn(bytes[1], static_cast<float>(bytes[2]) / 127.0f);
        else if (status == 0x80 || status == 0x90)
            performer->noteOff(bytes[1]);
        else if (status == 0xb0 && bytes[1] == 64)
            performer->setSustain(bytes[2] >= 64);
    }

    void Render(float* left, float* right, int frames) override { performer->process(left, right, frames); }

private:
    std::unique_ptr<acukoto::Performer> performer = std::make_unique<acukoto::Performer>();
};
} // namespace

int main(int argc, char** argv)
{
    demos::Instrument instrument;
    instrument.name = "Acukoto";
    // Real time on one core. The slower demo's median was 0.455x realtime
    // (Koto Hankumoijoshi) on a 2.1 GHz Xeon cloud container on 2026-10-29,
    // so three times it would allow more than real time; re-set this from a
    // CI measurement once the engine is lighter.
    instrument.cpuBudget = 1.0;
    for (const acukoto::InstrumentSpec* preset : acukoto::presets)
        instrument.presets.emplace_back(preset->name);
    instrument.open = [] (std::size_t preset, double sampleRate, int, std::uint32_t)
    { return std::make_unique<PresetVoice>(preset, sampleRate); };
    return demos::Main(argc, argv, instrument);
}
