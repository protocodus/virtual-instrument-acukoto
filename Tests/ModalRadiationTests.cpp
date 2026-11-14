// AVX radiation fusion must retain the portable bank's exact output bits.
// Exercise cancellation, wide magnitude ranges, zero/padded banks and every
// common bank size, independently of a musical signal's level or audibility.
// The whole AVX modal step (its drive and pull blocks, with and without a
// hand's strike) and the junction's matrix products must match the portable
// ones bit for bit at every bank size, over steps whose states evolve. The
// fused (FMA3) step and products must meet them to rounding.
#include "EngineTestImplementation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

int main()
{
    {
        auto bank = std::make_unique<acukoto::ModeBank>();
        std::uint32_t state = 0x19831117U;
        const auto draw = [&state]()
        {
            state = state * 1664525U + 1013904223U;
            const double signedUnit = static_cast<double>(state >> 8) / 8388608.0 - 1.0;
            return std::ldexp(signedUnit, static_cast<int>((state >> 24) % 17) - 8);
        };
        int cases = 0;
        for (int count : { 0, 8, 16, 24, 56, 128, 352, 360, acukoto::ModeBank::capacity })
            for (int trial = 0; trial < 16; ++trial)
            {
                bank->count = count;
                for (int k = 0; k < acukoto::ModeBank::capacity; ++k)
                {
                    const auto i = static_cast<std::size_t>(k);
                    bank->y[i] = trial == 0 ? 0.0 : draw();
                    bank->left[i] = draw();
                    bank->right[i] = draw();
                    bank->leftQuadrature[i] = draw();
                    bank->rightQuadrature[i] = draw();
                    if (k >= count)
                        bank->y[i] = std::numeric_limits<double>::quiet_NaN();
                }
                double reference[4], actual[4];
                acukoto::weightedSumPair(bank->left.data(), bank->right.data(), bank->y.data(), count,
                                        reference[0], reference[1]);
                acukoto::weightedSumPair(bank->leftQuadrature.data(), bank->rightQuadrature.data(), bank->y.data(), count,
                                        reference[2], reference[3]);
                acukoto::radiationSums(*bank, actual[0], actual[1], actual[2], actual[3]);
                if (std::memcmp(reference, actual, sizeof reference) != 0)
                {
                    std::fprintf(stderr, "FAIL portable radiationSums: count %d, trial %d\n", count, trial);
                    return 1;
                }
                ++cases;
            }
        std::printf("portable modal radiation: %d banks match weightedSumPair bit for bit\n", cases);
    }

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(ACUKOTO_DISABLE_AVX2)
    if (!instrumentCpu::hasAvx2())
        return 0; // This dispatch is not used on a CPU without AVX2.
    auto bank = std::make_unique<acukoto::ModeBank>();
    std::uint32_t state = 0x19831117U;
    const auto draw = [&state]()
    {
        state = state * 1664525U + 1013904223U;
        const double signedUnit = static_cast<double>(state >> 8) / 8388608.0 - 1.0;
        return std::ldexp(signedUnit, static_cast<int>((state >> 24) % 17) - 8);
    };
    int cases = 0;
    for (int count : { 0, 8, 16, 24, 56, 128, 352, 360, 400, 408, 416, acukoto::ModeBank::capacity })
        for (int trial = 0; trial < 16; ++trial)
        {
            bank->count = count;
            for (int k = 0; k < acukoto::ModeBank::capacity; ++k)
            {
                const auto i = static_cast<std::size_t>(k);
                bank->y[i] = trial == 0 ? 0.0 : draw();
                bank->left[i] = draw();
                bank->right[i] = draw();
                bank->leftQuadrature[i] = draw();
                bank->rightQuadrature[i] = draw();
                if (k >= count)
                    bank->y[i] = std::numeric_limits<double>::quiet_NaN();
            }
            double reference[4], actual[4];
            acukoto::weightedSumPair(bank->left.data(), bank->right.data(), bank->y.data(), count,
                                    reference[0], reference[1]);
            acukoto::weightedSumPair(bank->leftQuadrature.data(), bank->rightQuadrature.data(), bank->y.data(), count,
                                    reference[2], reference[3]);
            acukoto::radiationSumsAvx(*bank, actual[0], actual[1], actual[2], actual[3]);
            if (std::memcmp(reference, actual, sizeof reference) != 0)
            {
                std::fprintf(stderr, "FAIL modal radiation: count %d, trial %d\n", count, trial);
                return 1;
            }
            ++cases;
        }
    std::printf("modal radiation: %d banks match portable lane reductions bit for bit\n", cases);

    auto portable = std::make_unique<acukoto::ModeBank>();
    std::array<float, acukoto::ModeBank::capacity> shape {};
    int steps = 0;
    for (int count = acukoto::ModeBank::lanes; count <= acukoto::ModeBank::capacity; count += acukoto::ModeBank::lanes)
        for (int struck = 0; struck < 2; ++struck)
        {
            *bank = {};
            bank->count = count;
            for (int k = 0; k < count; ++k)
            {
                const auto i = static_cast<std::size_t>(k);
                bank->b0[i] = draw();
                bank->a1[i] = draw();
                bank->a2[i] = draw();
                bank->s1[i] = draw();
                bank->s2[i] = draw();
                bank->left[i] = draw();
                bank->right[i] = draw();
                bank->leftQuadrature[i] = draw();
                bank->rightQuadrature[i] = draw();
                bank->pullState[i] = static_cast<float>(draw());
                shape[i] = static_cast<float>(draw());
                for (auto& row : bank->participation)
                    row[i] = static_cast<float>(draw());
            }
            *portable = *bank;
            for (int step = 0; step < 3; ++step, ++steps)
            {
                acukoto::BarVector force {};
                for (double& f : force)
                    f = draw();
                acukoto::StrikeDrive hitA {}, hitB {};
                if (struck != 0)
                {
                    hitA.shape = hitB.shape = shape.data();
                    hitA.force = hitB.force = draw();
                }
                double a[4], b[4];
                acukoto::BarVector pullA {}, pullB {};
                acukoto::runModes(*portable, force, hitA, a[0], a[1], a[2], a[3], pullA);
                acukoto::runModesAvx(*bank, force, hitB, b[0], b[1], b[2], b[3], pullB);
                if (std::memcmp(a, b, sizeof a) != 0 || std::memcmp(&pullA, &pullB, sizeof pullA) != 0
                    || hitA.velocity != hitB.velocity
                    || portable->s1 != bank->s1 || portable->s2 != bank->s2 || portable->y != bank->y
                    || portable->pullState != bank->pullState || portable->drive != bank->drive)
                {
                    std::fprintf(stderr, "FAIL modal step: count %d, struck %d, step %d\n", count, struck, step);
                    return 1;
                }
            }
        }
    std::printf("modal step: %d steps match the portable bank bit for bit\n", steps);

    int products = 0;
    for (int trial = 0; trial < 64; ++trial, ++products)
    {
        acukoto::BarMatrix matrix;
        acukoto::BarVector v {};
        for (auto& row : matrix.m)
            for (double& x : row)
                x = draw();
        for (double& x : v)
            x = trial == 0 ? 0.0 : draw();
        const acukoto::BarVector reference = matrix.transposeApply(v);
        const acukoto::BarVector actual = acukoto::transposeApplyAvx(matrix, v);
        if (std::memcmp(&reference, &actual, sizeof reference) != 0)
        {
            std::fprintf(stderr, "FAIL junction product: trial %d\n", trial);
            return 1;
        }
    }
    std::printf("junction product: %d products match the portable sum bit for bit\n", products);

    // The fused kernels round once per term, so they meet the portable bank
    // to rounding. Each output is held within 1e-4 of its vector's largest
    // magnitude: these random sums of up to 424 terms cancel, so their
    // rounding is relative to the terms, not to each result, and one missing
    // or doubled term (about a twentieth of the largest result) still fails.
    if (!instrumentCpu::hasAvx2Fma())
        return 0;
    const auto near = [](const double* reference, const double* actual, std::size_t n, double tolerance)
    {
        double scale = 1.0;
        for (std::size_t i = 0; i < n; ++i)
            scale = std::max(scale, std::abs(reference[i]));
        for (std::size_t i = 0; i < n; ++i)
            if (!(std::abs(actual[i] - reference[i]) <= tolerance * scale))
                return false;
        return true;
    };
    const auto nearFloats = [&near](const float* reference, const float* actual, std::size_t n)
    {
        std::array<double, acukoto::ModeBank::capacity> r {}, a {};
        for (std::size_t i = 0; i < n; ++i)
        {
            r[i] = reference[i];
            a[i] = actual[i];
        }
        return near(r.data(), a.data(), n, 1.0e-4);
    };
    int fusedSteps = 0;
    for (int count = acukoto::ModeBank::lanes; count <= acukoto::ModeBank::capacity; count += acukoto::ModeBank::lanes)
        for (int struck = 0; struck < 2; ++struck)
        {
            *bank = {};
            bank->count = count;
            for (int k = 0; k < count; ++k)
            {
                const auto i = static_cast<std::size_t>(k);
                bank->b0[i] = draw();
                bank->a1[i] = draw();
                bank->a2[i] = draw();
                bank->s1[i] = draw();
                bank->s2[i] = draw();
                bank->left[i] = draw();
                bank->right[i] = draw();
                bank->leftQuadrature[i] = draw();
                bank->rightQuadrature[i] = draw();
                bank->pullState[i] = static_cast<float>(draw());
                shape[i] = static_cast<float>(draw());
                for (auto& row : bank->participation)
                    row[i] = static_cast<float>(draw());
            }
            *portable = *bank;
            for (int step = 0; step < 3; ++step, ++fusedSteps)
            {
                acukoto::BarVector force {};
                for (double& f : force)
                    f = draw();
                acukoto::StrikeDrive hitA {}, hitB {};
                if (struck != 0)
                {
                    hitA.shape = hitB.shape = shape.data();
                    hitA.force = hitB.force = draw();
                }
                double a[4], b[4];
                acukoto::BarVector pullA {}, pullB {};
                acukoto::runModes(*portable, force, hitA, a[0], a[1], a[2], a[3], pullA);
                acukoto::runModesFma(*bank, force, hitB, b[0], b[1], b[2], b[3], pullB);
                const auto n = static_cast<std::size_t>(count);
                double strikeScale = 1.0;
                for (std::size_t k = 0; k < n; ++k)
                    strikeScale += std::abs(static_cast<double>(shape[k]) * portable->y[k]);
                const bool ok = std::abs(hitB.velocity - hitA.velocity) <= 1.0e-4 * strikeScale
                    && near(a, b, 4, 1.0e-4) && near(pullA.data(), pullB.data(), pullA.size(), 1.0e-4)
                    && near(portable->s1.data(), bank->s1.data(), n, 1.0e-4)
                    && near(portable->s2.data(), bank->s2.data(), n, 1.0e-4)
                    && near(portable->y.data(), bank->y.data(), n, 1.0e-4)
                    && nearFloats(portable->drive.data(), bank->drive.data(), n);
                if (!ok)
                {
                    std::fprintf(stderr, "FAIL fused modal step: count %d, struck %d, step %d\n", count, struck, step);
                    return 1;
                }
                // Each step starts from the portable bank's state, so a
                // rounding difference is measured once, never compounded.
                *bank = *portable;
            }
        }
    std::printf("fused modal step: %d steps meet the portable bank to rounding\n", fusedSteps);

    for (int trial = 0; trial < 64; ++trial)
    {
        acukoto::BarMatrix matrix;
        acukoto::BarVector v {};
        for (auto& row : matrix.m)
            for (double& x : row)
                x = draw();
        for (double& x : v)
            x = trial == 0 ? 0.0 : draw();
        const acukoto::BarVector reference = matrix.transposeApply(v);
        const acukoto::BarVector actual = acukoto::transposeApplyFma(matrix, v);
        if (!near(reference.data(), actual.data(), reference.size(), 1.0e-12))
        {
            std::fprintf(stderr, "FAIL fused junction product: trial %d\n", trial);
            return 1;
        }
    }
    std::printf("fused junction product: 64 products meet the portable sum to rounding\n");
#endif
    return 0;
}
