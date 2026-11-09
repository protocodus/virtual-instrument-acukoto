// Compare the paired SIMD loop directly with its retained scalar reference.
// Physical coefficients are shared by the two polarisations, as designPlanes,
// updateDamping and pluck require; their delay, wave and filter history differ.
// Exact bits are required: this optimization preserves each lane's operation
// order, including pending-pluck consumption and damping ramp endpoints.
// Include the implementation only in this test to reach the private Loop;
// no test hooks or diagnostic work enter the shipping engine.
#include "EngineTestImplementation.h"

#include <cstdio>
#include <memory>

namespace
{
int failures = 0;

template <typename T>
bool same(const T& a, const T& b)
{
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

bool sameLoss(const acukoto::UnitDcSection& a, const acukoto::UnitDcSection& b)
{
    return same(a.previousInput, b.previousInput)
        && same(a.previousDifference, b.previousDifference)
        && same(a.u1, b.u1) && same(a.u2, b.u2);
}

bool sameHistory(const acukoto::Loop& a, const acukoto::Loop& b)
{
    return a.write == b.write && a.whole == b.whole
        && a.firstOrder == b.firstOrder
        && a.pendingAhead == b.pendingAhead
        && same(a.fraction, b.fraction)
        && same(a.thiranA1, b.thiranA1) && same(a.thiranA2, b.thiranA2)
        && same(a.tx1, b.tx1) && same(a.tx2, b.tx2)
        && same(a.ty1, b.ty1) && same(a.ty2, b.ty2)
        && same(a.last, b.last)
        && same(a.dispersionState, b.dispersionState)
        && same(a.pairState, b.pairState)
        && sameLoss(a.constantLoss, b.constantLoss)
        && sameLoss(a.bendingLoss, b.bendingLoss)
        && same(a.muteState, b.muteState)
        && same(a.damping, b.damping)
        && same(a.dampingTarget, b.dampingTarget)
        && same(a.dampingStep, b.dampingStep);
}

void seed(acukoto::Loop& loop, int lane, int configuration)
{
    loop.write = acukoto::delayCapacity - 19;
    loop.loopDelay = 88.0;
    loop.restLineDelay = lane == 0 ? 73.25 : 81.875;
    for (std::size_t i = 0; i < loop.line.size(); ++i)
        loop.line[i] = static_cast<float>(static_cast<int>((i * 7 + lane * 13) % 31) - 15) / 512.0f;
    loop.setLineDelay(loop.restLineDelay);
    loop.tx1 += (lane + 1) / 128.0;
    loop.tx2 -= (lane + 1) / 256.0;
    loop.ty1 = (lane + 1) / 1024.0;
    loop.ty2 = -(lane + 1) / 2048.0;
    loop.dispersionStages = configuration == 0 ? 0 : (configuration == 1 ? 1 : acukoto::maxDispersionStages);
    loop.dispersion = -0.375;
    loop.dispersionPairs = configuration < 2 ? 0 : acukoto::dispersionPairs;
    for (std::size_t i = 0; i < loop.dispersionState.size(); ++i)
        loop.dispersionState[i] = (lane + 1) * (i + 1) / 65536.0;
    for (std::size_t i = 0; i < loop.pair.size(); ++i)
    {
        loop.pair[i] = { -0.25, 0.125 };
        loop.pairState[i] = { (lane + 1) * (i + 1) / 32768.0,
                            -(lane + 1) * static_cast<double>(i + 1) / 65536.0 };
    }
    for (acukoto::UnitDcSection* loss : { &loop.constantLoss, &loop.bendingLoss })
    {
        loss->active = configuration != 0;
        loss->c0 = 0.0625;
        loss->c1 = -0.015625;
        loss->a1 = -0.25;
        loss->a2 = 0.03125;
        loss->previousInput = (lane + 1) / 128.0;
        loss->previousDifference = -(lane + 1) / 256.0;
        loss->u1 = (lane + 1) / 512.0;
        loss->u2 = -(lane + 1) / 1024.0;
    }
    // Also cover one active section while the other bypasses.
    if (configuration == 1)
        loop.bendingLoss.active = false;
    loop.gain = 0.99609375;
    loop.muteCoefficient = configuration == 3 ? 0.625 : 0.0;
    loop.muteState = (lane + 1) / 256.0;
    loop.pendingAhead = lane == 0 ? 47 : 0;
    for (int k = 0; k < loop.pendingAhead; ++k)
        loop.pending[static_cast<std::size_t>((loop.write + k) & (acukoto::delayCapacity - 1))]
            = static_cast<float>((k % 9) - 4) / 256.0f;
}

void exercise(int configuration, bool shortLines)
{
    // Delay buffers are large; the production engine also keeps them off-stack.
    auto scalar = std::make_unique<std::array<acukoto::Loop, 2>>();
    auto paired = std::make_unique<std::array<acukoto::Loop, 2>>();
    seed((*scalar)[0], 0, configuration);
    seed((*scalar)[1], 1, configuration);
    if (shortLines)
    {
        (*scalar)[0].setLineDelay(1.4);
        (*scalar)[1].setLineDelay(2.1);
    }
    *paired = *scalar;
    bool okay = true;
    int sample = 0;
    for (; sample < 1024 && okay; ++sample)
    {
        for (auto* loops : { scalar.get(), paired.get() })
        {
            if (sample == 0 || sample == 199 || sample == 411)
                for (auto& loop : *loops)
                    loop.setDamping(sample == 0 ? 0.375 : (sample == 199 ? 0.9375 : 0.9375));
            if (sample == 93 || sample == 251 || sample == 500)
                for (int lane = 0; lane < 2; ++lane)
                    (*loops)[static_cast<std::size_t>(lane)].setLineDelay(
                        (lane == 0 ? 73.25 : 81.875) + (sample == 93 ? 0.03125 : (sample == 251 ? 2.5 : -1.25)));
            if (shortLines && sample % 31 == 0)
                for (int lane = 0; lane < 2; ++lane)
                    (*loops)[static_cast<std::size_t>(lane)].setLineDelay(
                        1.01 + ((sample / 31 + lane * 3) % 17) * 0.45);
            if (sample == 137)
            {
                // The other plane receives a new excitation while its partner
                // has already consumed its entire pending wave.
                auto& loop = (*loops)[1];
                loop.pendingAhead = 61;
                for (int k = 0; k < loop.pendingAhead; ++k)
                    loop.pending[static_cast<std::size_t>((loop.write + k) & (acukoto::delayCapacity - 1))]
                        = static_cast<float>((k % 7) - 3) / 512.0f;
            }
            if (sample == 711)
                for (auto& loop : *loops)
                    loop.reset();
        }

        const double reference[2] { (*scalar)[0].read(), (*scalar)[1].read() };
        double actual[2];
#if defined(__clang__) || defined(__GNUC__)
        acukoto::Loop::readPair((*paired)[0], (*paired)[1], actual[0], actual[1]);
#else
        // The supported portable implementation is scalar on these compilers.
        actual[0] = (*paired)[0].read();
        actual[1] = (*paired)[1].read();
#endif
        for (int lane = 0; lane < 2; ++lane)
        {
            const auto i = static_cast<std::size_t>(lane);
            okay = okay && std::isfinite(reference[lane]) && same(reference[lane], actual[lane])
                && sameHistory((*scalar)[i], (*paired)[i]);
            const double excitation = ((sample * (lane + 3)) % 23 - 11) / 1024.0;
            (*scalar)[i].writeSample(0.5 * reference[lane] + excitation);
            (*paired)[i].writeSample(0.5 * actual[lane] + excitation);
        }
    }
    for (std::size_t lane = 0; lane < 2; ++lane)
        okay = okay && same((*scalar)[lane].line, (*paired)[lane].line)
            && same((*scalar)[lane].pending, (*paired)[lane].pending);
    if (!okay)
    {
        std::fprintf(stderr, "FAIL loop pair configuration %d after sample %d\n", configuration, sample);
        ++failures;
    }
}

void stableShortDelays()
{
    auto loop = std::make_unique<acukoto::Loop>();
    for (double delay : {acukoto::minimumLineDelay, 1.01, 1.2, 1.999999, 2.0, std::nextafter(2.0, 3.0), 2.000001, 2.9, 3.0, std::nextafter(3.0, 4.0), 3.000001,
                         4.0, 5.0, 6.0, 7.999999, 8.0, 8.000001})
    {
        loop->setLineDelay(delay);
        const std::complex<double> discriminant = std::sqrt(std::complex<double>(
            loop->thiranA1 * loop->thiranA1 - 4.0 * loop->thiranA2, 0.0));
        const double pole = loop->firstOrder ? std::abs(loop->thiranA1)
            : std::max(std::abs((-loop->thiranA1 + discriminant) * 0.5),
                       std::abs((-loop->thiranA1 - discriminant) * 0.5));
        if (!(pole < 1.0))
        {
            std::fprintf(stderr, "FAIL delay %.9g pole %.17g\n", delay, pole);
            ++failures;
        }
    }
    const auto response = [&] (double delay, double omega)
    {
        loop->setLineDelay(delay);
        const auto z = std::polar(1.0, -omega);
        const auto section = loop->firstOrder
            ? (loop->thiranA1 + z) / (1.0 + loop->thiranA1 * z)
            : (loop->thiranA2 + loop->thiranA1 * z + z * z)
                / (1.0 + loop->thiranA1 * z + loop->thiranA2 * z * z);
        return std::pow(z, loop->whole) * section;
    };
    for (double boundary : {2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0})
        for (double omega : {0.2, 1.5, 2.8, 3.0})
        {
            const auto below = response(boundary - 1.0e-7, omega);
            const auto above = response(boundary + 1.0e-7, omega);
            if (std::abs(below - above) > 1.0e-4
                || std::abs(std::abs(below) - 1.0) > 1.0e-10)
            {
                std::fprintf(stderr, "FAIL short-delay phase boundary %.1f omega %.1f\n", boundary, omega);
                ++failures;
            }
        }
}
}

int main()
{
    stableShortDelays();
    for (int configuration = 0; configuration < 4; ++configuration)
        for (bool shortLines : {false, true})
            exercise(configuration, shortLines);
    if (failures == 0)
        std::puts("Acukoto.LoopPair: exact scalar/paired histories, independent delays and pending waves, loss, dispersion, mute, damping ramps, cursor wrap and reset");
    return failures == 0 ? 0 : 1;
}
