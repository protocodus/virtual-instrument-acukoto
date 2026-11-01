#include "AcukotoEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <utility>

namespace acukoto
{
namespace
{

constexpr double pi = 3.14159265358979323846;
using Complex = std::complex<double>;

// Air at 20 C.
constexpr double airDensity = 1.204;      // kg/m^3
constexpr double airViscosity = 1.81e-5;  // Pa s
constexpr double soundSpeed = 343.0;      // m/s
// The microphones: an ORTF pair, two cardioids 17 cm apart with 110 degrees
// between them, the French broadcasters' near-coincident standard. It stands
// 1.2 m from the middle of the top, in front of the koto and 40 degrees above
// it, aimed there, the head on the left as a listener facing the player hears
// it. Authored, like the room. Set beside the owner's Sakura reference by the
// same notes (2026-10-29), its left-right correlation by octave band over
// each note's first 25 ms, 0.92, 0.59 and 0.26 at 250, 500 and 1000 Hz,
// falls as the recording's does (0.84, 0.63, 0.30). The two omnidirectional
// microphones it replaced, 70 cm apart and 0.5 m over the top, gave 0.02,
// -0.20 and -0.41, and heard the walls' local modes so closely that the
// fundamentals sat 3.5 dB further under the second partials than in five
// professional recordings; from here they sat 0.5 dB under, with 0.2.0's
// body (Tools/measure_koto_notes.py --compare).
constexpr double pairDistance = 1.2;
constexpr double pairElevation = 40.0 * pi / 180.0;
constexpr double pairSpacing = 0.17;
constexpr double pairAngle = 55.0 * pi / 180.0;

// Loss at the yoke end (tuning pin and crossbar), per round trip. Authored:
// no lyre measurement separates it, and it only matters where the air and
// the material leave a string almost lossless.
constexpr double yokeRoundTripGain = 0.9995;

// No hand holds a force still. Young adults' steady finger contractions at
// moderate force fluctuate by a coefficient of variation of about 1-2%
// (Galganski, Fuglevand and Enoka, J. Neurophysiol. 69 (1993) 2108), most of
// it slow, with the physiological tremor at 6-12 Hz above (Kutch et al., J.
// Neurophysiol. 100 (2008) 2455). Taken here as 1.5%, low-passed at an
// estimated 5 Hz. Pressing the string down by d behind the ji takes a force
// near T d (1/a + 1/b) and adds tension as d^2, so the added tension
// fluctuates by twice the force's share: a whole-tone press wavers by about
// 7 cents RMS, a semitone's by 3.
constexpr double pressForceSpread = 0.015;
constexpr double pressWaverHz = 5.0;
// The left hand's glide onto a press after the pluck (ato-oshi) and off it,
// over about 80 ms.
constexpr double pressGlide = 0.08;
// A key a semitone or a whole tone above a string presses it 3% past that
// interval, 103 or 206 cents: the owner's choice (Docs/decisions.md,
// 2026-10-31), toward the five professional recordings, whose pressed notes
// sit 11-18 cents sharp (README, Known gaps). Pressed by the interval alone,
// E4 and F4 took E-flat 4's tuning, 13 cents flat.
constexpr double pressReach = 1.03;

constexpr int delayCapacity = 8192;  // a string down to A0 (27.5 Hz) at 192 kHz
// The keys of a piano, A0 to C8: every one sounds at its own pitch
// (AcukotoEngine::noteOn, State::extension).
constexpr int lowestKey = 21;
constexpr int highestKey = 108;
constexpr int controlPeriod = 16;
constexpr int maxDispersionStages = 8;
constexpr int dispersionPairs = 4;

// ---------------------------------------------------------------------------
// String loss laws, copied from Acustra 1.9.0 (Source/DSP/AcustraEngine.cpp),
// where their derivations and references are given in full: the bending loss
// of Valette's and Woodhouse's string damping models, 1/Q_n = eta B n^2 /
// (1 + B n^2), as a unit-DC two-pole section, and a loss angle constant over
// the band (Paté, Le Carrou and Fabre, JASA 135 (2014) 3045) as a unit-DC
// two-pole two-zero section of two relaxations. Acukoto's change: that
// section also carries the measured air damping's rise over the
// fundamental's (airRoot, see setInstrument).

double stretchedPartial(double partial, double inharmonicity) noexcept
{
    return partial * std::sqrt((1.0 + inharmonicity * partial * partial)
                               / (1.0 + inharmonicity));
}

double bendingLossRate(double partial, double inharmonicity, double factor,
                       double fundamental) noexcept
{
    const double bending = inharmonicity * partial * partial;
    return pi * fundamental * stretchedPartial(partial, inharmonicity)
         * factor * bending / (1.0 + bending);
}

std::complex<double> insideRoot(std::complex<double> u) noexcept
{
    const std::complex<double> sum = 2.0 - u;
    const std::complex<double> root = std::sqrt(sum * sum - 4.0);
    const std::complex<double> first = 0.5 * (sum + root);
    const std::complex<double> second = 0.5 * (sum - root);
    return std::abs(first) < std::abs(second) ? first : second;
}

// H(z) = (gain + n1 z^-1 + n2 z^-2) / (1 + a1 z^-1 + a2 z^-2), unit gain at DC.
struct LossSectionDesign
{
    double gain { 1.0 };
    double a1 { 0.0 };
    double a2 { 0.0 };
    double n1 { 0.0 };
    double n2 { 0.0 };
};

LossSectionDesign bendingLossSection(double factor, double inharmonicity,
                                     double fundamental, double sampleRate) noexcept
{
    LossSectionDesign section;
    if (!(factor > 0.0) || !(inharmonicity > 0.0) || !(fundamental > 0.0)
        || !(sampleRate > 0.0))
        return section;
    constexpr double collocationLimit = 0.3;
    double below = 1.0;
    double above = collocationLimit * sampleRate / fundamental;
    if (!(above > 1.0))
        return section;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double middle = 0.5 * (below + above);
        if (stretchedPartial(middle, inharmonicity) * fundamental
            < collocationLimit * sampleRate)
            below = middle;
        else
            above = middle;
    }
    const double highest = below;
    const auto partialAt = [&] (double rate)
    {
        double lower = 1.0;
        double upper = highest;
        if (bendingLossRate(upper, inharmonicity, factor, fundamental) <= rate)
            return upper;
        for (int iteration = 0; iteration < 48; ++iteration)
        {
            const double middle = 0.5 * (lower + upper);
            if (bendingLossRate(middle, inharmonicity, factor, fundamental) < rate)
                lower = middle;
            else
                upper = middle;
        }
        return 0.5 * (lower + upper);
    };
    constexpr double decibelsPerNeper = 8.685889638065035;
    const double upperPartial = partialAt(160.0 / decibelsPerNeper);
    const double lowerPartial = std::min(partialAt(20.0 / decibelsPerNeper),
                                         0.5 * upperPartial);
    if (!(lowerPartial >= 1.0) || !(upperPartial > lowerPartial))
        return section;
    double target[2] {};
    double s[2] {};
    const double partials[2] { lowerPartial, upperPartial };
    for (int index = 0; index < 2; ++index)
    {
        const double loss = bendingLossRate(partials[index], inharmonicity,
                                            factor, fundamental) / fundamental;
        target[index] = std::expm1(2.0 * loss);
        const double halfOmega = pi * fundamental
            * stretchedPartial(partials[index], inharmonicity) / sampleRate;
        s[index] = std::sin(halfOmega) * std::sin(halfOmega);
    }
    const double determinant = 64.0 * s[0] * s[1] * (s[1] - s[0]);
    if (!(std::abs(determinant) > 1.0e-300))
        return section;
    double p = 16.0 * (target[0] * s[1] * s[1] - target[1] * s[0] * s[0]) / determinant;
    double q = 4.0 * (s[0] * target[1] - s[1] * target[0]) / determinant;
    if (q < 0.0)
    {
        q = 0.0;
        p = target[1] / (4.0 * s[1]);
    }
    else if (p < 0.0)
    {
        p = 0.0;
        q = target[1] / (16.0 * s[1] * s[1]);
    }
    if (!(p > 0.0 || q > 0.0) || !std::isfinite(p) || !std::isfinite(q))
        return section;
    if (q > 0.0)
    {
        const std::complex<double> discriminant
            = std::sqrt(std::complex<double>(p * p - 4.0 * q, 0.0));
        const std::complex<double> first = insideRoot((-p + discriminant) / (2.0 * q));
        const std::complex<double> second = insideRoot((-p - discriminant) / (2.0 * q));
        section.a1 = -(first + second).real();
        section.a2 = (first * second).real();
    }
    else
    {
        section.a1 = -insideRoot(std::complex<double>(-1.0 / p, 0.0)).real();
        section.a2 = 0.0;
    }
    section.gain = 1.0 + section.a1 + section.a2;
    if (!std::isfinite(section.gain) || !(section.gain > 0.0)
        || !(std::abs(section.a2) < 1.0)
        || !(std::abs(section.a1) < 1.0 + section.a2))
        return {};
    return section;
}

LossSectionDesign constantLossSection(double delta, double airRoot, double inharmonicity,
                                      double fundamental, double sampleRate) noexcept
{
    LossSectionDesign section;
    if (!(delta > 0.0 || airRoot > 0.0) || !(inharmonicity >= 0.0) || !(fundamental > 0.0)
        || !(sampleRate > 0.0))
        return section;
    constexpr double decibelsPerNeper = 8.685889638065035;
    const double topHz = std::min(0.45 * sampleRate, 20000.0);
    double below = 1.0;
    double above = topHz / fundamental + 1.0;
    if (!(stretchedPartial(2.0, inharmonicity) * fundamental < topHz))
        return section;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double middle = 0.5 * (below + above);
        if (stretchedPartial(middle, inharmonicity) * fundamental < topHz)
            below = middle;
        else
            above = middle;
    }
    const double top = below;
    const auto rateAt = [&] (double partial)
    {
        const double frequency = fundamental * stretchedPartial(partial, inharmonicity);
        return pi * frequency * delta + airRoot * (std::sqrt(frequency) - std::sqrt(fundamental));
    };
    const auto partialAt = [&] (double rate)
    {
        double lower = 1.0;
        double upper = top;
        if (rateAt(upper) <= rate)
            return upper;
        if (rateAt(lower) >= rate)
            return lower;
        for (int iteration = 0; iteration < 48; ++iteration)
        {
            const double middle = 0.5 * (lower + upper);
            if (rateAt(middle) < rate)
                lower = middle;
            else
                upper = middle;
        }
        return 0.5 * (lower + upper);
    };
    double lowest = std::max(2.0, partialAt(3.0 / decibelsPerNeper));
    if (!(lowest < 0.8 * top))
        lowest = std::max(2.0, std::min(lowest, 0.25 * top));
    double highest = std::min(top, partialAt(300.0 / decibelsPerNeper));
    highest = std::max(highest, std::min(top, 1.5 * lowest));
    if (!(highest > lowest))
        return section;
    constexpr int bandCount = 12;
    constexpr int tailCount = 4;
    const bool tail = top > 1.05 * highest;
    const int count = bandCount + (tail ? tailCount : 0);
    double u[bandCount + tailCount] {};
    double target[bandCount + tailCount] {};
    double weight[bandCount + tailCount] {};
    double loss[bandCount + tailCount] {};
    for (int index = 0; index < count; ++index)
    {
        const bool inBand = index < bandCount;
        const double partial = inBand
            ? lowest * std::pow(highest / lowest,
                                static_cast<double>(index) / (bandCount - 1))
            : highest * std::pow(top / highest,
                                 static_cast<double>(index - bandCount + 1) / tailCount);
        const double half = pi * fundamental
            * stretchedPartial(partial, inharmonicity) / sampleRate;
        u[index] = 4.0 * std::sin(half) * std::sin(half);
        loss[index] = rateAt(partial) / fundamental;
        target[index] = std::expm1(2.0 * loss[index]);
        weight[index] = (inBand ? 1.0 : 0.1) / target[index];
    }
    const double first = u[0];
    const double last = u[bandCount - 1];
    if (!(first > 0.0) || !(last > first))
        return section;
    constexpr double lowerCorners[] { -0.25, 0.10, 0.15, 0.20 };
    constexpr double upperCorners[] { 0.90, 0.95, 1.00, 1.05, 1.50 };
    double bestError = std::numeric_limits<double>::infinity();
    double bestU1 = 0.0, bestU2 = 0.0, bestA = 0.0, bestB = 0.0;
    for (const double lowerCorner : lowerCorners)
        for (const double upperCorner : upperCorners)
        {
            const double u1 = first * std::pow(last / first, lowerCorner);
            const double u2 = first * std::pow(last / first, upperCorner);
            double s11 = 0.0, s12 = 0.0, s22 = 0.0, r1 = 0.0, r2 = 0.0;
            for (int index = 0; index < count; ++index)
            {
                const double denominator = (1.0 + u[index] / u1) * (1.0 + u[index] / u2);
                const double phi1 = weight[index] * u[index] / denominator;
                const double phi2 = phi1 * u[index];
                const double y = weight[index] * target[index];
                s11 += phi1 * phi1;
                s12 += phi1 * phi2;
                s22 += phi2 * phi2;
                r1 += phi1 * y;
                r2 += phi2 * y;
            }
            const double determinant = s11 * s22 - s12 * s12;
            double a = 0.0, b = 0.0;
            if (determinant > 1.0e-12 * s11 * s22)
            {
                a = (r1 * s22 - r2 * s12) / determinant;
                b = (r2 * s11 - r1 * s12) / determinant;
            }
            if (!(a >= 0.0) || !(a + 4.0 * b >= 0.0))
            {
                a = s11 > 0.0 ? r1 / s11 : 0.0;
                b = 0.0;
            }
            if (!(a > 0.0) || !std::isfinite(a) || !std::isfinite(b))
                continue;
            double error = 0.0;
            for (int index = 0; index < bandCount; ++index)
            {
                const double denominator = (1.0 + u[index] / u1) * (1.0 + u[index] / u2);
                const double fitted = 0.5 * std::log1p(
                    (a * u[index] + b * u[index] * u[index]) / denominator);
                error = std::max(error, std::abs(fitted / loss[index] - 1.0));
            }
            if (error < bestError)
            {
                bestError = error;
                bestU1 = u1;
                bestU2 = u2;
                bestA = a;
                bestB = b;
            }
        }
    if (!(bestError < std::numeric_limits<double>::infinity()))
        return section;
    const double zero1 = insideRoot(std::complex<double>(-bestU1, 0.0)).real();
    const double zero2 = insideRoot(std::complex<double>(-bestU2, 0.0)).real();
    const double b1 = -(zero1 + zero2);
    const double b2 = zero1 * zero2;
    const double p = 1.0 / bestU1 + 1.0 / bestU2 + bestA;
    const double q = 1.0 / (bestU1 * bestU2) + bestB;
    double a1 = 0.0, a2 = 0.0;
    if (q != 0.0)
    {
        const std::complex<double> discriminant
            = std::sqrt(std::complex<double>(p * p - 4.0 * q, 0.0));
        const std::complex<double> pole1 = insideRoot((-p + discriminant) / (2.0 * q));
        const std::complex<double> pole2 = insideRoot((-p - discriminant) / (2.0 * q));
        a1 = -(pole1 + pole2).real();
        a2 = (pole1 * pole2).real();
    }
    else
    {
        a1 = -insideRoot(std::complex<double>(-1.0 / p, 0.0)).real();
    }
    const double gain = (1.0 + a1 + a2) / (1.0 + b1 + b2);
    if (!std::isfinite(a1) || !std::isfinite(a2) || !std::isfinite(gain)
        || !(gain > 0.0) || !(std::abs(a2) < 1.0) || !(std::abs(a1) < 1.0 + a2)
        || !(std::abs(b2) < 1.0) || !(std::abs(b1) < 1.0 + b2))
        return section;
    section.gain = gain;
    section.a1 = a1;
    section.a2 = a2;
    section.n1 = gain * b1;
    section.n2 = gain * b2;
    return section;
}

// Second-order Thiran allpass for a delay of `samples` (Acustra 1.9.0).
void thiranCoefficients(double samples, double& a1, double& a2) noexcept
{
    a1 = -2.0 * (samples - 2.0) / (samples + 1.0);
    a2 = (samples - 2.0) * (samples - 1.0) / ((samples + 1.0) * (samples + 2.0));
}
// End of the code copied from Acustra.
// ---------------------------------------------------------------------------

// Phase lag of an allpass z^-N A(1/z)/A(z) is N omega + 2 arg A(e^{j omega});
// A's roots lie inside the circle, so arg A never wraps.
double allpassLag(int order, double c1, double c2, double omega) noexcept
{
    const Complex a = 1.0 + c1 * std::exp(Complex(0.0, -omega))
                    + c2 * std::exp(Complex(0.0, -2.0 * omega));
    return order * omega + 2.0 * std::arg(a);
}

// Dispersion as `stages` identical first-order allpass sections (Rauhala and
// Valimaki's tunable design, here searched rather than read off their
// formula): the stage count and coefficient whose worst partial up to the
// 12th, and below 5 kHz, lands nearest in cents to the stiff-string law, with
// the rest of the loop a pure delay tuned to f0. Acustra fits the same 12
// partials. One section fitted to a single high partial left a steel G3's 2nd
// to 5th partials 30 to 120 cents sharp. No cascade of identical sections
// fits a bass string's 25 partials below 5 kHz either: the steel G3's 13th to
// 21st fell 10 to 95 cents flat. Where partials past the 12th fall below
// 5 kHz (strings under about 417 Hz), two to four second-order sections are
// fitted with the first-order coefficient to every one of them
// (refineDispersion).
struct DispersionDesign
{
    int stages { 1 };
    double coefficient { 0.0 };
    double worstCents { 0.0 };
    // Second-order sections (c2 + c1 z^-1 + z^-2) / (1 + c1 z^-1 + c2 z^-2),
    // as { c1, c2 }; `pairs` of them are in use.
    int pairs { 0 };
    std::array<std::array<double, 2>, dispersionPairs> pair {};
};

// A first-order section's phase lag and group delay, (a + z^-1)/(1 + a z^-1).
double firstOrderLag(double a, double omega) noexcept
{
    return omega - 2.0 * std::atan2(a * std::sin(omega), 1.0 + a * std::cos(omega));
}

double firstOrderGroupDelay(double a, double omega) noexcept
{
    return (1.0 - a * a) / (1.0 + 2.0 * a * std::cos(omega) + a * a);
}

// The fit's parameters: the first-order coefficient, then each second-order
// section's pole radius and angle.
using DispersionFit = std::array<double, 1 + 2 * dispersionPairs>;
constexpr int maxFittedPartials = 64;

// Each partial's error in cents against the stiff-string law, partials 2 to
// `partials`, with the line a pure delay tuned to f0; false where the line
// would be under the 8 samples the Thiran section and the loss filters need.
bool dispersionErrors(const DispersionFit& fit, int stages, int pairs, double omega1, double inharmonicity,
                      int partials, std::array<double, maxFittedPartials>& cents) noexcept
{
    const auto lag = [&] (double omega)
    {
        double total = stages * firstOrderLag(fit[0], omega);
        for (int k = 0; k < pairs; ++k)
        {
            const Complex pole = std::polar(fit[static_cast<std::size_t>(1 + 2 * k)],
                                            fit[static_cast<std::size_t>(2 + 2 * k)]);
            const Complex z1 = std::exp(Complex(0.0, -omega));
            total += 2.0 * omega + 2.0 * (std::arg(1.0 - pole * z1) + std::arg(1.0 - std::conj(pole) * z1));
        }
        return total;
    };
    const auto groupDelay = [&] (double omega)
    {
        double total = stages * firstOrderGroupDelay(fit[0], omega);
        for (int k = 0; k < pairs; ++k)
        {
            const double r = fit[static_cast<std::size_t>(1 + 2 * k)];
            const double theta = fit[static_cast<std::size_t>(2 + 2 * k)];
            total += (1.0 - r * r) / (1.0 - 2.0 * r * std::cos(omega - theta) + r * r)
                   + (1.0 - r * r) / (1.0 - 2.0 * r * std::cos(omega + theta) + r * r);
        }
        return total;
    };
    const double delay = (2.0 * pi - lag(omega1)) / omega1;
    if (!(delay >= 8.0))
        return false;
    for (int n = 2; n <= partials; ++n)
    {
        const double target = omega1 * stretchedPartial(n, inharmonicity);
        double omega = target;
        for (int iteration = 0; iteration < 4; ++iteration)
            omega -= (omega * delay + lag(omega) - 2.0 * pi * n) / (delay + groupDelay(omega));
        cents[static_cast<std::size_t>(n - 2)] = 1200.0 * std::log2(omega / target);
    }
    return true;
}

double worstOf(const std::array<double, maxFittedPartials>& cents, int partials) noexcept
{
    double worst = 0.0;
    for (int n = 2; n <= partials; ++n)
        worst = std::max(worst, std::abs(cents[static_cast<std::size_t>(n - 2)]));
    return worst;
}

// Levenberg-Marquardt on the partials' cents over the first-order
// coefficient and `pairs` second-order sections, the Jacobian by forward
// differences, from `fit`, which it leaves where the fit settles and `cents`
// at that fit's errors; false where `fit` is out of bounds to begin with.
// The first-order coefficient stays negative, as designDispersion's search
// keeps it: a positive one delays the upper partials more, flattening them
// where a stiff string's run sharp. Left free, the gut D4's fit ran it to
// +0.98 at 88.2 and 96 kHz and stalled 6.8 cents out, so the string kept its
// first-order design: 2.4 cents out below 5 kHz and its 24th partial 12
// cents flat, against 0.6 at 48 kHz.
bool fitSections(DispersionFit& fit, int pairs, int stages, double omega1, double inharmonicity,
                 int partials, std::array<double, maxFittedPartials>& cents) noexcept
{
    const auto valid = [pairs] (const DispersionFit& p)
    {
        bool ok = p[0] <= 0.0 && p[0] > -0.98;
        for (int k = 0; k < pairs; ++k)
            ok = ok && p[static_cast<std::size_t>(1 + 2 * k)] >= 0.0 && p[static_cast<std::size_t>(1 + 2 * k)] < 0.99
                 && p[static_cast<std::size_t>(2 + 2 * k)] > 0.0 && p[static_cast<std::size_t>(2 + 2 * k)] < pi;
        return ok;
    };
    const auto cost = [&] (const DispersionFit& p, std::array<double, maxFittedPartials>& e)
    {
        if (!valid(p) || !dispersionErrors(p, stages, pairs, omega1, inharmonicity, partials, e))
            return std::numeric_limits<double>::infinity();
        double sum = 0.0;
        for (int n = 2; n <= partials; ++n)
            sum += e[static_cast<std::size_t>(n - 2)] * e[static_cast<std::size_t>(n - 2)];
        return sum;
    };
    const int parameters = 1 + 2 * pairs;
    constexpr int maxParameters = 1 + 2 * dispersionPairs;
    const int residuals = partials - 1;
    double current = cost(fit, cents);
    if (!(current < std::numeric_limits<double>::infinity()))
        return false;
    double damping = 1.0e-3;
    std::array<std::array<double, maxFittedPartials>, maxParameters> jacobian {};
    std::array<double, maxFittedPartials> trial {};
    for (int iteration = 0; iteration < 100 && damping < 1.0e10; ++iteration)
    {
        for (int j = 0; j < parameters; ++j)
        {
            // Forward, or backward at a bound; a parameter boxed in both
            // ways stays where it is.
            constexpr double step = 1.0e-7;
            auto& column = jacobian[static_cast<std::size_t>(j)];
            DispersionFit nudged = fit;
            nudged[static_cast<std::size_t>(j)] += step;
            double sign = 1.0;
            if (!(cost(nudged, trial) < std::numeric_limits<double>::infinity()))
            {
                nudged[static_cast<std::size_t>(j)] -= 2.0 * step;
                sign = -1.0;
                if (!(cost(nudged, trial) < std::numeric_limits<double>::infinity()))
                {
                    column.fill(0.0);
                    continue;
                }
            }
            for (int i = 0; i < residuals; ++i)
                column[static_cast<std::size_t>(i)]
                    = sign * (trial[static_cast<std::size_t>(i)] - cents[static_cast<std::size_t>(i)]) / step;
        }
        // (J^T J + damping diag(J^T J)) delta = -J^T e, by Gaussian
        // elimination with partial pivoting.
        std::array<std::array<double, maxParameters + 1>, maxParameters> system {};
        for (int row = 0; row < parameters; ++row)
        {
            const auto& u = jacobian[static_cast<std::size_t>(row)];
            for (int column = 0; column < parameters; ++column)
            {
                const auto& w = jacobian[static_cast<std::size_t>(column)];
                double sum = 0.0;
                for (int i = 0; i < residuals; ++i)
                    sum += u[static_cast<std::size_t>(i)] * w[static_cast<std::size_t>(i)];
                system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = sum;
            }
            double gradient = 0.0;
            for (int i = 0; i < residuals; ++i)
                gradient += u[static_cast<std::size_t>(i)] * cents[static_cast<std::size_t>(i)];
            system[static_cast<std::size_t>(row)][parameters] = -gradient;
            system[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)] *= 1.0 + damping;
        }
        bool solved = true;
        for (int column = 0; column < parameters && solved; ++column)
        {
            int pivot = column;
            for (int row = column + 1; row < parameters; ++row)
                if (std::abs(system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)])
                    > std::abs(system[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(column)]))
                    pivot = row;
            std::swap(system[static_cast<std::size_t>(column)], system[static_cast<std::size_t>(pivot)]);
            const double diagonal = system[static_cast<std::size_t>(column)][static_cast<std::size_t>(column)];
            solved = std::abs(diagonal) > 1.0e-300;
            for (int row = column + 1; row < parameters && solved; ++row)
            {
                const double factor = system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] / diagonal;
                for (int k = column; k <= parameters; ++k)
                    system[static_cast<std::size_t>(row)][static_cast<std::size_t>(k)]
                        -= factor * system[static_cast<std::size_t>(column)][static_cast<std::size_t>(k)];
            }
        }
        DispersionFit delta {};
        for (int row = parameters - 1; row >= 0 && solved; --row)
        {
            double sum = system[static_cast<std::size_t>(row)][parameters];
            for (int k = row + 1; k < parameters; ++k)
                sum -= system[static_cast<std::size_t>(row)][static_cast<std::size_t>(k)] * delta[static_cast<std::size_t>(k)];
            delta[static_cast<std::size_t>(row)] = sum / system[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)];
        }
        DispersionFit next = fit;
        for (int k = 0; k < parameters; ++k)
            next[static_cast<std::size_t>(k)] += delta[static_cast<std::size_t>(k)];
        const double candidate = solved ? cost(next, trial) : std::numeric_limits<double>::infinity();
        if (candidate < current)
        {
            // Settled once a step gains under a millionth.
            const bool settled = current - candidate < 1.0e-6 * current;
            fit = next;
            cents = trial;
            current = candidate;
            damping = std::max(1.0e-9, damping / 3.0);
            if (settled)
                break;
        }
        else
            damping *= 4.0;
    }
    return true;
}

// Two sections start near 1.25 and 2.5 kHz, 800 Hz wide (where a steel G3's
// fit settles), and hold the steel lyre harp's strings within 2 cents. Where
// a partial below 5 kHz still lands more than 5 cents from the law, a section
// is added at 3 kHz, 800 Hz wide, and the fit resumes from where the last one
// settled, up to four sections. At 48 kHz the gut lyre's D3 lands 25 cents
// off with two, 11 with three and 4.2 with four, its E3 8.9 with two and 2.7
// with three, the nylon G3 6.3 and 1.4. Fitted from scratch, a third section
// settled at 0 Hz and left the D3 33 cents off. The design is kept only where
// it beats the first-order one.
DispersionDesign refineDispersion(const DispersionDesign& firstOrder, double fundamental,
                                  double inharmonicity, double sampleRate) noexcept
{
    const double omega1 = 2.0 * pi * fundamental / sampleRate;
    // The partials whose stretched frequency is below the band's top.
    const double top = std::min(5000.0, 0.2 * sampleRate);
    int partials = 1;
    while (partials < maxFittedPartials + 1 && fundamental * stretchedPartial(partials + 1, inharmonicity) < top)
        ++partials;
    DispersionDesign result = firstOrder;
    if (partials <= 12)
        return result;
    std::array<double, maxFittedPartials> cents {};
    DispersionFit fit {};
    fit[0] = firstOrder.coefficient;
    if (!dispersionErrors(fit, firstOrder.stages, 0, omega1, inharmonicity, partials, cents))
        return result;
    result.worstCents = worstOf(cents, partials);

    for (int pairs = 2; pairs <= dispersionPairs; ++pairs)
    {
        for (int k = pairs == 2 ? 0 : pairs - 1; k < pairs; ++k)
        {
            fit[static_cast<std::size_t>(1 + 2 * k)] = std::exp(-2.0 * pi * 800.0 / sampleRate);
            fit[static_cast<std::size_t>(2 + 2 * k)] = 2.0 * pi * (pairs == 2 ? 1250.0 * (k + 1) : 3000.0) / sampleRate;
        }
        if (!fitSections(fit, pairs, firstOrder.stages, omega1, inharmonicity, partials, cents))
            break;
        const double worst = worstOf(cents, partials);
        if (worst < result.worstCents)
        {
            result.coefficient = fit[0];
            result.pairs = pairs;
            for (int k = 0; k < pairs; ++k)
            {
                const double r = fit[static_cast<std::size_t>(1 + 2 * k)];
                const double theta = fit[static_cast<std::size_t>(2 + 2 * k)];
                result.pair[static_cast<std::size_t>(k)] = { -2.0 * r * std::cos(theta), r * r };
            }
            result.worstCents = worst;
        }
        if (result.worstCents <= 5.0)
            break;
    }
    return result;
}

DispersionDesign designDispersion(double fundamental, double inharmonicity, double sampleRate,
                                  bool refine = true) noexcept
{
    const double omega1 = 2.0 * pi * fundamental / sampleRate;
    const int partials = std::clamp(static_cast<int>(std::floor(std::min(5000.0, 0.2 * sampleRate) / fundamental)), 2, 12);
    const auto lag = firstOrderLag;
    const auto groupDelay = firstOrderGroupDelay;
    const double infinity = std::numeric_limits<double>::infinity();
    // The worst partial's error in cents with coefficient a = -0.0025 step.
    const auto worstAt = [&] (int stages, int step)
    {
        const double a = -0.0025 * step;
        const double delay = (2.0 * pi - stages * lag(a, omega1)) / omega1;
        // The line, its Thiran section and the loss filters need some.
        if (delay < 8.0)
            return infinity;
        double worst = 0.0;
        for (int n = 2; n <= partials; ++n)
        {
            const double target = omega1 * stretchedPartial(n, inharmonicity);
            double omega = target;
            for (int iteration = 0; iteration < 4; ++iteration)
                omega -= (omega * delay + stages * lag(a, omega) - 2.0 * pi * n)
                       / (delay + stages * groupDelay(a, omega));
            worst = std::max(worst, std::abs(1200.0 * std::log2(omega / target)));
        }
        return worst;
    };
    // Coarse steps of 0.02, then every 0.0025 step around the best: a sixth
    // of the evaluations of the full grid (the setup of three instruments,
    // levers included, fits in about half a second).
    constexpr int lastStep = 380, coarse = 8;
    std::array<DispersionDesign, maxDispersionStages + 1> best {};
    for (int stages = 1; stages <= maxDispersionStages; ++stages)
    {
        DispersionDesign& found = best[static_cast<std::size_t>(stages)];
        found.worstCents = infinity;
        int centre = 0;
        for (int step = 0; step <= lastStep; step += coarse)
        {
            const double worst = worstAt(stages, step);
            if (worst == infinity)
                break;
            if (worst < found.worstCents)
            {
                found = { stages, -0.0025 * step, worst };
                centre = step;
            }
        }
        for (int step = std::max(0, centre - coarse + 1); step < std::min(lastStep + 1, centre + coarse); ++step)
        {
            const double worst = worstAt(stages, step);
            if (worst < found.worstCents)
                found = { stages, -0.0025 * step, worst };
        }
    }
    // The fewest sections within a cent; otherwise the closest fit.
    DispersionDesign chosen = best[1];
    for (int stages = 1; stages <= maxDispersionStages; ++stages)
    {
        const DispersionDesign& design = best[static_cast<std::size_t>(stages)];
        if (design.worstCents <= 1.0)
        {
            chosen = design;
            break;
        }
        if (design.worstCents < chosen.worstCents)
            chosen = design;
    }
    if (!refine)
        return chosen;
    DispersionDesign refined = refineDispersion(chosen, fundamental, inharmonicity, sampleRate);
    // Four sections fit the gut D3 within 4.2 cents over eight stages but
    // only 5.6 over seven, which the search above takes at 88.2, 176.4 and
    // 192 kHz (its raised D#3 6.8 over six): where the sections still leave a
    // partial more than 5 cents out, the fit is tried again over the most.
    const DispersionDesign& most = best[static_cast<std::size_t>(maxDispersionStages)];
    if (refined.worstCents > 5.0 && chosen.stages < maxDispersionStages && most.worstCents < infinity)
    {
        const DispersionDesign retried = refineDispersion(most, fundamental, inharmonicity, sampleRate);
        if (retried.worstCents < refined.worstCents)
            refined = retried;
    }
    return refined;
}

Complex lossSectionResponse(const LossSectionDesign& s, double omega) noexcept
{
    const Complex z1 = std::exp(Complex(0.0, -omega));
    const Complex z2 = z1 * z1;
    return (s.gain + s.n1 * z1 + s.n2 * z2) / (1.0 + s.a1 * z1 + s.a2 * z2);
}

// y = x - (1 - z^-1)(c0 + c1 z^-1) / A(z) x: the section above with its unit
// gain at DC exact in any precision (Acustra's StringLoop runs its loss
// sections this way).
struct UnitDcSection
{
    double c0 { 0.0 }, c1 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    double previousInput { 0.0 }, previousDifference { 0.0 };
    double u1 { 0.0 }, u2 { 0.0 };
    bool active { false };

    void design(const LossSectionDesign& s) noexcept
    {
        active = s.gain != 1.0 || s.a1 != 0.0 || s.a2 != 0.0;
        c0 = 1.0 - s.gain;
        c1 = s.n2 - s.a2;
        a1 = s.a1;
        a2 = s.a2;
    }
    void reset() noexcept { previousInput = previousDifference = u1 = u2 = 0.0; }
    double process(double x) noexcept
    {
        if (!active)
            return x;
        const double difference = x - previousInput;
        previousInput = x;
        const double w = c0 * difference + c1 * previousDifference;
        previousDifference = difference;
        const double u = w - a1 * u1 - a2 * u2;
        u2 = u1;
        u1 = u;
        return x - u;
    }
};

// The frequency ratios at which setup measures each loop's phase lag for the
// pitch bend and the tension glide: every quarter semitone over the bend's
// +-12 semitones. The bridge's reflection phase turns across a body mode
// within about half a semitone (a Q of 35); semitone steps over +-2 left
// bends up to 3 cents off between them, and extrapolation past +2 left an
// octave up on G3 59 cents flat.
constexpr int bendStepsPerSemitone = 4;
constexpr int bendPoints = 24 * bendStepsPerSemitone + 1;
// 2^((k - 48) / 48), as std::pow gives them. A literal, because the Rack
// SDK refuses any global that needs code to initialise it.
constexpr std::array<double, bendPoints> bendRatios {
    0.5, 0.5072726674687619, 0.514651118321746,
    0.5221368912137069, 0.5297315471796477, 0.5374366699603482,
    0.5452538663326288, 0.5531847664444167, 0.5612310241546865,
    0.5693943173783458, 0.5776763484361365, 0.5860788444096257,
    0.5946035575013605, 0.6032522654002609, 0.6120267716523276,
    0.620928906036742, 0.6299605249474366, 0.6391235117802152,
    0.6484197773255048, 0.6578512601668188, 0.6674199270850172,
    0.6771277734684463, 0.6869768237290446, 0.6969691317244996,
    0.7071067811865476, 0.7173918861555001, 0.7278265914210937,
    0.7384130729697497, 0.7491535384383408, 0.7600502275745574,
    0.7711054127039704, 0.7823213992038871, 0.7937005259840998,
    0.8052451659746271, 0.8169577266205499, 0.8288406503840438,
    0.8408964152537145, 0.8531275352613428, 0.8655365610061431,
    0.8781260801866497, 0.8908987181403393, 0.903857138391101,
    0.9170040432046712, 0.9303421741521466, 0.9438743126816935,
    0.9576032806985737, 0.9715319411536059, 0.9856631986401876,
    1.0, 1.0145453349375237, 1.029302236643492,
    1.0442737824274138, 1.0594630943592953, 1.0748733399206964,
    1.0905077326652577, 1.1063695328888334, 1.122462048309373,
    1.1387886347566916, 1.155352696872273, 1.1721576888192515,
    1.189207115002721, 1.2065045308005218, 1.2240535433046553,
    1.241857812073484, 1.2599210498948732, 1.2782470235604306,
    1.2968395546510096, 1.3157025203336377, 1.3348398541700344,
    1.3542555469368927, 1.3739536474580891, 1.3939382634489994,
    1.4142135623730951, 1.4347837723110002, 1.4556531828421873,
    1.4768261459394993, 1.4983070768766815, 1.5201004551491148,
    1.5422108254079407, 1.5646427984077742, 1.5874010519681994,
    1.6104903319492543, 1.6339154532411, 1.6576813007680873,
    1.681792830507429, 1.7062550705226855, 1.731073122012286,
    1.7562521603732995, 1.7817974362806785, 1.8077142767822019,
    1.8340080864093424, 1.8606843483042932, 1.8877486253633868,
    1.9152065613971474, 1.9430638823072117, 1.9713263972803752,
    2.0
};
constexpr bool bendRatiosOnGrid() noexcept
{
    for (std::size_t k = 0; k + 1 < bendRatios.size(); ++k)
    {
        const double step = bendRatios[k + 1] / bendRatios[k] - 1.0145453349375237;
        if (step > 1.0e-15 || step < -1.0e-15)
            return false;
    }
    return bendRatios[0] == 0.5 && bendRatios[bendPoints / 2] == 1.0 && bendRatios[bendPoints - 1] == 2.0;
}
static_assert(bendRatiosOnGrid(), "bendRatios steps by a quarter semitone from -12 to +12");

// The line's whole samples for a delay of `samples`, leaving the Thiran
// section a fraction in [1.3, 2.3): about 2, where the section is a pure
// delay at every frequency (Loop::setLineDelay).
int wholeFor(double samples) noexcept
{
    return static_cast<int>(std::floor(samples - 1.3));
}

// What sets one polarisation's pitch: its frequency, its filters and the
// line that tunes them. A sharping lever swaps one for another.
struct LoopDesign
{
    double frequency { 0.0 };
    double gain { 1.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    int dispersionPairs { 0 };
    std::array<std::array<double, 2>, acukoto::dispersionPairs> pair {};
    double dispersionCents { 0.0 };  // the design's worst partial error
    LossSectionDesign constantDesign {};
    LossSectionDesign bendingDesign {};
    // The line delay at rest (whole + fraction) and the whole loop's delay
    // at the fundamental, both in samples.
    double restLineDelay { 68.0 };
    double loopDelay { 70.0 };
    // The phase lag, in cycles, of everything but the line's delay (the
    // filters' dispersion and the bridge's reflection) at bendRatios.
    std::array<double, bendPoints> lagCycles {};

    // Phase lag of everything in the loop but the line's whole samples and
    // the Thiran section, at omega (rad/sample).
    double filterLag(double omega) const noexcept
    {
        double lag = 0.0;
        for (int stage = 0; stage < dispersionStages; ++stage)
            lag += allpassLag(1, dispersion, 0.0, omega);
        for (int k = 0; k < dispersionPairs; ++k)
            lag += allpassLag(2, pair[static_cast<std::size_t>(k)][0], pair[static_cast<std::size_t>(k)][1], omega);
        lag -= std::arg(lossSectionResponse(constantDesign, omega));
        lag -= std::arg(lossSectionResponse(bendingDesign, omega));
        return lag;
    }
};

// One polarisation of one string: the round trip from the bridge to the yoke
// and back, holding the velocity wave that will next arrive at the bridge.
// The yoke's inversion is folded into what the bridge writes.
struct Loop
{
    std::array<float, delayCapacity> line {};
    // A pluck's first round trip, still to arrive at the bridge, by sample.
    // It enters at the loop's output rather than the line because the loop's
    // filters hold part of a period (the dispersion stages up to a fifth of
    // it on a bass string): a wave squeezed into the line alone would leave
    // a gap and pluck at the wrong point.
    std::array<float, delayCapacity> pending {};
    int write { 0 };
    int whole { 64 };
    double fraction { 2.0 };
    double thiranA1 { 0.0 }, thiranA2 { 0.0 };
    double tx1 { 0.0 }, tx2 { 0.0 }, ty1 { 0.0 }, ty2 { 0.0 };
    // The wave that last arrived at the bridge.
    double last { 0.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    double dispersionCents { 0.0 };  // the design's worst partial error
    std::array<double, maxDispersionStages> dispersionState {};
    int dispersionPairs { 0 };
    std::array<std::array<double, 2>, acukoto::dispersionPairs> pair {};
    std::array<std::array<double, 2>, acukoto::dispersionPairs> pairState {};
    UnitDcSection constantLoss {};
    UnitDcSection bendingLoss {};
    LossSectionDesign constantDesign {};
    LossSectionDesign bendingDesign {};
    double gain { 1.0 };
    double damping { 1.0 };
    double dampingTarget { 1.0 };
    double dampingStep { 0.0 };
    // The line delay at rest (whole + fraction) and the whole loop's delay
    // at the fundamental, both in samples.
    double restLineDelay { 68.0 };
    double loopDelay { 70.0 };
    std::array<double, bendPoints> lagCycles {};

    // Stops the wave and everything still on its way, keeping the pitch and
    // the hand's damping.
    void silence() noexcept
    {
        line.fill(0.0f);
        pending.fill(0.0f);
        tx1 = tx2 = ty1 = ty2 = last = 0.0;
        dispersionState.fill(0.0);
        pairState = {};
        constantLoss.reset();
        bendingLoss.reset();
    }

    void reset() noexcept
    {
        silence();
        // Back to where prepare() leaves the line, so a reset plays as a
        // fresh instance does, sample for sample.
        write = 0;
        damping = dampingTarget = 1.0;
        dampingStep = 0.0;
        setLineDelay(restLineDelay);
    }

    // Keeps the Thiran fraction in [1.1, 2.3), and once it leaves moves the
    // tap to bring it back into [1.3, 2.3) (wholeFor): the section is a pure
    // delay at 2 and strays from it at high frequencies the further the
    // fraction is from 2, at 11 kHz and 48 kHz by under 0.05 samples in that
    // range, 0.27 at 3 and 1.2 at 4.5. Fractions resting 4 to 5 deep had
    // left the steel A5's partials above 6 kHz up to 50 cents sharp, and up
    // to 33 under a held bend of half a semitone while the tap could drift
    // to 5. A treble string's tension glide is under 0.4 samples at 48 kHz;
    // a hard pluck's glide moves a bass string's tap a few times. The
    // section's past inputs move with the tap, so the wave passes it
    // unbroken.
    void setLineDelay(double samples) noexcept
    {
        const double current = samples - whole;
        if (current < 1.1 || current >= 2.3)
        {
            whole = std::clamp(wholeFor(samples), 1, delayCapacity - 8);
            tx1 = line[static_cast<std::size_t>((write - 1 - whole) & (delayCapacity - 1))];
            tx2 = line[static_cast<std::size_t>((write - 2 - whole) & (delayCapacity - 1))];
        }
        fraction = samples - whole;
        thiranCoefficients(fraction, thiranA1, thiranA2);
    }

    double read() noexcept
    {
        const double x = line[static_cast<std::size_t>((write - whole) & (delayCapacity - 1))];
        double y = thiranA2 * x + thiranA1 * tx1 + tx2 - thiranA1 * ty1 - thiranA2 * ty2;
        tx2 = tx1;
        tx1 = x;
        ty2 = ty1;
        ty1 = y;
        for (int stage = 0; stage < dispersionStages; ++stage)
        {
            const double out = dispersion * y + dispersionState[stage];
            dispersionState[stage] = y - dispersion * out;
            y = out;
        }
        for (int k = 0; k < dispersionPairs; ++k)
        {
            const double c1 = pair[static_cast<std::size_t>(k)][0], c2 = pair[static_cast<std::size_t>(k)][1];
            auto& state = pairState[static_cast<std::size_t>(k)];
            const double out = c2 * y + state[0];
            state[0] = c1 * (y - out) + state[1];
            state[1] = y - c2 * out;
            y = out;
        }
        y = constantLoss.process(y);
        y = bendingLoss.process(y);
        if (dampingStep != 0.0)
        {
            damping += dampingStep;
            if ((dampingStep < 0.0 && damping <= dampingTarget)
                || (dampingStep > 0.0 && damping >= dampingTarget))
            {
                damping = dampingTarget;
                dampingStep = 0.0;
            }
        }
        float& arriving = pending[static_cast<std::size_t>(write)];
        y = y * gain * damping + arriving;
        arriving = 0.0f;
        last = y;
        return y;
    }

    void writeSample(double value) noexcept
    {
        line[static_cast<std::size_t>(write)] = static_cast<float>(value);
        write = (write + 1) & (delayCapacity - 1);
    }

    // The loss per round trip a resting hand adds, reached over one round trip.
    void setDamping(double target) noexcept
    {
        dampingTarget = target;
        const double steps = std::max(1.0, loopDelay);
        dampingStep = (target - damping) / steps;
        if (dampingStep == 0.0)
            damping = target;
    }

    // The line delay that keeps the loop resonant at `ratio` times its own
    // frequency, (1 - lag) loopDelay / ratio, the lag interpolated between
    // the ratios setup measured (extrapolated past the last, where the glide
    // takes a full bend). Scaling the line alone would leave the
    // dispersion's phase delay at its own frequency: 11 cents flat of a
    // two-semitone bend on A4.
    double lineDelayAt(double ratio) const noexcept
    {
        if (ratio == 1.0)
            return restLineDelay;
        const auto k = static_cast<std::size_t>(
            std::upper_bound(bendRatios.begin() + 1, bendRatios.end() - 1, ratio) - bendRatios.begin() - 1);
        const double t = (ratio - bendRatios[k]) / (bendRatios[k + 1] - bendRatios[k]);
        const double lag = lagCycles[k] + t * (lagCycles[k + 1] - lagCycles[k]);
        return (1.0 - lag) * loopDelay / ratio;
    }

    // Takes a new pitch while the wave keeps travelling.
    void apply(const LoopDesign& design) noexcept
    {
        // Sections the old design left idle hold stale state.
        for (int stage = dispersionStages; stage < design.dispersionStages; ++stage)
            dispersionState[static_cast<std::size_t>(stage)] = 0.0;
        for (int k = dispersionPairs; k < design.dispersionPairs; ++k)
            pairState[static_cast<std::size_t>(k)] = {};
        gain = design.gain;
        dispersionStages = design.dispersionStages;
        dispersion = design.dispersion;
        dispersionPairs = design.dispersionPairs;
        pair = design.pair;
        dispersionCents = design.dispersionCents;
        constantDesign = design.constantDesign;
        bendingDesign = design.bendingDesign;
        constantLoss.design(constantDesign);
        bendingLoss.design(bendingDesign);
        restLineDelay = design.restLineDelay;
        loopDelay = design.loopDelay;
        lagCycles = design.lagCycles;
        setLineDelay(restLineDelay);
    }
};

// The ji's generalized coordinates: every string's ji heaves with the top
// beneath it (coordinate `ji`, the string's motion normal to the top) and
// tips across the top about its foot (maxStrings + ji, the motion parallel
// to it).
constexpr std::size_t barCoordinates = 2 * maxStrings;
using BarVector = std::array<double, barCoordinates>;

// One soundbox mode with unit modal mass: velocity per generalized force
// s / (s^2 + (w/Q) s + w^2), bilinear at its own frequency (passive at every
// rate): b0 (1 - z^-2) / (1 + a1 z^-1 + a2 z^-2).
struct Mode
{
    double frequency { 0.0 };
    double q { 1.0 };
    double b0 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    BarVector participation {};  // each ji's heave, then each ji's tip
    // Each microphone's pressure per unit of the mode's volume acceleration,
    // W = in phase + j quadrature at the mode's frequency, which the mode
    // keeps at every frequency (PhaseNetwork).
    double radiationLeft { 0.0 }, radiationLeftQuadrature { 0.0 };
    double radiationRight { 0.0 }, radiationRightQuadrature { 0.0 };
    double monopole { 0.0 };
    // Each part's share of the mode's kinetic energy (ModeReport).
    double airShare { 0.0 };
    double bodyShare { 0.0 };
    double wallShare { 0.0 };
    double bridgeShare { 0.0 };
};

// The modes as process() runs them, in transposed direct form II: a
// structure of arrays, padded with silent modes to whole groups of `lanes`
// so the compiler can vectorize each group. `drive` is the participation
// times b0, `pull` the participation alone. The modes that meet only the
// even coordinates come first, up to evenEnd, then those that meet only the
// odd ones, up to oddEnd, then the rest, each part padded, so that process()
// runs each part with only its own coordinates.
struct ModeBank
{
    static constexpr int lanes = 8;
    static constexpr int capacity = maxModes + 3 * lanes;
    int evenEnd { 0 }, oddEnd { 0 }, count { 0 };
    std::array<std::array<double, capacity>, barCoordinates> drive {}, pull {};
    std::array<double, capacity> a1 {}, a2 {}, left {}, right {}, leftQuadrature {}, rightQuadrature {}, s1 {}, s2 {};
};

// Each mode radiates j w W times its velocity, with W found at its own
// frequency and kept at every other, its quadrature part as its in-phase
// part. Multiplying by j is a 90-degree shift, which no filter makes
// exactly: these two chains of first-order all-passes, (p - s) / (p + s)
// bilinear, hold their outputs 90 degrees apart within 0.65 degrees from
// 15 Hz to 20 kHz at 48 kHz and every higher rate (to 18.7 kHz at 44.1 kHz).
// The poles are an equiripple fit, in rad/s, prewarped at 48 kHz. The phase
// both chains share delays the low end against the highs, about 3 ms at
// 100 Hz.
//
// The quadrature once took -w_m Im W times the velocity, which is j w W
// only at the mode's own frequency w_m: below it each mode's quadrature was
// w_m / w times too large, and below the modes they summed to a sound of
// their own. Against the full radiation sum (Kirchhoff's grid, W found at
// each frequency instead of w_m), in third octaves over every ji and both
// microphones, that put 6 to 9 dB too much into 50 to 63 Hz, and at D3's ji
// 4 to 5 dB around its fundamental, where it stood 12 dB over what the
// body's modes radiate. Keeping W is within 1.6 dB of the full sum from 50 Hz
// to 5 kHz, as the old form was only from 200 Hz; above 5 kHz both run 2 to
// 4 dB over it.
struct PhaseNetwork
{
    static constexpr int sections = 6;
    static constexpr std::array<double, sections> inPhasePoles { 40.5732, 370.095, 1872.57, 9077.54, 43889.4, 228218.0 };
    static constexpr std::array<double, sections> quadraturePoles { 151.443, 839.834, 4151.93, 19980.1, 95894.8, 844886.0 };
    // Each section's c in (c + z^-1) / (1 + c z^-1).
    std::array<double, sections> inPhase {}, quadrature {};

    void design(double fs) noexcept
    {
        for (int i = 0; i < sections; ++i)
        {
            inPhase[static_cast<std::size_t>(i)] = (inPhasePoles[static_cast<std::size_t>(i)] - 2.0 * fs)
                                                 / (inPhasePoles[static_cast<std::size_t>(i)] + 2.0 * fs);
            quadrature[static_cast<std::size_t>(i)] = (quadraturePoles[static_cast<std::size_t>(i)] - 2.0 * fs)
                                                    / (quadraturePoles[static_cast<std::size_t>(i)] + 2.0 * fs);
        }
    }

    static double run(const std::array<double, sections>& c, std::array<double, sections>& state, double x) noexcept
    {
        for (std::size_t i = 0; i < sections; ++i)
        {
            const double y = c[i] * x + state[i];
            state[i] = x - c[i] * y;
            x = y;
        }
        return x;
    }
};

// Runs the bank's modes [from, to) one sample on, adding what they radiate to
// each microphone (in phase and in quadrature) and their pull on the bar's
// next velocity, each lane in its own sum so that a group runs as one vector.
void runModes(ModeBank& bank, int from, int to, const BarVector& force, double (&left)[ModeBank::lanes],
              double (&right)[ModeBank::lanes], double (&leftQuadrature)[ModeBank::lanes],
              double (&rightQuadrature)[ModeBank::lanes], double (&past)[barCoordinates][ModeBank::lanes]) noexcept
{
    for (int group = from; group < to; group += ModeBank::lanes)
    {
        // Each mode's generalized force, the coordinates in the same order
        // as one sum per mode, a group's lanes side by side.
        double drive[ModeBank::lanes];
        for (int lane = 0; lane < ModeBank::lanes; ++lane)
            drive[lane] = 0.0 + bank.drive[0][static_cast<std::size_t>(group + lane)] * force[0];
        for (std::size_t c = 1; c < barCoordinates; ++c)
            for (int lane = 0; lane < ModeBank::lanes; ++lane)
                drive[lane] += bank.drive[c][static_cast<std::size_t>(group + lane)] * force[c];
        for (int lane = 0; lane < ModeBank::lanes; ++lane)
        {
            const auto k = static_cast<std::size_t>(group + lane);
            const double x = drive[lane];
            const double y = x + bank.s1[k];
            const double s1 = bank.s2[k] - bank.a1[k] * y;
            bank.s2[k] = -x - bank.a2[k] * y;
            bank.s1[k] = s1;
            left[lane] += bank.left[k] * y;
            right[lane] += bank.right[k] * y;
            leftQuadrature[lane] += bank.leftQuadrature[k] * y;
            rightQuadrature[lane] += bank.rightQuadrature[k] * y;
        }
    }
    // The pull on the bar, one coordinate at a time over every group, so a
    // coordinate's lane sums stay in registers instead of all 26 spilling.
    // Each sum still adds the same products in the same group order.
    for (std::size_t c = 0; c < barCoordinates; ++c)
    {
        double sum[ModeBank::lanes];
        for (int lane = 0; lane < ModeBank::lanes; ++lane)
            sum[lane] = past[c][lane];
        const double* pull = bank.pull[c].data();
        const double* state = bank.s1.data();
        for (int group = from; group < to; group += ModeBank::lanes)
            for (int lane = 0; lane < ModeBank::lanes; ++lane)
                sum[lane] += pull[group + lane] * state[group + lane];
        for (int lane = 0; lane < ModeBank::lanes; ++lane)
            past[c][lane] = sum[lane];
    }
}

struct BarMatrix
{
    std::array<std::array<double, barCoordinates>, barCoordinates> m {};

    static BarMatrix identity() noexcept
    {
        BarMatrix r;
        for (std::size_t i = 0; i < barCoordinates; ++i)
            r.m[i][i] = 1.0;
        return r;
    }
    BarMatrix operator*(const BarMatrix& o) const noexcept
    {
        BarMatrix r;
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                for (std::size_t k = 0; k < barCoordinates; ++k)
                    r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }
    BarVector apply(const BarVector& v) const noexcept
    {
        BarVector r {};
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                r[i] += m[i][j] * v[j];
        return r;
    }
};

// Gauss-Jordan with partial pivoting (setup only).
template <typename T, std::size_t N>
std::array<std::array<T, N>, N> inverse(std::array<std::array<T, N>, N> a) noexcept
{
    std::array<std::array<T, N>, N> r {};
    for (std::size_t i = 0; i < N; ++i)
        r[i][i] = T(1.0);
    for (std::size_t c = 0; c < N; ++c)
    {
        std::size_t pivot = c;
        for (std::size_t i = c + 1; i < N; ++i)
            if (std::abs(a[i][c]) > std::abs(a[pivot][c]))
                pivot = i;
        std::swap(a[c], a[pivot]);
        std::swap(r[c], r[pivot]);
        const T scale = T(1.0) / a[c][c];
        for (std::size_t j = 0; j < N; ++j)
        {
            a[c][j] *= scale;
            r[c][j] *= scale;
        }
        for (std::size_t i = 0; i < N; ++i)
            if (i != c)
            {
                const T factor = a[i][c];
                for (std::size_t j = 0; j < N; ++j)
                {
                    a[i][j] -= factor * a[c][j];
                    r[i][j] -= factor * r[c][j];
                }
            }
    }
    return r;
}

struct Random
{
    std::uint64_t state { 0x9e3779b97f4a7c15ull };
    void seed(std::uint64_t value) noexcept { state = value ? value : 0x9e3779b97f4a7c15ull; }
    std::uint64_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
    // Uniform in [-1, 1).
    double bipolar() noexcept
    {
        return static_cast<double>(next() >> 11) * (2.0 / 9007199254740992.0) - 1.0;
    }
    // Standard normal (Box-Muller), cut at three deviations.
    double gaussian() noexcept
    {
        const double u = (static_cast<double>(next() >> 11) + 1.0) * (1.0 / 9007199254740992.0);
        return std::clamp(std::sqrt(-2.0 * std::log(u)) * std::cos(pi * bipolar()), -3.0, 3.0);
    }
};

double normalCdf(double u) noexcept { return 0.5 * std::erfc(-u / std::sqrt(2.0)); }
double normalPdf(double u) noexcept { return std::exp(-0.5 * u * u) / std::sqrt(2.0 * pi); }

// A small room around the microphones: early reflections and an eight-line
// feedback delay network (Householder feedback, one-pole loss per line set
// for a 1.1 s low and 0.45 s high reverberation time). Authored, like any
// room: it is the space the demos are heard in, not part of the instrument.
// What enters the network is first smeared by four allpasses, Dattorro's
// input diffusers (JAES 45(9) 1997, Fig. 1: 142, 107, 379 and 277 samples
// at 29.761 kHz, gains 0.75 and 0.625). A real room's reflections are dense
// within tens of milliseconds; fed a pluck directly, the network's tail
// stayed a train of separate echoes, its normalized echo density (Abel and
// Huang, AES 121, 2006; 1 for a dense tail) 0.10 at 50 ms and 0.35 at
// 100 ms, reaching 1 only after 300 ms. Diffused, it is 0.7 at 50 ms and 1
// from 70 ms, with the same decay and spectrum.
struct Room
{
    static constexpr int lineCount = 8;
    static constexpr int capacity = 16384;
    // A power of two above the longest early tap, the right channel's 31.7 ms,
    // at the Performer's highest engine rate, 192 kHz.
    static constexpr int earlyCapacity = 8192;
    std::array<std::array<float, capacity>, lineCount> lines {};
    std::array<int, lineCount> lengths {};
    std::array<int, lineCount> positions {};
    std::array<double, lineCount> gains {};
    std::array<double, lineCount> poles {};
    std::array<double, lineCount> filterStates {};
    std::array<float, earlyCapacity> early {};
    int earlyPosition { 0 };
    std::array<int, 6> earlyTapsLeft {}, earlyTapsRight {};
    std::array<double, 6> earlyGainsLeft {}, earlyGainsRight {};
    int predelay { 0 };
    static constexpr int diffuserCount = 4;
    // A power of two above the longest diffuser, 12.7 ms, at 192 kHz.
    static constexpr int diffuserCapacity = 4096;
    std::array<std::array<float, diffuserCapacity>, diffuserCount> diffusers {};
    std::array<int, diffuserCount> diffuserLengths {};
    int diffuserPosition { 0 };

    void prepare(double sampleRate) noexcept
    {
        constexpr double diffuserMs[diffuserCount] { 4.771, 3.595, 12.735, 9.307 };
        for (int index = 0; index < diffuserCount; ++index)
            diffuserLengths[static_cast<std::size_t>(index)]
                = static_cast<int>(std::lround(diffuserMs[index] * 1.0e-3 * sampleRate));
        constexpr double milliseconds[lineCount] { 23.1, 29.7, 34.3, 39.9, 44.3, 51.7, 57.1, 63.7 };
        constexpr double lowT60 = 1.1;
        constexpr double highT60 = 0.45;
        constexpr double highHz = 4000.0;
        for (int index = 0; index < lineCount; ++index)
        {
            lengths[static_cast<std::size_t>(index)] = std::min(capacity - 1,
                static_cast<int>(std::lround(milliseconds[index] * 1.0e-3 * sampleRate)));
            const double length = lengths[static_cast<std::size_t>(index)];
            const double low = std::pow(10.0, -3.0 * length / (sampleRate * lowT60));
            const double high = std::pow(10.0, -3.0 * length / (sampleRate * highT60));
            const double ratio = high / low;
            const double omega = 2.0 * pi * highHz / sampleRate;
            double lower = 0.0, upper = 0.999;
            for (int iteration = 0; iteration < 60; ++iteration)
            {
                const double pole = 0.5 * (lower + upper);
                const double magnitude = (1.0 - pole)
                    / std::sqrt(1.0 - 2.0 * pole * std::cos(omega) + pole * pole);
                if (magnitude > ratio)
                    lower = pole;
                else
                    upper = pole;
            }
            gains[static_cast<std::size_t>(index)] = low;
            poles[static_cast<std::size_t>(index)] = 0.5 * (lower + upper);
        }
        constexpr double earlyMs[6] { 7.3, 11.9, 14.6, 19.2, 23.8, 27.1 };
        constexpr double earlyGain[6] { 0.42, -0.33, 0.29, -0.24, 0.2, -0.16 };
        for (int index = 0; index < 6; ++index)
        {
            earlyTapsLeft[static_cast<std::size_t>(index)] = static_cast<int>(earlyMs[index] * 1.0e-3 * sampleRate);
            earlyTapsRight[static_cast<std::size_t>(index)] = static_cast<int>((earlyMs[index] * 1.13 + 1.1) * 1.0e-3 * sampleRate);
            earlyGainsLeft[static_cast<std::size_t>(index)] = earlyGain[index];
            earlyGainsRight[static_cast<std::size_t>(index)] = -earlyGain[5 - index];
        }
        predelay = static_cast<int>(9.0e-3 * sampleRate);
        reset();
    }
    void reset() noexcept
    {
        for (auto& line : lines)
            line.fill(0.0f);
        positions.fill(0);
        filterStates.fill(0.0);
        early.fill(0.0f);
        earlyPosition = 0;
        for (auto& diffuser : diffusers)
            diffuser.fill(0.0f);
        diffuserPosition = 0;
    }
    void process(double input, double& left, double& right) noexcept
    {
        early[static_cast<std::size_t>(earlyPosition)] = static_cast<float>(input);
        const auto earlyAt = [&] (int delay)
        {
            return static_cast<double>(early[static_cast<std::size_t>(
                (earlyPosition - delay) & (earlyCapacity - 1))]);
        };
        double earlyLeft = 0.0, earlyRight = 0.0;
        for (std::size_t tap = 0; tap < 6; ++tap)
        {
            earlyLeft += earlyGainsLeft[tap] * earlyAt(earlyTapsLeft[tap]);
            earlyRight += earlyGainsRight[tap] * earlyAt(earlyTapsRight[tap]);
        }
        double delayed = earlyAt(predelay);
        earlyPosition = (earlyPosition + 1) & (earlyCapacity - 1);
        constexpr double diffuserGains[diffuserCount] { 0.75, 0.75, 0.625, 0.625 };
        for (std::size_t index = 0; index < diffuserCount; ++index)
        {
            auto& line = diffusers[index];
            const double past = line[static_cast<std::size_t>(
                (diffuserPosition - diffuserLengths[index]) & (diffuserCapacity - 1))];
            const double fed = delayed + diffuserGains[index] * past;
            line[static_cast<std::size_t>(diffuserPosition)] = static_cast<float>(fed);
            delayed = past - diffuserGains[index] * fed;
        }
        diffuserPosition = (diffuserPosition + 1) & (diffuserCapacity - 1);

        std::array<double, lineCount> outputs {};
        double sum = 0.0;
        for (std::size_t index = 0; index < lineCount; ++index)
        {
            const int readPosition = positions[index] - lengths[index];
            const double value = lines[index][static_cast<std::size_t>(
                (readPosition + capacity) % capacity)];
            filterStates[index] = (1.0 - poles[index]) * value + poles[index] * filterStates[index];
            outputs[index] = gains[index] * filterStates[index];
            sum += outputs[index];
        }
        const double householder = 2.0 / lineCount * sum;
        double lateLeft = 0.0, lateRight = 0.0;
        for (std::size_t index = 0; index < lineCount; ++index)
        {
            const double feedback = outputs[index] - householder;
            lines[index][static_cast<std::size_t>(positions[index])]
                = static_cast<float>(feedback + 0.35 * delayed);
            positions[index] = (positions[index] + 1) % capacity;
            lateLeft += (index & 1u) ? -outputs[index] : outputs[index];
            lateRight += (index & 2u) ? -outputs[index] : outputs[index];
        }
        left = earlyLeft + 0.5 * lateLeft;
        right = earlyRight + 0.5 * lateRight;
    }
};

// The walls' mode damping. Below 1 kHz, paulownia's own Q across the grain,
// 60, the lower of the two Coaldrake measured (JASA 148 (2020) 3153, Table
// I; 140 along it): the wood's loss alone, as no koto measurement separates
// what its joints and its radiation add. Above 2 kHz, where the modes
// overlap, the median of Acustra's measured guitar bridge (g21,
// MeasuredBridgeData.h), whose Qs there span 5-36; the top strings' upper
// partials, which the silk's loss is fitted to, decay there. Until 0.3.0 the
// walls also took that bridge's lower quartile below 1 kHz, 23, as Aculyre's
// lyre does (chosen by ear for the lyre, Aculyre's Docs/decisions.md,
// 2026-10-28). D4, on the shell's breathing at 293 Hz, then died 2.3 times
// as fast as recorded, and dies 1.7 times as fast with 60
// (Docs/decisions.md, 2026-10-29).
double woodQ(double frequency) noexcept
{
    constexpr double anchorsHz[] { 300.0, 1000.0, 2000.0, 10000.0 };
    constexpr double anchorsQ[] { 60.0, 60.0, 10.0, 9.0 };
    if (frequency <= anchorsHz[0])
        return anchorsQ[0];
    for (int index = 1; index < 4; ++index)
        if (frequency <= anchorsHz[index])
        {
            const double t = std::log(frequency / anchorsHz[index - 1])
                           / std::log(anchorsHz[index] / anchorsHz[index - 1]);
            return anchorsQ[index - 1] * std::pow(anchorsQ[index] / anchorsQ[index - 1], t);
        }
    return anchorsQ[3];
}
// The air resonance: g21's air-mode group measures Q 16-21.
constexpr double airQ = 20.0;

// Eigen-decomposition of the symmetric n x n matrix a (row-major, setup
// only): Householder reduction to tridiagonal form and the QL method with
// implicit shifts (EISPACK's tred2 and tql2 as JAMA gives them). On return
// row j of a holds the unit eigenvector of values[j], ascending; e is n of
// scratch.
void symmetricEigen(double* a, int n, double* values, double* e) noexcept
{
    double* d = values;
    const auto at = [&] (int i, int j) -> double& { return a[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)]; };
    for (int j = 0; j < n; ++j)
        d[j] = at(n - 1, j);
    for (int i = n - 1; i > 0; --i)
    {
        double scale = 0.0, h = 0.0;
        for (int k = 0; k < i; ++k)
            scale += std::abs(d[k]);
        if (scale == 0.0)
        {
            e[i] = d[i - 1];
            for (int j = 0; j < i; ++j)
            {
                d[j] = at(i - 1, j);
                at(i, j) = 0.0;
                at(j, i) = 0.0;
            }
        }
        else
        {
            for (int k = 0; k < i; ++k)
            {
                d[k] /= scale;
                h += d[k] * d[k];
            }
            double f = d[i - 1];
            double g = f > 0.0 ? -std::sqrt(h) : std::sqrt(h);
            e[i] = scale * g;
            h -= f * g;
            d[i - 1] = f - g;
            for (int j = 0; j < i; ++j)
                e[j] = 0.0;
            for (int j = 0; j < i; ++j)
            {
                f = d[j];
                at(j, i) = f;
                g = e[j] + at(j, j) * f;
                for (int k = j + 1; k <= i - 1; ++k)
                {
                    g += at(k, j) * d[k];
                    e[k] += at(k, j) * f;
                }
                e[j] = g;
            }
            f = 0.0;
            for (int j = 0; j < i; ++j)
            {
                e[j] /= h;
                f += e[j] * d[j];
            }
            const double hh = f / (h + h);
            for (int j = 0; j < i; ++j)
                e[j] -= hh * d[j];
            for (int j = 0; j < i; ++j)
            {
                f = d[j];
                g = e[j];
                for (int k = j; k <= i - 1; ++k)
                    at(k, j) -= f * e[k] + g * d[k];
                d[j] = at(i - 1, j);
                at(i, j) = 0.0;
            }
        }
        d[i] = h;
    }
    for (int i = 0; i < n - 1; ++i)
    {
        at(n - 1, i) = at(i, i);
        at(i, i) = 1.0;
        const double h = d[i + 1];
        if (h != 0.0)
        {
            for (int k = 0; k <= i; ++k)
                d[k] = at(k, i + 1) / h;
            for (int j = 0; j <= i; ++j)
            {
                double g = 0.0;
                for (int k = 0; k <= i; ++k)
                    g += at(k, i + 1) * at(k, j);
                for (int k = 0; k <= i; ++k)
                    at(k, j) -= g * d[k];
            }
        }
        for (int k = 0; k <= i; ++k)
            at(k, i + 1) = 0.0;
    }
    for (int j = 0; j < n; ++j)
    {
        d[j] = at(n - 1, j);
        at(n - 1, j) = 0.0;
    }
    at(n - 1, n - 1) = 1.0;
    e[0] = 0.0;

    // The vectors are a's columns; QL works on them as rows, contiguous.
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            std::swap(at(i, j), at(j, i));
    for (int i = 1; i < n; ++i)
        e[i - 1] = e[i];
    e[n - 1] = 0.0;
    double f = 0.0, largest = 0.0;
    constexpr double epsilon = 2.220446049250313e-16;
    for (int l = 0; l < n; ++l)
    {
        largest = std::max(largest, std::abs(d[l]) + std::abs(e[l]));
        int m = l;
        while (m < n - 1 && std::abs(e[m]) > epsilon * largest)
            ++m;
        if (m > l)
        {
            for (int iteration = 0; iteration < 64 && std::abs(e[l]) > epsilon * largest; ++iteration)
            {
                double g = d[l];
                double p = (d[l + 1] - g) / (2.0 * e[l]);
                double r = std::hypot(p, 1.0);
                if (p < 0.0)
                    r = -r;
                d[l] = e[l] / (p + r);
                d[l + 1] = e[l] * (p + r);
                const double dl1 = d[l + 1];
                double h = g - d[l];
                for (int i = l + 2; i < n; ++i)
                    d[i] -= h;
                f += h;
                p = d[m];
                double c = 1.0, c2 = 1.0, c3 = 1.0, s = 0.0, s2 = 0.0;
                const double el1 = e[l + 1];
                for (int i = m - 1; i >= l; --i)
                {
                    c3 = c2;
                    c2 = c;
                    s2 = s;
                    g = c * e[i];
                    h = c * p;
                    r = std::hypot(p, e[i]);
                    e[i + 1] = s * r;
                    s = e[i] / r;
                    c = p / r;
                    p = c * d[i] - s * g;
                    d[i + 1] = h + s * (c * g + s * d[i]);
                    double* lower = &at(i, 0);
                    double* upper = &at(i + 1, 0);
                    for (int k = 0; k < n; ++k)
                    {
                        const double above = upper[k];
                        upper[k] = s * lower[k] + c * above;
                        lower[k] = c * lower[k] - s * above;
                    }
                }
                p = -s * s2 * c3 * el1 * e[l] / dl1;
                e[l] = s * p;
                d[l] = c * p;
            }
        }
        d[l] += f;
        e[l] = 0.0;
    }
    for (int i = 0; i < n - 1; ++i)
    {
        int k = i;
        for (int j = i + 1; j < n; ++j)
            if (d[j] < d[k])
                k = j;
        if (k != i)
        {
            std::swap(d[k], d[i]);
            for (int j = 0; j < n; ++j)
                std::swap(at(i, j), at(k, j));
        }
    }
}

// ---------------------------------------------------------------------------
// The body as the koto is built (BodySpec): the whole box as a beam, the top
// and the back between its walls, the air in its cavity and the ji on their
// feet, coupled in one eigenproblem (designBody).

constexpr int beamElements = 60;
constexpr int beamNodes = beamElements + 1;
constexpr int maxBeamModes = 16;
constexpr int maxWallModes = 280;
constexpr int airSegments = 36;
constexpr int holeCount = 2;
constexpr int maxBodyDofs = 3 * maxBeamModes + maxWallModes + (airSegments - 1) + holeCount
                          + static_cast<int>(barCoordinates);
// The radiating surface, sampled for the microphones: along the body, and
// across the top and the back, up each side.
constexpr int gridAlong = 120;
constexpr int gridAcross = 10;
constexpr int gridUp = 5;
constexpr int gridPoints = 2 * gridAlong * gridAcross + 2 * gridAlong * gridUp + holeCount;

// The box at one station along it, from a 1 mm grid over its cross-section:
// the top's outside an arch of the crown's rise over the two side walls, its
// inside the top's thickness below, and the back board across the bottom.
struct Section
{
    double width { 0.0 }, depth { 0.0 }, sideHeight { 0.0 };
    double area { 0.0 };      // the wood's, m^2
    double centroid { 0.0 };  // its height above the back, m
    double bendingVertical { 0.0 }, bendingLateral { 0.0 };  // EI, N m^2
    double inertiaVertical { 0.0 }, inertiaLateral { 0.0 };  // the area's second moments, m^4
    double shearVertical { 0.0 }, shearLateral { 0.0 };      // kappa G A, N
    double torsion { 0.0 };   // GJ, N m^2
    double cavity { 0.0 };    // the hollow's area, m^2
};

double topSurface(const Section& s, double crown, double y) noexcept
{
    const double u = 2.0 * y / s.width;
    return s.sideHeight + crown * (1.0 - u * u);
}

Section sectionAt(const BodySpec& body, double x, bool hollow) noexcept
{
    const WoodSpec& wood = body.wood;
    Section s;
    s.width = body.headWidth + (body.tailWidth - body.headWidth) * x / body.length;
    s.depth = body.endDepth + (body.middleDepth - body.endDepth) * std::sin(pi * x / body.length);
    const double crown = std::min(body.crown, 0.5 * s.depth);
    s.sideHeight = s.depth - crown;
    constexpr double step = 1.0e-3;
    const int across = static_cast<int>(s.width / step);
    const int up = static_cast<int>(s.depth / step) + 1;
    const double wall = 0.5 * s.width - body.sideThickness;
    double count = 0.0, z1 = 0.0, z2 = 0.0, y2 = 0.0, e0 = 0.0, ez1 = 0.0, ez2 = 0.0, ey2 = 0.0, hollowCells = 0.0;
    for (int i = 0; i < across; ++i)
    {
        const double y = -0.5 * s.width + (i + 0.5) * step;
        const double outside = topSurface(s, crown, y);
        const double inside = outside - body.topThickness;
        for (int k = 0; k < up; ++k)
        {
            const double z = (k + 0.5) * step;
            if (z >= outside)
                break;
            double modulus = wood.eL;
            if (hollow)
            {
                const bool inner = std::abs(y) < wall && z > body.backThickness && z < inside;
                if (inner)
                {
                    hollowCells += 1.0;
                    continue;
                }
                // The back board's grain runs across: along the body it is T.
                if (std::abs(y) < wall && z <= body.backThickness)
                    modulus = wood.eT;
            }
            count += 1.0;
            z1 += z;
            z2 += z * z;
            y2 += y * y;
            e0 += modulus;
            ez1 += modulus * z;
            ez2 += modulus * z * z;
            ey2 += modulus * y * y;
        }
    }
    const double cell = step * step;
    s.area = count * cell;
    s.centroid = count > 0.0 ? z1 / count : 0.5 * s.depth;
    const double c = s.centroid;
    s.bendingVertical = (ez2 - 2.0 * c * ez1 + c * c * e0) * cell;
    s.bendingLateral = ey2 * cell;
    s.inertiaVertical = (z2 - 2.0 * c * z1 + c * c * count) * cell;
    s.inertiaLateral = y2 * cell;
    s.cavity = hollowCells * cell;
    if (hollow)
    {
        // A thin-walled box: the side walls carry the vertical shear, the
        // top and the back the lateral; the twist is Bredt's, GJ = 4 A^2 /
        // (sum of s / G t) round the walls' mid-line. The side walls' plane
        // holds L and the log's radius, the top's and the back's L and its
        // tangent.
        const double side = s.sideHeight - 0.5 * body.backThickness;
        const double span = s.width - body.sideThickness;
        s.shearVertical = wood.gLR * 2.0 * body.sideThickness * (s.sideHeight - body.backThickness);
        s.shearLateral = wood.gLT * span * (body.topThickness + body.backThickness);
        const double enclosed = span * (side + 2.0 / 3.0 * crown);
        const double rise = 2.0 * crown / span;
        const double arc = span * (1.0 + 8.0 / 3.0 * rise * rise);
        const double path = arc / (wood.gLT * body.topThickness) + span / (wood.gLT * body.backThickness)
                          + 2.0 * side / (wood.gLR * body.sideThickness);
        s.torsion = 4.0 * enclosed * enclosed / path;
    }
    else
    {
        // The solid ends: a rectangle's shear (5/6) and twist (Saint-Venant's
        // 0.3 b h^3 for a section three or more times as wide as deep).
        s.shearVertical = 5.0 / 6.0 * wood.gLR * s.area;
        s.shearLateral = 5.0 / 6.0 * wood.gLT * s.area;
        s.torsion = wood.gLT * 0.3 * s.width * s.depth * s.depth * s.depth;
    }
    return s;
}

// ---- The walls' own modes: the box's cross-section as one shell. The top
// and the side walls are carved from one log and the back board is glued
// under them (Coaldrake, ICA 2019), so the section is a closed shell: the
// arched top, two side walls and the back, joined rigidly at their corners,
// each wall flexing against its neighbours rather than clamped by them.
// Finite strips (Cheung, Finite Strip Method in Structural Analysis, 1976):
// the section's mid-line in flat facets, each a strip along the cavity with
// membrane (linear across it) and bending (Hermite across it) displacement,
// orthotropic as its wood runs; along the cavity sin(m pi xi / Lc) for the
// motion in the section's plane and cos for the axial, the solid ends holding
// the section's shape. The section is uniform, the cavity's mean. The beam
// above carries the section moving as a whole, so each order's three rigid
// motions in the section's plane are pushed out of it by a mass-weighted
// penalty: the walls' modes carry none of the box's momentum, and the two
// meet only through the ji and the air. (Checked against the shell solved
// whole, which couples them: the level radiated per band moves by under
// 3 dB.) The strip is checked against the simply supported orthotropic
// plate (exact) and, with the penalty off, against the box's Euler bending.
constexpr int stripBack = 12;
constexpr int stripSide = 6;
constexpr int stripTop = 16;
constexpr int stripNodes = stripBack + 2 * stripSide + stripTop;
constexpr int stripDofs = 4 * stripNodes;  // per node: axial, across, up, rotation
constexpr int firstSideNode = stripBack;
constexpr int firstTopNode = stripBack + stripSide;
constexpr int secondSideNode = stripBack + stripSide + stripTop;
constexpr int maxWallOrder = 90;
constexpr int maxWallCandidates = 1400;
// Below it every wall mode is kept. Above it the modes are too dense to run
// one by one: of those that mostly move the top (the ones the strings drive),
// one in each run of consecutive modes is kept, scaled to carry the run's
// share of the top's motion, and radiating as the run does; the runs
// lengthen with frequency (wallModes).
constexpr double wallAllHz = 3000.0;
// Where the radiating grid samples each wall mode across the body: the top's
// and the back's gridAcross, each side's gridUp.
constexpr int wallSamples = 2 * gridAcross + 2 * gridUp;

// The section's mid-line: the back from the left corner, up the right side,
// back along the top's arch and down the left side. Facet e runs from node e
// to node e + 1 (the last closes on node 0); each wall's local normal points
// out of the box.
struct StripSection
{
    std::array<double, stripNodes> y {}, z {};
    double halfSpan { 0.0 };  // the side walls' mid-lines, +-
};

int facetWall(int e) noexcept  // 0 back, 1 side, 2 top
{
    if (e < firstSideNode)
        return 0;
    if (e < firstTopNode || e >= secondSideNode)
        return 1;
    return 2;
}

StripSection stripSection(const BodySpec& body) noexcept
{
    // The cavity's mean width and depth.
    double width = 0.0, depth = 0.0;
    constexpr int samples = 64;
    for (int i = 0; i < samples; ++i)
    {
        const double x = body.cavityStart + (i + 0.5) / samples * (body.cavityEnd - body.cavityStart);
        width += (body.headWidth + (body.tailWidth - body.headWidth) * x / body.length) / samples;
        depth += (body.endDepth + (body.middleDepth - body.endDepth) * std::sin(pi * x / body.length)) / samples;
    }
    const double crown = std::min(body.crown, 0.5 * depth);
    StripSection s;
    s.halfSpan = 0.5 * (width - body.sideThickness);
    const double bottom = 0.5 * body.backThickness;
    const auto top = [&] (double y)
    {
        const double u = 2.0 * y / width;
        return depth - crown + crown * (1.0 - u * u) - 0.5 * body.topThickness;
    };
    const double ys = s.halfSpan;
    for (int i = 0; i < stripBack; ++i)
    {
        s.y[static_cast<std::size_t>(i)] = -ys + 2.0 * ys * i / stripBack;
        s.z[static_cast<std::size_t>(i)] = bottom;
    }
    for (int i = 0; i < stripSide; ++i)
    {
        const double t = static_cast<double>(i) / stripSide;
        s.y[static_cast<std::size_t>(firstSideNode + i)] = ys;
        s.z[static_cast<std::size_t>(firstSideNode + i)] = bottom + (top(ys) - bottom) * t;
        s.y[static_cast<std::size_t>(secondSideNode + i)] = -ys;
        s.z[static_cast<std::size_t>(secondSideNode + i)] = top(-ys) - (top(-ys) - bottom) * t;
    }
    for (int i = 0; i < stripTop; ++i)
    {
        const double y = ys - 2.0 * ys * i / stripTop;
        s.y[static_cast<std::size_t>(firstTopNode + i)] = y;
        s.z[static_cast<std::size_t>(firstTopNode + i)] = top(y);
    }
    return s;
}

// A wall's membrane (A, N/m) and bending (D, N m) stiffness, x along the
// body and s across the wall.
struct WallStiffness
{
    double thickness;
    double a11, a22, a12, a66;
    double d11, d22, d12, d66;
};

WallStiffness wallStiffness(const BodySpec& body, int wall) noexcept
{
    const WoodSpec& wood = body.wood;
    // The top's grain runs along the body with the log's tangent across it;
    // the side walls' along the body with its radius up them (the log's axis
    // lies far below the top); the back board's across the body.
    double t = body.backThickness, ex = wood.eT, es = wood.eL, g = wood.gLT;
    if (wall == 2)
    {
        t = body.topThickness;
        ex = wood.eL;
        es = wood.eT;
    }
    else if (wall == 1)
    {
        t = body.sideThickness;
        ex = wood.eL;
        es = wood.eR;
        g = wood.gLR;
    }
    // nuLT is the larger ratio, strain across per strain along the grain.
    const double nuXs = ex >= es ? wood.nuLT : wood.nuLT * ex / es;
    const double nuSx = nuXs * es / ex;
    const double den = 1.0 - nuXs * nuSx;
    WallStiffness w { t, ex * t / den, es * t / den, nuSx * ex * t / den, g * t, 0.0, 0.0, 0.0, 0.0 };
    const double f = t * t / 12.0;
    w.d11 = w.a11 * f;
    w.d22 = w.a22 * f;
    w.d12 = w.a12 * f;
    w.d66 = w.a66 * f;
    return w;
}

// The strip's stiffness and mass per unit length along the body (the
// along-body integrals, Lc / 2 for both, are left out) for order m, in the
// section's nodal coordinates.
void stripMatrices(const BodySpec& body, const StripSection& s, double k, double* stiffness, double* mass) noexcept
{
    std::fill(stiffness, stiffness + stripDofs * stripDofs, 0.0);
    std::fill(mass, mass + stripDofs * stripDofs, 0.0);
    const double density = body.wood.density;
    for (int e = 0; e < stripNodes; ++e)
    {
        const int i = e, j = (e + 1) % stripNodes;
        const double dy = s.y[static_cast<std::size_t>(j)] - s.y[static_cast<std::size_t>(i)];
        const double dz = s.z[static_cast<std::size_t>(j)] - s.z[static_cast<std::size_t>(i)];
        const double b = std::sqrt(dy * dy + dz * dz), c = dy / b, sn = dz / b;
        const WallStiffness w = wallStiffness(body, facetWall(e));
        const double rho = density * w.thickness;
        // Across the strip: linear (axial u and tangential v), Hermite
        // (normal w and its slope).
        const double ml[2][2] { { b / 3.0, b / 6.0 }, { b / 6.0, b / 3.0 } };
        const double kl[2][2] { { 1.0 / b, -1.0 / b }, { -1.0 / b, 1.0 / b } };
        const double cl[2][2] { { -0.5, 0.5 }, { -0.5, 0.5 } };  // int N_i N_j'
        const double b2 = b * b;
        const double mh[4][4] { { 156.0, 22.0 * b, 54.0, -13.0 * b }, { 22.0 * b, 4.0 * b2, 13.0 * b, -3.0 * b2 },
                                { 54.0, 13.0 * b, 156.0, -22.0 * b }, { -13.0 * b, -3.0 * b2, -22.0 * b, 4.0 * b2 } };
        const double kh[4][4] { { 12.0, 6.0 * b, -12.0, 6.0 * b }, { 6.0 * b, 4.0 * b2, -6.0 * b, 2.0 * b2 },
                                { -12.0, -6.0 * b, 12.0, -6.0 * b }, { 6.0 * b, 2.0 * b2, -6.0 * b, 4.0 * b2 } };
        const double kg[4][4] { { 36.0, 3.0 * b, -36.0, 3.0 * b }, { 3.0 * b, 4.0 * b2, -3.0 * b, -b2 },
                                { -36.0, -3.0 * b, 36.0, -3.0 * b }, { 3.0 * b, -b2, -3.0 * b, 4.0 * b2 } };
        // Local dofs u1 v1 w1 w1' u2 v2 w2 w2'.
        double ke[8][8] {}, me[8][8] {};
        constexpr int u[2] { 0, 4 }, v[2] { 1, 5 }, wd[4] { 2, 3, 6, 7 };
        for (int p = 0; p < 2; ++p)
            for (int q = 0; q < 2; ++q)
            {
                ke[u[p]][u[q]] += w.a11 * k * k * ml[p][q] + w.a66 * kl[p][q];
                ke[v[p]][v[q]] += w.a22 * kl[p][q] + w.a66 * k * k * ml[p][q];
                const double uv = k * (-w.a12 * cl[p][q] + w.a66 * cl[q][p]);
                ke[u[p]][v[q]] += uv;
                ke[v[q]][u[p]] += uv;
                me[u[p]][u[q]] += rho * ml[p][q];
                me[v[p]][v[q]] += rho * ml[p][q];
            }
        // The D12 term's boundary part: int W W'' = -int W'W' + [W W'].
        double boundary[4][4] {};
        boundary[2][3] = 1.0;
        boundary[0][1] = -1.0;
        for (int p = 0; p < 4; ++p)
            for (int q = 0; q < 4; ++q)
            {
                ke[wd[p]][wd[q]] += w.d11 * k * k * k * k * b / 420.0 * mh[p][q] + w.d22 / (b * b2) * kh[p][q]
                                  + (4.0 * w.d66 + 2.0 * w.d12) * k * k / (30.0 * b) * kg[p][q]
                                  - w.d12 * k * k * (boundary[p][q] + boundary[q][p]);
                me[wd[p]][wd[q]] += rho * b / 420.0 * mh[p][q];
            }
        // To the nodes' axial, across (y), up (z) and rotation: u = U,
        // v = c Y + s Z, w = s Y - c Z (out of the box), w' = -rotation.
        double t[8][8] {};
        for (int o : { 0, 4 })
        {
            t[o][o] = 1.0;
            t[o + 1][o + 1] = c;
            t[o + 1][o + 2] = sn;
            t[o + 2][o + 1] = sn;
            t[o + 2][o + 2] = -c;
            t[o + 3][o + 3] = -1.0;
        }
        int index[8];
        for (int d = 0; d < 4; ++d)
        {
            index[d] = 4 * i + d;
            index[4 + d] = 4 * j + d;
        }
        for (int p = 0; p < 8; ++p)
            for (int q = 0; q < 8; ++q)
            {
                double ks = 0.0, ms = 0.0;
                for (int a = 0; a < 8; ++a)
                    for (int b8 = 0; b8 < 8; ++b8)
                    {
                        ks += t[a][p] * ke[a][b8] * t[b8][q];
                        ms += t[a][p] * me[a][b8] * t[b8][q];
                    }
                stiffness[index[p] * stripDofs + index[q]] += ks;
                mass[index[p] * stripDofs + index[q]] += ms;
            }
    }
}

// The walls' kept modes: each one's order along the cavity, frequency, and
// its shape at the section's nodes (across, up, rotation) per unit modal
// mass.
struct WallModes
{
    int count { 0 };
    std::array<int, maxWallModes> order {};
    std::array<double, maxWallModes> frequency {};
    std::array<double, maxWallModes> volume {};  // out of the box per unit length, m^2
    std::array<std::array<double, 3 * stripNodes>, maxWallModes> shape {};
    std::array<double, maxWallModes> radiationScale {};  // designBody's radiation, wallModes
};

struct WallCandidates
{
    int count { 0 };
    std::array<int, maxWallCandidates> order {};
    std::array<double, maxWallCandidates> frequency {}, topShare {};
    std::array<std::array<double, 3 * stripNodes>, maxWallCandidates> shape {};
};

// A wall mode's displacement on facet e at t (0-1 along it): across (y), up
// (z) and out of the box.
void wallDisplacement(const StripSection& s, const std::array<double, 3 * stripNodes>& shape, int e, double t,
                      double& across, double& up, double& out) noexcept
{
    const int i = e, j = (e + 1) % stripNodes;
    const double dy = s.y[static_cast<std::size_t>(j)] - s.y[static_cast<std::size_t>(i)];
    const double dz = s.z[static_cast<std::size_t>(j)] - s.z[static_cast<std::size_t>(i)];
    const double b = std::sqrt(dy * dy + dz * dz), c = dy / b, sn = dz / b;
    const auto node = [&] (int n, int d) { return shape[static_cast<std::size_t>(3 * n + d)]; };
    const double v1 = c * node(i, 0) + sn * node(i, 1), v2 = c * node(j, 0) + sn * node(j, 1);
    const double w1 = sn * node(i, 0) - c * node(i, 1), w2 = sn * node(j, 0) - c * node(j, 1);
    const double r1 = -node(i, 2), r2 = -node(j, 2);
    const double t2 = t * t, t3 = t2 * t;
    const double v = (1.0 - t) * v1 + t * v2;
    out = (1.0 - 3.0 * t2 + 2.0 * t3) * w1 + b * (t - 2.0 * t2 + t3) * r1 + (3.0 * t2 - 2.0 * t3) * w2
        + b * (t3 - t2) * r2;
    across = c * v + sn * out;
    up = sn * v - c * out;
}

// The top at y across the body (clamped to its span): facet and place on it.
void topFacet(const StripSection& s, double y, int& e, double& t) noexcept
{
    const double u = std::clamp((s.halfSpan - y) / (2.0 * s.halfSpan), 0.0, 1.0) * stripTop;
    const int k = std::min(static_cast<int>(u), stripTop - 1);
    e = firstTopNode + k;
    t = u - k;
}

// Solves K y = w^2 M y (M full, positive definite) for the n x n matrices in
// stiffness and mass: mass becomes its Cholesky factor L, stiffness L^-1 K
// L^-T and then its eigenvectors as rows (ascending, values in `values`),
// each of which the caller maps back by L^-T (backSubstitute).
void generalizedEigen(double* stiffness, double* mass, int n, double* values, double* scratch) noexcept
{
    const auto m = [&] (int i, int j) -> double& { return mass[i * n + j]; };
    const auto k = [&] (int i, int j) -> double& { return stiffness[i * n + j]; };
    for (int j = 0; j < n; ++j)
    {
        double d = m(j, j);
        for (int p = 0; p < j; ++p)
            d -= m(j, p) * m(j, p);
        m(j, j) = std::sqrt(std::max(d, 1.0e-300));
        for (int i = j + 1; i < n; ++i)
        {
            double sum = m(i, j);
            for (int p = 0; p < j; ++p)
                sum -= m(i, p) * m(j, p);
            m(i, j) = sum / m(j, j);
        }
    }
    // X = L^-1 K by columns, then L^-1 X^T: K symmetric, so the result is.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int col = 0; col < n; ++col)
        {
            for (int i = 0; i < n; ++i)
            {
                double sum = k(i, col);
                for (int p = 0; p < i; ++p)
                    sum -= m(i, p) * scratch[p];
                scratch[i] = sum / m(i, i);
            }
            for (int i = 0; i < n; ++i)
                k(i, col) = scratch[i];
        }
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                std::swap(k(i, j), k(j, i));
    }
    symmetricEigen(stiffness, n, values, scratch);
}

void backSubstitute(const double* factor, int n, double* vector) noexcept
{
    for (int i = n - 1; i >= 0; --i)
    {
        double sum = vector[i];
        for (int p = i + 1; p < n; ++p)
            sum -= factor[p * n + i] * vector[p];
        vector[i] = sum / factor[i * n + i];
    }
}

// One family of the box's modes as a beam, each shape at the nodes per unit
// modal mass.
struct BeamModes
{
    int count { 0 };
    std::array<double, maxBeamModes> frequency {};
    std::array<bool, maxBeamModes> rigid {};
    std::array<std::array<double, beamNodes>, maxBeamModes> shape {};
};

// Scratch for designBody: about 3.5 MB, held with the engine's state.
constexpr int archStations = 160;
constexpr int archSamples = 24;

struct BodyWork
{
    std::array<double, static_cast<std::size_t>(maxBodyDofs) * maxBodyDofs> matrix {};
    std::array<double, maxBodyDofs> mass {}, values {}, scratch {};
    std::array<std::array<double, maxBodyDofs>, airSegments> compression {};
    std::array<std::array<double, maxBodyDofs>, barCoordinates> base {};
    std::array<Section, beamElements> sections {};
    BeamModes vertical {}, lateral {}, twist {};
    // The walls: their section, kept modes and the candidates they are
    // chosen from, and one order's strip matrices.
    StripSection strip {};
    WallModes walls {};
    WallCandidates candidates {};
    std::array<double, stripDofs * stripDofs> stripStiffness {}, stripMass {};
    // Each beam mode at the grid's stations along the body; each wall order
    // along the cavity and each wall mode across the body at the grid.
    std::array<std::array<std::array<double, gridAlong>, maxBeamModes>, 3> beamAtGrid {};
    std::array<std::array<double, gridAlong>, maxWallOrder + 1> wallAlong {};
    std::array<std::array<double, wallSamples>, maxWallModes> wallAcross {};
    std::array<std::array<double, wallSamples>, maxWallOrder + 1> orderSum {};
    // The arch (designBody): each wall order's along integral against each
    // other's, and each wall mode's outward motion across the top.
    std::array<std::array<double, maxWallOrder + 1>, maxWallOrder + 1> archAlong {};
    std::array<std::array<double, archSamples>, maxWallModes> archOut {};
    std::array<double, gridPoints> area {}, displacement {};
    // At each microphone, then at each from the patch's image in the floor.
    std::array<std::array<double, gridPoints>, 4> distance {}, directivity {};
};

// The rigid box on its rests: 20-40 Hz, which Coaldrake found in every
// physical test of a koto (ICA 2019, Sec. 2.1; JASA 148 (2020) 3153).
constexpr double restHz = 30.0;
// Past about 1.5 kHz the box's cross-section no longer moves as one (its
// walls' own modes take over), so the beam keeps its modes below.
constexpr double beamLimitHz = 1500.0;

// Solves K y = w^2 M y for M diagonal (work.mass) and K in work.matrix (n x
// n), keeping the modes below beamLimitHz; every stride-th coordinate is a
// node's displacement. Rigid modes rest on restHz.
void keepBeamModes(BodyWork& work, int n, int stride, BeamModes& out) noexcept
{
    double* a = work.matrix.data();
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            a[i * n + j] /= std::sqrt(work.mass[static_cast<std::size_t>(i)] * work.mass[static_cast<std::size_t>(j)]);
    symmetricEigen(a, n, work.values.data(), work.scratch.data());
    out = {};
    int rigid = 0;
    for (int j = 0; j < n && out.count < maxBeamModes; ++j)
    {
        double frequency = std::sqrt(std::max(0.0, work.values[static_cast<std::size_t>(j)])) / (2.0 * pi);
        const bool isRigid = frequency < 5.0;
        if (isRigid)
            frequency = restHz * (1.0 + 0.25 * rigid++);
        if (frequency >= beamLimitHz)
            break;
        const auto k = static_cast<std::size_t>(out.count++);
        out.frequency[k] = frequency;
        out.rigid[k] = isRigid;
        for (int node = 0; node < beamNodes; ++node)
            out.shape[k][static_cast<std::size_t>(node)]
                = a[j * n + stride * node] / std::sqrt(work.mass[static_cast<std::size_t>(stride * node)]);
    }
}

// Bending, vertical or lateral: Timoshenko elements (displacement and
// rotation at each node; Friedman and Kosmatka's stiffness, Comput. Struct.
// 47 (1993) 473) with lumped translational and rotary mass, the solid ends'
// masses at the end nodes, free at both ends.
void bendingModes(const BodySpec& body, bool vertical, BodyWork& work, BeamModes& out) noexcept
{
    constexpr int n = 2 * beamNodes;
    const double l = body.length / beamElements;
    std::fill(work.matrix.begin(), work.matrix.begin() + n * n, 0.0);
    std::fill(work.mass.begin(), work.mass.begin() + n, 0.0);
    for (int e = 0; e < beamElements; ++e)
    {
        const Section& s = work.sections[static_cast<std::size_t>(e)];
        const double ei = vertical ? s.bendingVertical : s.bendingLateral;
        const double shear = vertical ? s.shearVertical : s.shearLateral;
        const double phi = 12.0 * ei / (shear * l * l);
        const double scale = ei / ((1.0 + phi) * l * l * l);
        const double ke[4][4] {
            { 12.0, 6.0 * l, -12.0, 6.0 * l },
            { 6.0 * l, (4.0 + phi) * l * l, -6.0 * l, (2.0 - phi) * l * l },
            { -12.0, -6.0 * l, 12.0, -6.0 * l },
            { 6.0 * l, (2.0 - phi) * l * l, -6.0 * l, (4.0 + phi) * l * l } };
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                work.matrix[static_cast<std::size_t>((2 * e + i) * n + 2 * e + j)] += scale * ke[i][j];
        const double translational = body.wood.density * s.area * l / 2.0;
        const double rotary = body.wood.density * (vertical ? s.inertiaVertical : s.inertiaLateral) * l / 2.0;
        for (int node = e; node <= e + 1; ++node)
        {
            work.mass[static_cast<std::size_t>(2 * node)] += translational;
            work.mass[static_cast<std::size_t>(2 * node + 1)] += rotary;
        }
    }
    work.mass[0] += body.headMass;
    work.mass[static_cast<std::size_t>(2 * beamElements)] += body.tailMass;
    keepBeamModes(work, n, 2, out);
}

// Twist about the box's axis: linear elements, GJ and the wall's polar
// inertia, each solid end as a block of its section.
void torsionModes(const BodySpec& body, BodyWork& work, BeamModes& out) noexcept
{
    constexpr int n = beamNodes;
    const double l = body.length / beamElements;
    std::fill(work.matrix.begin(), work.matrix.begin() + n * n, 0.0);
    std::fill(work.mass.begin(), work.mass.begin() + n, 0.0);
    for (int e = 0; e < beamElements; ++e)
    {
        const Section& s = work.sections[static_cast<std::size_t>(e)];
        const double k = s.torsion / l;
        work.matrix[static_cast<std::size_t>(e * n + e)] += k;
        work.matrix[static_cast<std::size_t>((e + 1) * n + e + 1)] += k;
        work.matrix[static_cast<std::size_t>(e * n + e + 1)] -= k;
        work.matrix[static_cast<std::size_t>((e + 1) * n + e)] -= k;
        const double polar = body.wood.density * (s.inertiaVertical + s.inertiaLateral) * l / 2.0;
        work.mass[static_cast<std::size_t>(e)] += polar;
        work.mass[static_cast<std::size_t>(e + 1)] += polar;
    }
    const Section& head = work.sections.front();
    const Section& tail = work.sections.back();
    work.mass[0] += body.headMass * (head.width * head.width + head.depth * head.depth) / 12.0;
    work.mass[static_cast<std::size_t>(beamElements)] += body.tailMass * (tail.width * tail.width + tail.depth * tail.depth) / 12.0;
    keepBeamModes(work, n, 1, out);
}

double beamAt(const BeamModes& modes, int k, double x, double length) noexcept
{
    const double u = std::clamp(x / length * beamElements, 0.0, static_cast<double>(beamElements));
    const int i = std::min(static_cast<int>(u), beamElements - 1);
    const double t = u - i;
    const auto& shape = modes.shape[static_cast<std::size_t>(k)];
    return (1.0 - t) * shape[static_cast<std::size_t>(i)] + t * shape[static_cast<std::size_t>(i + 1)];
}

// Designs the walls' modes below `limit` into work.walls (wallAllHz above).
void wallModes(const BodySpec& body, double limit, BodyWork& work) noexcept
{
    StripSection& s = work.strip;
    s = stripSection(body);
    const double length = body.cavityEnd - body.cavityStart;
    double* stiffness = work.stripStiffness.data();
    double* mass = work.stripMass.data();
    double* values = work.values.data();
    double* scratch = work.scratch.data();
    auto& candidates = work.candidates;
    candidates.count = 0;
    constexpr int n = stripDofs;
    // Far above the audio band: where the penalty puts the section's rigid
    // motions.
    constexpr double penalty = (2.0 * pi * 1.0e6) * (2.0 * pi * 1.0e6);
    for (int order = 1; order <= maxWallOrder; ++order)
    {
        const double k = order * pi / length;
        stripMatrices(body, s, k, stiffness, mass);
        // The section's rigid motions: across, up, and turning about its
        // centre of mass.
        double weight = 0.0, centre = 0.0;
        for (int node = 0; node < stripNodes; ++node)
        {
            weight += mass[(4 * node + 2) * n + 4 * node + 2];
            centre += mass[(4 * node + 2) * n + 4 * node + 2] * s.z[static_cast<std::size_t>(node)];
        }
        centre /= weight;
        std::array<std::array<double, stripDofs>, 3> rigid {}, massRigid {};
        for (int node = 0; node < stripNodes; ++node)
        {
            const auto i = static_cast<std::size_t>(4 * node);
            rigid[0][i + 1] = 1.0;
            rigid[1][i + 2] = 1.0;
            rigid[2][i + 1] = -(s.z[static_cast<std::size_t>(node)] - centre);
            rigid[2][i + 2] = s.y[static_cast<std::size_t>(node)];
            rigid[2][i + 3] = 1.0;
        }
        for (int r = 0; r < 3; ++r)
            for (int i = 0; i < n; ++i)
            {
                double sum = 0.0;
                for (int j = 0; j < n; ++j)
                    sum += mass[i * n + j] * rigid[static_cast<std::size_t>(r)][static_cast<std::size_t>(j)];
                massRigid[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)] = sum;
            }
        double g[3][3] {};
        for (int r = 0; r < 3; ++r)
            for (int q = 0; q < 3; ++q)
                for (int i = 0; i < n; ++i)
                    g[r][q] += rigid[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)]
                             * massRigid[static_cast<std::size_t>(q)][static_cast<std::size_t>(i)];
        const double det = g[0][0] * (g[1][1] * g[2][2] - g[1][2] * g[2][1]) - g[0][1] * (g[1][0] * g[2][2] - g[1][2] * g[2][0])
                         + g[0][2] * (g[1][0] * g[2][1] - g[1][1] * g[2][0]);
        double inverse[3][3];
        for (int r = 0; r < 3; ++r)
            for (int q = 0; q < 3; ++q)
            {
                const int r1 = (q + 1) % 3, r2 = (q + 2) % 3, q1 = (r + 1) % 3, q2 = (r + 2) % 3;
                inverse[r][q] = (g[r1][q1] * g[r2][q2] - g[r1][q2] * g[r2][q1]) / det;
            }
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
            {
                double sum = 0.0;
                for (int r = 0; r < 3; ++r)
                    for (int q = 0; q < 3; ++q)
                        sum += massRigid[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)] * inverse[r][q]
                             * massRigid[static_cast<std::size_t>(q)][static_cast<std::size_t>(j)];
                stiffness[i * n + j] += penalty * sum;
            }

        generalizedEigen(stiffness, mass, n, values, scratch);
        bool any = false;
        for (int q = 0; q < n; ++q)
        {
            const double frequency = std::sqrt(std::max(0.0, values[q])) / (2.0 * pi);
            if (frequency >= limit)
                break;
            if (frequency < 20.0)
                continue;
            any = true;
            // Back to the nodes, per unit modal mass over the cavity (its
            // along-body integral is Lc / 2), and each node's share of the
            // kinetic energy (M phi = L y).
            std::array<double, stripDofs> phi {}, energy {};
            for (int i = 0; i < n; ++i)
                phi[static_cast<std::size_t>(i)] = stiffness[q * n + i];
            for (int i = 0; i < n; ++i)
            {
                double sum = 0.0;
                for (int p = 0; p <= i; ++p)
                    sum += mass[i * n + p] * phi[static_cast<std::size_t>(p)];
                energy[static_cast<std::size_t>(i)] = sum;
            }
            backSubstitute(mass, n, phi.data());
            double total = 0.0, top = 0.0;
            for (int node = 0; node < stripNodes; ++node)
                for (int d = 1; d <= 2; ++d)
                {
                    const auto i = static_cast<std::size_t>(4 * node + d);
                    const double e = phi[i] * energy[i];
                    total += e;
                    if (node >= firstTopNode && node <= secondSideNode)
                        top += e;
                }
            const double share = total > 0.0 ? top / total : 0.0;
            if (frequency >= wallAllHz && share < 0.5)
                continue;
            int slot = candidates.count;
            if (candidates.count >= maxWallCandidates)
            {
                slot = 0;
                for (int c = 1; c < candidates.count; ++c)
                    if (candidates.frequency[static_cast<std::size_t>(c)] > candidates.frequency[static_cast<std::size_t>(slot)])
                        slot = c;
                if (candidates.frequency[static_cast<std::size_t>(slot)] <= frequency)
                    continue;
            }
            else
                ++candidates.count;
            const auto c = static_cast<std::size_t>(slot);
            candidates.order[c] = order;
            candidates.frequency[c] = frequency;
            candidates.topShare[c] = share;
            const double scale = std::sqrt(2.0 / length);
            for (int node = 0; node < stripNodes; ++node)
                for (int d = 0; d < 3; ++d)
                    candidates.shape[c][static_cast<std::size_t>(3 * node + d)] = scale * phi[static_cast<std::size_t>(4 * node + 1 + d)];
        }
        if (!any)
            break;
    }

    // Lowest first; every one below wallAllHz, then one of each run above.
    std::array<int, maxWallCandidates> sorted {};
    for (int c = 0; c < candidates.count; ++c)
        sorted[static_cast<std::size_t>(c)] = c;
    std::sort(sorted.begin(), sorted.begin() + candidates.count, [&] (int a, int b)
              { return candidates.frequency[static_cast<std::size_t>(a)] < candidates.frequency[static_cast<std::size_t>(b)]; });
    int low = 0;
    while (low < candidates.count && candidates.frequency[static_cast<std::size_t>(sorted[static_cast<std::size_t>(low)])] < wallAllHz)
        ++low;
    low = std::min(low, maxWallModes);
    const int high = candidates.count - low;
    const int room = maxWallModes - low;
    // As many runs as runs of equal length fitting the room make.
    const int run = high > 0 && room > 0 ? (high + room - 1) / room : 1;
    const int runs = room > 0 ? (high + run - 1) / run : 0;
    WallModes& out = work.walls;
    out = {};
    // A kept mode scaled by s to stand for a run of N carries the run's
    // motion at the ji, s^2 = N for alike modes, so the strings lose to it
    // what they lose to the run. Its radiation must not be scaled again: the
    // run's modes radiate with unrelated signs and add in power, N times one
    // mode's, where a mode s times as large both driven and radiating
    // radiates s^4 = N^2 times. So it radiates with its own unscaled shape.
    // Until 0.8.0 it did not, and the walls above 3 kHz radiated up to 10 dB
    // too much: against the same body with every wall mode to 12.5 kHz kept
    // (1467 modes), the demos' sixth octaves from 3.2 to 5.6 kHz sat 5-11 dB
    // high, and with this 1-4 dB.
    const auto keep = [&] (int c, double scale)
    {
        const auto k = static_cast<std::size_t>(out.count++);
        const auto from = static_cast<std::size_t>(c);
        out.radiationScale[k] = 1.0 / scale;
        out.order[k] = candidates.order[from];
        out.frequency[k] = candidates.frequency[from];
        double volume = 0.0;
        for (int i = 0; i < 3 * stripNodes; ++i)
            out.shape[k][static_cast<std::size_t>(i)] = scale * candidates.shape[from][static_cast<std::size_t>(i)];
        for (int e = 0; e < stripNodes; ++e)
        {
            const int i = e, j = (e + 1) % stripNodes;
            const double dy = s.y[static_cast<std::size_t>(j)] - s.y[static_cast<std::size_t>(i)];
            const double dz = s.z[static_cast<std::size_t>(j)] - s.z[static_cast<std::size_t>(i)];
            const double b = std::sqrt(dy * dy + dz * dz), cs = dy / b, sn = dz / b;
            const auto& shape = out.shape[k];
            const double w1 = sn * shape[static_cast<std::size_t>(3 * i)] - cs * shape[static_cast<std::size_t>(3 * i + 1)];
            const double w2 = sn * shape[static_cast<std::size_t>(3 * j)] - cs * shape[static_cast<std::size_t>(3 * j + 1)];
            volume += 0.5 * b * (w1 + w2)
                    + b * b / 12.0 * (shape[static_cast<std::size_t>(3 * j + 2)] - shape[static_cast<std::size_t>(3 * i + 2)]);
        }
        out.volume[k] = volume;
    };
    for (int i = 0; i < low; ++i)
        keep(sorted[static_cast<std::size_t>(i)], 1.0);
    // The runs lengthen in proportion to frequency: each candidate weighs
    // 1/f, and each run takes an equal share of the weight still left (at
    // least one candidate, the last run the rest). So the walls are finest
    // from 3 to 6 kHz, where the upper strings' partials are strong, and
    // coarsest toward 16 kHz. Until 0.9.0 the runs were of equal length.
    // Against the same body with every wall mode to 12.5 kHz kept (1467
    // modes), the demos' sixth octaves from 4.5 to 5.6 kHz then stood a
    // mean 5.1 dB over a line through their 2-11 kHz spectrum, where that
    // body's stand 2.6, and they stand 2.4 with these runs; from 5.6 to 11
    // kHz the demos' sixth octaves sit a mean 1.3 dB from that body's, where
    // they sat 2.4.
    double weightLeft = 0.0;
    for (int i = low; i < candidates.count; ++i)
        weightLeft += 1.0 / candidates.frequency[static_cast<std::size_t>(sorted[static_cast<std::size_t>(i)])];
    for (int r = 0, first = low; r < runs; ++r)
    {
        const int runsLeft = runs - r;
        int last = first;
        double weight = 0.0;
        if (runsLeft == 1)
            last = candidates.count;
        else
            while (last < candidates.count - (runsLeft - 1))
            {
                weight += 1.0 / candidates.frequency[static_cast<std::size_t>(sorted[static_cast<std::size_t>(last++)])];
                if (weight >= weightLeft / runsLeft)
                    break;
            }
        weightLeft -= weight;
        int best = sorted[static_cast<std::size_t>(first)];
        double shares = 0.0;
        for (int i = first; i < last; ++i)
        {
            const int c = sorted[static_cast<std::size_t>(i)];
            shares += candidates.topShare[static_cast<std::size_t>(c)];
            if (candidates.topShare[static_cast<std::size_t>(c)] > candidates.topShare[static_cast<std::size_t>(best)])
                best = c;
        }
        if (out.count < maxWallModes)
            keep(best, std::sqrt(shares / candidates.topShare[static_cast<std::size_t>(best)]));
        first = last;
    }
}

// How the parts lose energy, as each coupled mode's kinetic energy is shared
// among them (designBody). The box on its rests: estimated, a heavy damped
// support. The box bending and twisting: Coaldrake measured paulownia's Q
// along the grain at 140 and across it at 60 (JASA 148 (2020) 3153,
// Table I); estimated at 60 for the glued box with its back board, its
// joints and its rests. The ji on its feet: estimated, on the soft top.
constexpr double restQ = 8.0;
constexpr double boxQ = 60.0;
constexpr double bridgeQ = 30.0;

// Designs the body's coupled modes into `modes`, lowest first, and returns
// how many there are. A mode's participations are its displacement at each
// string's ji, the top of the ji: heave at the string's index, rock across
// the top at maxStrings + index.
int designBody(const InstrumentSpec& instrument, int stringCount, double fs, BodyWork& work,
               std::array<Mode, maxModes>& modes) noexcept
{
    const BodySpec& body = instrument.body;
    const double length = body.length;
    const double frequencyLimit = std::min(20000.0, 0.45 * fs);

    for (int e = 0; e < beamElements; ++e)
    {
        const double x = (e + 0.5) * length / beamElements;
        work.sections[static_cast<std::size_t>(e)] = sectionAt(body, x, x > body.cavityStart && x < body.cavityEnd);
    }
    bendingModes(body, true, work, work.vertical);
    bendingModes(body, false, work, work.lateral);
    torsionModes(body, work, work.twist);
    wallModes(body, std::min(16000.0, frequencyLimit), work);
    const WallModes& walls = work.walls;
    const StripSection& strip = work.strip;
    const double cavity = body.cavityEnd - body.cavityStart;
    const auto wallAlongAt = [&] (int order, double x)
    {
        const double u = (x - body.cavityStart) / cavity;
        return u > 0.0 && u < 1.0 ? std::sin(order * pi * u) : 0.0;
    };

    // The coordinates: the beam's modes and the walls' (unit modal mass),
    // the volumes of air passing between the cavity's segments and out of
    // each hole, and each ji's heave and rock.
    const BeamModes* const families[3] { &work.vertical, &work.lateral, &work.twist };
    const int lateralBase = work.vertical.count;
    const int twistBase = lateralBase + work.lateral.count;
    const int familyBase[3] { 0, lateralBase, twistBase };
    const int wallBase = twistBase + work.twist.count;
    const int columnBase = wallBase + walls.count;
    const int holeBase = columnBase + airSegments - 1;
    const int heaveBase = holeBase + holeCount;
    const int rockBase = heaveBase + stringCount;
    const int n = rockBase + stringCount;
    const auto at = [&] (int i, int j) -> double& { return work.matrix[static_cast<std::size_t>(i * n + j)]; };
    std::fill(work.matrix.begin(), work.matrix.begin() + n * n, 0.0);
    auto& mass = work.mass;
    for (int f = 0; f < 3; ++f)
        for (int j = 0; j < families[f]->count; ++j)
        {
            const double omega = 2.0 * pi * families[f]->frequency[static_cast<std::size_t>(j)];
            at(familyBase[f] + j, familyBase[f] + j) = omega * omega;
            mass[static_cast<std::size_t>(familyBase[f] + j)] = 1.0;
        }
    for (int j = 0; j < walls.count; ++j)
    {
        const double omega = 2.0 * pi * walls.frequency[static_cast<std::size_t>(j)];
        at(wallBase + j, wallBase + j) = omega * omega;
        mass[static_cast<std::size_t>(wallBase + j)] = 1.0;
    }

    // The top is arched along the body, its depth rising as sin(pi x / L)
    // (sectionAt), so it curves along it by 1/R = (middleDepth - endDepth)
    // (pi / L)^2 sin(pi x / L), 1 / 5.6 m in the middle. Moving out by w
    // stretches a shell curved so by w / R along it, and the box's bending
    // stretches the top by -lever w'' (lever: the top's mid-thickness over
    // the section's centroid). Its longitudinal strain energy, 1/2 E_L t
    // (-lever w'' + w / R)^2 over the top, couples the vertical bending to
    // the walls' modes and stiffens them by E_L t (w / R)^2: the box changes
    // its volume as it bends, which a straight box cannot, so its first
    // bending radiates as Coaldrake's acoustic camera found it does (ICA
    // 2019). The top's own give along the body is left out (it moves as the
    // box's section), so this is the coupling's upper bound. The strip
    // section's span stands for the top's width at every station, as it
    // does for the walls' own modes.
    {
        const double ea = body.wood.eL * body.topThickness;
        const double dy = 2.0 * strip.halfSpan / archSamples, dx = cavity / archStations;
        const double h = length / beamElements;
        // The top's lever is quadratic across it (topSurface), so each wall
        // mode needs only its outward motion's zeroth and second moments.
        std::array<double, maxWallModes> moment0 {}, moment2 {};
        for (int iy = 0; iy < archSamples; ++iy)
        {
            const double y = strip.halfSpan * (2.0 * (iy + 0.5) / archSamples - 1.0);
            int e;
            double t;
            topFacet(strip, y, e, t);
            for (int k = 0; k < walls.count; ++k)
            {
                double across, up, out;
                wallDisplacement(strip, walls.shape[static_cast<std::size_t>(k)], e, t, across, up, out);
                work.archOut[static_cast<std::size_t>(k)][static_cast<std::size_t>(iy)] = out;
                moment0[static_cast<std::size_t>(k)] += out * dy;
                moment2[static_cast<std::size_t>(k)] += y * y * out * dy;
            }
        }
        std::array<bool, maxWallOrder + 1> used {};
        for (int k = 0; k < walls.count; ++k)
            used[static_cast<std::size_t>(walls.order[static_cast<std::size_t>(k)])] = true;
        for (auto& row : work.archAlong)
            row.fill(0.0);
        for (int ix = 0; ix < archStations; ++ix)
        {
            const double x = body.cavityStart + (ix + 0.5) * dx;
            const double curvature = (body.middleDepth - body.endDepth) * (pi / length) * (pi / length)
                                   * std::sin(pi * x / length);
            const Section section = sectionAt(body, x, true);
            const double crown = std::min(body.crown, 0.5 * section.depth);
            // lever(y) = rise - crown (2y / width)^2, rise its value on the axis.
            const double rise = section.sideHeight + crown - 0.5 * body.topThickness - section.centroid;
            const double fall = 4.0 * crown / (section.width * section.width);
            for (int j = 0; j < work.vertical.count; ++j)
            {
                const double bend = (beamAt(work.vertical, j, x + h, length) - 2.0 * beamAt(work.vertical, j, x, length)
                                     + beamAt(work.vertical, j, x - h, length)) / (h * h);
                for (int k = 0; k < walls.count; ++k)
                {
                    const auto kk = static_cast<std::size_t>(k);
                    const double along = wallAlongAt(walls.order[kk], x);
                    const double coupling = -ea * bend * curvature * along
                                          * (rise * moment0[kk] - fall * moment2[kk]) * dx;
                    at(j, wallBase + k) += coupling;
                    at(wallBase + k, j) += coupling;
                }
            }
            for (int a = 1; a <= maxWallOrder; ++a)
            {
                if (!used[static_cast<std::size_t>(a)])
                    continue;
                for (int b = a; b <= maxWallOrder; ++b)
                    if (used[static_cast<std::size_t>(b)])
                        work.archAlong[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]
                            += curvature * curvature * wallAlongAt(a, x) * wallAlongAt(b, x) * dx;
            }
        }
        for (int k = 0; k < walls.count; ++k)
            for (int l = k; l < walls.count; ++l)
            {
                const int a = std::min(walls.order[static_cast<std::size_t>(k)], walls.order[static_cast<std::size_t>(l)]);
                const int b = std::max(walls.order[static_cast<std::size_t>(k)], walls.order[static_cast<std::size_t>(l)]);
                const double along = work.archAlong[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
                if (along == 0.0)
                    continue;
                double across = 0.0;
                for (int iy = 0; iy < archSamples; ++iy)
                    across += work.archOut[static_cast<std::size_t>(k)][static_cast<std::size_t>(iy)]
                            * work.archOut[static_cast<std::size_t>(l)][static_cast<std::size_t>(iy)];
                const double stiffness = ea * along * across * dy;
                at(wallBase + k, wallBase + l) += stiffness;
                if (l != k)
                    at(wallBase + l, wallBase + k) += stiffness;
            }
    }

    // The air: a chain of segments along the cavity, each a compliance
    // rho c^2 / V, the air between neighbours a mass rho dx / S, closed at
    // both ends; each hole a plug of air rho l / S, l the back's thickness
    // with the end corrections of a hole flanged on both sides (1.7 r). A
    // segment is compressed by the air it gains and by its walls moving in: Christensen and Vistisen's Helmholtz resonator on a
    // flexible wall (JASA 68(3) 1980) along a long cavity.
    const double segment = (body.cavityEnd - body.cavityStart) / airSegments;
    std::array<double, airSegments> compliance {};
    for (int i = 0; i < airSegments; ++i)
    {
        const double x = body.cavityStart + (i + 0.5) * segment;
        compliance[static_cast<std::size_t>(i)] = airDensity * soundSpeed * soundSpeed
                                                / (sectionAt(body, x, true).cavity * segment);
    }
    for (int i = 0; i + 1 < airSegments; ++i)
    {
        const double face = sectionAt(body, body.cavityStart + (i + 1) * segment, true).cavity;
        mass[static_cast<std::size_t>(columnBase + i)] = airDensity * segment / face;
    }
    for (int h = 0; h < holeCount; ++h)
    {
        const SoundHole& hole = body.holes[static_cast<std::size_t>(h)];
        mass[static_cast<std::size_t>(holeBase + h)] = airDensity * (body.backThickness + 1.7 * hole.radius)
                                                     / (pi * hole.radius * hole.radius);
    }
    for (auto& row : work.compression)
        std::fill(row.begin(), row.begin() + n, 0.0);
    for (int i = 0; i + 1 < airSegments; ++i)
    {
        work.compression[static_cast<std::size_t>(i)][static_cast<std::size_t>(columnBase + i)] -= 1.0;
        work.compression[static_cast<std::size_t>(i + 1)][static_cast<std::size_t>(columnBase + i)] += 1.0;
    }
    for (int h = 0; h < holeCount; ++h)
    {
        const int i = std::clamp(static_cast<int>((body.holes[static_cast<std::size_t>(h)].position - body.cavityStart)
                                                  / segment), 0, airSegments - 1);
        work.compression[static_cast<std::size_t>(i)][static_cast<std::size_t>(holeBase + h)] -= 1.0;
    }
    // A wall mode's volume over each segment, moving out of the box taking
    // air from it.
    const auto wallVolume = [&] (int j, double from, double to)
    {
        const int m = walls.order[static_cast<std::size_t>(j)];
        return walls.volume[static_cast<std::size_t>(j)] * cavity / (m * pi)
             * (std::cos(m * pi * from / cavity) - std::cos(m * pi * to / cavity));
    };
    for (int j = 0; j < walls.count; ++j)
        for (int i = 0; i < airSegments; ++i)
            work.compression[static_cast<std::size_t>(i)][static_cast<std::size_t>(wallBase + j)]
                -= wallVolume(j, i * segment, (i + 1) * segment);
    for (int p = wallBase; p < heaveBase; ++p)
        for (int q = p; q < heaveBase; ++q)
        {
            double sum = 0.0;
            for (int i = 0; i < airSegments; ++i)
                sum += compliance[static_cast<std::size_t>(i)] * work.compression[static_cast<std::size_t>(i)][static_cast<std::size_t>(p)]
                     * work.compression[static_cast<std::size_t>(i)][static_cast<std::size_t>(q)];
            at(p, q) += sum;
            if (q != p)
                at(q, p) += sum;
        }

    // Each ji: its top heaves on its feet's stiffness over the top's mean
    // heave under its two feet (the box's bending and twist, the walls' own
    // modes), and rocks across the top over the base's motion at the
    // string's height: the box's sway, its twist about its axis, the top
    // moving across under it and the ji tilting with the top's slope between
    // its feet.
    const double heaveMass = body.bridgeMass, rockMass = body.bridgeMass / 3.0;
    const double heaveStiffness = heaveMass * std::pow(2.0 * pi * body.bridgeHeaveHz, 2.0);
    const double rockStiffness = rockMass * std::pow(2.0 * pi * body.bridgeRockHz, 2.0);
    for (int index = 0; index < stringCount; ++index)
    {
        const auto i = static_cast<std::size_t>(index);
        const double x = body.ryukaku + instrument.strings[i].length;
        const double y = (index - 0.5 * (stringCount - 1)) * body.stringSpacing;
        const Section section = sectionAt(body, x, x > body.cavityStart && x < body.cavityEnd);
        const double crown = std::min(body.crown, 0.5 * section.depth);
        const double lever = topSurface(section, crown, y) - section.centroid + body.bridgeHeight;
        auto& heave = work.base[i];
        auto& rock = work.base[maxStrings + i];
        std::fill(heave.begin(), heave.begin() + n, 0.0);
        std::fill(rock.begin(), rock.begin() + n, 0.0);
        for (int j = 0; j < work.vertical.count; ++j)
            heave[static_cast<std::size_t>(j)] = beamAt(work.vertical, j, x, length);
        for (int j = 0; j < work.lateral.count; ++j)
            rock[static_cast<std::size_t>(lateralBase + j)] = beamAt(work.lateral, j, x, length);
        for (int j = 0; j < work.twist.count; ++j)
        {
            const double twist = beamAt(work.twist, j, x, length);
            heave[static_cast<std::size_t>(twistBase + j)] = twist * y;
            rock[static_cast<std::size_t>(twistBase + j)] = -twist * lever;
        }
        const double feet = 0.5 * body.bridgeFeet;
        for (int j = 0; j < walls.count; ++j)
        {
            const double along = wallAlongAt(walls.order[static_cast<std::size_t>(j)], x);
            if (along == 0.0)
                continue;
            const auto& shape = walls.shape[static_cast<std::size_t>(j)];
            double across[3], up[3], out;
            const double place[3] { y, y + feet, y - feet };
            for (int f = 0; f < 3; ++f)
            {
                int e;
                double t;
                topFacet(strip, place[f], e, t);
                wallDisplacement(strip, shape, e, t, across[f], up[f], out);
            }
            heave[static_cast<std::size_t>(wallBase + j)] = along * 0.5 * (up[1] + up[2]);
            rock[static_cast<std::size_t>(wallBase + j)] = along * (across[0] - body.bridgeHeight * (up[1] - up[2]) / body.bridgeFeet);
        }
        mass[static_cast<std::size_t>(heaveBase + index)] = heaveMass;
        mass[static_cast<std::size_t>(rockBase + index)] = rockMass;
        // 1/2 k (q_ji - row . q)^2 for each.
        for (int pass = 0; pass < 2; ++pass)
        {
            const auto& row = pass == 0 ? heave : rock;
            const double k = pass == 0 ? heaveStiffness : rockStiffness;
            const int own = (pass == 0 ? heaveBase : rockBase) + index;
            at(own, own) += k;
            for (int p = 0; p < columnBase; ++p)
            {
                const double rp = row[static_cast<std::size_t>(p)];
                if (rp == 0.0)
                    continue;
                at(own, p) -= k * rp;
                at(p, own) -= k * rp;
                for (int q = 0; q < columnBase; ++q)
                    at(p, q) += k * rp * row[static_cast<std::size_t>(q)];
            }
        }
    }

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            at(i, j) /= std::sqrt(mass[static_cast<std::size_t>(i)] * mass[static_cast<std::size_t>(j)]);
    symmetricEigen(work.matrix.data(), n, work.values.data(), work.scratch.data());

    // ---- Radiation: each mode's normal velocity over the box's surface,
    // summed at each microphone as free-field monopoles, rho / (4 pi r)
    // times each patch's volume acceleration, with its delay at the mode's
    // own frequency (Kirchhoff's approximation with no baffle and no shadow),
    // and weighed by the cardioid's (1 + cos a) / 2 toward the patch. The top
    // and the back move with the box's bending and twist and with their own
    // modes, the sides with its sway and twist, and each hole breathes at a
    // point. The delays are taken from the pair's distance, so a mode keeps
    // both its phase between the microphones and its phase across the body.
    // The koto lies on the floor, which reflects as a rigid plane: every
    // patch is heard again from its image under the floor (an image source
    // of the same volume acceleration). The back and the holes face the
    // floor a few centimetres above it, so below a few hundred hertz their
    // images arrive in phase and double them, while the top's comb with its
    // image starts near 1 kHz. The floor's air load on the holes, which
    // would lower the air modes, is left out: they stay fitted to
    // Coaldrake's 85 and 157 Hz.
    const auto clearance = [&] (double x)
    {
        return body.headClearance + (body.tailClearance - body.headClearance) * std::clamp(x / length, 0.0, 1.0);
    };
    const double station = length / gridAlong;
    const double gridWidth = body.headWidth + (body.tailWidth - body.headWidth) * 0.5;
    // x runs from the head, y across the top toward the player, z up from
    // the back; the pair faces the player across the koto.
    const double middleX = 0.5 * length, middleZ = body.middleDepth;
    std::array<std::array<double, 3>, 2> microphone {}, aim {};
    for (int mic = 0; mic < 2; ++mic)
    {
        const double side = mic == 0 ? -1.0 : 1.0;  // the left one toward the head
        microphone[static_cast<std::size_t>(mic)] = { middleX + 0.5 * side * pairSpacing,
                                                      -pairDistance * std::cos(pairElevation),
                                                      middleZ + pairDistance * std::sin(pairElevation) };
        // Toward the middle of the top, turned pairAngle toward its own side.
        const double turn = -side * pairAngle;
        aim[static_cast<std::size_t>(mic)] = { -std::cos(pairElevation) * std::sin(turn),
                                               std::cos(pairElevation) * std::cos(turn),
                                               -std::sin(pairElevation) };
    }
    const auto hear = [&] (int p, double x, double y, double z)
    {
        for (int path = 0; path < 4; ++path)
        {
            const int mic = path % 2;
            const double source = path < 2 ? z : -2.0 * clearance(x) - z;
            const auto& at = microphone[static_cast<std::size_t>(mic)];
            const auto& toward = aim[static_cast<std::size_t>(mic)];
            const double dx = x - at[0], dy = y - at[1], dz = source - at[2];
            const double r = std::sqrt(dx * dx + dy * dy + dz * dz);
            work.distance[static_cast<std::size_t>(path)][static_cast<std::size_t>(p)] = r;
            work.directivity[static_cast<std::size_t>(path)][static_cast<std::size_t>(p)]
                = 0.5 * (1.0 + (dx * toward[0] + dy * toward[1] + dz * toward[2]) / r);
        }
    };
    std::array<double, gridAlong> centroid {}, sideHeight {};
    for (int ix = 0; ix < gridAlong; ++ix)
    {
        const double x = (ix + 0.5) * station;
        const Section section = sectionAt(body, x, x > body.cavityStart && x < body.cavityEnd);
        const double crown = std::min(body.crown, 0.5 * section.depth);
        centroid[static_cast<std::size_t>(ix)] = section.centroid;
        sideHeight[static_cast<std::size_t>(ix)] = section.sideHeight;
        for (int f = 0; f < 3; ++f)
            for (int j = 0; j < families[f]->count; ++j)
                work.beamAtGrid[static_cast<std::size_t>(f)][static_cast<std::size_t>(j)][static_cast<std::size_t>(ix)]
                    = beamAt(*families[f], j, x, length);
        for (int m = 1; m <= maxWallOrder; ++m)
            work.wallAlong[static_cast<std::size_t>(m)][static_cast<std::size_t>(ix)] = wallAlongAt(m, x);
        const auto place = [&] (int p, double y, double z, double area)
        {
            work.area[static_cast<std::size_t>(p)] = area;
            hear(p, x, y, z);
        };
        for (int iy = 0; iy < gridAcross; ++iy)
        {
            const double y = gridWidth * ((iy + 0.5) / gridAcross - 0.5);
            const double area = station * gridWidth / gridAcross;
            place(ix * gridAcross + iy, y, topSurface(section, crown, y), area);
            place(gridAlong * gridAcross + ix * gridAcross + iy, y, 0.0, area);
        }
        for (int iz = 0; iz < gridUp; ++iz)
        {
            const double z = (iz + 0.5) / gridUp * section.sideHeight;
            const double area = station * section.sideHeight / gridUp;
            place(2 * gridAlong * gridAcross + ix * gridUp + iz, 0.5 * section.width, z, area);
            place(2 * gridAlong * gridAcross + gridAlong * gridUp + ix * gridUp + iz, -0.5 * section.width, z, area);
        }
    }
    // Each wall mode out of the box at the grid's places across it: the top
    // and the back by their share of the span, each side by its share of
    // the height.
    for (int j = 0; j < walls.count; ++j)
    {
        const auto& shape = walls.shape[static_cast<std::size_t>(j)];
        auto& across = work.wallAcross[static_cast<std::size_t>(j)];
        double y, z, out;
        for (int iy = 0; iy < gridAcross; ++iy)
        {
            const double share = (iy + 0.5) / gridAcross;
            int e;
            double t;
            topFacet(strip, strip.halfSpan * (2.0 * share - 1.0), e, t);
            wallDisplacement(strip, shape, e, t, y, z, out);
            across[static_cast<std::size_t>(iy)] = out;
            const double u = share * stripBack;
            const int b = std::min(static_cast<int>(u), stripBack - 1);
            wallDisplacement(strip, shape, b, u - b, y, z, out);
            across[static_cast<std::size_t>(gridAcross + iy)] = out;
        }
        for (int iz = 0; iz < gridUp; ++iz)
        {
            const double u = (iz + 0.5) / gridUp * stripSide;
            const int k = std::min(static_cast<int>(u), stripSide - 1);
            wallDisplacement(strip, shape, firstSideNode + k, u - k, y, z, out);
            across[static_cast<std::size_t>(2 * gridAcross + iz)] = out;
            const double d = stripSide - u;  // the left side runs down
            const int kd = std::min(static_cast<int>(d), stripSide - 1);
            wallDisplacement(strip, shape, secondSideNode + kd, d - kd, y, z, out);
            across[static_cast<std::size_t>(2 * gridAcross + gridUp + iz)] = out;
        }
    }
    constexpr int holePoint = gridPoints - holeCount;
    for (int h = 0; h < holeCount; ++h)
    {
        const double x = body.holes[static_cast<std::size_t>(h)].position;
        work.area[static_cast<std::size_t>(holePoint + h)] = 1.0;
        hear(holePoint + h, x, 0.0, 0.0);
    }

    int count = 0;
    for (int j = 0; j < n && count < maxModes; ++j)
    {
        const double frequency = std::sqrt(std::max(0.0, work.values[static_cast<std::size_t>(j)])) / (2.0 * pi);
        // The air flowing in at one hole and out at the other has no
        // stiffness: a mode at 0 Hz that nothing drives.
        if (!(frequency > 20.0) || frequency >= frequencyLimit)
            continue;
        const double* y = &work.matrix[static_cast<std::size_t>(j * n)];
        const auto psi = [&] (int i) { return y[i] / std::sqrt(mass[static_cast<std::size_t>(i)]); };
        double rest = 0.0, box = 0.0, wall = 0.0, air = 0.0, bridge = 0.0;
        for (int f = 0; f < 3; ++f)
            for (int k = 0; k < families[f]->count; ++k)
                (families[f]->rigid[static_cast<std::size_t>(k)] ? rest : box) += y[familyBase[f] + k] * y[familyBase[f] + k];
        for (int i = wallBase; i < columnBase; ++i)
            wall += y[i] * y[i];
        for (int i = columnBase; i < heaveBase; ++i)
            air += y[i] * y[i];
        for (int i = heaveBase; i < n; ++i)
            bridge += y[i] * y[i];
        Mode& mode = modes[static_cast<std::size_t>(count++)];
        mode = {};
        mode.frequency = frequency;
        mode.q = 1.0 / (rest / restQ + box / boxQ + wall / woodQ(frequency) + air / airQ + bridge / bridgeQ);
        mode.airShare = air;
        mode.bodyShare = rest + box;
        mode.wallShare = wall;
        mode.bridgeShare = bridge;
        for (int index = 0; index < stringCount; ++index)
        {
            mode.participation[static_cast<std::size_t>(index)] = psi(heaveBase + index);
            mode.participation[maxStrings + static_cast<std::size_t>(index)] = psi(rockBase + index);
        }

        // The surface's outward displacement per unit of this mode.
        std::array<std::array<double, gridAlong>, 3> beam {};
        for (int f = 0; f < 3; ++f)
            for (int k = 0; k < families[f]->count; ++k)
            {
                const double weight = psi(familyBase[f] + k);
                const auto& shape = work.beamAtGrid[static_cast<std::size_t>(f)][static_cast<std::size_t>(k)];
                for (int ix = 0; ix < gridAlong; ++ix)
                    beam[static_cast<std::size_t>(f)][static_cast<std::size_t>(ix)] += weight * shape[static_cast<std::size_t>(ix)];
            }
        const auto& heaving = beam[0];
        const auto& swaying = beam[1];
        const auto& twisting = beam[2];
        auto& u = work.displacement;
        double monopole = 0.0;
        for (int ix = 0; ix < gridAlong; ++ix)
        {
            for (int iy = 0; iy < gridAcross; ++iy)
            {
                const double yy = gridWidth * ((iy + 0.5) / gridAcross - 0.5);
                const double up = heaving[static_cast<std::size_t>(ix)] + twisting[static_cast<std::size_t>(ix)] * yy;
                u[static_cast<std::size_t>(ix * gridAcross + iy)] = up;
                u[static_cast<std::size_t>(gridAlong * gridAcross + ix * gridAcross + iy)] = -up;  // the back's outward is down
            }
            for (int iz = 0; iz < gridUp; ++iz)
            {
                const double z = (iz + 0.5) / gridUp * sideHeight[static_cast<std::size_t>(ix)] - centroid[static_cast<std::size_t>(ix)];
                const double outward = swaying[static_cast<std::size_t>(ix)] - twisting[static_cast<std::size_t>(ix)] * z;
                u[static_cast<std::size_t>(2 * gridAlong * gridAcross + ix * gridUp + iz)] = outward;
                u[static_cast<std::size_t>(2 * gridAlong * gridAcross + gridAlong * gridUp + ix * gridUp + iz)] = -outward;
            }
        }
        // The walls' modes, summed by order along the cavity.
        std::array<bool, maxWallOrder + 1> used {};
        for (int j = 0; j < walls.count; ++j)
        {
            const auto order = static_cast<std::size_t>(walls.order[static_cast<std::size_t>(j)]);
            if (!used[order])
            {
                used[order] = true;
                work.orderSum[order].fill(0.0);
            }
            // A run's kept mode radiates unscaled (wallModes).
            const double weight = psi(wallBase + j) * walls.radiationScale[static_cast<std::size_t>(j)];
            if (order % 2 == 1)
                monopole += weight * walls.volume[static_cast<std::size_t>(j)] * 2.0 * cavity / (static_cast<double>(order) * pi);
            const auto& across = work.wallAcross[static_cast<std::size_t>(j)];
            for (int a = 0; a < wallSamples; ++a)
                work.orderSum[order][static_cast<std::size_t>(a)] += weight * across[static_cast<std::size_t>(a)];
        }
        for (int order = 1; order <= maxWallOrder; ++order)
        {
            if (!used[static_cast<std::size_t>(order)])
                continue;
            const auto& along = work.wallAlong[static_cast<std::size_t>(order)];
            const auto& sum = work.orderSum[static_cast<std::size_t>(order)];
            for (int ix = 0; ix < gridAlong; ++ix)
            {
                const double a = along[static_cast<std::size_t>(ix)];
                if (a == 0.0)
                    continue;
                for (int iy = 0; iy < gridAcross; ++iy)
                {
                    u[static_cast<std::size_t>(ix * gridAcross + iy)] += a * sum[static_cast<std::size_t>(iy)];
                    u[static_cast<std::size_t>(gridAlong * gridAcross + ix * gridAcross + iy)] += a * sum[static_cast<std::size_t>(gridAcross + iy)];
                }
                for (int iz = 0; iz < gridUp; ++iz)
                {
                    u[static_cast<std::size_t>(2 * gridAlong * gridAcross + ix * gridUp + iz)] += a * sum[static_cast<std::size_t>(2 * gridAcross + iz)];
                    u[static_cast<std::size_t>(2 * gridAlong * gridAcross + gridAlong * gridUp + ix * gridUp + iz)]
                        += a * sum[static_cast<std::size_t>(2 * gridAcross + gridUp + iz)];
                }
            }
        }
        for (int h = 0; h < holeCount; ++h)
        {
            u[static_cast<std::size_t>(holePoint + h)] = psi(holeBase + h);
            monopole += psi(holeBase + h);
        }
        const double wavenumber = 2.0 * pi * frequency / soundSpeed;
        double radiation[2][2] {};
        for (int mic = 0; mic < 2; ++mic)
        {
            double re = 0.0, im = 0.0;
            for (int path = mic; path < 4; path += 2)
                for (int p = 0; p < gridPoints; ++p)
                {
                    const double r = work.distance[static_cast<std::size_t>(path)][static_cast<std::size_t>(p)];
                    const double weight = u[static_cast<std::size_t>(p)] * work.area[static_cast<std::size_t>(p)]
                                        * work.directivity[static_cast<std::size_t>(path)][static_cast<std::size_t>(p)]
                                        * pairDistance / r;
                    const double phase = wavenumber * (r - pairDistance);
                    re += weight * std::cos(phase);
                    im -= weight * std::sin(phase);
                }
            radiation[mic][0] = re;
            radiation[mic][1] = im;
        }
        mode.monopole = monopole;
        mode.radiationLeft = radiation[0][0];
        mode.radiationLeftQuadrature = radiation[0][1];
        mode.radiationRight = radiation[1][0];
        mode.radiationRightQuadrature = radiation[1][1];
    }
    return count;
}

} // namespace

struct AcukotoEngine::State
{
    InstrumentSpec instrument {};

    // A string's pitch with its lever down or up: the lever shortens the
    // speaking length a semitone's worth at the same tension.
    struct PitchDesign
    {
        std::array<LoopDesign, 2> planes {};  // normal, parallel
        double length { 0.0 };
        double inharmonicity { 0.0 };
        double intrinsicT60 { 0.0 };
        double glidePerEnergy { 0.0 };
        int roundTrip { 1 };  // samples, the nearest whole number
    };

    struct StringVoice
    {
        Loop normal {};
        Loop parallel {};
        std::array<PitchDesign, 2> pitch {};  // lever down, lever up
        // The rest of the string, from the ji on to the unkaku: plucked
        // never, it rings with the ji's motion, and the left hand pressing
        // there for oshide stops it.
        Loop leftNormal {};
        Loop leftParallel {};
        std::array<LoopDesign, 2> left {};
        double leftLength { 0.0 };
        // The sounding pitch (pitch[raised ? 1 : 0]).
        double frequency { 0.0 };
        double length { 0.0 };
        double inharmonicity { 0.0 };
        double intrinsicT60 { 0.0 };
        // Delta f / f per unit of the loop's mean squared wave velocity.
        double glidePerEnergy { 0.0 };
        // The last round trip's squared wave velocities, both planes, and
        // their sum: `energy` is their mean. A mean over exactly one period
        // holds no ripple at the partials' frequencies; a one-pole of that
        // time constant left a ripple at twice the pitch that, through the
        // glide, modulated the line at 2 f0 and pumped the fundamental (a
        // hard-plucked steel G3 kept its energy for seconds). A pluck enters
        // its wave's squares at once: the string is stretched from the
        // moment it is released, not a round trip later.
        std::array<float, delayCapacity> squares {};
        int squaresIndex { 0 };
        int roundTrip { 1 };
        double squaresSum { 0.0 };
        double tension { 0.0 };
        double impedance { 0.0 };
        double linearMass { 0.0 };
        int ji { 0 };          // its ji's coordinates: ji (heave), maxStrings + ji (tip)
        // The left hand's press behind the ji (oshide), as a pitch ratio: the
        // target and where the glide has reached.
        double pressTarget { 1.0 };
        double pressRatio { 1.0 };
        // The pressing hand's force fluctuation, in standard deviations.
        double pressWaver { 0.0 };
        // PlayingControls::handPress as a pitch ratio, where the hand has
        // reached on this string: 1 on every string but the hand's.
        double handRatio { 1.0 };
        double pressSmoothing { 1.0 };
        double energy { 0.0 };
        bool keyDown { false };
        bool sustained { false };
        bool plucked { false };   // played, and still sounding from that pluck
        bool raised { false };    // the left hand presses it
        int extensionKey { -1 };  // the key its moved ji sounds (extension), or -1
        bool dampingDirty { true };
        int midiNote { -1 };      // its own note, lever down
        int ownerNote { -1 };     // the note that last plucked it
        int keys { 0 };           // ownerNote's presses still down

        // How the ji's coordinates move the string's end normal to the top.
        BarVector normalPort() const noexcept
        {
            BarVector port {};
            port[static_cast<std::size_t>(ji)] = 1.0;
            return port;
        }
        std::size_t tipCoordinate() const noexcept { return maxStrings + static_cast<std::size_t>(ji); }
    };
    std::array<StringVoice, maxStrings> strings {};
    // Keys no string sounds by its own note or by oshide, lowestKey to
    // highestKey: the nearest string below the key (the lowest string, for a
    // key below it) with its ji moved to sound the key, its speaking length
    // scaled at its own tension, as a player retunes a string by moving its
    // ji. The rest of the string, from the ji on, keeps its own length. Below
    // about B2 the ji would stand past the unkaku and above about C6 nearer
    // the ryukaku than the tsume plays: no koto has those strings, an
    // artificial range so that every key of a piano sounds at its own pitch
    // (owner requirement, 2026-10-30). extensionString is the string, or -1
    // where a string's own note is the key.
    std::array<PitchDesign, 128> extension {};
    std::array<int, 128> extensionString {};

    std::array<Mode, maxModes> modes {};
    int modeCount { 0 };
    ModeBank bank {};
    // The modes' state at the bridge, sum(pull s1): process() sums it as it
    // updates them, noteOn's preload makes it stale.
    BarVector modalPast {};
    bool modalPastStale { true };

    // The ji junction (process() derives it): the immediate generalized
    // mobility Y0 of the modes, which hold each ji's own mass and feet, the
    // strings' generalized impedance Zg (both stretches of every string) and
    // solve = (I + Y0 Zg)^-1.
    BarMatrix immediateMobility {};
    BarMatrix stringImpedance {};
    BarMatrix solve {};
    BodyWork work {};

    double radiationScale { 0.0 };
    PhaseNetwork phase {};
    // In phase left, quadrature left, in phase right, quadrature right.
    std::array<std::array<double, PhaseNetwork::sections>, 4> phaseState {};
    double previousLeft { 0.0 };
    double previousRight { 0.0 };
    double pressureLeft { 0.0 };
    double pressureRight { 0.0 };
    Room room {};
    double outputGain { 1.0 };
    // The piezo's coupling into its preamp (prepare()), as a bilinear
    // one-pole high-pass: y = piezoScale (x - x[n-1]) + piezoPole y[n-1].
    double piezoGain { 0.0 };
    double piezoScale { 0.0 };
    double piezoPole { 0.0 };
    double piezoPreviousForce { 0.0 };
    double piezoPrevious { 0.0 };
    // The strip's compliance under the bar's mass (setInstrument()), a
    // biquad on the bar's heave force: b0 b1 b2 a1 a2, transposed direct
    // form II.
    std::array<double, 5> piezoMount { 1.0, 0.0, 0.0, 0.0, 0.0 };
    double piezoMountZ1 { 0.0 }, piezoMountZ2 { 0.0 };
    bool sustain { false };
    Random random {};
    // The left hand's own draws, so a press leaves the plucks' unchanged.
    Random hand {};
    double waverPole { 0.0 };
    PluckReport lastPluck {};
    bool idle { true };
    int quietSamples { 0 };
    int controlCountdown { 0 };
    BarVector lastGeneralizedVelocity {};
    std::array<double, delayCapacity> pluckWave {};

    // PlayingControls as the signal path uses them. The hands' damping as
    // decay rates (1/s, they add); the output gains glide to their targets
    // over about 10 ms (per sample), the bend at the control rate.
    double releaseRate { 0.0 };
    double sympathyRate { 0.0 };
    double muteRate { 0.0 };
    double bendTarget { 1.0 };
    double bend { 1.0 };
    // PlayingControls::handPress as a pitch ratio, on handString: the last
    // string plucked, or -1.
    double handTarget { 1.0 };
    int handString { -1 };
    // Room send, direct and crossed gain, volume, piezo blend.
    std::array<double, 5> outputTarget {};
    std::array<double, 5> output {};
    double outputSmoothing { 0.0 };
    double bendSmoothing { 0.0 };
};

AcukotoEngine::AcukotoEngine() : state_(std::make_unique<State>()) {}
AcukotoEngine::~AcukotoEngine() = default;

void AcukotoEngine::prepare(double sampleRate)
{
    sampleRate_ = std::isfinite(sampleRate) && sampleRate >= 22050.0 && sampleRate <= 192000.0
        ? sampleRate : 48000.0;
    state_->room.prepare(sampleRate_);
    state_->outputSmoothing = 1.0 - std::exp(-1.0 / (0.01 * sampleRate_));
    // The piezo is Aculele's pickup system (Acustra's documented chain): an
    // element of 0.2 V/N at 1.45 nF, 3 m of cable, the ESP Project 202
    // preamp at a gain of 1.62 and a 1 MOhm DI. At a lyre's bridge forces it
    // stays linear: its op-amps reach their limits at about 13 N, and a
    // glissando at full velocity on any factory patch presses at most 7.6 N.
    // Linear, the chain is flat but for its coupling capacitors, and only C3
    // into R5 || R6 (220 nF into 23.5 kOhm) lies above 2 Hz: a first-order
    // high-pass at 31 Hz. Its level is InstrumentSpec::piezoGain.
    {
        const double corner = 1.0 / (2.0 * pi * 23.5e3 * 220.0e-9);
        const double k = std::tan(pi * corner / sampleRate_);
        state_->piezoScale = 1.0 / (1.0 + k);
        state_->piezoPole = (1.0 - k) / (1.0 + k);
    }
    state_->bendSmoothing = 1.0 - std::exp(-controlPeriod / (0.01 * sampleRate_));
    state_->waverPole = std::exp(-2.0 * pi * pressWaverHz * controlPeriod / sampleRate_);
    if (stringCount_ > 0)
        setInstrument(state_->instrument);
}

void AcukotoEngine::setInstrument(const InstrumentSpec& instrument)
{
    State& s = *state_;
    s.instrument = instrument;
    const double fs = sampleRate_;
    stringCount_ = std::clamp(instrument.stringCount, 1, maxStrings);
    s.random.seed(0x5eed1ac0ffeeull);
    s.hand.seed(0x1ef7a11dull);

    // ---- The body ---------------------------------------------------------
    const BodySpec& body = instrument.body;
    s.modeCount = designBody(instrument, stringCount_, fs, s.work, s.modes);
    for (int index = 0; index < s.modeCount; ++index)
    {
        Mode& mode = s.modes[static_cast<std::size_t>(index)];
        const double omega = 2.0 * pi * mode.frequency;
        const double k = omega / std::tan(omega / (2.0 * fs));
        const double damping = omega / mode.q;
        const double d0 = k * k + damping * k + omega * omega;
        mode.b0 = k / d0;
        mode.a1 = 2.0 * (omega * omega - k * k) / d0;
        mode.a2 = (k * k - damping * k + omega * omega) / d0;
    }
    s.radiationScale = airDensity / (4.0 * pi * pairDistance) * 2.0 * fs;
    s.phase.design(fs);

    // ---- The strings ----------------------------------------------------
    const StringMaterial& material = instrument.material;
    // The player's tuning (StringSpec::cents), and this player's own scatter
    // about it, the same every time this instrument is set up. The scatter
    // moves the tension, as a ji moved by a millimetre or two would move the
    // pitch: a few cents is under 1.5% of it.
    std::array<double, maxStrings> detune {};
    {
        std::uint64_t hash = 0xcbf29ce484222325ull;  // FNV-1a of the name
        for (const char* c = instrument.name; c && *c; ++c)
            hash = (hash ^ static_cast<unsigned char>(*c)) * 0x100000001b3ull;
        Random tuning;
        tuning.seed(hash);
        for (int index = 0; index < stringCount_; ++index)
        {
            const StringSpec& spec = instrument.strings[static_cast<std::size_t>(index)];
            detune[static_cast<std::size_t>(index)] = spec.cents + spec.centsSpread * tuning.gaussian();
        }
    }
    // The keys no string's own note plays, each given to the nearest string
    // below it, or to the lowest string (State::extension).
    s.extensionString.fill(-1);
    for (int key = lowestKey; key <= highestKey; ++key)
    {
        int nearest = -1, lowest = 0;
        bool own = false;
        for (int index = 0; index < stringCount_; ++index)
        {
            const int note = instrument.strings[static_cast<std::size_t>(index)].midiNote;
            own = own || note == key;
            if (note < key && (nearest < 0 || note > instrument.strings[static_cast<std::size_t>(nearest)].midiNote))
                nearest = index;
            if (note < instrument.strings[static_cast<std::size_t>(lowest)].midiNote)
                lowest = index;
        }
        if (!own)
            s.extensionString[static_cast<std::size_t>(key)] = nearest >= 0 ? nearest : lowest;
    }
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        voice = {};
        const StringSpec& spec = instrument.strings[static_cast<std::size_t>(index)];
        voice.midiNote = spec.midiNote;
        const bool wound = spec.coreDiameter > 0.0;
        const double bendingDiameter = wound ? spec.coreDiameter : spec.diameter;
        const double area = 0.25 * pi * bendingDiameter * bendingDiameter;
        voice.linearMass = wound ? spec.woundMass : material.density * area;
        const double waveSpeed = 2.0 * spec.length * 440.0
            * std::pow(2.0, (spec.midiNote - 69) / 12.0 + detune[static_cast<std::size_t>(index)] / 1200.0);
        voice.tension = voice.linearMass * waveSpeed * waveSpeed;
        voice.impedance = std::sqrt(voice.tension * voice.linearMass);
        const double secondMoment = pi * std::pow(bendingDiameter, 4.0) / 64.0;
        const double modulus = material.youngModulus + material.modulusPerStress * voice.tension / area;
        // A wound string's friction acts on its tension's share of each
        // partial's energy, 1 / (1 + B n^2), as its bending loss acts on the
        // rest: (delta_W + eta B n^2) / (1 + B n^2) = delta_W + (eta -
        // delta_W) B n^2 / (1 + B n^2), the constant section taking delta_W
        // and the bending section eta - delta_W (Acustra's split, after Paté,
        // Le Carrou and Fabre, JASA 135 (2014) 3045, Sec. III.A).
        const double constantLoss = material.constantLoss + (wound ? material.windingFriction : 0.0);
        const double bendingLoss = wound ? std::max(0.0, material.woundBendingLoss - material.windingFriction)
                                         : material.lossModulus / modulus;
        // Woodhouse and Lynch-Aird's air damping (Acta Acustica 105 (2019)
        // 516, Equation (13) with their (d + 0.2) correction, d in mm): a
        // loss factor (d + 0.2) 8 sqrt(2) rho_a / (rho d) sqrt(nu / (2 pi f)),
        // so a decay rate pi f eta = airRoot sqrt(f) that rises with the
        // partial. Valette's law (below) gives every partial the
        // fundamental's rate.
        const double airRoot = material.measuredAirDamping
            ? pi * (spec.diameter * 1.0e3 + 0.2) * 8.0 * std::sqrt(2.0) * airDensity
                / (material.density * spec.diameter) * std::sqrt(airViscosity / airDensity / (2.0 * pi))
            : 0.0;
        voice.ji = index;
        voice.pressSmoothing = 1.0;

        // Both planes of one stretch of the string, at its tension.
        const auto designPlanes = [&] (double stretch, std::array<LoopDesign, 2>& planes, bool refine)
        {
            const double frequency = waveSpeed / (2.0 * stretch);
            const double inharmonicity = pi * pi * modulus * secondMoment / (voice.tension * stretch * stretch);
            // Both planes share the stretch's length, so only the ji's pull
            // on each (the junction, below) separates their pitches. Until
            // 0.2.0 the parallel plane was one string diameter longer, a
            // guitar's end correction after Woodhouse (Acta Acustica 90
            // (2004) 945, Sec. 4.3). On a koto's short top strings that put
            // it 7-10 cents low, so a note beat at 4-5 Hz and settled up to
            // 13 cents flat. No koto measurement supports it, and Acustra
            // dropped the same correction by ear (Docs/decisions.md,
            // 2026-10-29).
            // Air damping (Valette, in Mechanics of Musical Instruments,
            // 1995): R = 2 pi eta + 2 pi d sqrt(pi eta rho f), decay rate
            // R / (2 mu).
            const double airResistance = 2.0 * pi * airViscosity
                + 2.0 * pi * spec.diameter * std::sqrt(pi * airViscosity * airDensity * frequency);
            const double airDecay = material.measuredAirDamping ? airRoot * std::sqrt(frequency)
                                                                : airResistance / (2.0 * voice.linearMass);
            DispersionDesign design {};
            for (int plane = 0; plane < 2; ++plane)
            {
                LoopDesign& loop = planes[static_cast<std::size_t>(plane)];
                loop.frequency = frequency;
                loop.gain = std::exp(-airDecay / frequency) * yokeRoundTripGain;
                loop.constantDesign = constantLossSection(constantLoss, airRoot, inharmonicity, frequency, fs);
                loop.bendingDesign = bendingLossSection(bendingLoss, inharmonicity, frequency, fs);
                // Dispersion (designDispersion), the line then solved so the
                // fundamental is exact once the junction exists and the
                // ji's reflection phase is known (tuning below).
                if (plane == 0)
                    design = designDispersion(frequency, inharmonicity, fs, refine);
                loop.dispersionStages = design.stages;
                loop.dispersion = design.coefficient;
                loop.dispersionPairs = design.pairs;
                loop.pair = design.pair;
                loop.dispersionCents = design.worstCents;
                loop.loopDelay = fs / frequency;
            }
            return airDecay;
        };
        const auto designPitch = [&] (State::PitchDesign& pitch, double length, bool refine)
        {
            pitch.length = length;
            pitch.inharmonicity = pi * pi * modulus * secondMoment / (voice.tension * length * length);
            pitch.glidePerEnergy = modulus * area / (2.0 * voice.tension * waveSpeed * waveSpeed);
            const double frequency = waveSpeed / (2.0 * length);
            const double intrinsicDecay = designPlanes(length, pitch.planes, refine)
                + pi * frequency * constantLoss - std::log(yokeRoundTripGain) * frequency;
            pitch.intrinsicT60 = 3.0 * std::log(10.0) / intrinsicDecay;
            pitch.roundTrip = std::clamp(static_cast<int>(std::lround(fs / frequency)), 1, delayCapacity - 1);
        };
        designPitch(voice.pitch[0], spec.length, true);
        voice.leftLength = std::max(0.05, body.stringSpan - spec.length);
        designPlanes(voice.leftLength, voice.left, true);
        // Its moved ji's keys take the first-order dispersion fit alone: the
        // sections' refinement over a bass key's hundred partials would take
        // most of the setup's time.
        for (int key = lowestKey; key <= highestKey; ++key)
            if (s.extensionString[static_cast<std::size_t>(key)] == index)
                designPitch(s.extension[static_cast<std::size_t>(key)],
                            spec.length * std::pow(2.0, (spec.midiNote - key) / 12.0), false);
    }

    // ---- The junction ---------------------------------------------------
    BarMatrix y0m {};
    for (int index = 0; index < s.modeCount; ++index)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(index)];
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                y0m.m[i][j] += mode.b0 * mode.participation[i] * mode.participation[j];
    }
    s.immediateMobility = y0m;
    ModeBank& bank = s.bank;
    bank = {};
    // Every ji meets every mode, so the bank is one part: evenEnd and oddEnd
    // stay at its start.
    for (int index = 0; index < s.modeCount; ++index)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(index)];
        const auto k = static_cast<std::size_t>(bank.count++);
        for (std::size_t c = 0; c < barCoordinates; ++c)
        {
            bank.drive[c][k] = mode.b0 * mode.participation[c];
            bank.pull[c][k] = mode.participation[c];
        }
        bank.a1[k] = mode.a1;
        bank.a2[k] = mode.a2;
        // Its share of the volume acceleration (process()) carries the
        // k / 2fs of its bilinear design, k = w / tan(w / 2fs), so the shared
        // differentiator, j tan(w / 2fs), gives it j w / 2fs; the part in
        // quadrature takes its j from PhaseNetwork.
        const double omega = 2.0 * pi * mode.frequency;
        const double warp = omega / std::tan(omega / (2.0 * fs)) / (2.0 * fs);
        bank.left[k] = mode.radiationLeft * warp;
        bank.right[k] = mode.radiationRight * warp;
        bank.leftQuadrature[k] = mode.radiationLeftQuadrature * warp;
        bank.rightQuadrature[k] = mode.radiationRightQuadrature * warp;
    }
    bank.count = (bank.count + ModeBank::lanes - 1) / ModeBank::lanes * ModeBank::lanes;
    bank.evenEnd = bank.oddEnd = 0;
    // Both stretches of a string meet at its ji, each a wave impedance.
    BarMatrix zg {};
    for (int index = 0; index < stringCount_; ++index)
    {
        const State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        zg.m[static_cast<std::size_t>(voice.ji)][static_cast<std::size_t>(voice.ji)] += 2.0 * voice.impedance;
        zg.m[voice.tipCoordinate()][voice.tipCoordinate()] += 2.0 * voice.impedance;
    }
    s.stringImpedance = zg;
    BarMatrix junction = BarMatrix::identity();
    const BarMatrix product = y0m * zg;
    for (std::size_t i = 0; i < barCoordinates; ++i)
        for (std::size_t j = 0; j < barCoordinates; ++j)
            junction.m[i][j] += product.m[i][j];
    s.solve.m = inverse(junction.m);

    // ---- Tuning: each port's reflection phase from the continuous model --
    // Each stretch's loop, both planes, sees the ji loaded by every other
    // stretch's wave impedance.
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        // `bends`: the reflection phase is found again at every bend ratio;
        // otherwise its phase at rest stands for the bend's (a moved ji's
        // keys, which would take several times the whole setup's time; a
        // bend there can sit a few cents off where it crosses a body mode).
        const auto tune = [&] (LoopDesign& loop, int plane, bool bends)
        {
            const double f0 = loop.frequency;
            const std::size_t port = plane == 0 ? static_cast<std::size_t>(voice.ji) : voice.tipCoordinate();
            // The port's admittance with every other port loading the ji.
            const auto reflectionLead = [&] (double frequency)
            {
                const double omega = 2.0 * pi * frequency;
                std::array<std::array<Complex, barCoordinates>, barCoordinates> mobility {};
                for (int m = 0; m < s.modeCount; ++m)
                {
                    const Mode& mode = s.modes[static_cast<std::size_t>(m)];
                    const double wm = 2.0 * pi * mode.frequency;
                    const Complex response = Complex(0.0, omega)
                        / Complex(wm * wm - omega * omega, omega * wm / mode.q);
                    for (std::size_t i = 0; i < barCoordinates; ++i)
                        for (std::size_t j = 0; j < barCoordinates; ++j)
                            mobility[i][j] += response * mode.participation[i] * mode.participation[j];
                }
                // Y_port = e^T (I + Y W)^-1 Y e, W the other stretches.
                std::array<std::array<Complex, barCoordinates>, barCoordinates> system {};
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    for (std::size_t j = 0; j < barCoordinates; ++j)
                    {
                        const double others = zg.m[j][j] - (j == port ? voice.impedance : 0.0);
                        system[i][j] = (i == j ? 1.0 : 0.0) + mobility[i][j] * others;
                    }
                const auto inverted = inverse(system);
                Complex admittance = 0.0;
                for (std::size_t k = 0; k < barCoordinates; ++k)
                    admittance += inverted[port][k] * mobility[k][port];
                const Complex zy = voice.impedance * admittance;
                // R = (ZY - 1)/(1 + ZY); the loop carries -R.
                return std::arg((1.0 - zy) / (1.0 + zy));
            };
            const auto lagAt = [&] (double omega, double lineDelay, double lead)
            {
                const int whole = wholeFor(lineDelay);
                const double fraction = lineDelay - whole;
                double t1 = 0.0, t2 = 0.0;
                thiranCoefficients(fraction, t1, t2);
                return omega * whole + allpassLag(2, t1, t2, omega) + loop.filterLag(omega) - lead;
            };
            const double omega0 = 2.0 * pi * f0 / fs;
            const double lead0 = reflectionLead(f0);
            double lower = 6.0, upper = delayCapacity - 16.0;
            for (int iteration = 0; iteration < 60; ++iteration)
            {
                const double middle = 0.5 * (lower + upper);
                if (lagAt(omega0, middle, lead0) < 2.0 * pi)
                    lower = middle;
                else
                    upper = middle;
            }
            loop.restLineDelay = 0.5 * (lower + upper);
            for (std::size_t k = 0; k < bendRatios.size(); ++k)
            {
                const double omega = omega0 * bendRatios[k];
                const double lead = bends ? reflectionLead(omega * fs / (2.0 * pi)) : lead0;
                loop.lagCycles[k] = (lagAt(omega, loop.restLineDelay, lead) - omega * loop.restLineDelay) / (2.0 * pi);
            }
        };
        for (int design = 0; design < 4; ++design)
        {
            const int plane = design & 1;
            tune(design < 2 ? voice.pitch[0].planes[static_cast<std::size_t>(plane)]
                            : voice.left[static_cast<std::size_t>(plane)],
                 plane, true);
        }
        for (int key = lowestKey; key <= highestKey; ++key)
            if (s.extensionString[static_cast<std::size_t>(key)] == index)
                for (int plane = 0; plane < 2; ++plane)
                    tune(s.extension[static_cast<std::size_t>(key)].planes[static_cast<std::size_t>(plane)], plane, false);
        voice.normal.whole = wholeFor(voice.pitch[0].planes[0].restLineDelay);
        voice.parallel.whole = wholeFor(voice.pitch[0].planes[1].restLineDelay);
        voice.leftNormal.whole = wholeFor(voice.left[0].restLineDelay);
        voice.leftParallel.whole = wholeFor(voice.left[1].restLineDelay);
        applyPitch(index, false);
    }

    // The piezo strip is no rigid seat: the bar's mass rides on the element,
    // so the force the element feels falls away above their resonance.
    // Acustra's element is the sibling evidence: a 3.8 g saddle on it rings
    // at 6 kHz (the documented 5-7 kHz), so the element and its seat are
    // 3.8e-3 (2 pi 6000)^2 / 0.072 = 7.5e7 N/m per metre of its 72 mm saddle,
    // with a loss factor of 1/18 (Acustra's PiezoDesign; Zollner, Physics of
    // the Electric Guitar, 2005, ch. 6). Here the strip runs the bar's length
    // under the bar's mass: 8.7 kHz on the lyre harp, 6.5 kHz on the gut
    // lyre. As in Acustra, F_p / F_r = Zk Q / (1 + Zk Q), Zk = k/s + c,
    // Q = 1/(s M + SZ) + G, with SZ the strings' summed wave impedance and G
    // the top's heave conductance under the bar, the mean of Re(Y) within a
    // sixth of the resonance (as Acustra's 5-7 kHz about 6). Bilinear,
    // prewarped at the resonance.
    {
        const double length = stringCount_ * body.stringSpacing;
        const double stiffness = 3.8e-3 * std::pow(2.0 * pi * 6000.0, 2.0) / 0.072 * length;
        const double mass = body.bridgeMass;
        const double omega0 = std::sqrt(stiffness / mass);
        const double loss = stiffness / (18.0 * omega0);
        double impedance = 0.0;
        for (int index = 0; index < stringCount_; ++index)
            impedance += s.strings[static_cast<std::size_t>(index)].impedance;
        constexpr int points = 64;
        double g = 0.0;
        for (int point = 0; point < points; ++point)
        {
            const double omega = omega0 * (5.0 / 6.0 + (point + 0.5) / (3.0 * points));
            for (int m = 0; m < s.modeCount; ++m)
            {
                const Mode& mode = s.modes[static_cast<std::size_t>(m)];
                const double wm = 2.0 * pi * mode.frequency;
                const Complex response = Complex(0.0, omega) / Complex(wm * wm - omega * omega, omega * wm / mode.q);
                g += response.real() * mode.participation[0] * mode.participation[0] / points;
            }
        }
        const double n2 = loss * g * mass;
        const double n1 = loss * (1.0 + g * impedance) + stiffness * g * mass;
        const double n0 = stiffness * (1.0 + g * impedance);
        const double d2 = n2 + mass;
        const double d1 = n1 + impedance;
        const double d0 = n0;
        const double k = omega0 / std::tan(std::min(0.49 * pi, 0.5 * omega0 / fs));
        const double a0 = d2 * k * k + d1 * k + d0;
        s.piezoMount = { (n2 * k * k + n1 * k + n0) / a0, 2.0 * (n0 - n2 * k * k) / a0,
                         (n2 * k * k - n1 * k + n0) / a0, 2.0 * (d0 - d2 * k * k) / a0,
                         (d2 * k * k - d1 * k + d0) / a0 };
    }

    s.outputGain = instrument.outputGain;
    s.piezoGain = instrument.piezoGain;
    PlayingControls controls {};
    controls.technique = instrument.technique;
    controls.pluckDistance = instrument.pluckDistance;
    controls.normalShare = instrument.normalShare;
    controls.room = instrument.room;
    setControls(controls);
    reset();
}

void AcukotoEngine::reset() noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        // Every ji back in its place, as a fresh instance has them, and
        // always: a ringing string's round trip follows its tension glide.
        applyPitch(index, false);
        voice.pressTarget = voice.pressRatio = 1.0;
        voice.pressWaver = 0.0;
        voice.handRatio = 1.0;
        voice.raised = false;
        voice.normal.reset();
        voice.parallel.reset();
        voice.leftNormal.reset();
        voice.leftParallel.reset();
        voice.squares.fill(0.0f);
        voice.squaresSum = 0.0;
        voice.squaresIndex = 0;
        voice.energy = 0.0;
        voice.keyDown = voice.sustained = voice.plucked = false;
        voice.ownerNote = -1;
        voice.keys = 0;
        voice.dampingDirty = true;
    }
    s.bank.s1.fill(0.0);
    s.bank.s2.fill(0.0);
    s.modalPast = {};
    s.modalPastStale = true;
    s.previousLeft = s.previousRight = 0.0;
    s.pressureLeft = s.pressureRight = 0.0;
    s.phaseState = {};
    s.piezoPreviousForce = s.piezoPrevious = 0.0;
    s.piezoMountZ1 = s.piezoMountZ2 = 0.0;
    s.room.reset();
    s.sustain = false;
    s.idle = true;
    s.quietSamples = 0;
    s.controlCountdown = 0;
    s.random.seed(0x5eed1ac0ffeeull);
    s.hand.seed(0x1ef7a11dull);
    s.bend = s.bendTarget;
    s.handString = -1;
    s.output = s.outputTarget;
}

const InstrumentSpec& AcukotoEngine::instrument() const noexcept { return state_->instrument; }

int AcukotoEngine::stringForNote(int midiNote) const noexcept
{
    for (int index = 0; index < stringCount_; ++index)
        if (state_->strings[static_cast<std::size_t>(index)].midiNote == midiNote)
            return index;
    return -1;
}

namespace
{
// A hand control as a decay rate (1/s): 0, a hand resting on the string, an
// amplitude time constant of 30 ms (Acustra's); 1, no hand at all. Between,
// the time constant grows geometrically, to 6 s just short of 1.
double handRate(double setting) noexcept
{
    return setting >= 1.0 ? 0.0 : 1.0 / (0.03 * std::pow(200.0, setting));
}

double unitClamp(double x) noexcept { return x > 1.0 ? 1.0 : (x >= 0.0 ? x : 0.0); }

// Below this mean squared wave velocity a released string has stopped
// sounding from its pluck.
constexpr double silentEnergy = 1.0e-6;
// The engine sleeps once its output, before the volume and with the room's
// whole tail, has stayed below -170 dBFS for half a second and every
// string's energy is below one where a ringing string radiates about that.
constexpr double idleLevel = 3.0e-9;
constexpr double idleEnergy = 1.0e-12;
} // namespace

void AcukotoEngine::applyPitch(int string, bool raised, int extensionKey) noexcept
{
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
    const State::PitchDesign& pitch = extensionKey >= 0 ? state_->extension[static_cast<std::size_t>(extensionKey)]
                                                        : voice.pitch[raised ? 1u : 0u];
    voice.raised = raised;
    voice.extensionKey = extensionKey;
    voice.frequency = pitch.planes[0].frequency;
    voice.length = pitch.length;
    voice.inharmonicity = pitch.inharmonicity;
    voice.intrinsicT60 = pitch.intrinsicT60;
    voice.glidePerEnergy = pitch.glidePerEnergy;
    // The new round trip's window over the squares already held.
    voice.roundTrip = pitch.roundTrip;
    voice.squaresSum = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    voice.energy = voice.squaresSum / voice.roundTrip;
    voice.normal.apply(pitch.planes[0]);
    voice.parallel.apply(pitch.planes[1]);
    voice.leftNormal.apply(voice.left[0]);
    voice.leftParallel.apply(voice.left[1]);
    voice.dampingDirty = true;
}

// Every hand on the string, as one loss per round trip reached over a round
// trip: the palm on all strings, and on a string nobody holds, the playing
// hand (plucked) or the resting hand (idle).
void AcukotoEngine::updateDamping(int string) noexcept
{
    const State& s = *state_;
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
    voice.dampingDirty = false;
    double rate = s.muteRate;
    if (!voice.keyDown && !voice.sustained)
        rate += voice.plucked ? s.releaseRate : s.sympathyRate;
    voice.normal.setDamping(rate > 0.0 ? std::exp(-rate / voice.frequency) : 1.0);
    voice.parallel.setDamping(rate > 0.0 ? std::exp(-rate / voice.frequency) : 1.0);
    // Beyond the ji only the palm reaches the string, the left hand while it
    // presses there, as a hand resting on it (handRate(0)), and on a string
    // nobody plays the hand Sympathy rests, as on its speaking length: a
    // string at rest rings in sympathy along its whole length or not at all.
    // Until 0.11.2 the stretch beyond G5's ji, whose second partial stands
    // at D4's pitch, rang on at D4 10 dB over a D4 pressed up a whole tone,
    // at Sympathy 0 as at 1.
    double left = s.muteRate;
    if (voice.pressTarget > 1.0 || (string == s.handString && s.handTarget > 1.0))
        left += handRate(0.0);
    else if (!voice.plucked && !voice.keyDown && !voice.sustained)
        left += s.sympathyRate;
    for (Loop* loop : { &voice.leftNormal, &voice.leftParallel })
        loop->setDamping(left > 0.0 ? std::exp(-left * loop->loopDelay / sampleRate_) : 1.0);
}

void AcukotoEngine::setControls(const PlayingControls& controls) noexcept
{
    State& s = *state_;
    const PlayingControls previous = controls_;
    PlayingControls& c = controls_;
    c = controls;
    c.pluckDistance = std::isfinite(c.pluckDistance) ? std::clamp(c.pluckDistance, 0.005, 0.5) : 0.05;
    c.normalShare = std::clamp(unitClamp(c.normalShare), 0.01, 0.9);
    c.release = unitClamp(c.release);
    c.mute = unitClamp(c.mute);
    c.sympathy = unitClamp(c.sympathy);
    c.room = unitClamp(c.room);
    c.width = 2.0 * unitClamp(0.5 * c.width);
    c.volume = c.volume >= 0.0 && c.volume <= 4.0 ? c.volume : (c.volume > 4.0 ? 4.0 : 0.0);
    c.pitchBend = c.pitchBend >= -12.0 && c.pitchBend <= 12.0 ? c.pitchBend
                                                               : (c.pitchBend > 12.0 ? 12.0 : 0.0);
    c.handPress = c.handPress >= 0.0 && c.handPress <= 3.0 ? c.handPress : (c.handPress > 3.0 ? 3.0 : 0.0);

    s.releaseRate = handRate(c.release);
    s.sympathyRate = handRate(c.sympathy);
    // A full palm: an amplitude time constant of about 33 ms. Authored.
    s.muteRate = 30.0 * c.mute * c.mute;
    if (c.release != previous.release || c.sympathy != previous.sympathy || c.mute != previous.mute)
        for (int index = 0; index < stringCount_; ++index)
            s.strings[static_cast<std::size_t>(index)].dampingDirty = true;
    if (c.pitchBend != previous.pitchBend)
        s.bendTarget = std::pow(2.0, c.pitchBend / 12.0);
    if (c.handPress != previous.handPress)
    {
        s.handTarget = std::pow(2.0, c.handPress / 12.0);
        // The hand resting on the stretch beyond the ji comes and goes with
        // the press.
        if (s.handString >= 0 && (c.handPress > 0.0) != (previous.handPress > 0.0))
            s.strings[static_cast<std::size_t>(s.handString)].dampingDirty = true;
    }
    // Mid/side: width 1 is the two microphones as they are.
    s.outputTarget = { 0.5 * c.room, 0.5 * (1.0 + c.width), 0.5 * (1.0 - c.width), c.volume, 0.0 };
    // A silent engine has nothing to glide: take the controls at once, as
    // process() does while idle, so the first pluck after prepare() sounds
    // as one after reset() does rather than fading in from zero.
    if (s.idle)
    {
        s.output = s.outputTarget;
        s.bend = s.bendTarget;
        if (s.handString >= 0)
            s.strings[static_cast<std::size_t>(s.handString)].handRatio = s.handTarget;
    }
}

void AcukotoEngine::noteOn(int midiNote, float velocity) noexcept
{
    if (!(velocity > 0.0f))
        return;
    int index = stringForNote(midiNote);
    // Legato: a note played while the key of a sounding string is still
    // down, which that string reaches by its press alone, is the left hand
    // moving the press with the string ringing on, not a new pluck: ato-oshi
    // a semitone or a whole tone up from its own note, or letting the press
    // go back down to it. A note an open string of its own sounds is
    // plucked there, as a player would. Only a single line slurs: with any
    // other key down, the new note belongs to a chord and is plucked (the
    // owner's call from playing the Rack device, Docs/decisions.md
    // 2026-10-31). PlayingControls::legato turns it on.
    int keysDown = 0;
    for (int string = 0; string < stringCount_; ++string)
        keysDown += state_->strings[static_cast<std::size_t>(string)].keyDown ? 1 : 0;
    if (controls_.oshide && controls_.legato && keysDown == 1)
        for (int string = 0; string < stringCount_; ++string)
        {
            State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
            const int step = midiNote - voice.midiNote;
            if (!voice.keyDown || voice.ownerNote == midiNote || !(voice.energy > silentEnergy)
                || voice.extensionKey >= 0 || step < 0 || step > 2 || (index >= 0 && index != string))
                continue;
            press(string, step * pressReach, pressGlide);
            voice.ownerNote = midiNote;
            voice.keys = 1;
            return;
        }
    int pressed = 0;
    // Oshide: the nearest string below, a semitone or a whole tone, and not
    // one another key or the pedal holds if the next one down will do: in a
    // chord with E-flat 4, a player takes E4 on D4 pressed a whole tone and
    // leaves E-flat 4 sounding. Until 0.11.2 E4 always took E-flat 4's
    // string and cut it.
    if (index < 0 && controls_.oshide)
    {
        for (int step = 1; step <= 2 && index < 0; ++step)
        {
            const int string = stringForNote(midiNote - step);
            if (string >= 0 && !state_->strings[static_cast<std::size_t>(string)].keyDown
                && !state_->strings[static_cast<std::size_t>(string)].sustained)
            {
                index = string;
                pressed = step;
            }
        }
        for (int step = 1; step <= 2 && index < 0; ++step)
            if ((index = stringForNote(midiNote - step)) >= 0)
                pressed = step;
    }
    // A key no string or press reaches: the nearest string below, its ji
    // moved to the key (State::extension).
    int extensionKey = -1;
    if (index < 0 && midiNote >= lowestKey && midiNote <= highestKey)
    {
        index = state_->extensionString[static_cast<std::size_t>(midiNote)];
        extensionKey = index >= 0 ? midiNote : -1;
    }
    if (index < 0)
        return;
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(index)];
    // The ji is moved, or back in its place, before the pluck.
    if (voice.extensionKey != extensionKey)
        applyPitch(index, false, extensionKey);
    // The left hand presses before the right plucks: the press has settled.
    const double target = std::pow(2.0, pressed * pressReach / 12.0);
    voice.pressTarget = voice.pressRatio = target;
    voice.raised = pressed > 0;
    pluck(index, midiNote, velocity);
}

void AcukotoEngine::pluckString(int string, float velocity) noexcept
{
    if (string < 0 || string >= stringCount_ || !(velocity > 0.0f))
        return;
    if (state_->strings[static_cast<std::size_t>(string)].extensionKey >= 0)
        applyPitch(string, false);
    pluck(string, state_->strings[static_cast<std::size_t>(string)].midiNote, velocity);
}

void AcukotoEngine::press(int string, double semitones, double seconds) noexcept
{
    if (string < 0 || string >= stringCount_)
        return;
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
    const double amount = std::isfinite(semitones) ? std::clamp(semitones, 0.0, 3.0) : 0.0;
    voice.pressTarget = std::pow(2.0, amount / 12.0);
    voice.raised = amount > 0.0;
    voice.dampingDirty = true;
    const double time = std::isfinite(seconds) ? std::max(seconds, 0.0) : 0.0;
    voice.pressSmoothing = time > 0.0 ? 1.0 - std::exp(-controlPeriod / (time * sampleRate_)) : 1.0;
    if (!(time > 0.0))
        voice.pressRatio = voice.pressTarget;
}

void AcukotoEngine::pluck(int index, int midiNote, float velocity) noexcept
{
    State& s = *state_;
    if (s.idle)
    {
        s.idle = false;
        s.controlCountdown = 0;
    }
    s.quietSamples = 0;
    State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
    // The left hand follows the right to the string it plucks and presses it
    // by handPress before the pluck; the string it leaves lets go.
    if (s.handString != index)
    {
        if (s.handString >= 0)
            s.strings[static_cast<std::size_t>(s.handString)].dampingDirty = true;
        s.handString = index;
        voice.dampingDirty = true;
    }
    voice.handRatio = s.handTarget;
    const InstrumentSpec& instrument = s.instrument;
    const PlayingControls& controls = controls_;
    const double v = std::clamp(static_cast<double>(velocity), 0.0, 1.0);

    // A finger meeting a sounding string stops it before it lets go: a
    // harpist's finger holds the string 56 to 431 ms (Chadefaux et al., JASA
    // 131 (2012) 844, Table II), several times the 10 ms in which a finger's
    // contact silences a plucked string (Laurson et al., CMJ 25(3) 2001). A
    // plectrum glances off and stops most of it (Acustra's picking-hand
    // contact, as one step). The finger's 10 ms are slow against the body's
    // periods, so the body takes the force the stopped wave leaves as the
    // held force below: preloaded, without the click of a step.
    BarVector preload {};
    const BarVector port = voice.normalPort();
    if (voice.keyDown || voice.sustained || voice.energy > silentEnergy)
    {
        const double stopped = controls.technique == Technique::Plectrum ? 0.7 : 1.0;
        const double normalForce = 2.0 * voice.impedance * stopped * voice.normal.last;
        for (std::size_t c = 0; c < barCoordinates; ++c)
            preload[c] -= normalForce * port[c];
        preload[voice.tipCoordinate()] -= 2.0 * voice.impedance * stopped * voice.parallel.last;
        for (Loop* loop : { &voice.normal, &voice.parallel })
        {
            if (controls.technique != Technique::Plectrum)
            {
                loop->silence();
                continue;
            }
            for (float& sample : loop->line)
                sample *= 0.3f;
            for (float& sample : loop->pending)
                sample *= 0.3f;
        }
    }
    if (voice.keyDown || voice.sustained || voice.energy > silentEnergy)
    {
        const float kept = controls.technique == Technique::Plectrum ? 0.09f : 0.0f;
        for (float& square : voice.squares)
            square *= kept;
    }

    // The tsume plucks just beside the ryukaku, the far end from the ji, the
    // hand at one place whatever the string (pluckDistance): a short string
    // is plucked at a larger share of its length, so its attack falls off
    // faster with partial number. The place is taken here from the ji. A
    // koto guide has the strings plucked 3-5 cm from the right end ("How to
    // play the KOTO", www.asahi-net.or.jp/~NP5Y-HRUC/kt-play1.html), so each
    // pluck lands within the same centimetre or two of the ryukaku on every
    // string: an estimated 8 mm standard deviation, those 2 cm a little over
    // two. The eight professional recordings bear it out: the partial nearest
    // the first notch a 5 cm pluck leaves on the strings from B-flat 4 up sits
    // a median -0.6 to +0.1 dB from its two neighbours in their 185 notes
    // there, register by register, and in the model as 0.7.0 plucked, 5 cm
    // away, -6 to 0 with this spread and -10 to -14 with a harpist's
    // (Chadefaux et al., JASA 131 (2012) 844, Fig. 7: a share of the string,
    // 2.5 mm on A5 but 28 mm on D3, where the whole pluck is 5 cm from the
    // ryukaku).
    constexpr double distanceSpread = 0.008;
    const double distance = std::clamp((controls.pluckDistance + distanceSpread * s.random.gaussian()) / voice.length,
                                       0.01, 0.5);
    const double position = 1.0 - distance;
    // The player evens the scale by what is heard: the microphones, the
    // piezo, or between the two as the blend sets.
    const StringSpec& string = instrument.strings[static_cast<std::size_t>(index)];
    const double touch = string.touch;
    // Each pluck differs as a harpist's do too (Chadefaux et al. 2012). How
    // hard: one player's string displacement at release spreads by about 14%
    // (Fig. 8, 95% intervals of about 15% over five to seven plucks). Which
    // way: the angle the string leaves at, measured from the strings' plane
    // (here the top's), by about 4 degrees (Table VI, median 95% interval 5
    // degrees over five plucks), around the angle the normal share sets: the
    // share is the angle's sine squared.
    const double force = instrument.pluckForce * touch * std::pow(v, 1.4) * std::exp(0.14 * s.random.gaussian());
    const double angle = std::asin(std::sqrt(std::clamp(controls.normalShare, 0.0, 1.0)))
                       + 4.0 * pi / 180.0 * s.random.gaussian();
    const double normalShare = std::clamp(std::sin(angle) * std::sin(angle), 0.01, 0.9);

    // Contact width and release time of the technique: a fingertip, a
    // nail's or a plectrum's edge. A harpist's finger touches at most 2 cm
    // of string, which Chadefaux et al. find too little to filter the
    // string and so take the pluck as a point (JASA 131 (2012) 844,
    // Sec. IV.A.5, Table V); the string leaves the fingertip's edge wrapped
    // round it only over its kink angle, at most a few hundredths of a
    // radian, so the finger is a point here too. A harpist's string slips
    // 0.04 mm along the harder fingertip before it leaves at about 1.5 m/s
    // (Chadefaux, Le Carrou and Fabre, JASA 133 (2013) 2444); accelerating
    // evenly, that takes 0.053 ms, the finger's release time constant.
    // Chosen by ear over the pad's 0.2 mm (0.27 ms) and an authored 0.10 ms
    // (Docs/decisions.md, 2026-10-28). The nail's and the plectrum's are
    // authored, of the order of Acustra's (a 0.2 mm edge over a metre or two
    // per second).
    double contact = 0.0, release = 0.053e-3;
    switch (controls.technique)
    {
        // A bare finger on a koto string is the pad, not a harpist's
        // fingertip edge: the string slips 0.2 mm over it (0.27 ms, the pad's
        // figure in Chadefaux, Le Carrou and Fabre 2013) across an estimated
        // 8 mm of flesh.
        case Technique::Finger: contact = 0.008; release = 0.27e-3; break;
        case Technique::Nail: contact = 0.0012; release = 0.035e-3; break;
        // The tsume's rounded ivory edge: an estimated 2 mm. Its release is
        // fitted with the pluck's distance from the ryukaku (AcukotoPresets.h)
        // to the first 60 ms of 724 isolated notes in eight professional koto
        // recordings (Docs/validation/attack-comparison-2026-10-30.md): 0.03
        // ms, a little quicker than off a nail (0.035 ms), as off a hard edge.
        // It was 0.10 until 0.8.0, when the walls' highs, radiated too
        // strongly, had stood in for a quicker release.
        case Technique::Plectrum: contact = 0.002; release = 0.03e-3; break;
    }
    // A harder stroke slips off faster, so it leaves brighter. Harpists'
    // release velocity rises with the string's displacement, about as D^1.2
    // from 0.5 mm at 0.2 m/s to 7 mm at 5 m/s around an average of 4 mm at
    // 2 m/s (Chadefaux et al., JASA 131 (2012) 844, Sec. III.B.1), and the
    // displacement follows the force, v^1.4 here: the release time goes as
    // v^-1.68 about v = 0.5, within the 0.2-5 m/s the harpists reached.
    // Chosen by ear over a gentler authored law (Docs/decisions.md,
    // 2026-10-28). The slip's speed also scatters more than the displacement
    // does: one harpist's release velocity by about 25%, the displacement by
    // 14% (Fig. 8), so the release time takes the rest, about 20%, and each
    // pluck's brightness with it.
    release *= std::clamp(std::pow(v / 0.5, -1.4 * 1.2), 0.4, 10.0)
             * std::exp(0.2 * s.random.gaussian());
    s.lastPluck = { position, force, normalShare, release };

    // A key struck again while it is still down (two overlapping notes of
    // one pitch, as a sequencer can send) counts twice, so its first key-up
    // does not let the string go under the second.
    voice.keys = voice.keyDown && voice.ownerNote == midiNote ? voice.keys + 1 : 1;
    voice.keyDown = true;
    voice.sustained = false;
    voice.plucked = true;
    voice.ownerNote = midiNote;
    updateDamping(index);

    // The loop's period as the string is released: the tension glide its
    // stretch sets at once (what a re-pluck kept, plus the released
    // triangle's mean squared wave velocity, (c/2)^2 A^2 (1/x + 1/(L - x)) / L
    // in each plane) and the bend. The wave is laid over that period, so its
    // first round trip meets its own return.
    double releasedEnergy = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        releasedEnergy += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    releasedEnergy /= voice.roundTrip;
    const double length = voice.length;
    const double frequency = voice.frequency;
    for (int plane = 0; plane < 2; ++plane)
    {
        const double pluckPoint = position * length;
        const double amplitude = force * pluckPoint * (length - pluckPoint) / (voice.tension * length)
                               * std::sqrt(plane == 0 ? normalShare : 1.0 - normalShare);
        releasedEnergy += length * length * frequency * frequency * amplitude * amplitude
                        * (1.0 / pluckPoint + 1.0 / (length - pluckPoint)) / length;
    }
    const double releasedRatio = (tensionModulation_ ? 1.0 + voice.glidePerEnergy * releasedEnergy : 1.0) * s.bend;

    for (int plane = 0; plane < 2; ++plane)
    {
        Loop& loop = plane == 0 ? voice.normal : voice.parallel;
        const double waveSpeed = 2.0 * length * frequency;
        const double pluckPoint = position * length;
        // The displacement the held force sets, A = F x (L - x) / (T L).
        const double amplitude = force * pluckPoint * (length - pluckPoint) / (voice.tension * length)
                               * std::sqrt(plane == 0 ? normalShare : 1.0 - normalShare);
        const double kink = amplitude * (1.0 / pluckPoint + 1.0 / (length - pluckPoint));
        // The released shape, a triangle rounded by a Gaussian of the
        // contact's width.
        const auto shape = [&] (double x)
        {
            if (contact == 0.0)
                return amplitude * x / pluckPoint - kink * std::max(0.0, x - pluckPoint);
            const double u = (x - pluckPoint) / contact;
            return amplitude * x / pluckPoint - kink * contact * (u * normalCdf(u) + normalPdf(u));
        };
        // The wave reaching the bridge over the next round trip, one period
        // of the loop: first the half that travels straight there,
        // (c/2) y0'(c t), then the half that went to the yoke,
        // (c/2) y0'(2L - c t). Each sample holds its cell's mean slope, which
        // band-limits the release; the last cell ends at the round trip.
        const double period = loop.loopDelay / releasedRatio;
        const int count = static_cast<int>(std::ceil(period));
        const double cellLength = 2.0 * length / period;
        std::array<double, delayCapacity>& wave = s.pluckWave;
        double mean = 0.0;
        for (int k = 0; k < count; ++k)
        {
            const double start = cellLength * k;
            const double end = std::min(2.0 * length, cellLength * (k + 1));
            double integral = 0.0;
            if (end <= length)
                integral = shape(end) - shape(start);
            else if (start >= length)
                integral = shape(2.0 * length - start) - shape(2.0 * length - end);
            else
                integral = (shape(length) - shape(start)) + (shape(length) - shape(2.0 * length - end));
            wave[static_cast<std::size_t>(k)] = 0.5 * waveSpeed * integral / cellLength;
        }
        // The release: the held force unloads exponentially as the string
        // slides off, as Acustra's slip does (a Gaussian release of the same
        // time left a lyre harp phrase 10 dB short of its recording from
        // 1.25 to 5 kHz, the exponential 7.6 dB). Each kink reaching the
        // bridge arrives smoothed by that one-pole; over the loop's period it
        // runs circularly, starting from its steady state at the period's end.
        const double pole = std::exp(-1.0 / (release * sampleRate_));
        double weighted = 0.0, weight = 1.0;
        for (int k = count - 1; k >= 0; --k)
        {
            weighted += weight * wave[static_cast<std::size_t>(k)];
            weight *= pole;
        }
        double smoothed = (1.0 - pole) * weighted / (1.0 - weight);
        for (int k = 0; k < count; ++k)
        {
            smoothed = pole * smoothed + (1.0 - pole) * wave[static_cast<std::size_t>(k)];
            wave[static_cast<std::size_t>(k)] = smoothed;
            mean += smoothed;
        }
        mean /= count;
        for (int k = 0; k < count; ++k)
        {
            const double value = wave[static_cast<std::size_t>(k)] - mean;
            const auto slot = static_cast<std::size_t>((loop.write + k) & (delayCapacity - 1));
            loop.pending[slot] += static_cast<float>(value);
            if (k < voice.roundTrip)
            {
                float& square = voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip + k)
                                                                       & (delayCapacity - 1))];
                square += static_cast<float>(value * value);
            }
        }
        // The held force stood on the bridge before the release: the body
        // starts deflected under it, so only what the release changes
        // reaches it, and the top springs back as the wave's mean force
        // falls to zero.
        const double heldForce = 2.0 * voice.impedance * (wave[0] - mean);
        if (plane == 0)
        {
            for (std::size_t c = 0; c < barCoordinates; ++c)
                preload[c] += heldForce * port[c];
        }
        else
        {
            preload[voice.tipCoordinate()] += heldForce;
        }
    }
    voice.squaresSum = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    voice.energy = voice.squaresSum / voice.roundTrip;
    ModeBank& bank = s.bank;
    for (int k = 0; k < bank.count; ++k)
    {
        const auto m = static_cast<std::size_t>(k);
        double modal = 0.0;
        for (std::size_t c = 0; c < barCoordinates; ++c)
            modal += bank.drive[c][m] * preload[c];
        bank.s1[m] -= modal;
        bank.s2[m] -= modal;
    }
    s.modalPastStale = true;
}

void AcukotoEngine::noteOff(int midiNote) noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        // The left hand keeps an oshide pressed while its string sounds, as
        // a player holds a pressed note to its end: letting go would bend a
        // ringing note down a semitone or a whole tone at every key-up
        // (Docs/decisions.md 2026-10-31). The hand leaves once the string
        // has died away (process()) or is plucked again.
        if (!voice.keyDown || voice.ownerNote != midiNote || --voice.keys > 0)
            continue;
        voice.keyDown = false;
        voice.sustained = s.sustain;
        updateDamping(index);
    }
}

void AcukotoEngine::setSustain(bool down) noexcept
{
    State& s = *state_;
    s.sustain = down;
    if (down)
        return;
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        if (voice.sustained)
        {
            voice.sustained = false;
            updateDamping(index);
        }
    }
}

void AcukotoEngine::sleep() noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        voice.normal.silence();
        voice.parallel.silence();
        voice.leftNormal.silence();
        voice.leftParallel.silence();
        voice.squares.fill(0.0f);
        voice.squaresSum = 0.0;
        voice.energy = 0.0;
        if (voice.plucked)
        {
            voice.plucked = false;
            voice.dampingDirty = true;
        }
    }
    s.bank.s1.fill(0.0);
    s.bank.s2.fill(0.0);
    s.modalPastStale = true;
    s.lastGeneralizedVelocity = {};
    s.previousLeft = s.previousRight = 0.0;
    s.pressureLeft = s.pressureRight = 0.0;
    s.phaseState = {};
    s.piezoPreviousForce = s.piezoPrevious = 0.0;
    s.piezoMountZ1 = s.piezoMountZ2 = 0.0;
    s.room.reset();
    s.idle = true;
}

bool AcukotoEngine::idle() const noexcept { return state_->idle; }

void AcukotoEngine::process(float* left, float* right, int numSamples) noexcept
{
    State& s = *state_;
    if (s.idle)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        s.output = s.outputTarget;
        s.bend = s.bendTarget;
        return;
    }
    const int count = stringCount_;
    std::array<double, maxStrings> normalIn {};
    std::array<double, maxStrings> parallelIn {};
    std::array<double, maxStrings> leftNormalIn {};
    std::array<double, maxStrings> leftParallelIn {};
    const BarMatrix& y0 = s.immediateMobility;
    const BarMatrix& zg = s.stringImpedance;
    const BarMatrix& solve = s.solve;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (s.controlCountdown-- <= 0)
        {
            s.controlCountdown = controlPeriod - 1;
            s.bend += (s.bendTarget - s.bend) * s.bendSmoothing;
            for (int index = 0; index < count; ++index)
            {
                State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
                // A released string that has died away is idle again: the
                // playing hand has left it.
                if (voice.plucked && !voice.keyDown && !voice.sustained && voice.energy < silentEnergy)
                {
                    voice.plucked = false;
                    voice.dampingDirty = true;
                }
                // The left hand leaves a pressed string only once it is
                // inaudible. silentEnergy is too high for that: a pressed E4
                // released there, about 32 dB under its first half second,
                // was heard to drop to E-flat 4 in Recon (0.11.0). idleEnergy
                // is where a string radiates about -170 dBFS.
                if (voice.raised && !voice.keyDown && !voice.sustained && voice.energy < idleEnergy)
                {
                    voice.pressTarget = voice.pressRatio = 1.0;
                    voice.raised = false;
                    voice.dampingDirty = true;
                }
                if (voice.dampingDirty)
                    updateDamping(index);
                // A held key or pedal does not keep a string sounding: a
                // string that has died away under it lets the engine sleep.
                if (voice.energy > idleEnergy)
                    s.quietSamples = 0;
                // Tension modulation (Kirchhoff-Carrier): the string's mean
                // squared slope stretches it, Delta f / f = EA <y'^2> / (4T);
                // the pitch bend scales every string alike.
                // The left hand's press glides to its target.
                voice.pressRatio += (voice.pressTarget - voice.pressRatio) * voice.pressSmoothing;
                double press = voice.pressRatio;
                if (voice.pressTarget > 1.0 || voice.pressRatio > 1.0 + 1.0e-9)
                {
                    // A unit-variance one-pole of the hand's draws.
                    voice.pressWaver = s.waverPole * voice.pressWaver
                                     + std::sqrt(1.0 - s.waverPole * s.waverPole) * s.hand.gaussian();
                    press = std::sqrt(1.0 + (press * press - 1.0)
                                          * (1.0 + 2.0 * pressForceSpread * voice.pressWaver));
                }
                else
                {
                    voice.pressWaver = 0.0;
                }
                // The hand's press follows it as the bend does.
                voice.handRatio += ((index == s.handString ? s.handTarget : 1.0) - voice.handRatio)
                                 * s.bendSmoothing;
                const double ratio = (tensionModulation_
                    ? 1.0 + voice.glidePerEnergy * voice.energy : 1.0) * s.bend * press * voice.handRatio;
                for (Loop* loop : { &voice.normal, &voice.parallel, &voice.leftNormal, &voice.leftParallel })
                    loop->setLineDelay(loop->lineDelayAt(ratio));
                // The mean follows the period the bend and the glide set.
                const int roundTrip = std::clamp(static_cast<int>(std::lround(sampleRate_ / (voice.frequency * ratio))),
                                                 1, delayCapacity - 1);
                while (voice.roundTrip < roundTrip)
                {
                    ++voice.roundTrip;
                    voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                               & (delayCapacity - 1))];
                }
                while (voice.roundTrip > roundTrip)
                {
                    voice.squaresSum = std::max(0.0, voice.squaresSum
                        - voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                 & (delayCapacity - 1))]);
                    --voice.roundTrip;
                }
            }
        }

        BarVector drive {};
        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            const double vn = voice.normal.read();
            const double vp = voice.parallel.read();
            const double ln = voice.leftNormal.read();
            const double lp = voice.leftParallel.read();
            normalIn[static_cast<std::size_t>(index)] = vn;
            parallelIn[static_cast<std::size_t>(index)] = vp;
            leftNormalIn[static_cast<std::size_t>(index)] = ln;
            leftParallelIn[static_cast<std::size_t>(index)] = lp;
            const double twiceZ = 2.0 * voice.impedance;
            drive[static_cast<std::size_t>(voice.ji)] += twiceZ * (vn + ln);
            drive[voice.tipCoordinate()] += twiceZ * (vp + lp);
            // The loop's mean squared velocity over a round trip; both
            // directions' slopes count, hence the factor in glidePerEnergy.
            const float square = static_cast<float>(vn * vn + vp * vp);
            float& slot = voice.squares[static_cast<std::size_t>(voice.squaresIndex)];
            const float leaving = voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                         & (delayCapacity - 1))];
            voice.squaresSum = std::max(0.0, voice.squaresSum + static_cast<double>(square) - leaving);
            slot = square;
            voice.squaresIndex = (voice.squaresIndex + 1) & (delayCapacity - 1);
            voice.energy = voice.squaresSum / voice.roundTrip;
        }

        ModeBank& bank = s.bank;
        if (s.modalPastStale)
        {
            s.modalPast = {};
            for (std::size_t c = 0; c < barCoordinates; ++c)
                for (int k = 0; k < bank.count; ++k)
                    s.modalPast[c] += bank.pull[c][static_cast<std::size_t>(k)] * bank.s1[static_cast<std::size_t>(k)];
            s.modalPastStale = false;
        }
        const BarVector& past = s.modalPast;
        BarVector rhs = y0.apply(drive);
        for (std::size_t i = 0; i < barCoordinates; ++i)
            rhs[i] += past[i];
        const BarVector velocity = solve.apply(rhs);
        const BarVector stringLoad = zg.apply(velocity);
        BarVector plateForce {};
        for (std::size_t i = 0; i < barCoordinates; ++i)
            plateForce[i] = drive[i] - stringLoad[i];

        constexpr int lanes = ModeBank::lanes;
        double laneLeft[lanes] {}, laneRight[lanes] {}, laneLeftQuadrature[lanes] {}, laneRightQuadrature[lanes] {},
            lanePast[barCoordinates][lanes] {};
        runModes(bank, bank.oddEnd, bank.count, plateForce, laneLeft, laneRight, laneLeftQuadrature,
                 laneRightQuadrature, lanePast);
        double radiatedLeft = 0.0, radiatedRight = 0.0, quadratureLeft = 0.0, quadratureRight = 0.0;
        s.modalPast = {};
        for (int lane = 0; lane < lanes; ++lane)
        {
            radiatedLeft += laneLeft[lane];
            radiatedRight += laneRight[lane];
            quadratureLeft += laneLeftQuadrature[lane];
            quadratureRight += laneRightQuadrature[lane];
            for (std::size_t c = 0; c < barCoordinates; ++c)
                s.modalPast[c] += lanePast[c][lane];
        }

        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            const double heave = velocity[static_cast<std::size_t>(voice.ji)];
            const double rock = velocity[voice.tipCoordinate()];
            voice.normal.writeSample(normalIn[static_cast<std::size_t>(index)] - heave);
            voice.parallel.writeSample(parallelIn[static_cast<std::size_t>(index)] - rock);
            voice.leftNormal.writeSample(leftNormalIn[static_cast<std::size_t>(index)] - heave);
            voice.leftParallel.writeSample(leftParallelIn[static_cast<std::size_t>(index)] - rock);
        }
        s.lastGeneralizedVelocity = velocity;

        // Far-field pressure: rho / (4 pi r) times the volume acceleration,
        // each mode's as its bilinear design takes s times its velocity,
        // k (1 - z^-1) / (1 + z^-1). The weights carry each k / 2fs and one
        // recursion applies the rest to their sum, every term of which holds
        // the velocity's zero at Nyquist. A first difference of the velocity
        // missed the acceleration of the modes' tails, which the bilinear
        // transform compresses toward Nyquist: at 48 kHz each instrument's
        // 10 to 16 kHz fell 3.4 to 3.6 dB below its 4x sound, and the
        // acceleration brought it to 1.7 to 2.0. The parts in quadrature
        // join before it, 90 degrees on (PhaseNetwork).
        const double volumeLeft = PhaseNetwork::run(s.phase.inPhase, s.phaseState[0], radiatedLeft)
                                + PhaseNetwork::run(s.phase.quadrature, s.phaseState[1], quadratureLeft);
        const double volumeRight = PhaseNetwork::run(s.phase.inPhase, s.phaseState[2], radiatedRight)
                                 + PhaseNetwork::run(s.phase.quadrature, s.phaseState[3], quadratureRight);
        const double pressureLeft = (volumeLeft - s.previousLeft) * s.radiationScale - s.pressureLeft;
        const double pressureRight = (volumeRight - s.previousRight) * s.radiationScale - s.pressureRight;
        s.previousLeft = volumeLeft;
        s.previousRight = volumeRight;
        s.pressureLeft = pressureLeft;
        s.pressureRight = pressureRight;
        for (std::size_t i = 0; i < s.output.size(); ++i)
            s.output[i] += (s.outputTarget[i] - s.output[i]) * s.outputSmoothing;
        // The room runs even unsent, so turning it up meets a live tail.
        double wetLeft = 0.0, wetRight = 0.0;
        s.room.process(0.5 * (pressureLeft + pressureRight), wetLeft, wetRight);
        // The piezo strip under the bar presses with the bar's whole force on
        // the top, so it hears the heave and not the rock; it runs unheard
        // too, for the same reason as the room.
        const std::array<double, 5>& mount = s.piezoMount;
        const double element = mount[0] * plateForce[0] + s.piezoMountZ1;
        s.piezoMountZ1 = mount[1] * plateForce[0] - mount[3] * element + s.piezoMountZ2;
        s.piezoMountZ2 = mount[2] * plateForce[0] - mount[4] * element;
        const double piezo = s.piezoScale * (element - s.piezoPreviousForce) + s.piezoPole * s.piezoPrevious;
        s.piezoPreviousForce = element;
        s.piezoPrevious = piezo;
        const double level = std::max(s.outputGain * std::max(std::max(std::abs(pressureLeft), std::abs(pressureRight)),
                                                              std::max(std::abs(wetLeft), std::abs(wetRight))),
                                      s.piezoGain * std::abs(piezo));
        s.quietSamples = level > idleLevel ? 0 : s.quietSamples + 1;
        const double outLeft = pressureLeft + s.output[0] * wetLeft;
        const double outRight = pressureRight + s.output[0] * wetRight;
        const double micLeft = s.outputGain * (s.output[1] * outLeft + s.output[2] * outRight);
        const double micRight = s.outputGain * (s.output[1] * outRight + s.output[2] * outLeft);
        // Which way the element is wired is a choice: this way round, a
        // blend with the microphones adds in the low mids instead of
        // cancelling there.
        const double pickup = -s.piezoGain * piezo;
        const double blend = s.output[4];
        const double volume = s.output[3];
        // Unlimited: the Performer's output stage keeps it under full scale.
        // Written so that either end of the blend is exactly one source.
        left[sample] = static_cast<float>(volume * ((1.0 - blend) * micLeft + blend * pickup));
        right[sample] = static_cast<float>(volume * ((1.0 - blend) * micRight + blend * pickup));
        if (s.quietSamples >= static_cast<int>(0.5 * sampleRate_))
        {
            sleep();
            std::fill(left + sample + 1, left + numSamples, 0.0f);
            std::fill(right + sample + 1, right + numSamples, 0.0f);
            return;
        }
    }
}

AcukotoEngine::StringReport AcukotoEngine::stringReport(int string) const noexcept
{
    StringReport report {};
    if (string < 0 || string >= stringCount_)
        return report;
    const auto& voice = state_->strings[static_cast<std::size_t>(string)];
    const auto& spec = state_->instrument.strings[static_cast<std::size_t>(string)];
    report.midiNote = voice.midiNote;
    report.frequency = voice.frequency;
    report.tension = voice.tension;
    report.impedance = voice.impedance;
    report.inharmonicity = voice.inharmonicity;
    const double carrying = spec.coreDiameter > 0.0 ? spec.coreDiameter : spec.diameter;
    report.stress = voice.tension / (0.25 * pi * carrying * carrying);
    report.intrinsicT60 = voice.intrinsicT60;
    report.dispersion = voice.normal.dispersion;
    report.dispersionStages = voice.normal.dispersionStages;
    report.dispersionPairs = voice.normal.dispersionPairs;
    report.dispersionCents = voice.normal.dispersionCents;
    return report;
}

AcukotoEngine::PluckReport AcukotoEngine::lastPluck() const noexcept { return state_->lastPluck; }

double AcukotoEngine::stringEnergy(int string) const noexcept
{
    if (string < 0 || string >= stringCount_)
        return 0.0;
    return state_->strings[static_cast<std::size_t>(string)].energy;
}

AcukotoEngine::StringActivity AcukotoEngine::stringActivity(int string) const noexcept
{
    if (string < 0 || string >= stringCount_)
        return {};
    const auto& voice = state_->strings[static_cast<std::size_t>(string)];
    const bool pressedByHand = string == state_->handString && state_->handTarget > 1.0;
    return { voice.energy, voice.keyDown || voice.sustained, voice.plucked, voice.raised || pressedByHand };
}

int AcukotoEngine::modeCount() const noexcept { return state_->modeCount; }

AcukotoEngine::ModeReport AcukotoEngine::modeReport(int mode) const noexcept
{
    ModeReport report {};
    if (mode < 0 || mode >= state_->modeCount)
        return report;
    const Mode& m = state_->modes[static_cast<std::size_t>(mode)];
    report.frequency = m.frequency;
    report.q = m.q;
    report.heave = m.participation[static_cast<std::size_t>(stringCount_ / 2)];
    report.monopole = m.monopole;
    report.airShare = m.airShare;
    report.bodyShare = m.bodyShare;
    report.wallShare = m.wallShare;
    report.bridgeShare = m.bridgeShare;
    return report;
}

double AcukotoEngine::bridgeMobilityMagnitude(double frequency, int string) const noexcept
{
    const State& s = *state_;
    if (string < 0 || string >= stringCount_)
        return 0.0;
    const double omega = 2.0 * pi * frequency;
    Complex mobility = 0.0;
    for (int m = 0; m < s.modeCount; ++m)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(m)];
        const double wm = 2.0 * pi * mode.frequency;
        const double p = mode.participation[static_cast<std::size_t>(string)];
        mobility += Complex(0.0, omega) / Complex(wm * wm - omega * omega, omega * wm / mode.q) * p * p;
    }
    return std::abs(mobility);
}

} // namespace acukoto
