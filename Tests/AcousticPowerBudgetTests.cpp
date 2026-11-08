// Independent closed-form radiation tests. These test normalization,
// interference and half-space integration, not a copy of the sampled sum.
#include "../Tools/AcousticPowerBudget.h"

#include <algorithm>
#include <cstdio>

namespace
{
int failures = 0;
void near(double actual, double expected, double relative, const char* label)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > relative * std::max(std::abs(expected), 1.0e-12))
    {
        std::fprintf(stderr, "FAIL %s actual %.12g expected %.12g\n", label, actual, expected);
        ++failures;
    }
}
double sinc(double x) { return x == 0.0 ? 1.0 : std::sin(x) / x; }
}

int main()
{
    using acukoto::diagnostics::RadiationPatch;
    using namespace acoustic_power;
    constexpr double frequency = 300.0, q = 53.0;
    const double omega = 2.0 * pi * frequency, k = omega / soundSpeed;
    const RadiationPatch monopole {0.0, 0.0, 0.0, 0.0, 1.0, 1.0};
    const auto free = integrate(&monopole, 1, frequency, q, 32, false);
    const auto floor = integrate(&monopole, 1, frequency, q, 32, true);
    const double monopoleResistance = airDensity * omega * omega / (4.0 * pi * soundSpeed);
    near(free.angularIntegral, 4.0 * pi, 1.0e-12, "unit monopole solid angle");
    near(free.radiationResistance, monopoleResistance, 1.0e-12, "SI monopole acoustic resistance");
    near(free.mechanicalResistance, omega / q, 1.0e-12, "mass-normalized oscillator dissipation");
    near(free.radiationOverLoss, airDensity * omega * q / (4.0 * pi * soundSpeed), 1.0e-12,
         "RMS power ratio (no missing factor of two)");
    near(floor.radiationResistance, 2.0 * monopoleResistance, 1.0e-12,
         "floor doubles total monopole power, not quadruples it");

    constexpr double spacing = 0.3;
    RadiationPatch pair[] {{0.0, 0.0, -0.5 * spacing, 0.0, 1.0, 1.0},
                          {0.0, 0.0,  0.5 * spacing, 0.0, 1.0, 1.0}};
    auto together = integrate(pair, 2, frequency, q, 128, false);
    near(together.angularIntegral, 8.0 * pi * (1.0 + sinc(k * spacing)), 2.0e-5,
         "two coherent monopoles interfere");
    pair[1].displacement = -1.0;
    auto opposed = integrate(pair, 2, frequency, q, 128, false);
    near(opposed.angularIntegral, 8.0 * pi * (1.0 - sinc(k * spacing)), 5.0e-5,
         "dipole cancellation is not added as incoherent power");

    constexpr double height = 0.15;
    const RadiationPatch aboveFloor {0.0, 0.0, height, -height, 1.0, 1.0};
    const auto image = integrate(&aboveFloor, 1, frequency, q, 128, true);
    near(image.radiationResistance, monopoleResistance * (1.0 + sinc(2.0 * k * height)), 2.0e-5,
         "image separation and hemisphere power");
    auto scaled = aboveFloor;
    scaled.displacement *= 3.0;
    near(integrate(&scaled, 1, frequency, q, 128, true).radiationResistance,
         9.0 * image.radiationResistance, 1.0e-12, "power scales with squared modal amplitude");
    if (failures == 0)
        std::puts("Acoustic power: SI normalization, RMS convention, interference and floor passed");
    return failures == 0 ? 0 : 1;
}
