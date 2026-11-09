#include "DSP/PluckShape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr double pi = 3.14159265358979323846;
int failures = 0;
void check(bool okay, const char* message, double value)
{
    if (!okay)
    {
        std::fprintf(stderr, "FAIL %s: %.12g\n", message, value);
        ++failures;
    }
}
}

int main()
{
    constexpr int samples = 8192;
    double worstOdd = 0.0, worstForce = 0.0;
    for (double length : { 0.044, 0.211, 0.631, 1.264 })
        for (double point : { 0.01, 0.2, 0.49, 0.97 })
            for (double width : { 0.0, 0.002, 0.008 })
            {
                const acukoto::detail::PluckShape ordinary(length, point * length, width);
                const acukoto::detail::PluckShape harmonic(length, point * length, width, true);
                check(ordinary(0.0) == 0.0 && ordinary(length) == 0.0,
                      "a held pluck moves a fixed end", length);
                check(harmonic(0.0) == 0.0 && harmonic(length) == 0.0,
                      "a harmonic moves a fixed end", length);
                const double h = length * 1.0e-6;
                // Static transverse force balance: the endpoint reactions
                // T y'(0) - T y'(L) must sum to the applied unit F/T.
                const double force = (ordinary(h) + ordinary(length - h)) / h;
                worstForce = std::max(worstForce, std::abs(force - 1.0));
                check(std::abs(force - 1.0) < 0.001, "contact loses its applied force outside the string", force);
                std::array<double, 8> original {}, projected {};
                double originalEnergy = 0.0, harmonicEnergy = 0.0;
                double previous = 0.0, previousHarmonic = 0.0;
                for (int k = 1; k <= samples; ++k)
                {
                    const double x = length * k / samples;
                    const double a = ordinary(x), b = harmonic(x);
                    check(a >= -1.0e-12 && std::isfinite(a) && std::isfinite(b), "invalid held shape", a);
                    originalEnergy += (a - previous) * (a - previous);
                    harmonicEnergy += (b - previousHarmonic) * (b - previousHarmonic);
                    previous = a;
                    previousHarmonic = b;
                    for (int n = 1; n <= 8; ++n)
                    {
                        const double mode = std::sin(n * pi * x / length) * 2.0 / samples;
                        original[static_cast<std::size_t>(n - 1)] += a * mode;
                        projected[static_cast<std::size_t>(n - 1)] += b * mode;
                    }
                }
                check(harmonicEnergy <= originalEnergy * (1.0 + 1.0e-12),
                      "midpoint touch adds string deformation energy", harmonicEnergy / originalEnergy);
                for (int n = 1; n <= 8; ++n)
                {
                    const auto i = static_cast<std::size_t>(n - 1);
                    if ((n & 1) != 0)
                    {
                        worstOdd = std::max(worstOdd, std::abs(projected[i]) / length);
                        check(std::abs(projected[i]) < length * 1.0e-12,
                              "midpoint touch keeps an odd displacement mode", projected[i]);
                    }
                    else
                        check(std::abs(projected[i] - original[i]) < length * 1.0e-12,
                              "midpoint touch changes an even displacement mode", projected[i] - original[i]);
                    if (width == 0.0)
                    {
                        const double expected = 2.0 * length * std::sin(n * pi * point) / (n * n * pi * pi);
                        check(std::abs(original[i] - expected) < length * 1.0e-7,
                              "point pluck differs from the fixed-string Green function", original[i] - expected);
                    }
                }
            }
    // Far from the contact the shape takes G's asymptote instead of erfc and
    // exp (PluckShape::primitive). The full expression, written out here,
    // must give the same number to the bit at every cell boundary a pluck
    // asks for, straight and reflected, on both sides of the contact.
    const auto full = [] (double length, double point, double width, double x)
    {
        const auto cdf = [] (double u) { return 0.5 * std::erfc(-u / std::sqrt(2.0)); };
        const auto primitive = [&] (double at)
        {
            const double u = (at - point) / width;
            return width * (u * cdf(u) + std::exp(-0.5 * u * u) / std::sqrt(2.0 * pi));
        };
        const double atLeft = primitive(0.0), atRight = primitive(length);
        const double share = cdf((length - point) / width) - cdf(-point / width);
        return x <= 0.0 || x >= length ? 0.0 : (atLeft + x * (atRight - atLeft) / length - primitive(x)) / share;
    };
    int farCells = 0;
    for (double length : { 0.12, 0.3, 0.55, 0.9, 1.4 })
        for (double point : { 0.012, 0.03, 0.045, 0.08, 0.2 })
            for (double width : { 0.0005, 0.0015, 0.003, 0.006, 0.012 })
            {
                if (point >= length)
                    continue;
                const acukoto::detail::PluckShape shape(length, point, width);
                for (double period : { 23.37, 48.001, 101.999, 326.53, 1306.12 })
                {
                    const double cell = 2.0 * length / period;
                    for (int k = 0; k <= static_cast<int>(period); ++k)
                        for (double x : { cell * k, 2.0 * length - cell * k })
                        {
                            check(shape(x) == full(length, point, width, x),
                                  "the far-field shape differs from the full expression", x);
                            farCells += std::abs(x - point) >= 8.5 * width ? 1 : 0;
                        }
                }
            }
    check(farCells > 100000, "the far field was not reached", farCells);
    std::printf("pluck shape: 48 geometries, endpoint force error <= %.4g, odd harmonic mode/L <= %.4g\n",
                worstForce, worstOdd);
    return failures == 0 ? 0 : 1;
}
