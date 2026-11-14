// The instrument as a front end plays it. Every preset is designed
// once in prepare() (not real time), so choosing an instrument is a
// real-time switch: the old one fades out over 20 ms while the new one
// starts. The Rack adapter plays through it, and so would a plug-in.
//
// Oversampling runs the instruments at 2 or 4 times the host rate and
// filters the result back down; it changes how finely the strings, bridge
// and body are sampled, not what they are designed to be.
//
// The output stage is the portfolio's (OutputStage.h) with its plucked
// guard, kVuGuard: a soft-knee compressor that keeps the loudest playing out
// of the red, leaves typical playing alone and still lets harder chords read
// louder, then a look-ahead peak limiter that holds every sample under
// -1.5 dBFS.

#pragma once

#include "AcukotoEngine.h"
#include "AcukotoPresets.h"
#include "OutputStage.h"

#include <array>
#include <cmath>
#include <memory>

namespace acukoto
{

class Performer
{
public:
    static constexpr int instrumentCount = static_cast<int>(presets.size());
    static constexpr int maxOversampling = 4;
    // The engine designs up to 192 kHz, so a high host rate lowers the
    // factor: 4x runs as 2x at 88.2 and 96 kHz and as 1x above.
    static constexpr double maxEngineRate = 192000.0;

    Performer();
    ~Performer();

    // Setup: designs every instrument at `oversampling` (1, 2 or 4) times
    // sampleRate. Not real-time.
    void prepare(double sampleRate, int oversampling = 1);
    // The rate prepare() runs at: a rate the engine cannot design for runs
    // at 48 kHz, as the engine itself would, so the output stage and the
    // engines agree.
    [[nodiscard]] static double usableRate(double sampleRate) noexcept
    {
#if defined(__clang__) || defined(__GNUC__)
        const bool finite = __builtin_isfinite(sampleRate);
#else
        const bool finite = std::isfinite(sampleRate);
#endif
        return finite && sampleRate >= 22050.0 && sampleRate <= maxEngineRate ? sampleRate : 48000.0;
    }
    void reset() noexcept;

    void setInstrument(int index) noexcept;
    // Applies to every instrument, so a switch keeps the player's settings.
    void setControls(const PlayingControls& controls) noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int instrument() const noexcept { return active_; }
    // The factor in use, after the engine's rate limit.
    [[nodiscard]] int oversampling() const noexcept { return factor_; }
    // Output delay in host samples: the limiter's look-ahead, plus the
    // decimating filter's when oversampling.
    [[nodiscard]] int latency() const noexcept { return stage_.DelaySamples() + (factor_ > 1 ? decimatorDelay : 0); }
    [[nodiscard]] const AcukotoEngine& engine() const noexcept { return *engines_[static_cast<std::size_t>(active_)]; }
    // An instrument's own way of playing (its preset's PlayingControls).
    [[nodiscard]] static PlayingControls defaultControls(int index) noexcept;

private:
    // A linear-phase Kaiser-windowed sinc, as Ceres decimates: 62 taps per
    // factor, so its delay is 31 host samples at 2x and 4x alike.
    static constexpr int decimatorDelay = 31;
    struct Decimator
    {
        static constexpr int maxTaps = 2 * decimatorDelay * maxOversampling + 1;
        int length { 1 };
        int cursor { 0 };
        std::array<double, maxTaps> taps {};
        // Each stereo frame is written twice, so the newest `length` frames are contiguous.
        std::array<std::array<double, 2>, 2 * maxTaps> history {};

        void design(int factor) noexcept;
        void clear() noexcept;
        void push(double left, double right) noexcept;
        void output(float& left, float& right) const noexcept;
    };

    // Runs the output stage. Once its delay holds only silence the stage is
    // reset and skipped while the silence lasts, as a sleeping engine is, so
    // what follows depends on nothing before it, whatever the block sizes.
    void finish(float* left, float* right, int numSamples) noexcept;
    [[nodiscard]] bool stageResting() const noexcept { return stageSilence_ > stage_.DelaySamples(); }

    void render(float* left, float* right, int numSamples) noexcept;

    std::array<std::unique_ptr<AcukotoEngine>, instrumentCount> engines_;
    PlayingControls controls_ {};
    int active_ { 0 };
    int fading_ { -1 };
    int fadeRemaining_ { 0 };
    int fadeLength_ { 960 };
    int factor_ { 1 };
    // Fast samples of silence pushed while the engines slept; once they
    // fill the decimator, it holds only zeros and is skipped.
    int silentPushes_ { 0 };
    int stageSilence_ { 0 };
    bool sustain_ { false };
    Decimator decimator_ {};
    std::array<std::array<float, 64 * maxOversampling>, 2> fast_ {};
    protocodus::OutputStage<2> stage_ {};
};

} // namespace acukoto
