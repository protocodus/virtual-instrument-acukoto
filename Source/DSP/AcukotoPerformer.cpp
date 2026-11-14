#include "AcukotoPerformer.h"

#include <algorithm>
#include <cmath>

namespace acukoto
{

Performer::Performer()
{
    for (auto& engine : engines_)
        engine = std::make_unique<AcukotoEngine>();
    controls_ = defaultControls(0);
}

Performer::~Performer() = default;

PlayingControls Performer::defaultControls(int index) noexcept
{
    const InstrumentSpec& spec = *presets[static_cast<std::size_t>(std::clamp(index, 0, instrumentCount - 1))];
    PlayingControls controls {};
    controls.technique = spec.technique;
    controls.pluckDistance = spec.pluckDistance;
    controls.normalShare = spec.normalShare;
    controls.room = spec.room;
    // A koto's string rings on after the pluck; the hand stops it only on
    // purpose (PlayingControls::mute).
    controls.release = 1.0;
    return controls;
}

void Performer::prepare(double sampleRate, int oversampling)
{
    sampleRate = usableRate(sampleRate);
    factor_ = 1;
    while (factor_ * 2 <= std::min(oversampling, maxOversampling) && sampleRate * factor_ * 2 <= maxEngineRate)
        factor_ *= 2;
    decimator_.design(factor_);
    stage_.Prepare(sampleRate, protocodus::kVuGuard);
    stageSilence_ = stage_.DelaySamples() + 1;
    for (int index = 0; index < instrumentCount; ++index)
    {
        AcukotoEngine& engine = *engines_[static_cast<std::size_t>(index)];
        engine.prepare(sampleRate * factor_);
        if (engine.stringCount() == 0)
            engine.setInstrument(*presets[static_cast<std::size_t>(index)], index > 0 ? engines_[0].get() : nullptr);
        engine.setControls(controls_);
        engine.reset();
    }
    fadeLength_ = std::max(1, static_cast<int>(0.02 * engines_[0]->sampleRate()));
    fading_ = -1;
    silentPushes_ = 0;
    sustain_ = false;
}

void Performer::reset() noexcept
{
    for (auto& engine : engines_)
        engine->reset();
    decimator_.clear();
    stage_.Reset();
    stageSilence_ = stage_.DelaySamples() + 1;
    fading_ = -1;
    silentPushes_ = 0;
    sustain_ = false;
}

void Performer::setInstrument(int index) noexcept
{
    index = std::clamp(index, 0, instrumentCount - 1);
    if (index == active_)
        return;
    // A second switch within the fade cuts the tail that was fading.
    if (fading_ >= 0 && fading_ != index)
    {
        AcukotoEngine& cut = *engines_[static_cast<std::size_t>(fading_)];
        cut.reset();
        cut.setSustain(sustain_);
    }
    fading_ = active_;
    active_ = index;
    fadeRemaining_ = fadeLength_;
}

void Performer::setControls(const PlayingControls& controls) noexcept
{
    controls_ = controls;
    for (auto& engine : engines_)
        engine->setControls(controls);
}

void Performer::noteOn(int midiNote, float velocity) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->noteOn(midiNote, velocity);
}

void Performer::noteOff(int midiNote) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->noteOff(midiNote);
}

void Performer::setSustain(bool down) noexcept
{
    sustain_ = down;
    for (auto& engine : engines_)
        engine->setSustain(down);
}

void Performer::process(float* left, float* right, int numSamples) noexcept
{
    if (factor_ == 1)
    {
        render(left, right, numSamples);
        finish(left, right, numSamples);
        return;
    }
    if (fading_ < 0 && engines_[static_cast<std::size_t>(active_)]->idle()
        && silentPushes_ >= decimator_.length && stageResting())
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }
    const int chunk = static_cast<int>(fast_[0].size()) / factor_;
    for (int done = 0; done < numSamples;)
    {
        const int count = std::min(chunk, numSamples - done);
        const bool silent = fading_ < 0 && engines_[static_cast<std::size_t>(active_)]->idle();
        render(fast_[0].data(), fast_[1].data(), count * factor_);
        silentPushes_ = silent ? silentPushes_ + count * factor_ : 0;
        for (int i = 0; i < count; ++i)
        {
            for (int k = 0; k < factor_; ++k)
            {
                const auto idx = static_cast<std::size_t>(i * factor_ + k);
                decimator_.push(fast_[0][idx], fast_[1][idx]);
            }
            decimator_.output(left[done + i], right[done + i]);
        }
        done += count;
    }
    finish(left, right, numSamples);
}

void Performer::finish(float* left, float* right, int numSamples) noexcept
{
    const int delay = stage_.DelaySamples();
    for (int i = 0; i < numSamples; ++i)
    {
        if (left[i] == 0.0f && right[i] == 0.0f)
        {
            if (stageResting())
                continue;
            // The delay now holds only silence, so this frame's output is
            // silence too.
            if (++stageSilence_ > delay)
            {
                stage_.Reset();
                continue;
            }
        }
        else
            stageSilence_ = 0;
        std::array<float, 2> frame { left[i], right[i] };
        stage_.Process(frame);
        left[i] = frame[0];
        right[i] = frame[1];
    }
}

void Performer::render(float* left, float* right, int numSamples) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->process(left, right, numSamples);
    int done = 0;
    while (fading_ >= 0 && done < numSamples)
    {
        constexpr int chunk = 64;
        float fadeLeft[chunk], fadeRight[chunk];
        const int count = std::min({ chunk, numSamples - done, fadeRemaining_ });
        AcukotoEngine& fading = *engines_[static_cast<std::size_t>(fading_)];
        fading.process(fadeLeft, fadeRight, count);
        for (int i = 0; i < count; ++i)
        {
            const float gain = static_cast<float>(fadeRemaining_ - i) / static_cast<float>(fadeLength_);
            left[done + i] += gain * fadeLeft[i];
            right[done + i] += gain * fadeRight[i];
        }
        done += count;
        fadeRemaining_ -= count;
        if (fadeRemaining_ <= 0)
        {
            fading.reset();
            fading.setSustain(sustain_);
            fading_ = -1;
        }
    }
}

void Performer::Decimator::design(int factor) noexcept
{
    length = factor > 1 ? 2 * decimatorDelay * factor + 1 : 1;
    if (factor == 1)
    {
        taps[0] = 1.0;
        clear();
        return;
    }
    // Cut off at 90% of the host Nyquist; beta 10 keeps the stopband
    // about 100 dB down.
    constexpr double pi = 3.14159265358979323846, beta = 10.0;
    const auto besselI0 = [] (double x)
    {
        double term = 1.0, sum = 1.0;
        for (int k = 1; k < 40 && term > sum * 1.0e-16; ++k)
        {
            term *= x * x * 0.25 / (static_cast<double>(k) * k);
            sum += term;
        }
        return sum;
    };
    const double cutoff = 0.45 / factor, middle = (length - 1) * 0.5;
    double sum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const double n = i - middle, r = n / middle;
        const double sinc = n == 0.0 ? 2.0 * cutoff : std::sin(2.0 * pi * cutoff * n) / (pi * n);
        taps[static_cast<std::size_t>(i)] = sinc * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r)));
        sum += taps[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < length; ++i)
        taps[static_cast<std::size_t>(i)] /= sum;
    clear();
}

void Performer::Decimator::clear() noexcept
{
    history = {};
    cursor = 0;
}

void Performer::Decimator::push(double left, double right) noexcept
{
    cursor = (cursor == 0 ? length : cursor) - 1;
#if defined(__clang__) || defined(__GNUC__)
    typedef double Stereo __attribute__((vector_size(16), aligned(8), may_alias));
    static_assert(alignof(Stereo) == 8, "unaligned 128-bit stereo vector");
    const Stereo frame { left, right };
    *reinterpret_cast<Stereo*>(history[static_cast<std::size_t>(cursor)].data()) = frame;
    *reinterpret_cast<Stereo*>(history[static_cast<std::size_t>(cursor + length)].data()) = frame;
#else
    history[static_cast<std::size_t>(cursor)] = { left, right };
    history[static_cast<std::size_t>(cursor + length)] = { left, right };
#endif
}

// The two channels keep independent sums in the original tap order.
// Parallelize channels, not taps: no reassociation of the FIR's recurrence.
void Performer::Decimator::output(float& left, float& right) const noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    typedef double Stereo __attribute__((vector_size(16), aligned(8), may_alias));
    const auto* window = history.data() + cursor;
    Stereo sum { 0.0, 0.0 };
    for (int i = 0; i < length; ++i)
    {
        const double tap = taps[static_cast<std::size_t>(i)];
        sum += Stereo { tap, tap } * *reinterpret_cast<const Stereo*>(window[i].data());
    }
    left = static_cast<float>(sum[0]);
    right = static_cast<float>(sum[1]);
#else
    const auto* window = history.data() + cursor;
    double sumL = 0.0, sumR = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const double tap = taps[static_cast<std::size_t>(i)];
        sumL += tap * window[i][0];
        sumR += tap * window[i][1];
    }
    left = static_cast<float>(sumL);
    right = static_cast<float>(sumR);
#endif
}

} // namespace acukoto
