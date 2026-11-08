#pragma once

#include "DSP/AcukotoRadiationDiagnostics.h"

#include <cmath>
#include <cstddef>

namespace acoustic_power
{
constexpr double pi = 3.14159265358979323846;
constexpr double airDensity = 1.204; // kg/m^3, the engine's air at 20 C
constexpr double soundSpeed = 343.0; // m/s

// This is a far-field acoustic-power observer, not stored mechanical energy.
// For mass-normalized modal RMS velocity v:
//   P_loss = (omega / Q) v^2,
//   p_rms(n,r) = rho omega v |W(n)| / (4 pi r),
//   P_rad = rho omega^2 v^2 integral(|W|^2 dOmega) / (16 pi^2 c).
// W sums area * modal normal displacement * exp(-i k n.r) over the *same*
// surface sources as the engine. A rigid floor adds each same-sign image and
// the integral covers only the upper hemisphere. Free field covers a sphere.
// RMS and peak conventions give the same ratio: the latter's 1/2 cancels.
//
// A force/velocity work ledger uses signed F*v at each physical port, positive
// into the subsystem, and includes stored energy plus separately identified
// losses/external work. Neither stereo WAV RMS nor a sum of microphone taps
// is that ledger. A ratio above one here detects an inconsistent combination
// of the acoustic approximation and prescribed mechanical loss; it does not
// prove runaway, and is not permission to add damping to an empirical Q.
struct Power
{
    double angularIntegral;         // integral |W|^2 dOmega
    double radiationResistance;     // s^-1, P_rad / modal RMS velocity squared
    double mechanicalResistance;    // s^-1, P_loss / modal RMS velocity squared
    double radiationOverLoss;
};

inline Power integrate(const acukoto::diagnostics::RadiationPatch* patches,
                       std::size_t count, double frequency, double q,
                       int polarSteps, bool floor) noexcept
{
    const double omega = 2.0 * pi * frequency;
    const double k = omega / soundSpeed;
    const int azimuthSteps = 2 * polarSteps;
    double sum = 0.0;
    // Equal-area midpoint quadrature in cos(theta) and azimuth. Call at N and
    // 2N and report convergence; a resolved low mode does not validate highs.
    for (int iz = 0; iz < polarSteps; ++iz)
    {
        const double nz = floor ? (iz + 0.5) / polarSteps
                                : -1.0 + 2.0 * (iz + 0.5) / polarSteps;
        const double radius = std::sqrt(1.0 - nz * nz);
        for (int ia = 0; ia < azimuthSteps; ++ia)
        {
            const double phi = 2.0 * pi * (ia + 0.5) / azimuthSteps;
            const double nx = radius * std::cos(phi), ny = radius * std::sin(phi);
            double real = 0.0, imaginary = 0.0;
            for (std::size_t p = 0; p < count; ++p)
            {
                const auto& patch = patches[p];
                const double weight = patch.area * patch.displacement;
                if (weight == 0.0)
                    continue;
                if (floor)
                {
                    // Exact sum of the two exponentials, including the
                    // production floor's per-source clearance convention.
                    const double middle = 0.5 * (patch.z + patch.imageZ);
                    const double halfSeparation = 0.5 * (patch.z - patch.imageZ);
                    const double phase = k * (nx * patch.x + ny * patch.y + nz * middle);
                    const double paired = 2.0 * weight * std::cos(k * nz * halfSeparation);
                    real += paired * std::cos(phase);
                    imaginary += paired * std::sin(phase);
                }
                else
                {
                    const double phase = k * (nx * patch.x + ny * patch.y + nz * patch.z);
                    real += weight * std::cos(phase);
                    imaginary += weight * std::sin(phase);
                }
            }
            sum += real * real + imaginary * imaginary;
        }
    }
    const double integral = sum * (floor ? 2.0 : 4.0) * pi / (polarSteps * azimuthSteps);
    const double radiation = airDensity * omega * omega * integral / (16.0 * pi * pi * soundSpeed);
    const double mechanical = omega / q;
    return { integral, radiation, mechanical, radiation / mechanical };
}
} // namespace acoustic_power
