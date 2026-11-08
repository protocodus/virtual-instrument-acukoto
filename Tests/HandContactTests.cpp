// A local hand contact must remain at its physical place when the string's
// travelling period changes. Inspect the internal waveguide directly: this
// tests a nodal witness and passivity without body/room modes masking either.
// No production diagnostics or branches are added by this test translation unit.
#include "EngineTestImplementation.h"
#include <cstdio>
#include <cstdlib>

namespace
{
int failures = 0;
void check(bool condition, const char* message, double value)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s: %.12g\n", message, value);
        ++failures;
    }
}

double energy(const acukoto::Loop& loop)
{
    double result = 0.0;
    for (float value : loop.line)
        result += static_cast<double>(value) * value;
    return result;
}

// A travelling sinusoid on the full round trip. Lumped filter lag reduces
// the accessible delay storage, but must not rescale the contact's place.
double contactLoss(acukoto::Loop& loop, double rate, double ratio, int partial, double phase)
{
    constexpr double frequency = 392.0;
    const double period = rate / (frequency * ratio);
    loop.loopDelay = rate / frequency;
    loop.restLineDelay = loop.loopDelay - 4.0;
    loop.setLineDelay(period - 4.0);
    loop.write = 0;
    loop.line.fill(0.0f);
    for (int delay = 0; delay < static_cast<int>(period); ++delay)
        loop.line[static_cast<std::size_t>((loop.write - 1 - delay) & (acukoto::delayCapacity - 1))]
            = static_cast<float>(std::sin(2.0 * acukoto::pi * partial * delay / period + phase));
    loop.setHand(0.5, 33.333333333333, frequency, ratio);
    const double before = energy(loop);
    loop.touch();
    return before - energy(loop);
}
}

int main()
{
    auto loop = std::make_unique<acukoto::Loop>();
    double worstNodalRatio = 0.0, maxGrowth = 0.0;
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (double ratio : { std::exp2(-2.0 / 12.0), 1.0, std::exp2(2.0 / 12.0), 2.0 })
        {
            double node = 0.0, antinode = 0.0;
            for (double phase : { 0.1, 0.7, 1.2, 1.8 })
            {
                node += contactLoss(*loop, rate, ratio, 2, phase);
                antinode += contactLoss(*loop, rate, ratio, 1, phase);
            }
            const double relative = std::abs(node) / antinode;
            worstNodalRatio = std::max(worstNodalRatio, relative);
            check(antinode > 0.001, "midpoint contact does not damp fundamental", antinode);
            check(relative < 0.002, "bend moves midpoint contact off second partial's node", relative);
        }

    // Scatter uses the transpose of the fractional read. Its change is
    // x' = x - k h(h^T x), so 0 <= k ||h||^2 <= 2 is contractive. Exercise
    // fractional taps, short lines and coincident/overlapping stencils too.
    std::uint32_t state = 0x137b5dU;
    for (int length : { 3, 4, 8, 31, 120, 240 })
        for (double point : { 0.01, 0.25, 0.5, 0.88, 0.999 })
            for (double ratio : { 0.75, 1.0, 1.5, 2.0 })
                for (double strength : { 0.0, 0.1, 33.333333333333, 1000000.0 })
                {
                    loop->loopDelay = length + 3.2;
                    loop->setLineDelay(length + 1.7);
                    loop->write = 17;
                    loop->line.fill(0.0f);
                    for (int k = 0; k < length + 8; ++k)
                    {
                        state = 1664525U * state + 1013904223U;
                        const float value = static_cast<float>((state >> 8) * (2.0 / 16777216.0) - 1.0);
                        loop->line[static_cast<std::size_t>((loop->write - 1 - k) & (acukoto::delayCapacity - 1))] = value;
                    }
                    loop->setHand(point, strength, 48000.0 / loop->loopDelay, ratio);
                    const double before = energy(*loop);
                    loop->touch();
                    const double growth = energy(*loop) - before;
                    maxGrowth = std::max(maxGrowth, growth);
                    check(growth <= 1.0e-6, "hand contact adds travelling-wave energy", growth);
                }
    std::printf("hand contact: nodal/antinode loss <= %.7g, maximum energy growth %.7g; 4 rates, 4 bends, 480 passive contacts\n",
                worstNodalRatio, maxGrowth);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
