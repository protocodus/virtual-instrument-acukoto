// Mechanical work at the string/body/hand junction, independent of the
// acoustic observer and output gain. Arriving/departing velocity-wave power
// Z(a^2-b^2) must equal F v at the actual modal endpoint, and modal input
// work must equal string plus hand work. Instrumentation observes the modes'
// output, not the junction's intended velocity. Positive tolerance accounts
// for the production bank's float force and participation accumulations.
#include "DSP/AcukotoEngine.h"
#include "DSP/AcukotoPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
int failures = 0;
void check(bool okay, const char* name, double value)
{
    if (!okay)
    {
        std::fprintf(stderr, "FAIL %s: %.12g\n", name, value);
        ++failures;
    }
}

void render(acukoto::AcukotoEngine& engine, double seconds)
{
    std::array<float, 64> left {}, right {};
    int remaining = static_cast<int>(std::lround(seconds * engine.sampleRate()));
    while (remaining > 0)
    {
        const int block = std::min(remaining, static_cast<int>(left.size()));
        engine.process(left.data(), right.data(), block);
        for (int k = 0; k < block; ++k)
            check(std::isfinite(left[static_cast<std::size_t>(k)]) && std::isfinite(right[static_cast<std::size_t>(k)]),
                  "finite strike", k);
        remaining -= block;
    }
}
}

int main()
{
    double worstVelocity = 0.0, worstJunction = 0.0, worstModal = 0.0;
    double stringWork = 0.0, bodyWork = 0.0, handWork = 0.0, modalWork = 0.0;
    int cases = 0;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto first = std::make_unique<acukoto::AcukotoEngine>();
        first->prepare(rate);
        first->setInstrument(*acukoto::presets[0]);
        for (int preset = 0; preset < 2; ++preset)
        {
            auto other = std::make_unique<acukoto::AcukotoEngine>();
            if (preset == 1)
            {
                other->prepare(rate);
                other->setInstrument(*acukoto::presets[1], first.get());
            }
            acukoto::AcukotoEngine& engine = preset == 0 ? *first : *other;
            auto controls = engine.controls();
            controls.release = 1.0;
            controls.room = 0.0;
            engine.setControls(controls);
            engine.setTensionModulation(false);
            // Every patch/hand at two intensities, both from rest and while
            // existing waves are meeting the bridge. key=-1 covers no strike.
            for (int key = -1; key < 12; ++key)
                for (float velocity : { 0.3f, 0.95f })
                {
                    engine.reset();
                    check(engine.junctionEnergyReport().samples == 0, "reset work observer", 0.0);
                    if (key < 0 || velocity > 0.5f)
                    {
                        engine.pluckString(4, 0.7f);
                        engine.pluckString(10, 0.5f);
                        render(engine, 0.03);
                    }
                    if (key >= 0)
                        engine.noteOn(38 + key, velocity);
                    render(engine, 0.12);
                    const auto report = engine.junctionEnergyReport();
                    check(report.samples > 0 && report.absoluteStringWork > 0.0,
                          "junction exercised", report.absoluteStringWork);
                    check(key < 0 ? report.contactSamples == 0 : report.contactSamples > 0,
                          "expected hand contact", static_cast<double>(report.contactSamples));
                    const double endpoint = report.maximumVelocityError / report.maximumVelocity;
                    const double junction = report.absoluteJunctionResidual / report.absoluteStringWork;
                    const double modal = report.absoluteModalResidual / report.absoluteSourceWork;
                    // This is a float-accumulation allowance, not a sound
                    // tolerance. The omitted direct hand term gives up to
                    // percent-level errors in endpoint velocity instead.
                    check(endpoint < 2.0e-5, "one velocity at each string/body endpoint", endpoint);
                    check(junction < 2.0e-5, "string wave work enters the body", junction);
                    check(modal < 2.0e-5, "source work enters the modes", modal);
                    worstVelocity = std::max(worstVelocity, endpoint);
                    worstJunction = std::max(worstJunction, junction);
                    worstModal = std::max(worstModal, modal);
                    stringWork += report.stringWork;
                    bodyWork += report.bodyStringWork;
                    handWork += report.handWork;
                    modalWork += report.modalInputWork;
                    ++cases;
                }
        }
    }
    std::printf("Acukoto.StrikeEnergy: %d cases; maximum relative endpoint %.9g, junction work %.9g, modal work %.9g\n",
                cases, worstVelocity, worstJunction, worstModal);
    std::printf("Signed work across cases (J): strings %.12g, body from strings %.12g, hand %.12g, modes %.12g\n",
                stringWork, bodyWork, handWork, modalWork);
    return failures == 0 ? 0 : 1;
}
