// AVX radiation fusion must retain the portable bank's exact output bits.
// Exercise cancellation, wide magnitude ranges, zero/padded banks and every
// common bank size, independently of a musical signal's level or audibility.
#include "EngineTestImplementation.h"

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
#endif
    return 0;
}
