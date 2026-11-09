// AVX radiation fusion must retain the portable bank's exact output bits.
// Exercise cancellation, wide magnitude ranges, zero/padded banks and every
// common bank size, independently of a musical signal's level or audibility.
// The whole AVX modal step (its drive and pull blocks, with and without a
// hand's strike) and the junction's matrix products must match the portable
// ones bit for bit at every bank size, over steps whose states evolve.
#include "EngineTestImplementation.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

int main()
{
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
#endif
    return 0;
}
