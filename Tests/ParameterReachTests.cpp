// U11 parameter reach over the Performer and the controls the Rack device
// gives it, through the shared portfolio harness (Tests/ParameterMatrix.h is
// a byte-identical copy of the master; change it in reason-rack-extensions).
// There is no plug-in yet, so the device stores each control normalised and
// maps it as the Rack adapter's publish() does (Examples/Acukoto/Acukoto.cpp):
// the panel's properties with motherboard_def.lua's defaults, the mod wheel
// and the sustain pedal, which Reason automates like them, and, with the
// editor-only controls since no automation reaches it, Reason's master tune,
// which the host's settings change. MIDI pitch bend is the pitch wheel, the
// left hand's press on the last plucked string. Oversampling is left out: it
// is a song setting, and the Rack device builds a new instance for it, so no
// sounding note sees it change.
//
//   AcukotoParameterReachTests --reach [--jobs N] [--case PARAMETER]

#include "DSP/AcukotoPerformer.h"
#include "ParameterMatrix.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>

namespace
{
using namespace acukoto;
namespace matrix = protocodus::matrix;

enum Control
{
    kTuning, kTechnique, kOshide, kPluckPosition, kPluckAngle, kRelease, kMute, kSympathy, kRoom, kWidth,
    kVolume, kModWheel, kSustain, kAutomatable,
    kMasterTune = kAutomatable, kControlCount
};

// Name, steps (0 continuous) and the Rack device's default (Acukoto.h,
// kDefaults, normalised).
const std::array<matrix::Parameter, kControlCount> kControls {{
    { "Tuning", 2, 0.0f }, { "Technique", 2, 0.0f }, { "Oshide", 3, 0.5f }, { "Pluck position", 0, 0.25f },
    { "Pluck angle", 0, 0.5129f }, { "Release", 0, 1.0f }, { "Mute", 0, 0.0f }, { "Sympathy", 0, 1.0f },
    { "Room", 0, 0.25f }, { "Width", 0, 0.5f }, { "Volume", 0, 0.72f }, { "Mod wheel", 0, 0.0f },
    { "Sustain pedal", 2, 0.0f }, { "Master tune", 0, 0.5f },
}};

int Index(float normalised, int steps)
{
    return static_cast<int>(std::lround(normalised * static_cast<float>(steps - 1)));
}

class AcukotoDevice final : public matrix::Device
{
public:
    AcukotoDevice() : performer(std::make_unique<Performer>()) { LoadPreset(0); }

    std::vector<matrix::Parameter> Parameters() const override
    {
        return { kControls.begin(), kControls.begin() + kAutomatable };
    }
    std::vector<matrix::Parameter> EditorOnlyParameters() const override
    {
        return { kControls.begin() + kAutomatable, kControls.end() };
    }
    // Each tuning is a preset, at the Rack device's default controls.
    int PresetCount() const override { return Performer::instrumentCount; }
    void Prepare(double rate, int) override
    {
        performer->prepare(rate);
        Apply();
    }
    void LoadPreset(int index) override
    {
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] = kControls[i].defaultValue;
        values[kTuning] = static_cast<float>(index) / static_cast<float>(Performer::instrumentCount - 1);
        Apply();
    }
    float GetParameter(std::size_t index) const override { return values[index]; }
    void SetParameter(std::size_t index, float value) override
    {
        values[index] = value;
        Apply();
    }
    // Performer::process takes no events, so the block is split at each one,
    // as the Rack device splits its batch at each property change.
    void Process(float* left, float* right, int frames, const matrix::Event* events, int count) override
    {
        int done = 0;
        for (int e = 0; e < count; ++e)
        {
            const int at = std::clamp(events[e].offset, done, frames);
            performer->process(left + done, right + done, at - done);
            done = at;
            Handle(events[e]);
        }
        performer->process(left + done, right + done, frames - done);
    }
    std::vector<std::uint8_t> SaveState() override
    {
        std::vector<std::uint8_t> state(sizeof values);
        std::memcpy(state.data(), values.data(), sizeof values);
        return state;
    }
    void RestoreState(const std::vector<std::uint8_t>& state) override
    {
        if (state.size() != sizeof values)
            return;
        std::memcpy(values.data(), state.data(), sizeof values);
        Apply();
    }

    // Every other control reaches the strings, the room or the output at once
    // (AcukotoEngine::setControls).
    matrix::ReachRule ReachFor(std::size_t index, float value) const override
    {
        using matrix::Reach;
        switch (static_cast<int>(index))
        {
        // Performer::setInstrument: the old tuning fades out over 20 ms,
        // whatever it holds or rings, and the new one starts silent. The
        // held chord stops, and probe 1 plays on an instrument the long play
        // never touched, where T's tuning still rings with it: probe 1 sounds
        // within 0.03 dB of a fresh hankumoijoshi playing it alone, T's probe
        // 1 2.8 dB from that (2026-11-15).
        case kTuning:
            return { Reach::Held, "", {},
                     "the strings the long play left ringing, which a switch fades out with the old tuning" };
        // PlayingControls: the plucking fields take effect at the next
        // pluck; a sounding string keeps the pluck it was given.
        case kTechnique:
            return { Reach::NewNotes, "how each new pluck meets the string, tsume or finger" };
        case kPluckPosition:
        case kPluckAngle:
            return { Reach::NewNotes, "where and at what angle each new pluck meets the string" };
        case kOshide:
            // PlayingControls::oshide: the key chooses its string and press
            // before the pluck, and a press holds while its string sounds.
            if (Index(value, 3) == 0)
                return { Reach::NewNotes, "which string and press each new key takes; a sounding string keeps its press" };
            // PlayingControls::legato slurs only a step a held, sounding
            // string reaches by its press while no other key is down; the
            // phrase steps in fifths and plays chords. Acukoto.Engine slurs one.
            return { Reach::Exempt, "Legato acts only on a slurred step of one line, which the phrase never plays" };
        // At Release 1 a let-go string rings on until it dies away, so the
        // pedal has nothing to hold and every string the long play plucked
        // still sounds from its pluck, leaving none to ring in sympathy
        // (AcukotoEngine::updateDamping). The hand stopping let-go strings
        // gives both something to act on.
        case kSympathy:
        case kSustain:
            return { Reach::Held, "", { { kRelease, 0.0f } } };
        default:
            return {};
        }
    }

private:
    void Handle(const matrix::Event& event)
    {
        const std::uint8_t status = event.bytes[0] & 0xF0;
        const int data1 = event.bytes[1], data2 = event.bytes[2];
        if (status == 0x90 && data2 > 0)
            performer->noteOn(data1, static_cast<float>(data2) / 127.0f);
        else if (status == 0x80 || status == 0x90)
            performer->noteOff(data1);
        else if (status == 0xE0)
        {
            pitchWheel = static_cast<double>(data1 | data2 << 7) / 16383.0;
            Apply();
        }
    }
    // Acukoto.cpp, CAcukoto::publish().
    void Apply()
    {
        constexpr double pi = 3.14159265358979323846;
        const auto value = [&](Control control) { return static_cast<double>(values[control]); };
        PlayingControls c;
        c.technique = Index(values[kTechnique], 2) >= 1 ? Technique::Finger : Technique::Plectrum;
        c.pluckDistance = 0.02 + 0.10 * value(kPluckPosition);
        const double angle = (5.0 + 55.0 * value(kPluckAngle)) * pi / 180.0;
        c.normalShare = std::sin(angle) * std::sin(angle);
        c.release = value(kRelease);
        c.mute = std::max(value(kMute), value(kModWheel));
        c.sympathy = value(kSympathy);
        c.room = value(kRoom);
        c.width = 2.0 * value(kWidth);
        c.volume = std::pow(10.0, (-18.0 + 30.0 * value(kVolume)) / 20.0);
        // Reason's master tune, -100 to +100 cents, in semitones.
        c.pitchBend = 2.0 * value(kMasterTune) - 1.0;
        c.handPress = (pitchWheel - 0.5) * (pitchWheel < 0.5 ? 2.0 : 4.0);
        c.oshide = Index(values[kOshide], 3) >= 1;
        c.legato = Index(values[kOshide], 3) >= 2;
        performer->setControls(c);
        performer->setInstrument(Index(values[kTuning], 2));
        performer->setSustain(Index(values[kSustain], 2) >= 1);
    }

    std::unique_ptr<Performer> performer;
    std::array<float, kControlCount> values {};
    double pitchWheel = 0.5;
};

} // namespace

int main(int argc, char** argv)
{
    return protocodus::matrix::Main(argc, argv, [] { return std::make_unique<AcukotoDevice>(); });
}
